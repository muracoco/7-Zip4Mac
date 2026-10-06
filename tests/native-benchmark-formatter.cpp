// SPDX-License-Identifier: LGPL-3.0-or-later
// Exercise the same official arithmetic and GUI presentation as the bundle.
#include "CPP/Common/MyInitGuid.h"
#include "CPP/7zip/UI/Common/ArchiveExtractCallback.h"
#include "NativeBenchmark.h"
#include <cstdio>
#include <cstdlib>
#include <string>
int Main2(int argc, char **argv) {
    if (argc == 4 && std::string(argv[1]) == "--memory") {
        std::printf("%llu\n", (unsigned long long)GetBenchMemoryUsage((UInt32)std::strtoul(argv[3], nullptr, 10), 5, std::strtoull(argv[2], nullptr, 10), false));
        return 0;
    }
    unsigned passed = 0, failed = 0;
    auto check = [&](const char *name, bool value) {
        if (value) { ++passed; std::printf("PASS: %s\n", name); }
        else { ++failed; std::fprintf(stderr, "FAIL: %s\n", name); }
    };
    CTotalBenchRes a, b; a.Init(); b.Init();
    check("Undefined result remains undefined", PortBenchmark::ResultValue(a, 0) == "{\"defined\":false}");
    a.NumIterations2 = b.NumIterations2 = 1;
    a.Speed = 2047; b.Speed = 2049;
    a.Rating = a.RPU = 1400000; b.Rating = b.RPU = 2400000;
    a.Usage = b.Usage = 65536;
    a.Update_With_Res(b);
    const auto result = PortBenchmark::ResultValue(a, UInt64(42) << 20);
    check("Unrounded speed and rating are accumulated before presentation", result.find("\"speed\":\"2\"") != std::string::npos && result.find("\"rating\":\"2\"") != std::string::npos);
    check("Windows GIPS, usage and MB units", result.find("\"usageText\":\"100%\"") != std::string::npos && result.find("\"ratingText\":\"0.002 GIPS\"") != std::string::npos && result.find("\"sizeText\":\"42 MB\"") != std::string::npos);
    const auto large = PortBenchmark::ResultValue(a, UInt64(1) << 40);
    check("Windows large-size threshold", large.find("\"sizeText\":\"1024 GB\"") != std::string::npos);
    std::printf("Native Benchmark formatter: %u passed, %u failed\n", passed, failed);
    return failed ? 2 : 0;
}
