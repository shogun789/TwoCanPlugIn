# TwoCan CANable V2 Windows driver

This directory contains the replacement `cantact.dll` used by the Windows/OpenCPN 5.14 build.

## Authorship and license

The original TwoCan CANtact Windows driver was written by **Steven Adler / TwoCanPlugIn** and is available in the upstream driver repository:

https://github.com/TwoCanPlugIn/TwoCanPluginDrivers/tree/1bdbf59538e9f58e0297ba0e393165aec12f38bb/Cantact

The implementation in this directory was added for the `shogun789/TwoCanPlugIn` compatibility fork on **2026-09-15**. It preserves the original TwoCan driver ABI while replacing the old serial implementation for Windows 11/CANable V2.0. It is distributed under **GPL-3.0-or-later**, consistent with the original TwoCan driver license.

The original author's work and this fork's modifications are deliberately distinguished; the fork does not claim authorship of the original TwoCan/CANtact implementation. See the repository root `COPYING` and `THIRD_PARTY_NOTICES.md`.

## Supported adapters

- CANable V2.0 USB CDC/SLCAN: VID `16D0`, PID `117E`
- Legacy CANtact/CANable identifier used by the original TwoCan driver: VID `AD50`, PID `60C4`

The driver configures Lawicel/SLCAN with `C`, `S5`, `O`, i.e. CAN closed, 250 kbit/s and CAN open. 250 kbit/s is the NMEA 2000 bus rate. Extended `T` frames are parsed as 29-bit NMEA 2000 CAN frames.

## COM port selection

If no override is configured, the driver scans currently present Windows Plug-and-Play devices for the supported VID/PID values and reads their `PortName` automatically.

A manual COM port can be selected using any of the following mechanisms, in precedence order:

1. `SetAdapterPort(L"COM12")` exported by `cantact.dll` (for a GUI/client integration).
2. User/process environment variable `TWOCAN_CANABLE_COM=COM12`.
3. `cantact.ini` placed next to `cantact.dll`:

   ```ini
   [CANable]
   ComPort=COM12
   ```

4. `HKEY_CURRENT_USER\Software\TwoCan\CANable`, string value `ComPort`, e.g. `COM12`.

`COM1` through `COM4096` are accepted. The driver internally uses the Windows `\\.\COMx` device path, so COM10 and above work correctly.

## Serial and parser behaviour

The USB CDC port is configured as 115200, 8 data bits, no parity, one stop bit, no hardware/software flow control. CANable USB CDC firmware generally ignores the physical UART baud rate, but a valid DCB avoids legacy Windows configuration errors.

The SLCAN parser is stateful across `ReadFile()` calls. It therefore handles records split over arbitrary USB/CDC packet boundaries and multiple records returned in a single read. Optional SLCAN timestamps are tolerated. A reader queue separates serial ingestion from the legacy TwoCan one-frame/event interface so the serial port can continue draining while OpenCPN consumes a previous CAN frame.

The GitHub Actions Windows build compiles this DLL as Win32 using Visual Studio 2022 and runs parser/COM-normalization tests before inserting it into the OpenCPN tarball.
