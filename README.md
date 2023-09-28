# decomp2dbg plugin for x64dbg

This plugin implements bridging between decompilers and x64dbg,
by implementing the [decomp2dbg](https://github.com/mahaloz/decomp2dbg) protocol.

It's currently still in very early developement and not very functional/stable,
though it might already be useful to you.

Once this project is more mature, upstreaming into the main decomp2dbg repo is intended.

## Features

- [x] Implementation of decomp2dbg API client
- [x] Automated creation of functions and globals
- [ ] Automated creation of types (would first need to be implemented in Ghidra server plugin)
- [ ] Configuration (G)UI (currently hardcoded to localhost as server and the main .exe module as target)
- [x] Super janky decompiler output display as comments in disassembler
- [ ] Less janky decompiler output display in dedicated tab/popup window
- [ ] Automated application of arg and return types to functions
- [ ] Decent performance
- [x] Bugs

## Installation

Grab a build from github releases or the latest pipeline run,
then extract into the x64dbg plugin directories for x32 and x64.

Then, follow decomp2dbg's installation instructions to get the decompiler
side set up properly.

## Usage

The plugin currently does not expose any UI.

Once you start debugging, the plugin will connect to your decompiler
and populate functions + globals.

Whenever you step in the debugger or select a range in the disassembler,
the current function will be decompiled and the output shown as comments.

Note that decompilation of selected areas is still quite janky.

## How to build

I don't like developing on Windows, so this plugin is built without MSVC to keep it cross-platform.

You'll need a mingw-based toolchain with libcurl and libxmlrpc built for it as static libs,
as well as an appropriate cmake toolchain file to find everything.

On Windows you'll probably want to install the needed packages in msys2.

On Unix you can use the [mxe](https://github.com/mxe/mxe) project, see Dockerfile
for details on how to use it.

Building with MSVC should also be possible, you should take a look at the official x64dbg
plugin template for inspiration on how to do that.

If you manage to get it working without breaking the non-MSVC build, a PR is very welcome!
