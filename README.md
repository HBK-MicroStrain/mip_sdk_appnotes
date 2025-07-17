
MIP SDK AppNotes Repository
===========================

This repository houses example projects relating to various application notes for MicroStrain's 3DM inertial sensors.

Each project is in its own subdirectory.


Building and Running
====================

Prerequisites
-------------

* git
* cmake (v3.22 or later)
* A working C++ compiler

Steps
-----

1. Create a `build` subdirectory and enter it.  
   `mkdir build && cd build`
2. Run `cmake .. -G <generator>` from within `build` (substitute `<generator>` with a suitable generator for your
   platform, e.g. "Ninja", "Unix Makefiles", or "Visual Studio 14").  
   `cmake .. -G "Ninja"`
3. Run `cmake --build .` to build all projects. You may specify `--target <project>`, substituting the project name to
   build just one project. The make targets use the TitleCase naming convention.
   `cmake --build . --target SquareWaveOutput`
4. Run the project executable  
   `square_wave_output/SquareWaveOutput` (Linux) or `square_wave_output/SquareWaveOutput.exe` (Windows)

Note that you'll likely need to modify the configuration options in the source code, which are described in the README
for each project. In particular, the `SERIAL_PORT` and `SERIAL_BAUD` parameters may need to be changed to match your
setup.
