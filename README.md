# LEGO Island for the original Xbox

<p align="center"><b>isle-portable on the original Xbox (2001) — LEGO Island running natively on the console, built with the open RXDK SDK</b></p>

<p align="center">
  <a href="https://github.com/Team-Resurgent/RXDK-LEGO-Island/blob/xboxog/LICENSE"><img src="https://img.shields.io/badge/License-LGPLv3-blue.svg" alt="License: LGPL v3"></a>
  <a href="https://discord.gg/VcdSfajQGK"><img src="https://img.shields.io/badge/chat-on%20discord-7289da.svg?logo=discord" alt="Discord"></a>
</p>

<p align="center">
  <a href="https://ko-fi.com/J3J7L5UMN"><img src="https://img.shields.io/badge/ko--fi-Support-FF5E5B?style=for-the-badge&logo=ko-fi&logoColor=white" alt="ko-fi"></a>
  <a href="https://www.patreon.com/teamresurgent"><img src="https://img.shields.io/badge/Patreon-F96854?style=for-the-badge&logo=patreon&logoColor=white" alt="Patreon"></a>
</p>

This is Team Resurgent's port of [isle-portable](https://github.com/isledecomp/isle-portable), the portable
version of LEGO Island built on the [isledecomp](https://github.com/isledecomp/isle) decompilation, to the
original Xbox. It runs on the console with [RXDK SDL3](https://github.com/Team-Resurgent/RXDK-SDL3) for video,
audio, controllers and threads.

**Status: work in progress.** The game runs on the console and on xemu, drawn with the Xbox's Direct3D 8: the
intro movies, the Information Center and its 3D scenes. It plays with a controller or a USB mouse. The rest of the
island has not been tested yet.

**An existing copy of LEGO Island is required.**

## Installing the game files

Put the game's `LEGO` folder next to `default.xbe`, combining two folders from the CD:

| From the CD | To, next to `default.xbe` |
|---|---|
| `LEGO\Scripts` | `LEGO\Scripts` |
| `DATA\disk\LEGO\data` | `LEGO\data` |

The title looks for them in `D:\LEGO\` (`D:` is the folder it was launched from). Saves and `isle.ini` go to the
title's own storage on the hard disk, `T:\isledecomp\isle\`.

## Controls

| Controller | Action |
|---|---|
| Right stick, d-pad | Move the cursor |
| A, right trigger | Click |
| B | Space |
| Left stick | Move |
| Back | Escape (skip) |
| Start | Pause |

A USB mouse in any controller port (through a port-to-USB adapter) moves the cursor and clicks too.

## Building

You need:

- [RXDK](https://github.com/Team-Resurgent/RXDK-Tools) v1.3.11 or later. The RXDK extension for
  [VS Code](https://github.com/Team-Resurgent/RXDK-VSCode) installs it on Windows, Linux or macOS (run
  **Complete Setup** from its sidebar); the one for [Visual Studio](https://github.com/Team-Resurgent/RXDK-VS20XX)
  installs it on Windows.
- Python 3 and git.

RXDK installs its tools into a folder that depends on the system:

| System | RXDK tools folder |
|---|---|
| Windows | `C:\ProgramData\RXDK\tools` |
| Linux | `~/.local/share/rxdk/tools` (or `$XDG_DATA_HOME/rxdk/tools`) |
| macOS | `~/Library/Application Support/RXDK/tools` |

From a clone of this repository (the `xboxog` branch), generate the build manifests, then build with RXDK's
`Rxdk.Cli` from that folder. On Windows:

```bash
python xboxog/gen_manifest.py
"C:/ProgramData/RXDK/tools/Rxdk.Cli.exe" build --project-root xboxog
```

On Linux (on macOS, use the macOS folder above):

```bash
python3 xboxog/gen_manifest.py
~/.local/share/rxdk/tools/Rxdk.Cli build --project-root xboxog
```

Or, after `gen_manifest.py`, open the `xboxog` folder in VS Code with the RXDK extension and build there. The first
build compiles SDL3 and the game, which takes several minutes; later builds only recompile what changed. This
port has so far been built on Windows; nothing in it is Windows-specific, so please report how it goes elsewhere.

`gen_manifest.py` fetches the two dependencies the CMake build downloads (iniparser, and RXDK SDL3 in place of
SDL3) into `xboxog/deps`, and writes the RXDK build manifests from the project's CMake files. It reads each
target's source list from CMake and evaluates the `if()` conditions around it for the Xbox; a condition it has no
value for stops it, so a new platform switch upstream gets decided rather than guessed. Re-run it after upstream
changes.

The build produces `xboxog/out/Build/isle/default.xbe`. It also writes `xboxog/out/XISO/isle.iso`, but that
holds only the XBE, so it does not run as it is: the game files have to go next to it.

## Running it

Make a folder with `default.xbe` and the `LEGO` folder described in
[Installing the game files](#installing-the-game-files):

```
LEGO Island\
  default.xbe
  LEGO\Scripts\...
  LEGO\data\...
```

- **On a console:** copy that folder to the hard disk (for example `E:\Games\LEGO Island`) by FTP or with
  RXDK's `xbcp`, and launch `default.xbe` from your dashboard.
- **On xemu:** pack the folder into an ISO and load it as the disc:

  ```bash
  "C:/ProgramData/RXDK/tools/xdvdfs.exe" pack "LEGO Island" isle.iso
  ```

  (`xdvdfs` is in the RXDK tools folder above on every system.)

The Xbox platform is set up like the DOS port: miniwin, static libraries, and no extensions. It draws with a
Direct3D 8 renderer (`miniwin/src/d3drm/backends/xboxd3d8`), with the software renderer as the fallback. Its
settings are in `ISLE/xboxog/config.cpp`.

## Help and issues

Ask about the Xbox port, or report a problem with it, on the Team Resurgent
[Discord](https://discord.gg/VcdSfajQGK), not upstream: isle-portable does not support the original Xbox.

## Credits

The game code is [isle-portable](https://github.com/isledecomp/isle-portable) by the
[isledecomp](https://github.com/isledecomp) team, built on their
[decompilation of LEGO Island](https://github.com/isledecomp/isle). This port adds the Xbox platform: the
`ISLE/xboxog` configuration, the Direct3D 8 renderer in `miniwin/src/d3drm/backends/xboxd3d8`, and the RXDK build
in `xboxog/`. It is based on upstream's `master`, and everything else comes from there.

Licensed under the [LGPL v3](LICENSE), as isle-portable is.
