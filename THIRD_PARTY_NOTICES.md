# Third-party notices and source provenance

This fork contains the original TwoCan codebase together with fork-specific Windows/OpenCPN 5.14 compatibility work. The notices below are intended to preserve upstream authorship and identify bundled components which retain their own license notices.

## Original TwoCan project

TwoCan was created and maintained upstream by **Steven Adler / TwoCanPlugIn**.

- Upstream repository: https://github.com/TwoCanPlugIn/TwoCanPlugIn
- Upstream baseline used by this fork: commit `2e36d999566e84ea66497d5585a70b89af53f9b1`
- Upstream contact appearing in the original sources: `twocanplugin@hotmail.com`
- License used by the TwoCan source files: GNU General Public License, version 3 or (at your option) any later version (`GPL-3.0-or-later`).

Original copyright and license headers in the source files remain intact. Fork-specific modifications are identified separately and do not replace the original attribution.

## Windows/OpenCPN 5.14 compatibility work in this fork

The OpenCPN 5.14 / Windows 11 / CANable V2.0 compatibility changes were added in 2026 in the `shogun789/TwoCanPlugIn` fork. These changes are distributed under `GPL-3.0-or-later`, consistent with the upstream TwoCan license.

The rewritten CANable/CANtact Windows driver keeps compatibility with the TwoCan driver interface and is based conceptually on the original CANtact driver by Steven Adler. Source files for the rewritten driver carry SPDX and modification notices.

## OpenCPN plug-in API files

Files under `libs/ocpn-api` include OpenCPN plug-in API material authored by **David S. Register**. The retained source headers state GNU GPL version 2 or later (`GPL-2.0-or-later`). Their original notices are preserved in the files.

OpenCPN project: https://github.com/OpenCPN/OpenCPN

## NMEA 0183 support classes

Files under `nmea183` contain NMEA 0183 support code credited in the retained headers to **Samuel R. Blackburn** and **David S. Register**. The files include the original GPL version 2-or-later notice and historical licensing notes from the upstream code. Those notices remain authoritative for that material.

## wxJSON

The `wxJSON` directory contains wxJSON code including material credited to **Luciano Cattani**. The retained headers identify the **wxWidgets licence** for this code. Those original notices remain intact.

wxWidgets licensing information: https://www.wxwidgets.org/about/licence/

## Legacy Windows TwoCan driver DLLs

The release intentionally retains the legacy Windows driver DLLs because removing them would remove functionality supported by upstream TwoCan. With the exception of `cantact.dll`, which is rebuilt from source in this fork, these binaries are carried **unchanged from the upstream TwoCan repository baseline** at commit:

`2e36d999566e84ea66497d5585a70b89af53f9b1`

The retained upstream binaries are:

- `axiomtek.dll`
- `candumplog.dll`
- `filedevice.dll`
- `keeslog.dll`
- `kvaser.dll`
- `pcap.dll`
- `RusokuToucanMarine_v_0_1.dll`
- `yachtdeviceslog.dll`

Their exact blobs in this fork match the corresponding blobs in the upstream TwoCan baseline above.

The separate upstream driver source project is:

- https://github.com/TwoCanPlugIn/TwoCanPluginDrivers
- pinned source repository head: `1bdbf59538e9f58e0297ba0e393165aec12f38bb`
- the upstream driver repository states GPL v3 or later for the TwoCan plug-in drivers.

The release workflow also attaches a source snapshot of that pinned driver repository so recipients downloading the binary release have a clearly identified driver-source archive next to it. `pcap.dll` functionality also has related source in the main TwoCan tree (for example `src/twocanpcap.cpp`).

This provenance note does not claim that every retained historical DLL is reproducible byte-for-byte from a modern compiler. It identifies the upstream binary baseline and the maintained upstream source project rather than obscuring their origin.

## Trademarks

`NMEA 2000®` is a registered trademark of the National Marine Electronics Association. References to CANable, CANtact, OpenCPN, wxWidgets and other project/product names are descriptive and do not imply ownership or endorsement by this fork.

## Full GPL text

The full GNU GPL version 3 text is provided in [`COPYING`](COPYING). Components carrying other license notices retain those notices in their source files.
