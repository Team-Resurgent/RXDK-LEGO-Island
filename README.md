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

You need, on Windows:

- [RXDK](https://github.com/Team-Resurgent/RXDK-Tools) v1.3.11 or later, installed by the RXDK extension for
  [VS Code](https://github.com/Team-Resurgent/RXDK-VSCode) or [Visual Studio](https://github.com/Team-Resurgent/RXDK-VS20XX)
  into `C:\ProgramData\RXDK`.
- Python 3 and git.

From a clone of this repository (the `xboxog` branch):

```bash
python xboxog/gen_manifest.py
"C:/ProgramData/RXDK/engine/Rxdk.Cli.exe" build --project-root xboxog
```

Or, after `gen_manifest.py`, open the `xboxog` folder in VS Code with the RXDK extension and build there. The first
build compiles SDL3 and the game, which takes several minutes; later builds only recompile what changed.

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

The Xbox platform is set up like the DOS port: miniwin, static libraries, and no extensions. It draws with a
Direct3D 8 renderer (`miniwin/src/d3drm/backends/xboxd3d8`), with the software renderer as the fallback. Its
settings are in `ISLE/xboxog/config.cpp`.

---

# LEGO Island, portable

[Development Vlog](https://www.youtube.com/playlist?list=PLbpl-gZkNl2Db4xcAsT_xOfOwRk-2DPHL) | [Contributing](/CONTRIBUTING.md) | [Matrix](https://matrix.to/#/#isledecomp:matrix.org) | [Forums](https://forum.mattkc.com/viewforum.php?f=1) | [Patreon](https://www.patreon.com/mattkc)
  
This initiative is a portable version of LEGO Island (Version 1.1, English) based on the [decompilation project](https://github.com/isledecomp/isle). Our primary goal is to transform the codebase to achieve platform independence, thereby enhancing compatibility across various systems while preserving the original game's experience as faithfully as possible.

Please note: this project is primarily dedicated to achieving platform independence without altering the core gameplay or rewriting code for improvement's sake. While those are worthwhile objectives, they are not within the scope of this project. `isle-portable` offers support for light modding using [`extensions`](/extensions). 

## Status

| Platform | Status |
| - | - | 
| Windows (x86, x64, arm64) | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) | 
| MacOS (arm64, x64) | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| Linux (x64, arm64) | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| FreeBSD (x64) | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| DOS (x86) | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| [Web](https://isle.pizza) | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| iOS | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| Android | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| Xbox One | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| Playstation Vita | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| Nintendo Switch | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| Nintendo 3DS | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |

We are actively working to support more platforms. If you have experience with a particular platform, we encourage you to contribute to `isle-portable`. You can find a [list of ongoing efforts](https://github.com/isledecomp/isle-portable/wiki/Work%E2%80%90in%E2%80%90progress-ports) in our Wiki.

## Usage

**An existing copy of LEGO Island is required to use this project.**

The [guided installer](https://isledecomp.github.io/isle-portable/) walks you through the installation on every supported platform, step by step, using the builds from the [Releases tab](https://github.com/isledecomp/isle-portable/releases/tag/continuous). These are continuous builds of the latest `master`, so an occasional regression is possible; if something does not work for you, please [open an issue](https://github.com/isledecomp/isle-portable/issues). Developers can also build the project from source, see below.

The installer's source lives in [`installer/`](/installer) and is deployed to GitHub Pages from `master`.

## Library substitutions

To achieve our goal of platform independence, we need to replace any Windows-only libraries with platform-independent alternatives. This ensures that our codebase remains versatile and compatible across various systems. The following table serves as an overview of major libraries / subsystems and their chosen replacements. For any significant changes or additions, it's recommended to discuss them with the team on the Matrix chat first to ensure consistency and alignment with our project's objectives.

| Library/subsystem | Substitution | Status | |
| - | - | - | - |
| Window, Events | [SDL3](https://www.libsdl.org/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Awindow%5D%22&type=code) |
| Windows Registry (Configuration) | [libiniparser](https://gitlab.com/iniparser/iniparser) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Aconfig%5D%22&type=code) |
| Filesystem | [SDL3](https://www.libsdl.org/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Afilesystem%5D%22&type=code) |
| Threads, Mutexes (Synchronization) | [SDL3](https://www.libsdl.org/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Asynchronization%5D%22&type=code) |
| Keyboard/Mouse, DirectInput (Input) | [SDL3](https://www.libsdl.org/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Ainput%5D%22&type=code) |
| Joystick/Gamepad, DirectInput (Input) | [SDL3](https://www.libsdl.org/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Ainput%5D%22&type=code) |
| WinMM, DirectSound (Audio) | [SDL3](https://www.libsdl.org/), [miniaudio](https://miniaud.io/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Aaudio%5D%22&type=code) |
| DirectDraw (2D video) | [SDL3](https://www.libsdl.org/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3A2d%5D%22&type=code) |
| [Smacker](https://github.com/isledecomp/isle/tree/master/3rdparty/smacker) | [libsmacker](https://github.com/foxtacles/libsmacker) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable%20%22%2F%2F%20%5Blibrary%3Alibsmacker%5D%22&type=code) |
| Direct3D (3D video) | [SDL3 (Vulkan, Metal, D3D12)](https://www.libsdl.org/), D3D9, OpenGL 1.1, OpenGL ES 2.0, OpenGL ES 3.0, Software | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3A3d%5D%22&type=code) |
| Direct3D Retained Mode | Custom re-implementation | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Aretained%5D%22&type=code) |
| [SmartHeap](https://github.com/isledecomp/isle/tree/master/3rdparty/smartheap) | Default memory allocator | - | - |

## Building

This project uses the [CMake](https://cmake.org/) build system, which allows for a high degree of versatility regarding compilers and development environments. Please refer to the [GitHub action](/.github/workflows//ci.yml) for guidance.

## Contributing

If you're interested in helping or contributing to this project, check out the [CONTRIBUTING](/CONTRIBUTING.md) page.
