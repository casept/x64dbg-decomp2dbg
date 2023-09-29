set shell := ["bash", "-uc"]

default:
	@just --list

# Build the 32-bit extension DLL
build-x32:
	#!/usr/bin/env bash
	set -euxo pipefail
	mkdir -p build-x32
	cd build-x32
	i686-w64-mingw32.static-cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
	make -j$(nproc)

# Build the 64-bit extension DLL
build-x64:
	#!/usr/bin/env bash
	set -euxo pipefail
	mkdir -p build-x64
	cd build-x64
	x86_64-w64-mingw32.static-cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
	make -j$(nproc)

# Install into x64dbg's plugin directory
install: build-x32 build-x64
	mkdir -p x64dbg/release/x32/plugins
	cp build-x32/decomp2dbg.dp32 x64dbg/release/x32/plugins
	mkdir -p x64dbg/release/x64/plugins
	cp build-x64/decomp2dbg.dp64 x64dbg/release/x64/plugins

# Launch x32dbg with plugin loaded
x32: install
	#!/usr/bin/env bash
	set -euxo pipefail
	export WINEPREFIX="$(pwd)/.wine"
	export WINEDEBUG="-all"
	wine x64dbg/release/x32/x32dbg.exe

# Launch x64dbg with plugin loaded
x64: install
	#!/usr/bin/env bash
	set -euxo pipefail
	export WINEPREFIX="$(pwd)/.wine"
	export WINEDEBUG="-all"
	wine x64dbg/release/x64/x64dbg.exe

# Package into release zip
package: build-x32 build-x64
	#!/usr/bin/env bash
	mkdir -p pkg && cd pkg
	cp ../build-x32/decomp2dbg.dp32 .
	cp ../build-x64/decomp2dbg.dp64 .
	zip -r decomp2dbg.zip *
	cd ../

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
	rm -rf build-x64/
	rm -rf build-demo/
	rm -rf pkg/