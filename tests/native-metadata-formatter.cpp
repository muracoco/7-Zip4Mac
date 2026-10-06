// SPDX-License-Identifier: LGPL-3.0-or-later
// Runs against the same native adapter and official formatter as the bundle.
#include "CPP/Common/MyInitGuid.h"
#include "NativeMetadata.h"
#include "CPP/7zip/UI/Common/ArchiveExtractCallback.h"
#include <cstdio>
#include <vector>
#include <string>
int Main2(int, char **) {
    unsigned passed = 0, failed = 0;
    auto check = [&](const char *name, const std::string &actual, const std::string &expected) {
        if (actual == expected) { ++passed; std::printf("PASS: %s\n", name); }
        else { ++failed; std::fprintf(stderr, "FAIL: %s: %s != %s\n", name, actual.c_str(), expected.c_str()); }
    };
    const Byte crc[]{0x12, 0x34, 0xab, 0xcd};
    check("raw CRC upper hex", PortMetadata::RawProperty(kpidCRC, crc, 4, false), "1234ABCD");
    check("raw checksum upper hex", PortMetadata::RawProperty(kpidChecksum, crc, 4, false), "1234ABCD");
    check("other raw lower hex", PortMetadata::RawProperty(kpidSha1, crc, 4, false), "1234abcd");
    std::vector<Byte> data(257, 0xab);
    std::string hex256; for (int i = 0; i < 256; ++i) hex256 += "ab";
    check("64-byte list boundary", PortMetadata::RawProperty(kpidSha1, data.data(), 64, true), hex256.substr(0, 128));
    check("65-byte list abbreviation", PortMetadata::RawProperty(kpidSha1, data.data(), 65, true), "data:65");
    check("256-byte Properties boundary", PortMetadata::RawProperty(kpidSha1, data.data(), 256, false), hex256);
    check("257-byte Properties abbreviation", PortMetadata::RawProperty(kpidSha1, data.data(), 257, false), "data:257");
    check("empty raw value", PortMetadata::RawProperty(kpidSha1, nullptr, 0, false), "");
    std::vector<Byte> secure(20, 0); secure[0] = 1;
    check("portable NT security descriptor", PortMetadata::RawProperty(kpidNtSecure, secure.data(), secure.size(), false), "S-1-0 S-1-0 20");
    check("short NT security descriptor", PortMetadata::RawProperty(kpidNtSecure, secure.data(), 19, false), "ERROR");
    secure[0] = 2;
    check("unsupported NT security revision", PortMetadata::RawProperty(kpidNtSecure, secure.data(), 20, false), "UNSUPPORTED");
    std::printf("Native metadata formatter: %u passed, %u failed\n", passed, failed);
    return failed ? 2 : 0;
}
