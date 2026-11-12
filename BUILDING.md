# Building Guide

This fork targets the North American Ocarina of Time Rev 1 (NTSC 1.1). You need a legally obtained matching ROM; neither ROMs nor game assets are included.

## 1. Clone the Zelda64Recomp Repository
This project makes use of submodules so you will need to clone the repository with the `--recurse-submodules` flag.

```bash
git clone --recurse-submodules https://github.com/NotoriousGOR/ocarina-of-time-recomp.git
# if you forgot to clone with --recurse-submodules
# cd /path/to/cloned/repo && git submodule update --init --recursive
```

## 2. Install Dependencies

### macOS
Install the Xcode command-line tools and Homebrew packages `cmake`, `ninja`, `llvm`, `make`, `libiconv`, `sdl2`, and `sdl3`.

### Linux
For Linux the instructions for Ubuntu are provided, but you can find the equivalent packages for your preferred distro.

```bash
# For Ubuntu, simply run:
sudo apt-get install cmake ninja-build libsdl2-dev libgtk-3-dev lld llvm clang
```

### Windows
You will need to install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/).
In the setup process you'll need to select the following options and tools for installation:
- Desktop development with C++
- C++ Clang Compiler for Windows
- C++ CMake tools for Windows

The other tool necessary will be `make` which can be installe via [Chocolatey](https://chocolatey.org/):
```bash
choco install make
```

## 3. Build the matching decompilation
Clone [zeldaret/oot](https://github.com/zeldaret/oot) beside this repository, put your NTSC-U Rev 1 ROM at `oot/baseroms/ntsc-1.1/baserom.z64`, then build it:

```bash
cd ../oot
gmake setup VERSION=ntsc-1.1 REGION=US
gmake -j4 VERSION=ntsc-1.1 REGION=US
cd -
```

Use the generated ELF and ROM from `oot/build/ntsc-1.1/` for the recompiler inputs. The ROM supplied to the finished app must be the original, standard-format ROM, not the decompilation output.

## 4. Set up the games

The Ocarina & Majora macOS launcher runs Ocarina of Time itself and opens Majora's Mask in a separate Zelda 64: Recompiled app. Install that app separately at `~/Applications/Zelda64Recompiled.app` or `/Applications/Zelda64Recompiled.app`; it is not bundled with this fork.

Place the original North American Rev 1 ROM at `~/Library/Application Support/Zelda64Recompiled/roms/oot.ntsc-u.rev1.z64`. The launcher detects it automatically and does not provide a ROM picker. Both applications use the same `Zelda64Recompiled` support folder for controls and settings. Do not commit ROMs.

With the decompilation checkout beside this repository, copy the original ROM into place from the repository root:

```bash
mkdir -p "$HOME/Library/Application Support/Zelda64Recompiled/roms"
cp ../oot/baseroms/ntsc-1.1/baserom.z64 "$HOME/Library/Application Support/Zelda64Recompiled/roms/oot.ntsc-u.rev1.z64"
```

## 5. Generating the C code

Build the bundled N64Recomp tools and make their executables available at the repository root. `N64Recomp` is invoked there by the patch-generation build step.

```bash
cmake -S lib/N64ModernRuntime/N64Recomp -B build-n64-tools -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-n64-tools --target N64Recomp RSPRecomp RecompModTool -j4
ln -s build-n64-tools/N64Recomp N64Recomp
ln -s build-n64-tools/RSPRecomp RSPRecomp
```

From this repository root, create the recompiler ELF with the required section rename, then generate the game and RSP code:

```bash
llvm-objcopy --rename-section ..makerom.ent=..makerom_ent ../oot/build/ntsc-1.1/oot-ntsc-1.1.elf oot.ntsc-1.1.elf
./N64Recomp oot.ntsc-1.1.toml
./RSPRecomp aspMain.ntsc-1.1.toml
./RSPRecomp njpgdspMain.ntsc-1.1.toml
```

Generate the symbol dumps used by the patch and HUD mod:

```bash
cd OoTRecompSyms
../N64Recomp ../OoTRecompSyms.dump.ntsc-1.1.toml --dump-context
mv dump.toml oot.ntsc-1.1.syms.toml
mv data_dump.toml oot.ntsc-1.1.datasyms.toml
cd ..
```

## 6. Building the Project

Finally, you can build the project! :rocket:

On Windows, you can open the repository folder with Visual Studio, and you'll be able to `[build / run / debug]` the project from there.

If you prefer the command line or you're on a Unix platform you can build the project using CMake:

```bash
cmake -S . -B build-cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -G Ninja -DCMAKE_BUILD_TYPE=Release # or Debug if you want to debug
cmake --build build-cmake --target Zelda64Recompiled -j4 --config Release # or Debug
```

## 7. Success

Voilà! On macOS, the app bundle is `build-cmake/Ocarina & Majora.app`. Run it with the repository root as its working directory so it can find the assets.

> [!IMPORTANT]
> In the game itself, use your original standard-format NTSC-U Rev 1 ROM, not the decompilation output.
