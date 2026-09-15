# TwoCan Plugin for OpenCPN 5.14

This repository is a compatibility-focused fork of the original [TwoCanPlugIn](https://github.com/TwoCanPlugIn/TwoCanPlugIn). It keeps the existing TwoCan NMEA 2000 functionality while updating the Windows build, packaging and CANable support for current OpenCPN.

## Authors and attribution

**TwoCan was originally created and maintained by Steven Adler / TwoCanPlugIn.** The original copyright, ownership and GPL notices remain in the upstream source files and are intentionally preserved in this fork.

- Original project: https://github.com/TwoCanPlugIn/TwoCanPlugIn
- Original author/maintainer: **Steven Adler / TwoCanPlugIn**
- Original source contact: `twocanplugin@hotmail.com`
- Upstream baseline for this compatibility fork: `2e36d999566e84ea66497d5585a70b89af53f9b1`
- OpenCPN 5.14 / Windows 11 / CANable V2 compatibility work: `shogun789/TwoCanPlugIn`, 2026

The fork-specific work is not presented as authorship of the original TwoCan project. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for component-level attribution and source provenance.

## Current Windows status

The fork is built and tested in GitHub Actions against the official OpenCPN 5.14.0 Windows runtime.

- OpenCPN 5.14.0
- Windows 11 / Win32 OpenCPN plugin ABI
- OpenCPN ABI: `msvc-wx32:10`
- Visual Studio 2022, Win32/x86
- wxWidgets 3.2.6
- OpenCPN Plugin API 1.16
- Plugin Manager tarball with top-level `metadata.xml`
- CANable V2.0 SLCAN support at 250 kbit/s

The CI pipeline verifies that the generated tarball can be imported by OpenCPN 5.14 and that `twocan_plugin_pi.dll` can be loaded without an ABI loader error.

## Installing on Windows

1. Download the `.tar.gz` package from the latest GitHub Release. Do not unpack it.
2. In OpenCPN 5.14 open **Options → Plugins** and use **Import Plugin**.
3. Select the TwoCan `.tar.gz` file and enable the plugin.
4. Open the TwoCan preferences and select **CANable SLCAN** as the NMEA 2000 interface when using CANable V2.0.
5. Leave the CANable serial port on **Automatic** or choose/type the required COM port manually.

## CANable V2.0 on Windows 11

The Windows CANable driver in this fork was rewritten for modern CANable V2.0 devices while preserving the TwoCan driver interface.

Supported behaviour:

- automatic detection of CANable V2.0 using USB VID `16D0` / PID `117E`;
- compatibility with the legacy CANtact VID/PID used by the old driver;
- all normal Windows COM ports including `COM10+`;
- manual COM-port selection from the TwoCan settings dialog;
- editable COM-port field and **Refresh** button;
- standard USB CDC serial configuration: 115200, 8N1, no flow control;
- SLCAN setup sequence `C`, `S5`, `O` for NMEA 2000 at 250 kbit/s;
- streaming SLCAN parser which preserves partial records between USB reads;
- extended 29-bit CAN frames (`T...`) used by NMEA 2000;
- receive queue between the USB reader and the original TwoCan Windows frame interface.

The selected port is persisted for the driver. Automatic mode clears the manual override and lets the driver detect CANable by VID/PID.

### Hardware validation note

GitHub Actions cannot attach a physical CANable or NMEA 2000 backbone. CI therefore validates compilation, x86 architecture, exported driver functions, SLCAN parser tests, package structure, OpenCPN 5.14 import and plugin ABI loading. A final on-hardware test of USB enumeration and live NMEA 2000 traffic is still required.

## What TwoCan does

TwoCan integrates NMEA 2000 data with OpenCPN, primarily by decoding supported NMEA 2000 PGNs and making the resulting navigation data available to OpenCPN. The original project also contains logging, active-device and NMEA 0183/NMEA 2000 gateway features.

The Windows compatibility work in this fork is intentionally focused on OpenCPN 5.14 and CANable V2.0. Linux and macOS source support from upstream remains in the tree, but these platforms are not regression-tested by the new Windows CI workflow.

## Building the Windows package

The reference build uses Visual Studio 2022 and wxWidgets 3.2.6 Win32. The GitHub Actions workflow in `.github/workflows/windows-opencpn514.yml` is the authoritative reproducible build.

A local build follows the same general pattern:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 `
  -DwxWidgets_ROOT_DIR="C:\path\to\wxWidgets" `
  -DwxWidgets_LIB_DIR="C:\path\to\wxWidgets\lib\vc14x_dll" `
  -DwxWidgets_CONFIGURATION=mswu
cmake --build build --config Release --parallel
```

The modern CANable driver is built separately from `drivers/cantact` and copied into the plugin data directory before CPack creates the import tarball.

## CI checks

The Windows workflow performs the following checks on every compatibility-branch build:

1. build TwoCan as Win32/x86 with VS2022 and wxWidgets 3.2.6;
2. build and unit-test the rewritten CANable SLCAN driver;
3. inspect plugin and driver PE architecture/exports with `dumpbin`;
4. generate a `msvc-wx32` OpenCPN tarball;
5. validate `metadata.xml`, `COPYING`, `THIRD_PARTY_NOTICES.md` and packaged `cantact.dll`;
6. import the tarball with the official OpenCPN 5.14 command-line importer;
7. load the plugin through the OpenCPN 5.14 API shim to detect ABI problems;
8. publish the package as a GitHub Actions artifact.

The release workflow additionally attaches the validated tarball and a pinned upstream legacy-driver source snapshot to the GitHub Release when `master` is updated.

## Legacy Windows drivers

The historical Windows driver DLLs are intentionally retained so the fork does not remove functionality present upstream. Apart from `cantact.dll`, which is replaced at build time by the new source-built CANable V2 driver, the DLLs in `data/drivers` are unchanged from the upstream TwoCan baseline identified above.

Their provenance and the pinned upstream driver-source repository are documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Changes in this fork

See [CHANGELOG.md](CHANGELOG.md). Release-specific notes are in [RELEASE_NOTES.md](RELEASE_NOTES.md).

## License and corresponding source

The original TwoCan source files and the fork-specific TwoCan modifications are distributed under **GNU GPL version 3 or later (`GPL-3.0-or-later`)**. The complete GPLv3 license text is provided in [COPYING](COPYING).

Some bundled source components retain their own compatible original notices, including OpenCPN API/NMEA 0183 code and wxJSON. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); the original per-file notices remain authoritative.

For binary GitHub Releases, the corresponding source for the fork is the source archive of the **same release tag**. The release also provides or identifies source for the retained legacy Windows drivers. No warranty is provided, as described by the applicable licenses.

`NMEA 2000®` is a registered trademark of the National Marine Electronics Association.
