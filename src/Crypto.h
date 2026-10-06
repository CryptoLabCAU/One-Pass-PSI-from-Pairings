#pragma once

#include <relic/relic.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct evp_md_ctx_st;

namespace onepassPSI
{
    using u8 = std::uint8_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using Bytes = std::vector<u8>;


    // RELIC is built with MULTI=PTHREAD, so every thread that touches RELIC
    // needs its own core context. Construct one at the top of each thread.
    class RelicThread
    {
    public:
        RelicThread();
        ~RelicThread();
        RelicThread(const RelicThread &) = delete;
        RelicThread &operator=(const RelicThread &) = delete;

    private:
        bool mOwns = false;
    };

    class Zp
    {
    public:
        static constexpr std::size_t kBytes = 32;

        Zp();
        Zp(u64 v);
        Zp(const Zp &o);
        Zp &operator=(const Zp &o);
        ~Zp();

        static Zp random();
        static Zp randomNonZero();
        static Zp order();
        static Zp fromHex(const std::string &hex);

        Zp operator+(const Zp &o) const;
        Zp operator-(const Zp &o) const;
        Zp operator*(const Zp &o) const;
        Zp operator-() const;
        Zp inverse() const;

        bool operator==(const Zp &o) const;
        bool isZero() const;

        void writeTo(u8 *out) const;
        static Zp readFrom(const u8 *in);

        bn_st *get() { return mV; }
        const bn_st *get() const { return mV; }
        operator bn_st *() { return mV; }
        operator const bn_st *() const { return mV; }

    private:
        bn_t mV;
    };

    class G1
    {
    public:
        static constexpr std::size_t kBytes = RLC_FP_BYTES + 1;

        G1();
        G1(const G1 &o);
        G1 &operator=(const G1 &o);
        ~G1();

        static G1 identity();
        static G1 generator();
        static G1 random();
        static G1 hash(std::span<const u8> msg);

        G1 operator+(const G1 &o) const;
        G1 operator-(const G1 &o) const;
        G1 operator-() const;
        G1 operator*(const Zp &k) const;
        G1 &operator+=(const G1 &o);

        bool operator==(const G1 &o) const;
        bool isIdentity() const;
        // On the curve, in the prime-order subgroup, and not the identity.
        bool isValid() const;

        void writeTo(u8 *out) const;
        static G1 readFrom(const u8 *in);

        ep_st *get() { return mV; }
        const ep_st *get() const { return mV; }
        operator ep_st *() { return mV; }
        operator const ep_st *() const { return mV; }

    private:
        g1_t mV;
    };

    class G2
    {
    public:
        static constexpr std::size_t kBytes = 2 * RLC_FP_BYTES + 1;

        G2();
        G2(const G2 &o);
        G2 &operator=(const G2 &o);
        ~G2();

        static G2 identity();
        static G2 generator();
        static G2 random();
        static G2 hash(std::span<const u8> msg);

        G2 operator+(const G2 &o) const;
        G2 operator-(const G2 &o) const;
        G2 operator-() const;
        G2 operator*(const Zp &k) const;
        G2 &operator+=(const G2 &o);

        bool operator==(const G2 &o) const;
        bool isIdentity() const;
        // On the curve, in the prime-order subgroup, and not the identity.
        bool isValid() const;

        void writeTo(u8 *out) const;
        static G2 readFrom(const u8 *in);

        ep2_st *get() { return mV; }
        const ep2_st *get() const { return mV; }
        operator ep2_st *() { return mV; }
        operator const ep2_st *() const { return mV; }

    private:
        g2_t mV;
    };

    class Gt
    {
    public:
        static constexpr std::size_t kBytes = 12 * RLC_FP_BYTES;

        Gt();
        Gt(const Gt &o);
        Gt &operator=(const Gt &o);
        ~Gt();

        static Gt identity();
        static Gt generator();

        Gt operator*(const Gt &o) const;
        Gt &operator*=(const Gt &o);
        Gt inverse() const;
        Gt pow(const Zp &k) const;

        bool operator==(const Gt &o) const;
        bool isIdentity() const;

        void writeTo(u8 *out) const;
        static Gt readFrom(const u8 *in);

        fp6_t *get() { return mV; }
        const fp6_t *get() const { return mV; }

    private:
        gt_t mV;
    };

    G1 operator*(const Zp &k, const G1 &p);
    G2 operator*(const Zp &k, const G2 &q);

    Gt pairing(const G1 &p, const G2 &q);

    // prod_i e(p[i], q[i]) with a single final exponentiation.
    Gt pairingProduct(std::span<const G1> p, std::span<const G2> q);

    using Digest = std::array<u8, 32>;

    class Sha3
    {
    public:
        Sha3();
        ~Sha3();
        Sha3(const Sha3 &) = delete;
        Sha3 &operator=(const Sha3 &) = delete;

        Sha3 &update(std::span<const u8> data);
        Sha3 &update(std::string_view data);

        template <class T>
            requires requires(const T &t, u8 *out) { t.writeTo(out); T::kBytes; }
        Sha3 &update(const T &t)
        {
            std::array<u8, T::kBytes> buf;
            t.writeTo(buf.data());
            return update(std::span<const u8>(buf));
        }

        Digest final();

    private:
        evp_md_ctx_st *mCtx;
    };

    Digest sha3_256(std::span<const u8> data);

}
