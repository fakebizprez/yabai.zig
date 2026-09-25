# The test binary links the same universal Zig objects shipped in the
# daemon (built by ../zig/build.sh, pinned Zig 0.16.0), so the parity
# suite proves the shipped objects, not a separate Debug build.
ZIG_OBJ = ../zig/zig-geometry.o ../zig/zig-message.o ../zig/zig-view.o

.PHONY: clean build run all

all: clean build run

clean:
	rm -rf ./bin

build: $(ZIG_OBJ)
	mkdir -p ./bin
	clang ./src/tests.m $(ZIG_OBJ) -o ./bin/tests -DTESTS -DPROFILE=1 -F/System/Library/PrivateFrameworks -framework Carbon -framework Cocoa -framework CoreServices -framework CoreVideo -framework SkyLight

$(ZIG_OBJ): ../zig/geometry.zig ../zig/message.zig ../zig/view.zig ../zig/build.sh ../zig/ZIG_VERSION
	../zig/build.sh

run:
	./bin/tests
