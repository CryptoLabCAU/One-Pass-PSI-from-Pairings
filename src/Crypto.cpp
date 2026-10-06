#include "Crypto.h"

#include <algorithm>
#include <cstring>
#include <openssl/evp.h>
#include <stdexcept>
#include <type_traits>

namespace onepassPSI
{
    static_assert(std::is_standard_layout_v<G1> && sizeof(G1) == sizeof(g1_t));
    static_assert(std::is_standard_layout_v<G2> && sizeof(G2) == sizeof(g2_t));

    RelicThread::RelicThread()
    {
        if (core_get() != nullptr)
            return;
        if (core_init() != RLC_OK)
            throw std::runtime_error("RELIC core_init failed");
        if (pc_param_set_any() != RLC_OK)
        {
            core_clean();
            throw std::runtime_error("RELIC pc_param_set_any failed");
        }
        mOwns = true;
    }

    RelicThread::~RelicThread()
    {
        if (mOwns)
            core_clean();
    }

    // ---- Zp -----------------------------------------------------------------

    namespace
    {
        void reduce(bn_t a)
        {
            bn_t n;
            bn_null(n);
            bn_new(n);
            pc_get_ord(n);
            bn_mod(a, a, n);
            if (bn_sign(a) == RLC_NEG)
                bn_add(a, a, n);
            bn_free(n);
        }
    }

    Zp::Zp()
    {
        bn_null(mV);
        bn_new(mV);
        bn_zero(mV);
    }

    Zp::Zp(u64 v) : Zp()
    {
        bn_set_dig(mV, v);
        reduce(mV);
    }

    Zp::Zp(const Zp &o) : Zp() { bn_copy(mV, o.mV); }

    Zp &Zp::operator=(const Zp &o)
    {
        bn_copy(mV, o.mV);
        return *this;
    }

    Zp::~Zp() { bn_free(mV); }

    Zp Zp::random()
    {
        Zp r, n = order();
        bn_rand_mod(r.mV, n.mV);
        return r;
    }

    Zp Zp::randomNonZero()
    {
        Zp r;
        do
            r = random();
        while (r.isZero());
        return r;
    }

    Zp Zp::order()
    {
        Zp n;
        pc_get_ord(n.mV);
        return n;
    }

    Zp Zp::fromHex(const std::string &hex)
    {
        Zp r;
        bn_read_str(r.mV, hex.c_str(), static_cast<int>(hex.size()), 16);
        reduce(r.mV);
        return r;
    }

    Zp Zp::operator+(const Zp &o) const
    {
        Zp r;
        bn_add(r.mV, mV, o.mV);
        reduce(r.mV);
        return r;
    }

    Zp Zp::operator-(const Zp &o) const
    {
        Zp r;
        bn_sub(r.mV, mV, o.mV);
        reduce(r.mV);
        return r;
    }

    Zp Zp::operator*(const Zp &o) const
    {
        Zp r;
        bn_mul(r.mV, mV, o.mV);
        reduce(r.mV);
        return r;
    }

    Zp Zp::operator-() const { return Zp() - *this; }

    Zp Zp::inverse() const
    {
        if (isZero())
            throw std::domain_error("Zp::inverse of zero");
        Zp r, n = order();
        bn_mod_inv(r.mV, mV, n.mV);
        return r;
    }

    bool Zp::operator==(const Zp &o) const { return bn_cmp(mV, o.mV) == RLC_EQ; }

    bool Zp::isZero() const { return bn_is_zero(mV); }

    void Zp::writeTo(u8 *out) const { bn_write_bin(out, kBytes, mV); }

    Zp Zp::readFrom(const u8 *in)
    {
        Zp r;
        bn_read_bin(r.mV, in, kBytes);
        return r;
    }

    // ---- G1 -----------------------------------------------------------------

    G1::G1()
    {
        g1_null(mV);
        g1_new(mV);
        g1_set_infty(mV);
    }

    G1::G1(const G1 &o) : G1() { g1_copy(mV, o.mV); }

