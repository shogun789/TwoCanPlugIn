# Modification record

This file records the material changes made by the `shogun789/TwoCanPlugIn` compatibility fork relative to the original TwoCan project. It is intended to make the modified status and relevant date prominent while preserving all original author and copyright notices.

## Upstream baseline

- Project: TwoCan Plugin for OpenCPN
- Original author/maintainer: **Steven Adler / TwoCanPlugIn**
- Upstream repository: https://github.com/TwoCanPlugIn/TwoCanPlugIn
- Baseline commit: `2e36d999566e84ea66497d5585a70b89af53f9b1`

## Fork modifications — 2026-09-15

The following files or areas were added or modified by the `shogun789/TwoCanPlugIn` compatibility fork on 2026-09-15:

- `.github/workflows/windows-opencpn514.yml` — VS2022/Win32 OpenCPN 5.14 build, package validation, ABI test and release automation.
- `cmake/PluginPackage.cmake` — OpenCPN 5.14 metadata packaging plus license/notice files.
- `plugin.xml.in` — modern OpenCPN Plugin Manager metadata for the Windows package.
- `src/twocansettings.cpp` — Windows CANable V2.0 COM-port enumeration, automatic detection, manual selection and persistence UI.
- `drivers/cantact/` — source-built Windows 11/CANable V2.0 SLCAN driver, parser and tests, compatible with the original TwoCan Windows driver interface.
- `README.md`, `CHANGELOG.md`, `RELEASE_NOTES.md` — current compatibility, build, release and attribution documentation.
- `COPYING`, `THIRD_PARTY_NOTICES.md`, `MODIFICATIONS.md` — licensing, attribution and modification records.

The fork-specific TwoCan modifications are distributed under `GPL-3.0-or-later`, consistent with the original TwoCan licensing. Original copyright/license notices in upstream files are retained.

## Legacy Windows driver binaries

The fork intentionally retains legacy driver DLLs from the upstream TwoCan baseline to preserve functionality. Except for `cantact.dll`, which is rebuilt from the new source under `drivers/cantact`, those retained DLL blobs are unchanged from the upstream baseline above. Source provenance is documented in `THIRD_PARTY_NOTICES.md`.
