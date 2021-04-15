# fx-CG50 Manager PLUS - gdbserver

A GDB Remote Serial Protocol implementation that hooks to fx-CG50 Manager PLUS allowing to debug code running on the emulated calculator.

* Demo (new at 33e6ab4) : [https://youtu.be/3ucQ3VpzQxI](https://youtu.be/3ucQ3VpzQxI) : This demo features the debugging of an add-in compiled with GCC and demonstrates the usage of software and hardware breakpoints.
* Demo (old at 7a848ec) : [https://youtu.be/wGWVSqz2svo](https://youtu.be/wGWVSqz2svo) : This demo features the debugging of an add-in written in assembler and demonstrates the modification of general purpose registers.

## Usage :
* Build with [Ninja](https://ninja-build.org/)
* Rename `CPU73050.dll` to `CPU73050.real.dll` in fx-CG50 Manager PLUS installation folder
* Copy `obj/CPU73050.dll` from the build tree to the said folder
* When starting fx-CG50 Manager PLUS, connect with GDB to port 31188

### Build with MSVC :
A solution file is available for building with MSVC but keep in mind that the MinGW + Ninja combo remains the primary build system. MSVC support is available for Windows users who don't want to install an entirely different toolchain and for debugging or profiling situations that require PDB instead of DWARF.

It might be required to install [https://www.nasm.us/](nasm) and make it available in your PATH.

## Major Issues :
This software is still in development, many problems remain.
* ~~The server waits for a `continue` command from GDB before starting. If this command takes too long to arrive, fx-CG50 Manager PLUS freaks out and thinks the installation is corrupted and will refuse to start.~~
* ~~Writing to memory and by extension putting breakpoints and single stepping are still not implemented. You must use the `BRK` (`0x003B`) instruction to trigger the debugger. This instruction is documented [here](https://www.st.com/resource/en/user_manual/cd00147165-sh-4-32-bit-cpu-core-architecture-stmicroelectronics.pdf) but most assemblers/disassemblers/debuggers/emulators/CPUs seems to not implement it. It might just not exist.~~
* The offsets of important values in memory are hard-coded, you should check `include/fxCG50gdb/emulator_offsets.h` to make sure you have the correct version of `CPU73050.dll`.
* ~~Reading to some weird addresses might segfault.~~
* It has only been tested with the GRAPH90+ E mode, other modes might have other issues or be completely broken.
* It is slow.