    G1 &G1::operator=(const G1 &o)
    {
        g1_copy(mV, o.mV);
        return *this;
    }

    G1::~G1() { g1_free(mV); }

    G1 G1::identity() { return G1(); }

    G1 G1::generator()
    {
        G1 r;
        g1_get_gen(r.mV);
        return r;
    }

    G1 G1::random()
    {
        G1 r;
        g1_rand(r.mV);
        return r;
    }

    G1 G1::hash(std::span<const u8> msg)
    {
        G1 r;
        g1_map(r.mV, msg.data(), msg.size());
        return r;
    }

    G1 G1::operator+(const G1 &o) const
    {
        G1 r;
        g1_add(r.mV, mV, o.mV);
        return r;
    }

    G1 G1::operator-(const G1 &o) const
    {
        G1 r;
        g1_sub(r.mV, mV, o.mV);
        return r;
    }

    G1 G1::operator-() const
    {
        G1 r;
        g1_neg(r.mV, mV);
        return r;
    }

    G1 G1::operator*(const Zp &k) const
    {
        G1 r;
        g1_mul(r.mV, mV, k.get());
        return r;
    }

    G1 &G1::operator+=(const G1 &o)
    {
        g1_add(mV, mV, o.mV);
        return *this;
    }

    bool G1::operator==(const G1 &o) const { return g1_cmp(mV, o.mV) == RLC_EQ; }

    bool G1::isIdentity() const { return g1_is_infty(mV); }

    bool G1::isValid() const { return g1_is_valid(mV); }

    // RELIC encodes the identity as a single zero byte; use an all-zero
    // fixed-length encoding instead so every element is exactly kBytes.
    void G1::writeTo(u8 *out) const
    {
        if (isIdentity())
            std::memset(out, 0, kBytes);
        else
            g1_write_bin(out, kBytes, mV, 1);
    }

    G1 G1::readFrom(const u8 *in)
    {
        G1 r;
        if (std::all_of(in, in + kBytes, [](u8 b) { return b == 0; }))
            return r;
        g1_read_bin(r.mV, in, kBytes);
        return r;
    }

    // ---- G2 -----------------------------------------------------------------

    G2::G2()
    {
        g2_null(mV);
        g2_new(mV);
        g2_set_infty(mV);
    }

    G2::G2(const G2 &o) : G2() { g2_copy(mV, o.mV); }

    G2 &G2::operator=(const G2 &o)
    {
        g2_copy(mV, o.mV);
        return *this;
    }

    G2::~G2() { g2_free(mV); }

    G2 G2::identity() { return G2(); }

    G2 G2::generator()
    {
        G2 r;
        g2_get_gen(r.mV);
        return r;
    }

    G2 G2::random()
    {
        G2 r;
        g2_rand(r.mV);
        return r;
    }

    G2 G2::hash(std::span<const u8> msg)
    {
        G2 r;
        g2_map(r.mV, msg.data(), msg.size());
        return r;
    }

    G2 G2::operator+(const G2 &o) const
    {
        G2 r;
        g2_add(r.mV, mV, o.mV);
        return r;
    }

    G2 G2::operator-(const G2 &o) const
    {
        G2 r;
        g2_sub(r.mV, mV, o.mV);
        return r;
    }

    G2 G2::operator-() const
    {
        G2 r;
        g2_neg(r.mV, mV);
        return r;
    }

    G2 G2::operator*(const Zp &k) const
    {
        G2 r;
        g2_mul(r.mV, mV, k.get());
        return r;
    }

    G2 &G2::operator+=(const G2 &o)
    {
        g2_add(mV, mV, o.mV);
        return *this;
    }

    bool G2::operator==(const G2 &o) const { return g2_cmp(mV, o.mV) == RLC_EQ; }

    bool G2::isIdentity() const { return g2_is_infty(mV); }

    bool G2::isValid() const { return g2_is_valid(mV); }

