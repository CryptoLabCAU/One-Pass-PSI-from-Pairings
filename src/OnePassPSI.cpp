#include "OnePassPSI.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace onepassPSI
{
    namespace
    {
        // Fixed CRS trapdoor from the original implementation (test key only).
        constexpr const char *kS = "403E1F9BAAAF22CC51723F6F8DCCE8F46539FCC1A5C32C6F60776556913119A1";

        constexpr std::size_t kTagBytes = sizeof(Digest);
        constexpr std::size_t kPsiBytes = 4 * RLC_PC_BYTES + 1; // uncompressed G2

        Zp trapdoor()
        {
            Zp s;
            bn_read_str(s.get(), kS, static_cast<int>(std::strlen(kS)), 16);
            return s;
        }

        // Hex string of x as written by bn_write_str, including its terminator.
        Bytes hexOf(const Zp &x)
        {
            Bytes buf(bn_size_str(x.get(), 16));
            bn_write_str(reinterpret_cast<char *>(buf.data()), static_cast<int>(buf.size()), x.get(), 16);
            return buf;
        }

        // F: {0,1}* -> G1, F(x) = map(SHA3(hex(x))).
        G1 F(const Zp &x)
        {
            Digest d = sha3_256(hexOf(x));
            return G1::hash(d);
        }

        // H(mu) = SHA3(mu); H_hat(x, mu) = SHA3(hex(x) || mu).
        Digest H(bool malicious, const Zp &x, const Gt &mu)
        {
            Sha3 sha;
            if (malicious)
                sha.update(std::span<const u8>(hexOf(x)));
            return sha.update(mu).final();
        }

        bool tagLess(const u8 *a, const u8 *b) { return std::memcmp(a, b, kTagBytes) < 0; }
    }

    OnePassPSIPublicParams setup()
    {
        return {G2::generator() * trapdoor()};
    }

    // ---- Receiver -----------------------------------------------------------

    void OnePassPSIReceiver::init(const OnePassPSIParams &params, const OnePassPSIPublicParams &pp, std::span<const Zp> set)
    {
        mParams = params;
        mPp = pp;
        mSet.assign(set.begin(), set.end());
        mK.clear();
        mIntersection.clear();
    }

    void OnePassPSIReceiver::preprocess()
    {
        const u64 m = mSet.size();
        Zp rho = Zp::random();
        Zp s = trapdoor();
        Zp rhoInv = rho.inverse();
        mK.resize(m);

        parallelFor(m, mParams.numThreads, [&](u64 b, u64 e) {
            for (u64 j = b; j < e; ++j)
                mK[j] = F(mSet[j]) * rho; // K_j = F(y_j)^rho
        });
        setTimePoint("R F(y)^rho");

        parallelFor(m, mParams.numThreads, [&](u64 b, u64 e) {
            for (u64 j = b; j < e; ++j)
                mK[j] = mK[j] * s; // K_j = F(y_j)^(rho s)
        });
        setTimePoint("R F(y)^(rho s)");

        parallelFor(m, mParams.numThreads, [&](u64 b, u64 e) {
            for (u64 j = b; j < e; ++j)
                mK[j] = mK[j] * rhoInv; // K_j = F(y_j)^s
        });
        setTimePoint("R F(y)^s");
    }

    void OnePassPSIReceiver::online(Channel &chlS)
    {
        if (mK.size() != mSet.size())
            throw std::logic_error("receiver: online called before preprocess");

        const u64 m = mSet.size();
        mIntersection.clear();

        Bytes psiBytes(kPsiBytes);
        chlS.recv(std::span<u8>(psiBytes));
        const u64 n = mParams.senderSize;
        Bytes R(n * kTagBytes);
        chlS.recv(std::span<u8>(R));
        setTimePoint("R recv psi, R");

        G2 psi;
        g2_read_bin(psi.get(), psiBytes.data(), static_cast<int>(kPsiBytes));

        std::vector<const u8 *> sorted(n);
        for (u64 i = 0; i < n; ++i)
            sorted[i] = R.data() + i * kTagBytes;
        std::sort(sorted.begin(), sorted.end(), tagLess);

        std::vector<u8> hit(m, 0);
        parallelFor(m, mParams.numThreads, [&](u64 b, u64 e) {
            for (u64 j = b; j < e; ++j)
            {
                Digest t = H(mParams.malicious, mSet[j], pairing(mK[j], psi));
                hit[j] = std::binary_search(sorted.begin(), sorted.end(), t.data(), tagLess);
            }
        });
        for (u64 j = 0; j < m; ++j)
            if (hit[j])
                mIntersection.push_back(j);
        setTimePoint("R intersect");
    }

    // ---- Sender -------------------------------------------------------------

    void OnePassPSISender::init(const OnePassPSIParams &params, const OnePassPSIPublicParams &pp, std::span<const Zp> set)
    {
        mParams = params;
        mPp = pp;
        mSet.assign(set.begin(), set.end());
    }

    void OnePassPSISender::online(Channel &chlR)
    {
        const u64 n = mSet.size();
        if (n != mParams.senderSize)
            throw std::invalid_argument("sender: set size differs from params.senderSize");

        Zp r = Zp::random();
        G2 psi = G2::generator() * r;  // psi = g2^r
        G2 chi = mPp.gamma * r;        // chi = Gamma^r

        Bytes R(n * kTagBytes);
        parallelFor(n, mParams.numThreads, [&](u64 b, u64 e) {
            for (u64 i = b; i < e; ++i)
            {
                Digest t = H(mParams.malicious, mSet[i], pairing(F(mSet[i]), chi));
                std::memcpy(R.data() + i * kTagBytes, t.data(), kTagBytes);
            }
        });
        setTimePoint("S R_i");

        Bytes psiBytes(kPsiBytes);
        g2_write_bin(psiBytes.data(), static_cast<int>(kPsiBytes), psi.get(), 0);
        chlR.send(std::span<const u8>(psiBytes));
        chlR.send(std::span<const u8>(R));
        setTimePoint("S send");
    }
}
