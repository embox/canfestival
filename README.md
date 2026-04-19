# CanFestival

CanFestival is an open-source CANopen stack implementation in C, with a Python-based object dictionary editor.

## Building

CanFestival uses CMake:

```sh
mkdir build && cd build
cmake .. -DCF_TARGET=unix -DCF_CAN_DRIVER=virtual -DCF_TIMERS_DRIVER=unix
make
```

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `CF_TARGET` | `unix` | Target platform (`unix`, `win32`) |
| `CF_CAN_DRIVER` | `virtual` | CAN driver (`virtual`, `socket`, `peak_linux`, `kvaser`, ...) |
| `CF_TIMERS_DRIVER` | `unix` | Timer driver (`unix`, `win32`, `xeno`) |
| `CF_ENABLE_LSS` | `OFF` | Enable LSS (Layer Setting Services) |
| `CF_ENABLE_LSS_FS` | `OFF` | Enable LSS FastScan |
| `CF_ENABLE_DLL_DRIVERS` | `ON` | Build CAN drivers as shared libraries |
| `CF_BUILD_EXAMPLES` | `OFF` | Build example applications |

Stack constants (all configurable via `-D`):

`CF_MAX_CAN_BUS_ID`, `CF_SDO_MAX_LENGTH_TRANSFER`, `CF_SDO_BLOCK_SIZE`,
`CF_SDO_MAX_SIMULTANEOUS_TRANSFERS`, `CF_NMT_MAX_NODE_ID`, `CF_SDO_TIMEOUT_MS`,
`CF_MAX_NB_TIMER`, `CF_EMCY_MAX_ERRORS`, `CF_LSS_TIMEOUT_MS`, `CF_LSS_FS_TIMEOUT_MS`

### Build outputs

- `libcanfestival.a` -- Core CANopen stack
- `libcanfestival_<target>.a` -- Platform driver (includes timer driver)
- `libcanfestival_can_<driver>.so` -- CAN driver (shared, when `CF_ENABLE_DLL_DRIVERS=ON`)

### Cross-compilation

Use standard CMake toolchain files:

```sh
cmake .. -DCMAKE_TOOLCHAIN_FILE=path/to/toolchain.cmake
```

## Object Dictionary Editor

The `objdictgen/` directory contains a GUI editor for CANopen object dictionaries, written in Python 3 with wxPython 4.

```sh
cd objdictgen
python3 objdictedit.py
```

### Requirements

- Python 3
- wxPython 4 (`pip install wxPython`)

### Command-line code generation

Generate C source from a DCF file:

```sh
python3 objdictgen/objdictgen.py MyNode.dcf MyNode.c
```

### File format

Object dictionaries are stored as DCF (Device Configuration File) -- an INI-style format extending the standard CANopen EDS format. CanFestival uses extension sections (`[CanFestivalNode]`, `[CanFestivalParams]`, etc.) to preserve editor metadata.

## Available CAN drivers

| Driver | Platform | Description |
|--------|----------|-------------|
| `virtual` | All | Pipe-based virtual CAN for testing |
| `socket` | Linux | SocketCAN |
| `peak_linux` | Linux | PEAK PCAN |
| `peak_win32` | Windows | PEAK PCANLight |
| `kvaser` | Linux/Windows | Kvaser CANlib |
| `anagate_linux` | Linux | AnaGate CAN |
| `serial` | All | CAN over serial |
| `lincan` | Linux | LinCAN |
| `can4linux` | Linux | can4linux |

## Project structure

```
CMakeLists.txt          Top-level build
src/                    Core CANopen stack (C)
drivers/                Platform, timer, and CAN drivers
include/                Public headers
objdictgen/             Object dictionary generation (Python)
  objdictedit.py        Object dictionary editor
  networkedit.py        Network topology editor
  objdictgen.py         CLI code generator
  config/               CiA device profiles (.prf)
examples/               Example applications
doc/                    Documentation
```

## Examples

Enable with `-DCF_BUILD_EXAMPLES=ON`:

```sh
cmake .. -DCF_BUILD_EXAMPLES=ON -DCF_ENABLE_LSS=ON
make
```

Each example has its object dictionary stored as one or more `.dcf` files and its main C sources alongside. At configure time CMake runs `objdictgen.py` on each DCF to produce the matching `.c`/`.h` in the build tree, then links them with the hand-written sources and the CanFestival libraries. An example is skipped with a status message if any of its DCF files is missing.

| Example | DCF files | Notes |
|---------|-----------|-------|
| `CANOpenShell` | `CANOpenShellMasterOD.dcf`, `CANOpenShellSlaveOD.dcf` | Interactive CANopen shell |
| `DS401_Master` | `TestMaster.dcf` | DS-401 master example |
| `DS401_Slave_Gui` | `ObjDict.dcf` | wxWidgets C++ slave; requires wxWidgets-dev |
| `TestMasterSlave` | `TestMaster.dcf`, `TestSlave.dcf` | Master + slave in one process |
| `TestMasterSlaveLSS` | `TestMaster.dcf`, `TestSlaveA.dcf`, `TestSlaveB.dcf` | Requires `CF_ENABLE_LSS=ON` |

To edit a DCF in the GUI:

```sh
python3 objdictgen/objdictedit.py examples/TestMasterSlave/TestMaster.dcf
```

## License

LGPL v2.1 -- see [LICENCE](LICENCE).

## Authors

See [AUTHORS](AUTHORS).