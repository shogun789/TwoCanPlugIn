// SPDX-License-Identifier: GPL-3.0-or-later
#include "slcan_parser.h"

#include <algorithm>
#include <cwctype>

namespace twocan {
namespace {

int HexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool ParseHex(const std::string& text, std::size_t offset, std::size_t digits,
              std::uint32_t* value) {
    if (!value || offset + digits > text.size()) return false;
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < digits; ++i) {
        const int nibble = HexNibble(text[offset + i]);
        if (nibble < 0) return false;
        result = (result << 4) | static_cast<std::uint32_t>(nibble);
    }
    *value = result;
    return true;
}

}  // namespace

std::vector<SlcanFrame> SlcanStreamParser::Feed(const char* data, std::size_t length) {
    std::vector<SlcanFrame> frames;
    if (!data || length == 0) return frames;

    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char ch = static_cast<unsigned char>(data[i]);

        // Lawicel uses CR as the record terminator. Ignore LF so the parser is
        // also tolerant of bridges/terminal programs which convert CR to CRLF.
        if (ch == '\n') continue;

        if (ch == '\a') {
            // BEL is an SLCAN command/error response, not a CAN record.
            line_.clear();
            continue;
        }

        if (ch == '\r') {
            SlcanFrame frame;
            if (ParseExtendedFrame(line_, &frame)) frames.push_back(frame);
            line_.clear();
            continue;
        }

        // A valid CAN data frame is short (< 32 bytes). Keep generous room for
        // optional four-hex-digit SLCAN timestamps, but recover cleanly from a
        // corrupt/unbounded stream instead of overflowing a fixed buffer.
        if (line_.size() < 128) {
            line_.push_back(static_cast<char>(ch));
        } else {
            line_.clear();
        }
    }

    return frames;
}

void SlcanStreamParser::Reset() { line_.clear(); }

bool SlcanStreamParser::ParseExtendedFrame(const std::string& line, SlcanFrame* frame) {
    if (!frame) return false;

    // Extended data frame: Tiiiiiiiildd... [tttt]
    //  T        = 29-bit frame
    //  iiiiiiii = 8 hex CAN identifier digits
    //  l        = DLC 0..8
    //  dd       = two hex digits per payload byte
    //  tttt     = optional timestamp, ignored here
    if (line.size() < 10 || line[0] != 'T') return false;

    std::uint32_t id = 0;
    if (!ParseHex(line, 1, 8, &id) || id > 0x1FFFFFFFu) return false;

    const char dlcChar = line[9];
    if (dlcChar < '0' || dlcChar > '8') return false;
    const std::uint8_t dlc = static_cast<std::uint8_t>(dlcChar - '0');

    const std::size_t minimumLength = 10u + static_cast<std::size_t>(dlc) * 2u;
    if (line.size() < minimumLength) return false;

    SlcanFrame parsed;
    parsed.id = id;
    parsed.dlc = dlc;

    for (std::size_t i = 0; i < dlc; ++i) {
        std::uint32_t byteValue = 0;
        if (!ParseHex(line, 10u + i * 2u, 2, &byteValue)) return false;
        parsed.data[i] = static_cast<std::uint8_t>(byteValue);
    }

    *frame = parsed;
    return true;
}

std::wstring NormalizeComPort(const std::wstring& port) {
    if (port.empty()) return std::wstring();

    std::wstring p = port;
    p.erase(p.begin(), std::find_if(p.begin(), p.end(), [](wchar_t c) {
        return !std::iswspace(c);
    }));
    p.erase(std::find_if(p.rbegin(), p.rend(), [](wchar_t c) {
        return !std::iswspace(c);
    }).base(), p.end());

    const std::wstring prefix = L"\\\\.\\";
    if (p.size() >= prefix.size() &&
        _wcsnicmp(p.c_str(), prefix.c_str(), prefix.size()) == 0) {
        p.erase(0, prefix.size());
    }

    if (!p.empty() && p.back() == L':') p.pop_back();
    if (p.size() < 4 || _wcsnicmp(p.c_str(), L"COM", 3) != 0) return std::wstring();

    unsigned long number = 0;
    for (std::size_t i = 3; i < p.size(); ++i) {
        if (!std::iswdigit(p[i])) return std::wstring();
        number = number * 10u + static_cast<unsigned long>(p[i] - L'0');
        if (number > 4096u) return std::wstring();
    }
    if (number == 0) return std::wstring();

    return L"COM" + std::to_wstring(number);
}

std::wstring ComDevicePath(const std::wstring& port) {
    const std::wstring normalized = NormalizeComPort(port);
    if (normalized.empty()) return std::wstring();
    return L"\\\\.\\" + normalized;
}

}  // namespace twocan
