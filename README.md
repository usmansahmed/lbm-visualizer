# LBM Visualizer

This is a small C++/CUDA/OpenGL project for visualizing a 3D Lattice Boltzmann Method (LBM) fluid simulation in real time.

The solver runs on CUDA, and OpenGL is used to display 2D slices of the 3D domain. Dear ImGui is used for the controls, so the simulation can be changed and inspected while the program is running.

## Features

- 3D LBM simulation on the GPU
- CUDA/OpenGL interoperability using a Pixel Buffer Object (PBO)
- Real-time OpenGL texture visualization
- Slice views in XY, XZ, and YZ directions
- Visualization of velocity magnitude, velocity components, density, and obstacles
- Velocity arrows for the in-plane velocity
- Probe tool for inspecting individual cells
- Several obstacle presets
- Dear ImGui based controls with docking

## Requirements

You need the following installed:

- A C++ compiler
- CMake
- CUDA Toolkit
- OpenGL-capable GPU driver
- GLFW
- GLAD
- Dear ImGui

This project has currently only been built and run on Windows. Linux support has not been tested.

## Building the project

The build instructions below are for Windows. The project has not yet been built or tested on Linux.

From the project root, configure the project first:

```bash
cmake --preset windows-debug
```

Then build it:

```bash
cmake --build --preset windows-debug --parallel
```

For a release build, use the release preset if it exists in the project:

```bash
cmake --preset windows-release
cmake --build --preset windows-release --parallel
```

If the project does not use presets on your machine, you can also build it manually:

```bash
cmake -S . -B build
cmake --build build --parallel
```

## Running

Run the executable from the project root folder.

This is important because the program loads shader files using relative paths such as:

```text
shaders/quad.vert
shaders/quad.frag
```

If the executable is started from another working directory, the shaders may not be found.

## Basic usage

After starting the program, use the control panel to change the simulation and visualization.

Common controls:

- **Resume / Pause** starts or stops the simulation update.
- **Previous / Next** moves between slices.
- **Display field** selects what is shown.
- **Orientation** selects the slice direction: XY, XZ, or YZ.
- **Slice** moves through the 3D domain.
- **Automatic color scaling** adjusts the color range automatically.
- **Show velocity arrows** overlays arrows for the in-plane velocity.
- **Apply / Restart Simulation** applies grid or obstacle changes.

Some settings require restarting the simulation because CUDA memory and the obstacle mask need to be recreated.

## Visualization fields

The visualizer can show different scalar fields:

- **Velocity magnitude**: full 3D speed
- **Velocity X**
- **Velocity Y**
- **Velocity Z**
- **Density**

One thing to keep in mind: the arrows show only the velocity components inside the current slice.

For example:

```text
XY slice arrows use velocity X and velocity Y
XZ slice arrows use velocity X and velocity Z
YZ slice arrows use velocity Y and velocity Z
```

So a red region in velocity magnitude does not always mean the arrows must be large. The velocity may mainly point into or out of the current slice.

## Probe tool

The probe tool is used to inspect one cell in the simulation.

Click inside the simulation view to see:

- Cell position
- Density
- Velocity X
- Velocity Y
- Velocity Z
- Speed
- Whether the cell is fluid or obstacle

For obstacle cells, the velocity should normally be treated as zero for display purposes, because obstacle cells are not real fluid cells.

## Obstacle presets

The project supports several obstacle types:

- **None**
- **Single block**
- **Two blocks**
- **Single cylinder**
- **Two cylinders**
- **Cylinder array**
- **Sphere**
- **Two spheres**
- **Backward-facing step**
- **Wall with slit**
- **Random porous block**

The obstacle preset can be selected in the control panel. After changing the obstacle type or obstacle parameters, press **Apply / Restart Simulation**.

Some obstacle settings are only useful for certain obstacle types:

- **Obstacle size** is mainly used for block-like obstacles and wall thickness.
- **Obstacle radius** is used for cylinders and spheres.
- **Cylinder spacing X/Y** is used for the cylinder array.
- **Step height** is used for the backward-facing step.
- **Slit height** is used for the wall with slit.
- **Solid probability** and **random seed** are used for the random porous block.

For thin domains, such as `256 x 64 x 16`, cylinders and blocks are usually easier to see. Spheres look better when the Z dimension is larger, for example `256 x 128 x 64`.

## Simulation settings

The main simulation settings are:

- **Nx, Ny, Nz**: grid dimensions
- **Omega**: LBM relaxation parameter
- **Obstacle preset**: selected obstacle shape
- **Obstacle size/radius/spacing**: obstacle geometry settings

The derived values shown in the UI are:

```text
tau = 1 / omega
nu  = (1 / 3) * (tau - 0.5)
```

For testing, moderate values of omega are usually easier to work with. Very high omega values can make the simulation more sensitive.

## Approximate GPU memory usage

The solver stores two LBM distribution arrays, macroscopic fields, and the obstacle mask. The table below gives a useful estimate.

| Grid size | Cells | Approx. solver memory |
|---:|---:|---:|
| `256 x 64 x 16` | 262k | ~45 MB |
| `256 x 128 x 64` | 2.1M | ~355 MB |
| `256 x 256 x 64` | 4.2M | ~710 MB |
| `256 x 256 x 128` | 8.4M | ~1.4 GB |
| `256 x 256 x 256` | 16.8M | ~2.8 GB |
| `512 x 256 x 128` | 16.8M | ~2.8 GB |
| `512 x 512 x 512` | 134.2M | ~22.7 GB |

## Project structure

A rough overview of the project:

```text
src/app/          Application logic and state
src/lbm/          LBM solver and CUDA kernels
src/rendering/    OpenGL rendering, textures, PBOs, shaders
src/geometry/     Obstacle generation and arrow/probe geometry
src/ui/           Dear ImGui user interface
shaders/          OpenGL shader files
```