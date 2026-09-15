# Changelog

## 2.1.1.0 OpenCPN 5.14 / Windows compatibility fork — 2026-09-15

This fork keeps the upstream TwoCan 2.1.1.0 plugin version while adding a maintained Windows compatibility layer for OpenCPN 5.14.

### Authorship, licensing and provenance

- Explicitly identifies **Steven Adler / TwoCanPlugIn** as the original TwoCan author/maintainer in the fork documentation and release metadata.
- Preserves the original copyright and license notices in upstream source files.
- Added a root `COPYING` containing the unmodified GNU GPL version 3 text.
- Added `THIRD_PARTY_NOTICES.md` covering TwoCan, OpenCPN API/NMEA0183 material, wxJSON and bundled Windows driver provenance.
- Marks fork-modified source/build files with a dated 2026 modification notice where appropriate.
- Includes `COPYING` and `THIRD_PARTY_NOTICES.md` in the OpenCPN import tarball.
- Keeps the legacy Windows driver DLLs to preserve upstream functionality and documents that they are carried unchanged from upstream TwoCan commit `2e36d999566e84ea66497d5585a70b89af53f9b1`.
- Pins the upstream `TwoCanPluginDrivers` source repository at commit `1bdbf59538e9f58e0297ba0e393165aec12f38bb` and publishes a source snapshot next to the binary release.
- Documents that the source archive for the same GitHub release tag is the corresponding source for the fork-specific binary build.

### OpenCPN 5.14 packaging

- Ported the Windows CI build to Visual Studio 2022 and wxWidgets 3.2.6.
- Builds the plugin as Win32/x86 for the current standard OpenCPN 5.14 Windows runtime.
- Uses the `msvc-wx32` ABI family.
- Added modern OpenCPN Plugin Manager `metadata.xml` generation.
- Generates a `.tar.gz` import package accepted by OpenCPN 5.14.
- CI downloads the official OpenCPN 5.14.0 runtime, imports the tarball and checks plugin loading through the OpenCPN API shim.

### CANable V2.0 / SLCAN driver

- Replaced the legacy 2019 `cantact.dll` build with a VS2022 Win32 driver built from source in this repository.
- Added automatic CANable V2.0 detection for USB VID `16D0`, PID `117E`.
- Retained legacy CANtact VID/PID matching for backwards compatibility.
- Added proper support for `COM10+` using Windows `\\.\COMx` device paths.
- Corrected Windows serial-port parameters and timeout handling.
- Configures SLCAN for NMEA 2000 using `C`, `S5`, `O` at 250 kbit/s.
- Added a persistent streaming SLCAN parser which handles partial reads, multiple frames per read, CR/CRLF and optional timestamps.
- Added a bounded receive queue and removed the old per-frame `Sleep(5)` bottleneck.
- Added parser unit tests and export/PE checks in GitHub Actions.

### TwoCan settings UI

- Added a CANable V2.0 serial-port section to the Windows settings dialog.
- Lists currently available Windows COM ports with friendly device names.
- Marks automatically detected CANable V2.0 devices.
- Supports automatic VID/PID detection or manual editable COM selection.
- Added a Refresh button for re-enumerating ports.
- Persists the selected manual port for the driver and restores automatic detection when requested.

### CI / repository maintenance

- Added a dedicated Windows/OpenCPN 5.14 GitHub Actions workflow.
- Added `dumpbin` architecture and dependency inspection using the installed VS2022 toolchain.
- Added tarball content and metadata validation.
- Added release documentation and a current fork-focused README.

### Known limitation

The complete software path is CI-tested, including OpenCPN 5.14 import and ABI loading, but GitHub-hosted runners cannot perform a physical USB/CAN/NMEA 2000 test. Live CANable V2.0 hardware validation remains required.

---

For historical changes before this compatibility fork, see the upstream project history:
https://github.com/TwoCanPlugIn/TwoCanPlugIn
