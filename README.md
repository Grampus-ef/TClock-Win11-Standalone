# TClock11 Standalone — Windows 11 compatible taskbar-clock prototype

This repository contains a **separate-process, Win32 taskbar overlay** for Windows 11.
It does **not** inject a DLL into Explorer, modify Explorer taskbar internals, or
require ExplorerPatcher. The program is experimental and is **not yet feature
equivalent** to [TClock-Win10](https://github.com/MantisMountainMobile/TClock-Win10).

## Building with GitHub Actions

Open **Actions → Build TClock11 Standalone (x64) → Run workflow**,
or push changes to the standalone source or workflow.
After a successful run, download the
**TClock11Standalone-Windows-x64-prototype** artifact containing the
Windows x64 GUI EXE, INI, license, and Japanese instructions.

Compiler: Visual Studio 2022 MSVC, C++17, Windows SDK, Windows runner.

The CI checks compiler success and PE format (AMD64, PE32+, GUI).
**That is not equivalent to running the program on Windows 11.**

## Windows 11 initial testing

1. Test first on a disposable Windows 11 24H2+ machine or VM.
2. Extract the artifact, and keep TClock11Standalone.exe and .ini together.
3. Close the original injected TClock-Win10 before launching the prototype.
4. Adjust Width/Height/EdgeInset/ShiftX/ShiftY in the INI if it overlaps
   the built-in clock. Right-click the overlay → Reload configuration.
5. Right-click the overlay or tray icon → Exit TClock11 to quit.

Known limitations: not a real taskbar clock replacement, possible overlay
misalignment, incomplete TClock macro syntax, mixed-DPI/multiple-monitor
limitations, taskbar auto-hide/fullscreen interaction still requiring tests.
No claim of successful Windows 11 runtime validation is made here.

## Relation to the original sources

The standalone C++ implementation is based on the October 2026 handoff
prototype (not a source modification of the upstream injected DLL).
The user's original upstream source files are preserved separately in
the handoff archive. They have **not yet been imported into this GitHub
repository**; the standalone workflow requires none of them.
The full upstream project and additions remain available in the separately
provided handoff ZIP.

See [windows11-standalone/README-ja.md](windows11-standalone/README-ja.md).

GPL-3.0-or-later for standalone additions. Upstream license retained in LICENSE.
