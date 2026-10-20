#!/usr/bin/env python3
"""Generate the RXDK build manifests for the original Xbox (xboxog) port.

The RXDK engine builds from rxdk.project.json manifests that list every
source, so this writes them from the project's own CMake files: the source
lists of each target are read from its target_sources()/add_library() calls,
and the if() blocks around them are evaluated for the Xbox with the CONDITIONS
table below. A condition the table does not cover is an error, so a new
platform switch upstream has to be decided here rather than silently guessed.

It also fetches the two dependencies the CMake build downloads (iniparser at
the same tag, and the RXDK SDL3 port), into xboxog/deps/.

    python xboxog/gen_manifest.py              # fetch deps if missing, write manifests
    python xboxog/gen_manifest.py --no-fetch   # only write manifests
    python xboxog/gen_manifest.py --disc G:\\  # also copy the game files from a disc

Then build the XBE (and ISO) with the RXDK engine:

    <RXDK tools>/Rxdk.Cli build --project-root xboxog

With --disc, the game's files are copied from the LEGO Island disc (or a copy of
it) into xboxog/game/LEGO, which git ignores, and every build packs them into the
ISO next to the XBE: xboxog/out/XISO/isle.iso is then playable, and
xboxog/out/Build/isle/ is the folder to copy to a console. Without them the ISO
holds only the XBE.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DEPS = os.path.join(HERE, "deps")
LIBS = os.path.join(HERE, "libs")

# Dependencies the CMake build fetches with FetchContent.
INIPARSER_REPO = "https://gitlab.com/iniparser/iniparser.git"
INIPARSER_TAG = "v4.2.6"  # as CMakeLists.txt
SDL3_REPO = "https://github.com/Team-Resurgent/RXDK-SDL3.git"
SDL3_COMMIT = "059c91a1b44821a2cf0115498031ed6a317792d5"

# The value, on the original Xbox, of every condition guarding a source list
# this script reads. Keys are CMake if() conditions as written, minus outer
# whitespace; values are what CMake would evaluate them to for this platform.
CONDITIONS = {
    # Top-level CMakeLists.txt, lego1.
    "WIN32": False,  # the console has no Win32 subsystem; miniwin stands in
    "NOT ISLE_MINIWIN": False,
    "ISLE_EXTENSIONS": False,  # needs libweaver and websockets; off as on DOS
    "EMSCRIPTEN": False,
    "ISLE_USE_LWS": False,
    # miniwin/CMakeLists.txt.
    "NOT (VITA OR WINDOWS_STORE)": True,
    "OpenGL_FOUND": False,
    "EMSCRIPTEN OR OPENGL_ES2_LIBRARY": False,
    "OPENGL_ES2_LIBRARY": False,
    "OPENGL_ES3_LIBRARY": False,
    "EMSCRIPTEN OR OPENGL_ES3_LIBRARY": False,
    "ISLE_USE_GLIDE": False,
    "NINTENDO_SWITCH": False,
    "VITA": False,
    "NINTENDO_3DS": False,
    "WIN32 AND NOT WINDOWS_STORE": False,
    "WINDOWS_STORE": False,
    "ISLE_RENDERER_SOFTWARE": True,
    "ISLE_RENDERER_SDLGPU": False,  # SDL's GPU API is not available
    "USE_SDL_GPU IN_LIST GRAPHICS_BACKENDS": False,
    "USE_SOFTWARE_RENDER IN_LIST GRAPHICS_BACKENDS": True,
    "USE_PALETTE_SW_RENDER IN_LIST GRAPHICS_BACKENDS": False,
    "ISLE_COMPILE_SHADERS": False,
    "XBOXOG": True,
    "ISLE_BUILD_APP": True,  # the isle title
    "DOS": False,
    "ANDROID": False,
    "IOS": False,
    "MSVC AND NOT WINDOWS_STORE": False,
}

# miniaudio is built for mixing only; these are 3rdparty/CMakeLists.txt's
# PUBLIC definitions, so every target that includes miniaudio.h needs them.
MINIAUDIO_DEFINES = [
    "MA_ENABLE_ONLY_SPECIFIC_BACKENDS",
    "MA_NO_DECODING",
    "MA_NO_ENCODING",
    "MA_NO_WAV",
    "MA_NO_FLAC",
    "MA_NO_MP3",
    "MA_NO_DEVICE_IO",
    "MA_NO_RESOURCE_MANAGER",
    "MA_NO_GENERATION",
    "MA_NO_THREADING",
    "MA_NO_RUNTIME_LINKING",
]

# Definitions every isle target gets: XBOXOG selects the platform code, and
# miniwin (linked into every target) makes MINIWIN public.
COMMON_DEFINES = ["XBOXOG", "MINIWIN", "NDEBUG"]

# _WIN32 is predefined for the C compiler (the RXDK target is
# i686-pc-windows-gnu) but the console has no Win32 API; undefine it so C
# libraries take their portable paths, as C++ already does.
C_NO_WIN32 = ["-U_WIN32"]

# The SDK libraries the title links, as the SDL3 port's own titles do.
SDK_LIBRARIES = [
    "libd3d8.lib",
    "libxgraphics.lib",  # XGSwizzleRect, for textures
    "libdsound.lib",
    "libxapi.lib",
    "libcpp.lib",
    "libc.lib",
    "libkernel.lib",
]


# ---------------------------------------------------------------------------
# A small reader for the CMake commands that list sources.

def cmake_commands(path):
    """Yield (name, args, conditions) for each command in a CMake file, where
    conditions is the list of if()/elseif()/else() conditions (as
    (text, negated-predecessors) pairs) enclosing it."""
    with open(path, encoding="utf-8") as f:
        text = f.read()
    text = re.sub(r"#[^\n]*", "", text)
    stack = []  # [(condition, [previous conditions of this if chain])]
    pos = 0
    for m in re.finditer(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", text):
        if m.start() < pos:
            continue
        name = m.group(1)
        depth, i = 1, m.end()
        while depth and i < len(text):
            if text[i] == "(":
                depth += 1
            elif text[i] == ")":
                depth -= 1
            i += 1
        args = text[m.end():i - 1].strip()
        pos = i
        lname = name.lower()
        if lname == "if":
            stack.append((args, []))
        elif lname == "elseif":
            cond, prev = stack.pop()
            stack.append((args, prev + [cond]))
        elif lname == "else":
            cond, prev = stack.pop()
            stack.append((None, prev + [cond]))
        elif lname == "endif":
            stack.pop()
        elif lname in ("function", "macro", "foreach", "block"):
            stack.append(("__scope__", []))
        elif lname in ("endfunction", "endmacro", "endforeach", "endblock"):
            stack.pop()
        else:
            yield name, args, list(stack)


def evaluate(cond, where):
    cond = " ".join(cond.split())
    if cond not in CONDITIONS:
        sys.exit(f"{where}: condition '{cond}' is not in CONDITIONS; decide its "
                 "value for the original Xbox in xboxog/gen_manifest.py")
    return CONDITIONS[cond]


def enabled(stack, where):
    for cond, prev in stack:
        if cond == "__scope__":
            return False  # inside a function/foreach: not a plain source list
        if any(evaluate(p, where) for p in prev):
            return False
        if cond is not None and not evaluate(cond, where):
            return False
    return True


def target_sources(cmake_file, target):
    """The sources CMake gives `target` on the original Xbox, relative to the
    CMake file's directory: add_library(target ...) and target_sources(target
    ...) entries that are compiled (.c/.cpp), skipping generator variables."""
    base = os.path.dirname(cmake_file)
    sources = []
    for name, args, stack in cmake_commands(cmake_file):
        words = args.split()
        if not words or words[0] != target:
            continue
        if name.lower() not in ("add_library", "target_sources", "add_executable"):
            continue
        if not enabled(stack, f"{os.path.relpath(cmake_file, ROOT)} ({target})"):
            continue
        for w in words[1:]:
            w = w.strip('"')
            if w in ("PRIVATE", "PUBLIC", "INTERFACE", "STATIC", "SHARED", "WIN32",
                     "EXCLUDE_FROM_ALL") or "$" in w:
                continue
            if os.path.splitext(w)[1].lower() in (".c", ".cpp"):
                sources.append(os.path.normpath(os.path.join(base, w)))
    if not sources:
        sys.exit(f"no sources found for {target} in {cmake_file}")
    return sources


# ---------------------------------------------------------------------------

def fetch(repo, ref, dest, is_tag):
    if os.path.isdir(dest):
        return
    print(f"fetching {repo} @ {ref}")
    if is_tag:
        subprocess.check_call(["git", "clone", "-q", "--depth", "1", "-b", ref, repo, dest])
    else:
        subprocess.check_call(["git", "clone", "-q", repo, dest])
        subprocess.check_call(["git", "-C", dest, "checkout", "-q", ref])


def rel(path, project_dir):
    return os.path.relpath(path, project_dir).replace("\\", "/")


def write_manifest(project_dir, manifest):
    os.makedirs(project_dir, exist_ok=True)
    out = os.path.join(project_dir, "rxdk.project.json")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    print(f"wrote {os.path.relpath(out, ROOT)}: {len(manifest.get('sources', []))} sources")


def library(name, project_dir, sources, include_paths=(), public_include_paths=(),
            defines=(), compile_flags=(), references=()):
    d = project_dir
    m = {
        "name": name,
        "type": "library",
        "configuration": "release",
        "sources": [rel(s, d) for s in sources],
    }
    if public_include_paths:
        m["publicIncludePaths"] = [rel(p, d) for p in public_include_paths]
    if include_paths:
        m["includePaths"] = [rel(p, d) for p in include_paths]
    if references:
        m["projectReferences"] = [rel(r, d) for r in references]
    m["defines"] = list(defines)
    if compile_flags:
        m["compileFlags"] = list(compile_flags)
    m["cppStandard"] = "c++17"  # as CMakeLists.txt
    m["outputDir"] = "out"
    return m


# Where the build finds the game's files, inside the image next to the XBE, and
# the folders of the disc they come from (the title looks in D:\LEGO\).
GAME_DIR = os.path.join(HERE, "game")
DISC_FOLDERS = [
    (("LEGO", "Scripts"), ("LEGO", "Scripts")),
    (("DATA", "disk", "LEGO", "data"), ("LEGO", "data")),
]


def find_case_insensitive(root, parts):
    """The path under root whose components match parts ignoring case (discs differ: Lego, LEGO)."""
    path = root
    for part in parts:
        try:
            names = os.listdir(path)
        except OSError:
            return None
        match = next((n for n in names if n.lower() == part.lower()), None)
        if match is None:
            return None
        path = os.path.join(path, match)
    return path


def copy_game_files(disc):
    """Copy the game's files from a LEGO Island disc into xboxog/game/LEGO (git ignores it)."""
    copied = skipped = 0
    for src_parts, dst_parts in DISC_FOLDERS:
        src = find_case_insensitive(disc, src_parts)
        if src is None or not os.path.isdir(src):
            sys.exit(f"{disc} is not a LEGO Island disc: no {os.path.join(*src_parts)}")
        dst = os.path.join(GAME_DIR, *dst_parts)
        for dirpath, _, files in os.walk(src):
            out_dir = os.path.join(dst, os.path.relpath(dirpath, src))
            os.makedirs(out_dir, exist_ok=True)
            for name in files:
                a, b = os.path.join(dirpath, name), os.path.join(out_dir, name)
                if os.path.exists(b) and os.path.getsize(a) == os.path.getsize(b):
                    skipped += 1
                    continue
                shutil.copyfile(a, b)
                copied += 1
    print(f"game files: {copied} copied, {skipped} already there -> {os.path.join(GAME_DIR, 'LEGO')}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--no-fetch", action="store_true", help="do not fetch missing dependencies")
    ap.add_argument("--disc", metavar="PATH",
                    help="copy the game files from this LEGO Island disc (or copy of it) for the ISO")
    args = ap.parse_args()

    if args.disc:
        copy_game_files(args.disc)

    iniparser = os.path.join(DEPS, "iniparser")
    sdl3 = os.path.join(DEPS, "SDL3")
    if not args.no_fetch:
        os.makedirs(DEPS, exist_ok=True)
        fetch(INIPARSER_REPO, INIPARSER_TAG, iniparser, True)
        fetch(SDL3_REPO, SDL3_COMMIT, sdl3, False)

    j = lambda *p: os.path.join(ROOT, *p)
    third = j("3rdparty")

    # 3rd-party libraries.
    d = os.path.join(LIBS, "libsmacker")
    write_manifest(d, library(
        "libsmacker", d, [j("3rdparty", "libsmacker", "smacker.c")],
        public_include_paths=[j("3rdparty", "libsmacker")],
        defines=["NDEBUG"], compile_flags=C_NO_WIN32))

    d = os.path.join(LIBS, "miniaudio")
    write_manifest(d, library(
        "miniaudio", d, [j("3rdparty", "miniaudio", "miniaudio.c")],
        public_include_paths=[j("3rdparty", "miniaudio")],
        defines=["NDEBUG"] + MINIAUDIO_DEFINES, compile_flags=C_NO_WIN32))

    d = os.path.join(LIBS, "iniparser")
    write_manifest(d, library(
        "iniparser", d,
        [os.path.join(iniparser, "src", "iniparser.c"), os.path.join(iniparser, "src", "dictionary.c")],
        public_include_paths=[os.path.join(iniparser, "src")],
        defines=["NDEBUG"], compile_flags=C_NO_WIN32))

    # miniwin.
    d = os.path.join(LIBS, "miniwin")
    write_manifest(d, library(
        "miniwin", d, target_sources(j("miniwin", "CMakeLists.txt"), "miniwin"),
        public_include_paths=[j("miniwin", "include")],
        include_paths=[j("miniwin", "src", "internal"),
                       j("miniwin", "src", "d3drm", "backends", "sdl3gpu", "shaders", "generated")],
        # Direct3D 8 first, the software renderer as the fallback.
        defines=COMMON_DEFINES + ["USE_XBOX_D3D8", "USE_SOFTWARE_RENDER"],
        references=[sdl3]))

    # lego1.
    d = os.path.join(LIBS, "lego1")
    write_manifest(d, library(
        "lego1", d, target_sources(j("CMakeLists.txt"), "lego1"),
        public_include_paths=[j("util"), j("extensions", "include"), j("LEGO1"),
                              j("LEGO1", "omni", "include"), j("LEGO1", "lego", "sources"),
                              j("LEGO1", "lego", "legoomni", "include"),
                              j("LEGO1", "lego", "legoomni", "include", "actions"),
                              j("3rdparty", "vec")],
        defines=COMMON_DEFINES + MINIAUDIO_DEFINES,
        # target_precompile_headers(lego1 PRIVATE "<lego1_pch.h>"): every
        # lego1 source is compiled with it included first.
        compile_flags=["-include", "lego1_pch.h"],
        references=[sdl3, os.path.join(LIBS, "miniwin"), os.path.join(LIBS, "libsmacker"),
                    os.path.join(LIBS, "miniaudio")]))

    # The isle title.
    isle_sources = target_sources(j("CMakeLists.txt"), "isle") + [j("ISLE", "xboxog", "config.cpp")]
    title = {
        "name": "isle",
        "configuration": "release",
        "sources": [rel(s, HERE) for s in isle_sources],
        "includePaths": [rel(j("ISLE"), HERE)],
        "projectReferences": [rel(p, HERE) for p in (
            os.path.join(LIBS, "lego1"), os.path.join(LIBS, "miniwin"),
            os.path.join(LIBS, "iniparser"), os.path.join(LIBS, "libsmacker"),
            os.path.join(LIBS, "miniaudio"), sdl3)],
        "libraries": SDK_LIBRARIES,
        "defines": COMMON_DEFINES + ["ISLE_APP"] + MINIAUDIO_DEFINES,
        "cppStandard": "c++17",
        "outputDir": "out",
        # The game's files (see --disc), packed next to the XBE when present.
        "deployPaths": ["LEGO=game/LEGO"],
    }
    write_manifest(HERE, title)


if __name__ == "__main__":
    main()
