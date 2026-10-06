#pragma once

#include "Crypto.h"
#include "Network.h"
#include "Utils.h"

#include <span>
#include <vector>

namespace onepassPSI
{
    struct OnePassPSIParams
    {
        u64 senderSize = 0;
        u64 receiverSize = 0;
        u64 numThreads = 1;
        // Malicious model uses H_hat(x, mu) instead of H(mu).
        bool malicious = false;
    };

    // CRS: Gamma = g2^s for the fixed trapdoor s.
    struct OnePassPSIPublicParams
    {
        G2 gamma;
    };

    OnePassPSIPublicParams setup();

    class OnePassPSIReceiver : public TimerAdapter
    {
    public:
        void init(const OnePassPSIParams &params, const OnePassPSIPublicParams &pp, std::span<const Zp> set);

        // Receiver-set registration: K_j = F(y_j)^s, computed through the blinding rho.
        void preprocess();

        // Receives (psi, R_1..R_n) and computes the intersection.
        void online(Channel &chlS);

        // Indices into the receiver's set.
        std::vector<u64> mIntersection;

    private:
        OnePassPSIParams mParams;
        OnePassPSIPublicParams mPp;
        std::vector<Zp> mSet;
        std::vector<G1> mK;
    };

    class OnePassPSISender : public TimerAdapter
    {
    public:
        void init(const OnePassPSIParams &params, const OnePassPSIPublicParams &pp, std::span<const Zp> set);

        // Sends (psi, R_1..R_n); nothing is received.
        void online(Channel &chlR);

    private:
        OnePassPSIParams mParams;
        OnePassPSIPublicParams mPp;
        std::vector<Zp> mSet;
    };
}
