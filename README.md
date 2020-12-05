# fx-CG50 Manager PLUS - gdbserver

A GDB Remote Serial Protocol implementation that hooks to fx-CG50 Manager PLUS allowing to debug code running on the emulated calculator.

Demo : [https://youtu.be/wGWVSqz2svo](https://youtu.be/wGWVSqz2svo)

## Usage :
* Build with [ninja](https://ninja-build.org/)
* Rename `CPU73050.dll` to `CPU73050.real.dll` in fx-CG50 Manager PLUS installation folder
* Copy `obj/CPU73050.dll` from the build tree to the same folder
* When starting fx-CG50 Manager PLUS, connect with GDB to port 31188

## Major Issues :
This software is still in development, many problems remain.
* The server waits for a `continue` command from GDB before starting. If this command takes too long to arrive, fx-CG50 Manager PLUS freaks out and thinks the installation is corrupted and will refuse to start.
* Writing to memory and by extension putting breakpoints and single stepping are still not implemented. You must use the `BRK` (`0x003B`) instruction to trigger the debugger. This instruction is documented [here](https://www.st.com/resource/en/user_manual/cd00147165-sh-4-32-bit-cpu-core-architecture-stmicroelectronics.pdf) but most assemblers/disassemblers/debuggers/emulators/CPUs seems to not implement it. It might just not exist.
* The offsets of important values in memory are hard-coded, you should check `include/fxCG50gdb/emulator_offsets.h` to make sure you have the correct version of `CPU73050.dll`.
* Reading to some weird addresses might segfault.
* It has only been tested with the GRAPH90+ E mode, other modes might have other issues or be completely broken.
