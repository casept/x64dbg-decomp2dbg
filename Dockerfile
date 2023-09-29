# Environment for cross-compiling the plugin to Windows in CI.

FROM docker.io/debian:12 as mxe

RUN apt update && apt upgrade -y

# Install MXE from source, as binary packages are outdated
RUN apt install -y \
	git \
	autoconf \
	automake \
	autopoint \
	bash \
	bison \
	bzip2 \
	flex \
	g++ \
	g++-multilib \
	gettext \
	git \
	gperf \
	intltool \
	libc6-dev-i386 \
	libgdk-pixbuf2.0-dev \
	libltdl-dev \
	libssl-dev \
	libtool-bin \
	libxml-parser-perl \
	lzip \
	make \
	openssl \
	p7zip-full \
	patch \
	perl \
	python3 \
	python3-mako \
	ruby \
	sed \
	unzip \
	wget \
	xz-utils
RUN ln -s /usr/bin/python3 /usr/bin/python

WORKDIR /
RUN git clone https://github.com/mxe/mxe.git
WORKDIR /mxe
# Current newest commit, bump occasionally
RUN git checkout ea373fe51c02f60111ef0b7597f52dc380c5c162

# Compile needed packages
# Done in multiple commands to allow adding new ones without toolchain recompile
# 64 bit
RUN make -j$(nproc) MXE_TARGETS='x86_64-w64-mingw32.static' MXE_USE_CCACHE= cc
RUN make -j$(nproc) MXE_TARGETS='x86_64-w64-mingw32.static' MXE_USE_CCACHE= cmake
RUN make -j$(nproc) MXE_TARGETS='x86_64-w64-mingw32.static' MXE_USE_CCACHE= xmlrpc-c
# 32 bit
RUN make -j$(nproc) MXE_TARGETS='i686-w64-mingw32.static' MXE_USE_CCACHE= cc
RUN make -j$(nproc) MXE_TARGETS='i686-w64-mingw32.static' MXE_USE_CCACHE= cmake
RUN make -j$(nproc) MXE_TARGETS='i686-w64-mingw32.static' MXE_USE_CCACHE= xmlrpc-c

# Cleanup
RUN make clean-junk && rm -rf pkg/* .ccache/

# Clean container only containing what's actually needed to build
FROM debian:12
COPY --from=mxe /mxe /mxe

RUN apt update && apt upgrade -y &&	apt install -y zip make wget git &&	apt clean
RUN wget https://github.com/casey/just/releases/download/1.14.0/just-1.14.0-x86_64-unknown-linux-musl.tar.gz && \
	tar -xvf just*.tar.gz && mv just /usr/local/bin/ && \
	rm *.md *.1 Cargo.* *.tar.gz

# So tools are found
ENV PATH="/mxe/usr/bin:$PATH"
