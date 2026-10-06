#include "test/Test.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <iostream>

namespace onepassPSI
{
    namespace
    {
        constexpr const char *kGreen = "\033[32m";
        constexpr const char *kRed = "\033[31m";
        constexpr const char *kReset = "\033[0m";

        bool isNumber(const std::string &s)
        {
            return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
        }

        std::string lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
            return s;
        }
    }

    void expect(bool cond, const std::string &what)
    {
        if (!cond)
            throw std::runtime_error("expectation failed: " + what);
    }

    void TestCollection::list() const
    {
        for (std::size_t i = 0; i < mTests.size(); ++i)
            std::cout << std::setw(3) << i << " - " << mTests[i].name << "\n";
    }

    std::vector<std::size_t> TestCollection::select(const std::vector<std::string> &patterns) const
    {
        std::vector<std::size_t> idx;
        if (patterns.empty())
        {
            for (std::size_t i = 0; i < mTests.size(); ++i)
                idx.push_back(i);
            return idx;
        }

        for (auto &p : patterns)
        {
            if (auto dots = p.find(".."); dots != std::string::npos &&
                                          isNumber(p.substr(0, dots)) && isNumber(p.substr(dots + 2)))
            {
                std::size_t b = std::stoul(p.substr(0, dots)), e = std::stoul(p.substr(dots + 2));
                for (std::size_t i = b; i < std::min(e, mTests.size()); ++i)
                    idx.push_back(i);
            }
            else if (isNumber(p))
            {
                std::size_t i = std::stoul(p);
                if (i >= mTests.size())
                    throw std::out_of_range("no unit test with index " + p);
                idx.push_back(i);
            }
            else
            {
                for (std::size_t i = 0; i < mTests.size(); ++i)
                    if (lower(mTests[i].name).find(lower(p)) != std::string::npos)
                        idx.push_back(i);
            }
        }
        return idx;
    }

    int TestCollection::run(const Cli &cmd) const
    {
        if (cmd.isSet("list"))
        {
            list();
            return 0;
        }

        auto idx = select(cmd.values("u"));
        std::size_t w = 0;
        for (auto i : idx)
            w = std::max(w, mTests[i].name.size());

        int passed = 0, failed = 0;
        for (auto i : idx)
        {
            auto &t = mTests[i];
            std::cout << std::right << std::setw(3) << i << " - " << std::left << std::setw(static_cast<int>(w)) << t.name
                      << std::right << std::flush;

            auto start = std::chrono::steady_clock::now();
            std::string err;
            try
            {
                onepassPSI::RelicThread relic;
                t.fn(cmd);
            }
            catch (const std::exception &e)
            {
                err = e.what();
            }
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

            if (err.empty())
            {
                ++passed;
                std::cout << "  " << kGreen << "Passed" << kReset << "  " << std::fixed << std::setprecision(1)
                          << ms << " ms\n";
            }
            else
            {
                ++failed;
                std::cout << "  " << kRed << "Failed" << kReset << "  " << err << "\n";
            }
        }

        std::cout << "\n"
                  << passed << " passed, " << failed << " failed\n";
        return failed;
    }

    const TestCollection Tests([](TestCollection &t) {
        t.add("OnePassPSI_semihonest_test", OnePassPSI_semihonest_test);
        t.add("OnePassPSI_malicious_test", OnePassPSI_malicious_test);
    });
}
