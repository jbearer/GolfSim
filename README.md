# SimGolf, but it's 2019

This will be a loose clone of [SimGolf](https://en.wikipedia.org/wiki/Sid_Meier%27s_SimGolf), designed to run on modern platforms, with some tweaks to gameplay.

## Goals
* Be as fun to play as SimGolf
* Support modern platforms, especially Windows 10
* Portability, especially to:
    - Windows 10
    - OS X
    - Common flavors of Linux, e.g. Ubuntu
* Lightweight:
    - Small, self-contained binary
    - As few external dependencies as possible
    - Low memory and CPU load

## Dependencies

_System Libraries_
* libc
* OpenGL

_External Libraries_
* glfw (included in binary)
* glew (included in binary)

## Development Dependencies

* ANSI C99 compliant compiler
* CMake >= 3.0
* Doxygen (optional)

## Building

### On Linux

```
./setup.sh -b
```

Binary will be written to `build/bin/golf`.

To build in debug mode, add the `-g` flag to `setup.sh`. This will also cause development documention to be generated in `build/docs`.

### On any other platform

Good luck with that.

## Running

```
./setup.sh -r
```

Or, build and then run `build/bin/golf`.
