// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace twocan {

struct SlcanFrame {
    std::uint32_t id = 0;
    std::uint8_t dlc = 0;
    std::uint8_t data[8] = {0};
};

// Stateful parser for the Lawicel/SLCAN ASCII stream.  USB CDC reads may
// split a single SLCAN record at any byte boundary or return several records
// at once, so the partial line is retained between Feed() calls.
class SlcanStreamParser {
public:
    std::vector<SlcanFrame> Feed(const char* data, std::size_t length);
    void Reset();

    static bool ParseExtendedFrame(const std::string& line, SlcanFrame* frame);

private:
    std::string line_;
};

// Convert COM3, com12 or \\.\COM12 to canonical COM<number> form.
// Returns an empty string for an invalid port name.
std::wstring NormalizeComPort(const std::wstring& port);
std::wstring ComDevicePath(const std::wstring& port);

}  // namespace twocan
