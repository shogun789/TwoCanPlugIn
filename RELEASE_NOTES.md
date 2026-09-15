# TwoCan 2.1.1.0 — OpenCPN 5.14 / CANable V2 Windows

This release is a Windows compatibility release of the upstream TwoCan 2.1.1.0 codebase for current OpenCPN 5.14.

## Highlights

- OpenCPN 5.14.0 compatible `msvc-wx32` Win32/x86 package.
- Built with Visual Studio 2022 and wxWidgets 3.2.6.
- Modern `metadata.xml` included for OpenCPN Plugin Manager import.
- Rewritten `cantact.dll` for CANable V2.0 on Windows 11.
- Automatic CANable detection using USB VID `16D0` / PID `117E`.
- Manual COM-port selection directly in the TwoCan settings dialog.
- Correct support for `COM10+`.
- SLCAN configured for NMEA 2000 at 250 kbit/s using `S5`.
- Streaming SLCAN parser and receive queue for more reliable handling of fragmented USB CDC reads.
- CI verifies tarball import and plugin ABI loading against the official OpenCPN 5.14.0 runtime.

## Installation

Download the attached `.tar.gz` file and import it directly in OpenCPN 5.14 using **Options → Plugins → Import Plugin**. Do not unpack the tarball first.

For CANable V2.0, select **CANable SLCAN** in TwoCan settings. The new port selector can automatically detect CANable V2.0 or use a manually selected COM port.

## Validation status

The release is automatically checked for:

- Win32/x86 plugin and driver architecture;
- expected `cantact.dll` exports;
- SLCAN parser unit tests;
- valid OpenCPN `metadata.xml`;
- successful OpenCPN 5.14 tarball import;
- successful plugin load through the OpenCPN 5.14 API shim.

A GitHub Actions runner cannot attach a physical CANable V2.0 or NMEA 2000 backbone, so live hardware/USB/CAN traffic is the remaining field-validation step.

## Upstream

Based on the original TwoCan project:
https://github.com/TwoCanPlugIn/TwoCanPlugIn
