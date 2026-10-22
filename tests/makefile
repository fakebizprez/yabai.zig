# The test binary links the same universal Zig objects shipped in the
# daemon (built by ../zig/build.sh, pinned Zig 0.17.0), so the parity
# suite proves the shipped objects, not a separate Debug build.
ZIG_OBJ = ../zig/zig-geometry.o ../zig/zig-message.o ../zig/zig-view.o ../zig/zig-util.o ../zig/zig-rule.o ../zig/zig-window.o ../zig/zig-display.o ../zig/zig-mouse.o

.PHONY: clean build run all daemon osax27

all: clean build run

clean:
	rm -rf ./bin

# The CLI golden tests exec ../bin/yabai; rebuild it when any source is newer.
daemon:
	$(MAKE) -C .. ./bin/yabai

TEST_FLAGS = -DTESTS -DPROFILE=1 -F/System/Library/PrivateFrameworks

TEST_FRAMEWORKS = -framework Carbon -framework Cocoa -framework CoreServices -framework CoreVideo -framework SkyLight

# The suite is also built for x86_64 against the x86_64 slices of the Zig
# objects; `run` executes it under Rosetta when Rosetta is installed.
build: $(ZIG_OBJ) daemon ./bin/osax27
	mkdir -p ./bin
	clang ./src/tests.m $(ZIG_OBJ) -o ./bin/tests $(TEST_FLAGS) $(TEST_FRAMEWORKS)
	clang -arch x86_64 ./src/tests.m $(ZIG_OBJ) -o ./bin/tests-x86_64 $(TEST_FLAGS) $(TEST_FRAMEWORKS)

$(ZIG_OBJ): ../zig/abi.zig ../src/zig_abi.h ../zig/geometry.zig ../zig/message.zig ../zig/view.zig ../zig/util.zig ../zig/rule.zig ../zig/window.zig ../zig/display.zig ../zig/mouse.zig ../zig/build.sh ../zig/ZIG_VERSION
	../zig/build.sh

run:
	./bin/tests
	./bin/osax27
	@if arch -x86_64 /usr/bin/true 2>/dev/null; then arch -x86_64 ./bin/tests-x86_64; \
	else echo "skipping x86_64 test run: Rosetta is not installed"; fi

./bin/osax27: ./src/osax27.m ../src/osax/payload.m ../src/osax/arm64_payload.m ../src/osax/x64_payload.m ../src/osax/common.h
	mkdir -p ./bin
	clang ./src/osax27.m -o $@ $(TEST_FLAGS) -framework Foundation -framework Carbon -framework SkyLight
	clang -fsyntax-only -arch x86_64 ./src/osax27.m $(TEST_FLAGS)

osax27: ./bin/osax27
	./bin/osax27
