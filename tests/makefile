# The test binary links the same universal Zig objects shipped in the
# daemon (built by ../zig/build.sh, pinned Zig 0.17.0), so the parity
# suite proves the shipped objects, not a separate Debug build.
ZIG_OBJ = ../zig/zig-geometry.o ../zig/zig-message.o ../zig/zig-view.o ../zig/zig-util.o ../zig/zig-rule.o ../zig/zig-window.o ../zig/zig-display.o ../zig/zig-mouse.o

.PHONY: clean build run all daemon

all: clean build run

clean:
	rm -rf ./bin

# The CLI golden tests exec ../bin/yabai; rebuild it when any source is newer.
daemon:
	$(MAKE) -C .. ./bin/yabai

TEST_FLAGS = -DTESTS -DPROFILE=1 -F/System/Library/PrivateFrameworks

# The suite runs on the host arch only, so the sources (and the
# src/zig_abi.h pins they include) are also type-checked for x86_64.
build: $(ZIG_OBJ) daemon
	mkdir -p ./bin
	clang ./src/tests.m $(ZIG_OBJ) -o ./bin/tests $(TEST_FLAGS) -framework Carbon -framework Cocoa -framework CoreServices -framework CoreVideo -framework SkyLight
	clang -fsyntax-only -arch x86_64 ./src/tests.m $(TEST_FLAGS)

$(ZIG_OBJ): ../zig/abi.zig ../src/zig_abi.h ../zig/geometry.zig ../zig/message.zig ../zig/view.zig ../zig/util.zig ../zig/rule.zig ../zig/window.zig ../zig/display.zig ../zig/mouse.zig ../zig/build.sh ../zig/ZIG_VERSION
	../zig/build.sh

run:
	./bin/tests
