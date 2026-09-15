// SPDX-License-Identifier: GPL-3.0-or-later
#include "slcan_parser.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}

}  // namespace

int main() {
    using twocan::SlcanFrame;
    using twocan::SlcanStreamParser;

    // Deliberately split one extended NMEA2000 frame at awkward USB/CDC
    // boundaries. Feed() must retain the partial record between calls.
    SlcanStreamParser parser;
    std::vector<SlcanFrame> frames;
    const std::string p1 = "T19F112";
    const std::string p2 = "3408112233";
    const std::string p3 = "4455667788\r";
    auto out = parser.Feed(p1.data(), p1.size());
    Expect(out.empty(), "partial record produced a frame");
    out = parser.Feed(p2.data(), p2.size());
    Expect(out.empty(), "second partial record produced a frame");
    out = parser.Feed(p3.data(), p3.size());
    Expect(out.size() == 1, "fragmented extended frame not reconstructed");
    Expect(out[0].id == 0x19F11234u, "wrong 29-bit CAN identifier");
    Expect(out[0].dlc == 8, "wrong DLC");
    for (int i = 0; i < 8; ++i) {
        Expect(out[0].data[i] == static_cast<unsigned char>((i + 1) * 0x11), "wrong payload byte");
    }

    // Multiple records in one serial read, optional timestamp on the first,
    // CRLF tolerance on the second.
    const std::string multi =
        "T09F801230211220ABC\r"
        "T18EEFF0108AABBCCDDEEFF0011\r\n";
    out = parser.Feed(multi.data(), multi.size());
    Expect(out.size() == 2, "multiple SLCAN records were not parsed");
    Expect(out[0].id == 0x09F80123u && out[0].dlc == 2, "timestamped frame parse failed");
    Expect(out[0].data[0] == 0x11 && out[0].data[1] == 0x22, "timestamped frame payload failed");
    Expect(out[1].id == 0x18EEFF01u && out[1].dlc == 8, "second frame parse failed");

    // BEL response and malformed/standard CAN records must not leak a frame.
    const std::string invalid = "\a\rTFFFFFFFF8AABBCCDDEEFF0011\rt1231AA\r";
    out = parser.Feed(invalid.data(), invalid.size());
    Expect(out.empty(), "invalid SLCAN record accepted");

    // Windows COM normalization must support COM10+ and canonical device path.
    Expect(twocan::NormalizeComPort(L"COM3") == L"COM3", "COM3 normalize failed");
    Expect(twocan::NormalizeComPort(L" com12: ") == L"COM12", "COM12 normalize failed");
    Expect(twocan::NormalizeComPort(L"\\\\.\\COM256") == L"COM256", "device-path COM normalize failed");
    Expect(twocan::ComDevicePath(L"COM12") == L"\\\\.\\COM12", "COM10+ device path failed");
    Expect(twocan::NormalizeComPort(L"COM0").empty(), "COM0 should be invalid");
    Expect(twocan::NormalizeComPort(L"ttyUSB0").empty(), "non-COM port should be invalid");

    std::cout << "PASS: streaming SLCAN parser and Windows COM normalization" << std::endl;
    return 0;
}
