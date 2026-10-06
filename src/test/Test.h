#pragma once

#include "Crypto.h"
#include "Network.h"
#include "Utils.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace onepassPSI
{
    using TestFn = std::function<void(const Cli &)>;

    class TestCollection
    {
    public:
        struct Test
        {
            std::string name;
            TestFn fn;
        };

        TestCollection() = default;
        explicit TestCollection(std::function<void(TestCollection &)> init) { init(*this); }

        void add(std::string name, TestFn fn) { mTests.push_back({std::move(name), std::move(fn)}); }

        // Handles "-u [-list] [idx | a..b | substring]...". Returns the number of failures.
        int run(const Cli &cmd) const;

        void list() const;

    private:
        std::vector<std::size_t> select(const std::vector<std::string> &patterns) const;

        std::vector<Test> mTests;
    };

    void expect(bool cond, const std::string &what);

    struct TestSets
    {
        std::vector<Zp> sender;
        std::vector<Zp> receiver;
        std::vector<u64> expected; // sorted indices into receiver
    };

    // Random elements of Z_p, as in the original implementation, with exactly
    // `overlap` common elements at random positions.
    inline TestSets makeSets(u64 n, u64 m, u64 overlap)
    {
        if (overlap > std::min(n, m))
            throw std::invalid_argument("overlap exceeds set size");

        TestSets s;
        s.sender.resize(n);
        s.receiver.resize(m);
        for (auto &e : s.sender)
            e = Zp::random();
        for (auto &e : s.receiver)
            e = Zp::random();

        std::mt19937_64 prng(1);
        std::vector<u64> xi(n), yi(m);
        std::iota(xi.begin(), xi.end(), 0);
        std::iota(yi.begin(), yi.end(), 0);
        std::shuffle(xi.begin(), xi.end(), prng);
        std::shuffle(yi.begin(), yi.end(), prng);
        for (u64 i = 0; i < overlap; ++i)
        {
            s.receiver[yi[i]] = s.sender[xi[i]];
            s.expected.push_back(yi[i]);
        }
        std::sort(s.expected.begin(), s.expected.end());
        return s;
    }

    // Runs f0 on the calling thread and f1 on a new one, joining before
    // returning so each call acts as a phase barrier.
    template <class F0, class F1>
    double runPair(F0 &&f0, F1 &&f1)
    {
        auto start = std::chrono::steady_clock::now();
        std::exception_ptr e0, e1;
        std::thread t([&] {
            try
            {
                RelicThread relic;
                f1();
            }
            catch (...)
            {
                e1 = std::current_exception();
            }
        });
        try
        {
            f0();
        }
        catch (...)
        {
            e0 = std::current_exception();
        }
        t.join();
        if (e0)
            std::rethrow_exception(e0);
        if (e1)
            std::rethrow_exception(e1);
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }

    struct ChannelPair
    {
        std::unique_ptr<Channel> first;
        std::unique_ptr<Channel> second;
    };

    // -online: loopback TCP on -port (default 12345); otherwise in-memory.
    inline ChannelPair makeChannels(const Cli &cmd)
    {
        ChannelPair p;
        if (cmd.isSet("online"))
        {
            u32 port = cmd.getOr<u32>("port", 12345);
            runPair([&] { p.first = std::make_unique<TcpChannel>(TcpChannel::listen(port)); },
                    [&] { p.second = std::make_unique<TcpChannel>(TcpChannel::connect("127.0.0.1", port)); });
        }
        else
        {
            auto [a, b] = LocalChannel::makePair();
            p.first = std::make_unique<LocalChannel>(std::move(a));
            p.second = std::make_unique<LocalChannel>(std::move(b));
        }
        return p;
    }

    void OnePassPSI_semihonest_test(const Cli &cmd);
    void OnePassPSI_malicious_test(const Cli &cmd);

    extern const TestCollection Tests;
}
