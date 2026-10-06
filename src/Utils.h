#pragma once

#include "Crypto.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iomanip>
#include <map>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace onepassPSI
{
    class Timer
    {
    public:
        using Clock = std::chrono::steady_clock;

        void setTimePoint(std::string label) { mPoints.emplace_back(std::move(label), Clock::now()); }
        void reset() { mPoints.clear(); }

        // (label, ms since previous point); the first point is the origin.
        std::vector<std::pair<std::string, double>> deltas() const
        {
            std::vector<std::pair<std::string, double>> out;
            for (std::size_t i = 1; i < mPoints.size(); ++i)
                out.emplace_back(mPoints[i].first, ms(mPoints[i - 1].second, mPoints[i].second));
            return out;
        }

        double totalMs() const
        {
            return mPoints.size() < 2 ? 0.0 : ms(mPoints.front().second, mPoints.back().second);
        }

        friend std::ostream &operator<<(std::ostream &os, const Timer &t)
        {
            std::size_t w = 0;
            for (auto &[label, _] : t.mPoints)
                w = std::max(w, label.size());
            double acc = 0;
            for (auto &[label, d] : t.deltas())
            {
                acc += d;
                os << "  " << std::left << std::setw(static_cast<int>(w)) << label
                   << std::right << std::fixed << std::setprecision(3)
                   << std::setw(12) << d << " ms" << std::setw(12) << acc << " ms\n";
            }
            return os;
        }

    private:
        static double ms(Clock::time_point a, Clock::time_point b)
        {
            return std::chrono::duration<double, std::milli>(b - a).count();
        }

        std::vector<std::pair<std::string, Clock::time_point>> mPoints;
    };

    class TimerAdapter
    {
    public:
        void setTimer(Timer &t) { mTimer = &t; }

    protected:
        void setTimePoint(std::string label)
        {
            if (mTimer)
                mTimer->setTimePoint(std::move(label));
        }

    private:
        Timer *mTimer = nullptr;
    };

    // "-key v1 v2 -flag" style arguments. A token is a key if it starts with '-'
    // followed by a non-digit, so negative numbers are parsed as values.
    class Cli
    {
    public:
        Cli() = default;
        Cli(int argc, char **argv)
        {
            std::string key;
            for (int i = 1; i < argc; ++i)
            {
                std::string tok = argv[i];
                if (isKey(tok))
                {
                    key = tok.substr(1);
                    mArgs[key];
                }
                else if (!key.empty())
                    mArgs[key].push_back(tok);
                else
                    throw std::invalid_argument("unexpected argument: " + tok);
            }
        }

        bool isSet(const std::string &key) const { return mArgs.contains(key); }

        void set(const std::string &key, std::vector<std::string> values = {}) { mArgs[key] = std::move(values); }

        const std::vector<std::string> &values(const std::string &key) const
        {
            static const std::vector<std::string> empty;
            auto it = mArgs.find(key);
            return it == mArgs.end() ? empty : it->second;
        }

        template <class T>
        T get(const std::string &key) const
        {
            auto &v = values(key);
            if (v.empty())
                throw std::invalid_argument("missing value for -" + key);
            return parse<T>(key, v.front());
        }

        template <class T>
        T getOr(const std::string &key, T def) const
        {
            auto &v = values(key);
            return v.empty() ? def : parse<T>(key, v.front());
        }

    private:
        static bool isKey(const std::string &tok)
        {
            return tok.size() > 1 && tok[0] == '-' && !std::isdigit(static_cast<unsigned char>(tok[1]));
        }

        template <class T>
        static T parse(const std::string &key, const std::string &s)
        {
            if constexpr (std::is_same_v<T, std::string>)
                return s;
            else
            {
                std::istringstream in(s);
                T v{};
                if (!(in >> v) || !in.eof())
                    throw std::invalid_argument("bad value for -" + key + ": " + s);
                return v;
            }
        }

        std::map<std::string, std::vector<std::string>> mArgs;
    };

    // Calls fn(begin, end) on numThreads contiguous chunks of [0, n). Worker
    // threads get their own RELIC context; the caller's thread runs chunk 0.
    template <class Fn>
    void parallelFor(u64 n, u64 numThreads, Fn &&fn)
    {
        u64 nt = std::clamp<u64>(numThreads, 1, std::max<u64>(n, 1));
        if (nt == 1)
        {
            fn(u64{0}, n);
            return;
        }

        std::vector<std::exception_ptr> errs(nt);
        std::vector<std::thread> workers;
        auto chunk = [&](u64 t) {
            u64 b = n * t / nt, e = n * (t + 1) / nt;
            try
            {
                fn(b, e);
            }
            catch (...)
            {
                errs[t] = std::current_exception();
            }
        };

        for (u64 t = 1; t < nt; ++t)
            workers.emplace_back([&, t] {
                RelicThread relic;
                chunk(t);
            });
        chunk(0);
        for (auto &w : workers)
            w.join();
        for (auto &e : errs)
            if (e)
                std::rethrow_exception(e);
    }
}
