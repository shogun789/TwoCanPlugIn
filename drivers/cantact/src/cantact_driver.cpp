// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Windows CANable/CANtact driver for TwoCan.

#include "cantact_driver.h"
#include "slcan_parser.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace {

constexpr wchar_t kDataRxEvent[] = L"Global\\DataReceived";
constexpr wchar_t kDataMutex[] = L"Global\\DataMutex";
constexpr wchar_t kRegistryKey[] = L"Software\\TwoCan\\CANable";
constexpr wchar_t kRegistryValue[] = L"ComPort";
constexpr wchar_t kEnvPort[] = L"TWOCAN_CANABLE_COM";
constexpr wchar_t kVidPidCanable2[] = L"VID_16D0&PID_117E";
constexpr wchar_t kVidPidLegacy[] = L"VID_AD50&PID_60C4";

constexpr int TWOCAN_RESULT_SUCCESS = 0;
constexpr int TWOCAN_RESULT_FATAL = 0x60000000;
constexpr int TWOCAN_RESULT_ERROR = 0x40000000;
constexpr int TWOCAN_SOURCE_DRIVER = 0x04000000;
constexpr int TWOCAN_ERROR_CONFIGURE_ADAPTER = 4;
constexpr int TWOCAN_ERROR_CONFIGURE_PORT = 5;
constexpr int TWOCAN_ERROR_CREATE_FRAME_RECEIVED_EVENT = 1;
constexpr int TWOCAN_ERROR_CREATE_FRAME_RECEIVED_MUTEX = 2;
constexpr int TWOCAN_ERROR_CREATE_THREAD_HANDLE = 10;
constexpr int TWOCAN_ERROR_CREATE_SERIALPORT = 11;
constexpr int TWOCAN_ERROR_ADAPTER_NOT_FOUND = 38;
constexpr int TWOCAN_ERROR_TRANSMIT_FAILURE = 34;
constexpr int kQueueLimit = 4096;

int MakeError(int level, int code) {
    return level | TWOCAN_SOURCE_DRIVER | (code << 16);
}

void DebugPrintf(const wchar_t* fmt, ...) {
    wchar_t buffer[2048] = {};
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, fmt, args);
    va_end(args);
    OutputDebugStringW(buffer);
}

std::atomic<bool> g_running(false);
HANDLE g_serial = INVALID_HANDLE_VALUE;
HANDLE g_frameEvent = nullptr;
HANDLE g_frameMutex = nullptr;
byte* g_frameDestination = nullptr;
std::thread g_readerThread;
std::thread g_dispatchThread;
std::mutex g_queueMutex;
std::condition_variable g_queueCv;
std::deque<twocan::SlcanFrame> g_queue;
std::mutex g_writeMutex;
std::wstring g_selectedPort;
std::wstring g_manualPort;
HMODULE g_module = nullptr;