    // RELIC encodes the identity as a single zero byte; use an all-zero
    // fixed-length encoding instead so every element is exactly kBytes.
    void G2::writeTo(u8 *out) const
    {
        if (isIdentity())
            std::memset(out, 0, kBytes);
        else
            g2_write_bin(out, kBytes, mV, 1);
    }

    G2 G2::readFrom(const u8 *in)
    {
        G2 r;
        if (std::all_of(in, in + kBytes, [](u8 b) { return b == 0; }))
            return r;
        g2_read_bin(r.mV, in, kBytes);
        return r;
    }

    // ---- Gt -----------------------------------------------------------------

    Gt::Gt()
    {
        gt_null(mV);
        gt_new(mV);
        gt_set_unity(mV);
    }

    Gt::Gt(const Gt &o) : Gt() { gt_copy(mV, o.mV); }

    Gt &Gt::operator=(const Gt &o)
    {
        gt_copy(mV, o.mV);
        return *this;
    }

    Gt::~Gt() { gt_free(mV); }

    Gt Gt::identity() { return Gt(); }

    Gt Gt::generator()
    {
        Gt r;
        gt_get_gen(r.mV);
        return r;
    }

    Gt Gt::operator*(const Gt &o) const
    {
        Gt r;
        gt_mul(r.mV, mV, o.mV);
        return r;
    }

    Gt &Gt::operator*=(const Gt &o)
    {
        gt_mul(mV, mV, o.mV);
        return *this;
    }

    Gt Gt::inverse() const
    {
        Gt r;
        gt_inv(r.mV, mV);
        return r;
    }

    Gt Gt::pow(const Zp &k) const
    {
        Gt r;
        gt_exp(r.mV, mV, k.get());
        return r;
    }

    bool Gt::operator==(const Gt &o) const { return gt_cmp(mV, o.mV) == RLC_EQ; }

    bool Gt::isIdentity() const { return gt_is_unity(mV); }

    void Gt::writeTo(u8 *out) const { gt_write_bin(out, kBytes, mV, 0); }

    Gt Gt::readFrom(const u8 *in)
    {
        Gt r;
        gt_read_bin(r.mV, in, kBytes);
        return r;
    }

    // ---- pairing ------------------------------------------------------------

    G1 operator*(const Zp &k, const G1 &p) { return p * k; }
    G2 operator*(const Zp &k, const G2 &q) { return q * k; }

    Gt pairing(const G1 &p, const G2 &q)
    {
        Gt r;
        pc_map(r.get(), p.get(), q.get());
        return r;
    }

    Gt pairingProduct(std::span<const G1> p, std::span<const G2> q)
    {
        if (p.size() != q.size())
            throw std::invalid_argument("pairingProduct: size mismatch");
        Gt r;
        if (p.empty())
            return r;
        pc_map_sim(r.get(),
                   reinterpret_cast<const g1_t *>(p.data()),
                   reinterpret_cast<const g2_t *>(q.data()),
                   static_cast<int>(p.size()));
        return r;
    }

    Sha3::Sha3() : mCtx(EVP_MD_CTX_new())
    {
        if (mCtx == nullptr || EVP_DigestInit_ex(mCtx, EVP_sha3_256(), nullptr) != 1)
            throw std::runtime_error("SHA3-256 init failed");
    }

    Sha3::~Sha3() { EVP_MD_CTX_free(mCtx); }

    Sha3 &Sha3::update(std::span<const u8> data)
    {
        if (EVP_DigestUpdate(mCtx, data.data(), data.size()) != 1)
            throw std::runtime_error("SHA3-256 update failed");
        return *this;
    }

    Sha3 &Sha3::update(std::string_view data)
    {
        return update(std::span<const u8>(reinterpret_cast<const u8 *>(data.data()), data.size()));
    }

    Digest Sha3::final()
    {
        Digest d;
        unsigned int len = 0;
        if (EVP_DigestFinal_ex(mCtx, d.data(), &len) != 1 || len != d.size())
            throw std::runtime_error("SHA3-256 final failed");
        return d;
    }

    Digest sha3_256(std::span<const u8> data) { return Sha3().update(data).final(); }
}
