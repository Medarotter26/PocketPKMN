# Building PocketPKMN

## The app (.3dsx)

Needs devkitARM (devkitPro) with libctru, citro2d, citro3d and the tex3ds tool.

- **Linux / macOS / Windows with devkitPro installed:** `make` in the project folder makes `pocketpkmn.3dsx`.
- **Windows with WSL + Docker** (how the releases are built): `.\build.ps1` runs `make` inside the
  `devkitpro/devkitarm` Docker image in WSL. It expects a WSL distribution named exactly **`Ubuntu-24.04`** with
  Docker working inside it.

`make` does not notice changes in `romfs/`: delete `pocketpkmn.3dsx` before building after such a change.

## The CIA

`make cia` (or `.\build.ps1 cia`) also runs `cia/make_cia.sh`, which packs `pocketpkmn.cia` from the built `.elf`
and `romfs/` with the app's own banner, icon and jingle (`cia/`, made by `tools/cia_art.py`). It needs two tools,
looked for in `~/3ds-cia-tools` (or set `CIA_TOOLS`, `MAKEROM`, `BANNERTOOL`):

- **makerom** 0.19.0 - Linux build `makerom-v0.19.0-ubuntu_x86_64.zip` from the releases of
  [3DSGuy/Project_CTR](https://github.com/3DSGuy/Project_CTR).
- **bannertool** 1.2.3 - `bannertool-1.2.3-linux.tar.gz` from the releases of
  [carstene1ns/3ds-bannertool](https://github.com/carstene1ns/3ds-bannertool).

Those Linux builds need glibc 2.38 or newer (Ubuntu 24.04 has it; the devkitarm Docker image does not), which is why
`build.ps1` runs the packing step in WSL itself, outside Docker.

## PC tests

Linux / WSL with gcc and Python 3 + Pillow. See the README for which tests need your own saves (`hw/`). The catch
legality test (`tests/run_catches.sh`) also needs .NET 10 and the PKHeX.Core checker built from
`tools/pkhex_check_Program.cs` + `tools/pkhex_check_Templates.cs` as `~/pkhex-check` (a `dotnet new console` project
with the `PKHeX.Core` NuGet package; run with `DOTNET_ROOT` set).