bool ContainsVidPid(const wchar_t* multiString) {
    if (!multiString) return false;
    for (const wchar_t* p = multiString; *p; p += wcslen(p) + 1) {
        std::wstring id(p);
        std::transform(id.begin(), id.end(), id.begin(), ::towupper);
        if (id.find(kVidPidCanable2) != std::wstring::npos ||
            id.find(kVidPidLegacy) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

bool QueryPortNameFromDevice(HDEVINFO info, SP_DEVINFO_DATA* devInfo, std::wstring* port) {
    if (!port) return false;
    HKEY key = SetupDiOpenDevRegKey(info, devInfo, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
    if (key == INVALID_HANDLE_VALUE) return false;

    wchar_t value[256] = {};
    DWORD type = 0;
    DWORD size = sizeof(value);
    const LONG result = RegQueryValueExW(key, L"PortName", nullptr, &type,
                                         reinterpret_cast<LPBYTE>(value), &size);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return false;

    const std::wstring normalized = twocan::NormalizeComPort(value);
    if (normalized.empty()) return false;
    *port = normalized;
    return true;
}

bool AutoDetectCanable(std::wstring* port) {
    if (!port) return false;

    HDEVINFO info = SetupDiGetClassDevsW(nullptr, nullptr, nullptr,
                                         DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (info == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    SP_DEVINFO_DATA devInfo = {};
    devInfo.cbSize = sizeof(devInfo);

    for (DWORD index = 0; SetupDiEnumDeviceInfo(info, index, &devInfo); ++index) {
        wchar_t hardwareIds[4096] = {};
        DWORD type = 0;
        DWORD required = 0;
        if (!SetupDiGetDeviceRegistryPropertyW(info, &devInfo, SPDRP_HARDWAREID,
                                                &type,
                                                reinterpret_cast<PBYTE>(hardwareIds),
                                                sizeof(hardwareIds), &required)) {
            continue;
        }
        if (!ContainsVidPid(hardwareIds)) continue;

        std::wstring detected;
        if (QueryPortNameFromDevice(info, &devInfo, &detected)) {
            *port = detected;
            found = true;
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(info);
    return found;
}

bool ReadEnvironmentPort(std::wstring* port) {
    wchar_t value[128] = {};
    const DWORD count = GetEnvironmentVariableW(kEnvPort, value, _countof(value));
    if (count == 0 || count >= _countof(value)) return false;
    const std::wstring normalized = twocan::NormalizeComPort(value);
    if (normalized.empty()) return false;
    *port = normalized;
    return true;
}

bool ReadRegistryPort(std::wstring* port) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t value[128] = {};
    DWORD type = 0;
    DWORD size = sizeof(value);
    const LONG result = RegQueryValueExW(key, kRegistryValue, nullptr, &type,
                                         reinterpret_cast<LPBYTE>(value), &size);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || type != REG_SZ) return false;
    const std::wstring normalized = twocan::NormalizeComPort(value);
    if (normalized.empty()) return false;
    *port = normalized;
    return true;
}

bool ReadIniPort(std::wstring* port) {
    wchar_t modulePath[MAX_PATH] = {};
    if (!g_module || GetModuleFileNameW(g_module, modulePath, _countof(modulePath)) == 0) {
        return false;
    }
    std::wstring ini(modulePath);
    const std::size_t slash = ini.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return false;
    ini.erase(slash + 1);
    ini += L"cantact.ini";

    wchar_t value[128] = {};
    GetPrivateProfileStringW(L"CANable", L"ComPort", L"", value, _countof(value), ini.c_str());
    const std::wstring normalized = twocan::NormalizeComPort(value);
    if (normalized.empty()) return false;
    *port = normalized;
    return true;
}

bool ResolvePort(std::wstring* port) {
    if (!port) return false;
    if (!g_manualPort.empty()) {
        *port = g_manualPort;
        return true;
    }

    // Manual override precedence: environment -> ini next to DLL -> HKCU.
    if (ReadEnvironmentPort(port)) return true;
    if (ReadIniPort(port)) return true;
    if (ReadRegistryPort(port)) return true;

    return AutoDetectCanable(port);
}

bool PortExists(const std::wstring& port) {
    const std::wstring normalized = twocan::NormalizeComPort(port);
    if (normalized.empty()) return false;
    wchar_t target[1024] = {};
    return QueryDosDeviceW(normalized.c_str(), target, _countof(target)) != 0;
}

bool ConfigureSerialPort() {
    if (g_serial == INVALID_HANDLE_VALUE) return false;

    SetupComm(g_serial, 64 * 1024, 64 * 1024);

    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(g_serial, &dcb)) return false;

    // CANable is USB CDC, so this line coding is mostly advisory, but Windows
    // still expects a valid DCB. Use conventional SLCAN serial settings.
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(g_serial, &dcb)) return false;

    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = 10;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 250;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    return SetCommTimeouts(g_serial, &timeouts) != FALSE;
}

bool ReadCommandAck(DWORD timeoutMs) {
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    while (GetTickCount64() < deadline) {
        char ch = 0;
        DWORD read = 0;
        if (!ReadFile(g_serial, &ch, 1, &read, nullptr)) return false;
        if (read == 0) continue;
        if (ch == '\r') return true;
        if (ch == '\a') return false;
    }
    // Some SLCAN firmwares/bridges do not return an acknowledgement. The
    // command was still written successfully, so tolerate an ACK timeout.
    return true;
}

bool SendCommand(const char* command) {
    if (!command || g_serial == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const DWORD bytes = static_cast<DWORD>(std::strlen(command));
    if (!WriteFile(g_serial, command, bytes, &written, nullptr) || written != bytes) return false;
    FlushFileBuffers(g_serial);
    return ReadCommandAck(300);
}

bool ConfigureCanable250k() {
    PurgeComm(g_serial, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

    // Lawicel/SLCAN: C=close, S5=250 kbit/s, O=open.
    // This is the NMEA 2000 CAN bitrate.
    if (!SendCommand("C\r")) return false;
    if (!SendCommand("S5\r")) return false;
    if (!SendCommand("O\r")) return false;
    return true;
}

void EnqueueFrame(const twocan::SlcanFrame& frame) {
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        if (g_queue.size() >= kQueueLimit) {
            // Prefer fresh navigation data over indefinitely increasing latency.
            g_queue.pop_front();
        }
        g_queue.push_back(frame);
    }
    g_queueCv.notify_one();
}

void ReaderMain() {
    twocan::SlcanStreamParser parser;
    char buffer[4096];

    while (g_running.load()) {
        DWORD bytesRead = 0;
        if (!ReadFile(g_serial, buffer, sizeof(buffer), &bytesRead, nullptr)) {
            const DWORD error = GetLastError();
            if (!g_running.load() || error == ERROR_OPERATION_ABORTED) break;
            Sleep(5);
            continue;
        }
        if (bytesRead == 0) continue;

        const std::vector<twocan::SlcanFrame> frames = parser.Feed(buffer, bytesRead);
        for (const auto& frame : frames) EnqueueFrame(frame);
    }

    g_queueCv.notify_all();
}

bool WaitUntilConsumerReady() {
    while (g_running.load()) {
        // Manual-reset event stays signalled until TwoCan processes the shared
        // frame and calls ResetEvent(). Avoid overwriting the one-frame ABI.
        const DWORD state = WaitForSingleObject(g_frameEvent, 0);
        if (state == WAIT_TIMEOUT) return true;
        if (state == WAIT_FAILED) return false;
        Sleep(1);
    }
    return false;
}

void DispatchMain() {
    while (g_running.load()) {
        twocan::SlcanFrame frame;
        {
            std::unique_lock<std::mutex> lock(g_queueMutex);
            g_queueCv.wait(lock, [] { return !g_running.load() || !g_queue.empty(); });
            if (!g_running.load() && g_queue.empty()) break;
            frame = g_queue.front();
            g_queue.pop_front();
        }

        if (!WaitUntilConsumerReady()) break;

        const DWORD mutexResult = WaitForSingleObject(g_frameMutex, 500);
        if (mutexResult != WAIT_OBJECT_0) continue;

        byte raw[12] = {};
        const std::uint32_t id = frame.id;
        raw[0] = static_cast<byte>(id & 0xFFu);
        raw[1] = static_cast<byte>((id >> 8) & 0xFFu);
        raw[2] = static_cast<byte>((id >> 16) & 0xFFu);
        raw[3] = static_cast<byte>((id >> 24) & 0xFFu);
        std::memcpy(raw + 4, frame.data, 8);
        std::memcpy(g_frameDestination, raw, sizeof(raw));

        ReleaseMutex(g_frameMutex);
        SetEvent(g_frameEvent);
    }
}

void StopThreads() {
    g_running.store(false);
    g_queueCv.notify_all();
    if (g_serial != INVALID_HANDLE_VALUE) CancelIoEx(g_serial, nullptr);
    if (g_readerThread.joinable()) g_readerThread.join();
    if (g_dispatchThread.joinable()) g_dispatchThread.join();
    std::lock_guard<std::mutex> lock(g_queueMutex);
    g_queue.clear();
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

extern "C" TWOCAN_DRIVER_EXPORT wchar_t* DriverName(void) {
    static wchar_t value[] = L"CANable SLCAN";
    return value;
}

extern "C" TWOCAN_DRIVER_EXPORT wchar_t* DriverVersion(void) {
    static wchar_t value[] = L"2.0";
    return value;
}

extern "C" TWOCAN_DRIVER_EXPORT wchar_t* ManufacturerName(void) {
    static wchar_t value[] = L"CANable / CANtact";
    return value;
}

extern "C" TWOCAN_DRIVER_EXPORT int SetAdapterPort(const wchar_t* portName) {
    if (!portName || !*portName) {
        g_manualPort.clear();
        return TWOCAN_RESULT_SUCCESS;
    }
    const std::wstring normalized = twocan::NormalizeComPort(portName);
    if (normalized.empty()) return MakeError(TWOCAN_RESULT_ERROR, TWOCAN_ERROR_CONFIGURE_PORT);
    g_manualPort = normalized;
    return TWOCAN_RESULT_SUCCESS;
}

extern "C" TWOCAN_DRIVER_EXPORT int GetAdapterPort(wchar_t* portName, unsigned int portNameChars) {
    if (!portName || portNameChars == 0) return MakeError(TWOCAN_RESULT_ERROR, TWOCAN_ERROR_CONFIGURE_PORT);
    std::wstring value = g_selectedPort;
    if (value.empty() && !ResolvePort(&value)) return MakeError(TWOCAN_RESULT_ERROR, TWOCAN_ERROR_ADAPTER_NOT_FOUND);
    if (value.size() + 1 > portNameChars) return MakeError(TWOCAN_RESULT_ERROR, TWOCAN_ERROR_CONFIGURE_PORT);
    wcscpy_s(portName, portNameChars, value.c_str());
    return TWOCAN_RESULT_SUCCESS;
}

extern "C" TWOCAN_DRIVER_EXPORT int IsInstalled(void) {
    std::wstring port;
    if (!ResolvePort(&port)) return FALSE;
    return PortExists(port) ? TRUE : FALSE;
}

extern "C" TWOCAN_DRIVER_EXPORT int OpenAdapter(void) {
    if (g_serial != INVALID_HANDLE_VALUE) return TWOCAN_RESULT_SUCCESS;

    std::wstring port;
    if (!ResolvePort(&port)) {
        DebugPrintf(L"TwoCan CANable: CANable V2/CANtact not found\n");
        return MakeError(TWOCAN_RESULT_FATAL, TWOCAN_ERROR_ADAPTER_NOT_FOUND);
    }
    g_selectedPort = port;

    const std::wstring path = twocan::ComDevicePath(port);
    g_serial = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_serial == INVALID_HANDLE_VALUE) {
        DebugPrintf(L"TwoCan CANable: cannot open %s, error %lu\n", path.c_str(), GetLastError());
        return MakeError(TWOCAN_RESULT_FATAL, TWOCAN_ERROR_CREATE_SERIALPORT);
    }

    if (!ConfigureSerialPort()) {
        CloseHandle(g_serial);
        g_serial = INVALID_HANDLE_VALUE;
        return MakeError(TWOCAN_RESULT_FATAL, TWOCAN_ERROR_CONFIGURE_PORT);
    }

    g_frameEvent = CreateEventW(nullptr, TRUE, FALSE, kDataRxEvent);
    if (!g_frameEvent) {
        CloseHandle(g_serial);
        g_serial = INVALID_HANDLE_VALUE;
        return MakeError(TWOCAN_RESULT_FATAL, TWOCAN_ERROR_CREATE_FRAME_RECEIVED_EVENT);
    }

    g_frameMutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, kDataMutex);
    if (!g_frameMutex) {
        CloseHandle(g_frameEvent);
        g_frameEvent = nullptr;
        CloseHandle(g_serial);
        g_serial = INVALID_HANDLE_VALUE;
        return MakeError(TWOCAN_RESULT_FATAL, TWOCAN_ERROR_CREATE_FRAME_RECEIVED_MUTEX);
    }

    if (!ConfigureCanable250k()) {
        CloseHandle(g_frameMutex);
        g_frameMutex = nullptr;
        CloseHandle(g_frameEvent);
        g_frameEvent = nullptr;
        CloseHandle(g_serial);
        g_serial = INVALID_HANDLE_VALUE;
        return MakeError(TWOCAN_RESULT_FATAL, TWOCAN_ERROR_CONFIGURE_ADAPTER);
    }

    DebugPrintf(L"TwoCan CANable: opened %s at SLCAN S5 (250 kbit/s)\n", port.c_str());
    return TWOCAN_RESULT_SUCCESS;
}

extern "C" TWOCAN_DRIVER_EXPORT int ReadAdapter(byte* frame) {
    if (!frame || g_serial == INVALID_HANDLE_VALUE || !g_frameEvent || !g_frameMutex) {
        return MakeError(TWOCAN_RESULT_FATAL, TWOCAN_ERROR_CREATE_THREAD_HANDLE);
    }
    if (g_running.load()) return TWOCAN_RESULT_SUCCESS;

    g_frameDestination = frame;
    g_running.store(true);
    try {
        g_readerThread = std::thread(ReaderMain);
        g_dispatchThread = std::thread(DispatchMain);
    } catch (...) {
        StopThreads();
        return MakeError(TWOCAN_RESULT_FATAL, TWOCAN_ERROR_CREATE_THREAD_HANDLE);
    }
    return TWOCAN_RESULT_SUCCESS;
}

extern "C" TWOCAN_DRIVER_EXPORT int WriteAdapter(const unsigned int id, const int length, const byte* payload) {
    if (g_serial == INVALID_HANDLE_VALUE || !payload || length < 0 || length > 8 || id > 0x1FFFFFFFu) {
        return MakeError(TWOCAN_RESULT_ERROR, TWOCAN_ERROR_TRANSMIT_FAILURE);
    }

    char record[64] = {};
    int used = _snprintf_s(record, sizeof(record), _TRUNCATE, "T%08X%d", id, length);
    if (used < 0) return MakeError(TWOCAN_RESULT_ERROR, TWOCAN_ERROR_TRANSMIT_FAILURE);

    for (int i = 0; i < length; ++i) {
        const int remaining = static_cast<int>(sizeof(record)) - used;
        const int written = _snprintf_s(record + used, remaining, _TRUNCATE, "%02X", payload[i]);
        if (written < 0) return MakeError(TWOCAN_RESULT_ERROR, TWOCAN_ERROR_TRANSMIT_FAILURE);
        used += written;
    }
    if (used + 2 > static_cast<int>(sizeof(record))) return MakeError(TWOCAN_RESULT_ERROR, TWOCAN_ERROR_TRANSMIT_FAILURE);
    record[used++] = '\r';
    record[used] = '\0';

    std::lock_guard<std::mutex> lock(g_writeMutex);
    DWORD bytesWritten = 0;
    if (!WriteFile(g_serial, record, static_cast<DWORD>(used), &bytesWritten, nullptr) ||
        bytesWritten != static_cast<DWORD>(used)) {
        return MakeError(TWOCAN_RESULT_ERROR, TWOCAN_ERROR_TRANSMIT_FAILURE);
    }
    return TWOCAN_RESULT_SUCCESS;
}

extern "C" TWOCAN_DRIVER_EXPORT int CloseAdapter(void) {
    StopThreads();

    if (g_serial != INVALID_HANDLE_VALUE) {
        std::lock_guard<std::mutex> lock(g_writeMutex);
        DWORD written = 0;
        WriteFile(g_serial, "C\r", 2, &written, nullptr);
        FlushFileBuffers(g_serial);
        CloseHandle(g_serial);
        g_serial = INVALID_HANDLE_VALUE;
    }

    if (g_frameEvent) {
        CloseHandle(g_frameEvent);
        g_frameEvent = nullptr;
    }
    if (g_frameMutex) {
        CloseHandle(g_frameMutex);
        g_frameMutex = nullptr;
    }

    g_frameDestination = nullptr;
    g_selectedPort.clear();
    return TWOCAN_RESULT_SUCCESS;
}
