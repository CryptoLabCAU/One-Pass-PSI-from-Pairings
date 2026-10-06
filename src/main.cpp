#include "Utils.h"
#include "test/Test.h"

#include <exception>
#include <iostream>

namespace
{
    void printUsage()
    {
        std::cout
            << "Usage:\n"
            << "  -u -list           list tests\n"
            << "  -u                 run all tests\n"
            << "  -u <idx>           run the test with that index\n\n"
            << "Parameters:\n"
            << "  -nn <v>            log2 sender set size (default 10)\n"
            << "  -mm <v>            log2 receiver set size (default 10)\n"
            << "  -nt <v>            number of threads (default 1)\n"
            << "  -online            use loopback TCP instead of in-memory channels\n"
            << "  -v                 print detailed timers\n\n"
            << "Example:\n"
            << "  ./frontend -u 0 -nn 12 -mm 12     # run test 0: semi-honest One-Pass PSI\n"
            << "  ./frontend -u 1 -nn 12 -mm 12     # run test 1: malicious One-Pass PSI\n";
    }
}

int main(int argc, char **argv)
{
    try
    {
        onepassPSI::Cli cmd(argc, argv);
        if (cmd.isSet("u"))
            return onepassPSI::Tests.run(cmd) == 0 ? 0 : 1;
        printUsage();
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
