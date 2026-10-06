#include "OnePassPSI.h"
#include "test/Test.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace onepassPSI
{
    namespace
    {
        // Time is computation only: wall time minus time blocked waiting for the peer.
        struct PartyStats
        {
            double ms = 0;
            u64 sent = 0;
        };

        struct PhaseStats
        {
            double ms = 0;
            PartyStats first, second;
        };

        struct RunResult
        {
            double setupMs = 0;
            double preprocessMs = 0; // receiver only, no communication
            PhaseStats online;       // first = sender, second = receiver
            std::vector<u64> intersection;
            Timer rTimer, sTimer;
        };

        double msSince(std::chrono::steady_clock::time_point t0)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        }

        template <class F0, class F1>
        PhaseStats runPhase(F0 &&f0, Channel &c0, F1 &&f1, Channel &c1)
        {
            PhaseStats p;
            auto timed = [](auto &f, Channel &c, PartyStats &out) {
                auto t0 = std::chrono::steady_clock::now();
                f();
                out.ms = msSince(t0) - c.recvWaitMs();
                out.sent = c.bytesSent();
            };
            p.ms = runPair([&] { timed(f0, c0, p.first); }, [&] { timed(f1, c1, p.second); });
            return p;
        }

        RunResult runProtocol(const Cli &cmd, const OnePassPSIParams &params, const TestSets &sets)
        {
            auto chls = makeChannels(cmd); // first: receiver, second: sender

            RunResult r;
            OnePassPSIReceiver R;
            OnePassPSISender S;
            R.setTimer(r.rTimer);
            S.setTimer(r.sTimer);

            auto t0 = std::chrono::steady_clock::now();
            OnePassPSIPublicParams pp = setup();
            r.setupMs = msSince(t0);

            R.init(params, pp, sets.receiver);
            S.init(params, pp, sets.sender);

            r.rTimer.setTimePoint("begin");
            t0 = std::chrono::steady_clock::now();
            R.preprocess();
            r.preprocessMs = msSince(t0);

            r.sTimer.setTimePoint("begin");
            r.online = runPhase([&] { S.online(*chls.second); }, *chls.second,
                                [&] { R.online(*chls.first); }, *chls.first);

            r.intersection = R.mIntersection;
            std::sort(r.intersection.begin(), r.intersection.end());
            return r;
        }

        std::string comm(u64 bytes)
        {
            std::ostringstream os;
            os << std::fixed << std::setprecision(3) << static_cast<double>(bytes) / (1 << 20) << " MB ("
               << std::setprecision(2) << static_cast<double>(bytes) / (1 << 10) << " KB)";
            return os.str();
        }

        std::string ms(double v)
        {
            std::ostringstream os;
            os << std::fixed << std::setprecision(3) << v << " ms";
            return os.str();
        }

        void printPhase(const std::string &title, const PhaseStats &p, const char *first, const char *second)
        {
            std::size_t w = std::max(std::strlen(first), std::strlen(second)) + 5;
            auto row = [&](const std::string &label, const std::string &value) {
                std::cout << "  " << std::left << std::setw(static_cast<int>(w)) << label << " = " << value << "\n";
            };
            std::cout << "\n" << title << "\n";
            row("Total Time", ms(p.ms));
            row(std::string(first) + " Time", ms(p.first.ms));
            row(std::string(second) + " Time", ms(p.second.ms));
            row("Total Comm", comm(p.first.sent + p.second.sent));
            row(std::string(first) + " Comm", comm(p.first.sent));
            row(std::string(second) + " Comm", comm(p.second.sent));
        }

        void runTest(const Cli &cmd, bool malicious)
        {
            u64 nn = cmd.getOr<u64>("nn", 10), mm = cmd.getOr<u64>("mm", 10);
            u64 n = 1ull << nn, m = 1ull << mm;

            OnePassPSIParams params;
            params.senderSize = n;
            params.receiverSize = m;
            params.numThreads = cmd.getOr<u64>("nt", 1);
            params.malicious = malicious;

            auto sets = makeSets(n, m, std::min(n, m) / 2);
            auto r = runProtocol(cmd, params, sets);
            expect(r.intersection == sets.expected,
                   "intersection size " + std::to_string(r.intersection.size()) +
                       ", expected " + std::to_string(sets.expected.size()));

            std::cout << "\n\nOne-Pass PSI (" << (malicious ? "malicious" : "semi-honest")
                      << ")  n = 2^" << nn << ", m = 2^" << mm << ", nt = " << params.numThreads
                      << (cmd.isSet("online") ? ", tcp" : ", local") << "\n";
            std::cout << "\nSetup Phase\n  Time = " << ms(r.setupMs) << "\n";
            std::cout << "\nPreprocessing Phase\n  Receiver Time = " << ms(r.preprocessMs) << "\n";
            printPhase("Online Phase", r.online, "Sender", "Receiver");
            std::cout << "\n";

            if (cmd.isSet("v"))
                std::cout << "Receiver timer:\n"
                          << r.rTimer << "Sender timer:\n"
                          << r.sTimer;
        }
    }

    void OnePassPSI_semihonest_test(const Cli &cmd) { runTest(cmd, false); }

    void OnePassPSI_malicious_test(const Cli &cmd) { runTest(cmd, true); }
}
