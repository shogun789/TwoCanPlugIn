# TwoCan 2.1.1.0 — OpenCPN 5.14 / CANable V2 Windows

This release is a Windows compatibility release of the upstream TwoCan 2.1.1.0 codebase for current OpenCPN 5.14.

## Authorship and upstream

**TwoCan was originally created and maintained by Steven Adler / TwoCanPlugIn.** This fork preserves the original source copyright and GPL notices and does not present the OpenCPN 5.14 compatibility work as authorship of the original project.

- Original project: https://github.com/TwoCanPlugIn/TwoCanPlugIn
- Original author/maintainer: Steven Adler / TwoCanPlugIn
- Upstream baseline: `2e36d999566e84ea66497d5585a70b89af53f9b1`
- Windows/OpenCPN 5.14 compatibility fork: https://github.com/shogun789/TwoCanPlugIn

See `THIRD_PARTY_NOTICES.md` for component-level attribution and legacy-driver source provenance.

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
- Full GPLv3 text (`COPYING`) and third-party attribution included in the import package.
- CI verifies tarball import and plugin ABI loading against the official OpenCPN 5.14.0 runtime.

## Installation

Download the attached `.tar.gz` file and import it directly in OpenCPN 5.14 using **Options → Plugins → Import Plugin**. Do not unpack the tarball first.

For CANable V2.0, select **CANable SLCAN** in TwoCan settings. The new port selector can automatically detect CANable V2.0 or use a manually selected COM port.

## License and corresponding source

The original TwoCan code and fork-specific TwoCan modifications are distributed under **GPL-3.0-or-later**. The complete GNU GPLv3 text is included as `COPYING`.

The corresponding source for this binary release is the GitHub source archive of this **same release tag**. The release also attaches a pinned source snapshot of the upstream `TwoCanPluginDrivers` repository used to document the retained legacy Windows driver sources.

The historical Windows DLLs other than the rebuilt `cantact.dll` are retained unchanged from the upstream TwoCan baseline so this compatibility release does not remove existing functionality.

Some source components retain their own compatible original notices (including OpenCPN API/NMEA0183 material and wxJSON); see `THIRD_PARTY_NOTICES.md` and the original per-file headers.

## Validation status

The release is automatically checked for:

- Win32/x86 plugin and driver architecture;
- expected `cantact.dll` exports;
- SLCAN parser unit tests;
- valid OpenCPN `metadata.xml`;
- presence of `COPYING` and `THIRD_PARTY_NOTICES.md` in the tarball;
- successful OpenCPN 5.14 tarball import;
- successful plugin load through the OpenCPN 5.14 API shim.

A GitHub Actions runner cannot attach a physical CANable V2.0 or NMEA 2000 backbone, so live hardware/USB/CAN traffic is the remaining field-validation step.
