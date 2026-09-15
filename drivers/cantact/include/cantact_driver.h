// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <windows.h>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define TWOCAN_DRIVER_EXPORT __declspec(dllexport)

typedef unsigned char byte;

TWOCAN_DRIVER_EXPORT wchar_t* DriverName(void);
TWOCAN_DRIVER_EXPORT wchar_t* DriverVersion(void);
TWOCAN_DRIVER_EXPORT wchar_t* ManufacturerName(void);
TWOCAN_DRIVER_EXPORT int IsInstalled(void);
TWOCAN_DRIVER_EXPORT int OpenAdapter(void);
TWOCAN_DRIVER_EXPORT int CloseAdapter(void);
TWOCAN_DRIVER_EXPORT int ReadAdapter(byte* frame);
TWOCAN_DRIVER_EXPORT int WriteAdapter(const unsigned int id, const int length, const byte* payload);

// Optional extension used by newer TwoCan builds.  Existing TwoCan versions
// simply ignore these exports and continue to use automatic detection.
TWOCAN_DRIVER_EXPORT int SetAdapterPort(const wchar_t* portName);
TWOCAN_DRIVER_EXPORT int GetAdapterPort(wchar_t* portName, unsigned int portNameChars);

#ifdef __cplusplus
}
#endif
