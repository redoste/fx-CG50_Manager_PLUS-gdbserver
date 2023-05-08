# fx-CG50 Manager PLUS - gdbserver

A GDB Remote Serial Protocol implementation that hooks to fx-CG50 Manager PLUS allowing to debug code running on the emulated calculator.

* Demo (new at 33e6ab4) : [https://youtu.be/3ucQ3VpzQxI](https://youtu.be/3ucQ3VpzQxI) : This demo features the debugging of an add-in compiled with GCC and demonstrates the usage of software and hardware breakpoints.
* Demo (old at 7a848ec) : [https://youtu.be/wGWVSqz2svo](https://youtu.be/wGWVSqz2svo) : This demo features the debugging of an add-in written in assembler and demonstrates the modification of general purpose registers.

## Build :
Binaires are built by *GitHub Actions* and avaliable in [releases](https://github.com/redoste/fx-CG50_Manager_PLUS-gdbserver/releases) if you don't want to do it yourself.
<details>
  <summary>Using MSVC on Windows</summary>
  
  * Install [NASM](https://nasm.us) and [Visual Studio with its C compiler](https://visualstudio.microsoft.com/)
  * From the Start menu open the *x86 Native Tools Command Prompt*
  ```
  > cd C:\path\to\git\repo
  > set PATH=%PATH%;"C:\Program Files\NASM"
  > msbuild
  ```
  * The binary will be available in `Release\CPU73050.dll`
</details>
<details>
  <summary>Using MinGW-w64 on Windows with Cygwin</summary>
  
  * Install [Cygwin](https://www.cygwin.com/) and in the graphical installer make sure to check `ninja`, `nasm` and `mingw64-i686-gcc-core`
  * Open a new Cygwin terminal
  ```
  $ cd /cygdrive/c/path/to/git/repo
  $ ninja
  ```
  * The binary will be available in `obj/CPU73050.dll`
</details>
<details>
  <summary>Using MinGW-w64 on Windows with MSYS2</summary>
  
  * Install [MSYS2](https://www.msys2.org/)
  * Open a new MSYS2 terminal
  ```
  $ pacman -S ninja nasm mingw-w64-i686-gcc
  $ cd /c/path/to/git/repo
  $ PATH=$PATH:/mingw32/bin/ ninja
  ```
  * The binary will be available in `obj/CPU73050.dll`
</details>
<details>
  <summary>Using MinGW-w64 on Linux</summary>
  
  * Install [NASM](https://nasm.us), [ninja](https://ninja-build.org/) and an i686 build of [MinGW-w64](https://www.mingw-w64.org/).
  * NASM and ninja will probably be called as is in your package manager but MinGW-w64 will be harder to find, try searching for `mingw` and `i686` (e.g. on Arch Linux it's [`mingw-w64-gcc`](https://archlinux.org/packages/community/x86_64/mingw-w64-gcc/), on Debian [`gcc-mingw-w64-i686`](https://packages.debian.org/bullseye/gcc-mingw-w64-i686) or on Void Linux [`cross-i686-w64-mingw32`](https://github.com/void-linux/void-packages/blob/master/srcpkgs/cross-i686-w64-mingw32)).
  * Open a terminal
  ```
  $ cd /path/ro/git/repo
  $ $EDITOR build.ninja # Depending on your distribution you may want to update $cc and $cross-prefix
  $ ninja
  ```
  * The binary will be available in `obj/CPU73050.dll`
</details>

## Usage :
* Rename `CPU73050.dll` to `CPU73050.real.dll` in fx-CG50 Manager PLUS installation folder
* Copy the new `CPU73050.dll` in the same folder
* When starting fx-CG50 Manager PLUS, connect with GDB via your interface of choice

## GDB Interfaces :
*fx-CG50 Manager PLUS - gdbserver* provides 3 ways to connect to the debugger. You can select one by adding the following argument to `fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe` command line : `/gdb:[interface name]:[interface options]`.

|Name  |Option|Description                                                     |
|------|------|----------------------------------------------------------------|
|`tcp` |port  |TCP socket listening on IPv4 `0.0.0.0` with the specified port  |
|`com` |`COMn`|Serial port `COMn`                                              |
|`wine`|path  |Unix socket when *fxCG50gdb* is running through Wine on \*/Linux|

For some reason the `tcp` interface seems to be pretty slow. We recommand using `com` or `wine` when possible.

## Major Issues :
This software is still in development, many problems remain.
* ~~The server waits for a `continue` command from GDB before starting. If this command takes too long to arrive, fx-CG50 Manager PLUS freaks out and thinks the installation is corrupted and will refuse to start.~~
* ~~Writing to memory and by extension putting breakpoints and single stepping are still not implemented. You must use the `BRK` (`0x003B`) instruction to trigger the debugger. This instruction is documented [here](https://www.st.com/resource/en/user_manual/cd00147165-sh-4-32-bit-cpu-core-architecture-stmicroelectronics.pdf) but most assemblers/disassemblers/debuggers/emulators/CPUs seems to not implement it. It might just not exist.~~
* The offsets of important values in memory are hard-coded, you should check `include/fxCG50gdb/emulator_offsets.h` to make sure you have the correct version of `CPU73050.dll`.
* ~~Reading to some weird addresses might segfault.~~
* It has only been tested with the GRAPH90+ E mode, other modes might have other issues or be completely broken.
* It is slow.
