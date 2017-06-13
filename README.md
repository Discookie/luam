# LUA Merger - a pre-runtime require implementation

Not so long ago I encountered a [sandbox](https://github.com/discookie/shieldmod) where require() was disabled.
Here is my attempt at making a file merger that does just that, but without relying on the sandbox.

This tool is fully command-line and I do not intend to write a GUI for it.

## Usage

```luam [args] <input> [<output>] [<input2> <output2>]...```

More detailed usage available with `-h`, examples are available with `-u`.

## Build
Use `make.bat` to build it with g++ (needs to be in PATH).
Uses the C++11 standard with standard C++ libraries.

-----

(c) 2017 Discookie - Released under the MIT license.
