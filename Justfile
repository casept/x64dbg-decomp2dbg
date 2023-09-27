set shell := ["bash", "-uc"]

default:
	@just --list

# Build the extension DLL
build:
	#!/usr/bin/env bash
	set -euxo pipefail
	mkdir -p build-x32
	cd build-x32
	i686-w64-mingw32.static-cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
	make -j$(nproc)

# Install into x64dbg's plugin directory
install: build
	mkdir -p x64dbg/release/x32/plugins
	cp build-x32/decomp2dbg.dp32 x64dbg/release/x32/plugins

x32: install
	#!/usr/bin/env bash
	set -euxo pipefail
	export WINEPREFIX="$(pwd)/.wine"
	export WINEDEBUG="-all"
	wine x64dbg/release/x32/x32dbg.exe "$(pwd)/../../winrar/WinRAR.exe"

# Build and run a demo program for the actual XML-RPC client
demo:
	#!/usr/bin/env bash
	set -euxo pipefail

	mkdir -p build-demo && cd build-demo
	i686-w64-mingw32.static-cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
	make clientDemo
	wine ./client/clientDemo.exe

clean:
	rm -rf build-x32/
	rm -rf build-demo/