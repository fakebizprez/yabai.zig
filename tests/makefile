# Both test binaries link the same universal Zig objects shipped in the
# daemon (built by ../zig/build.sh, Zig pinned in ../zig/ZIG_VERSION), so
# the parity suite proves the shipped objects, not a separate Debug build.
ZIG_OBJ = ../zig/zig-geometry.o ../zig/zig-message.o ../zig/zig-view.o ../zig/zig-util.o ../zig/zig-rule.o ../zig/zig-window.o ../zig/zig-display.o ../zig/zig-mouse.o

.PHONY: clean build run all daemon osax27

all: clean build run

clean:
	rm -rf ./bin

# The CLI golden tests exec ../bin/yabai-zig; rebuild it when any source is newer.
# Waits for the Zig objects so make -j does not run build.sh twice at once.
daemon: $(ZIG_OBJ)
	$(MAKE) -C .. ./bin/yabai-zig

TEST_FLAGS = -DTESTS -DPROFILE=1 -F/System/Library/PrivateFrameworks

TEST_FRAMEWORKS = -framework Carbon -framework Cocoa -framework CoreServices -framework CoreVideo -framework SkyLight

# The suite is also built for x86_64 against the x86_64 slices of the Zig
# objects; `run` executes it under Rosetta when Rosetta is installed.
build: $(ZIG_OBJ) daemon ./bin/osax27
	mkdir -p ./bin
	clang ./src/tests.m $(ZIG_OBJ) -o ./bin/tests $(TEST_FLAGS) $(TEST_FRAMEWORKS)
	clang -arch x86_64 ./src/tests.m $(ZIG_OBJ) -o ./bin/tests-x86_64 $(TEST_FLAGS) $(TEST_FRAMEWORKS)

# Same stamp as ../makefile: one build.sh run for all objects.
$(ZIG_OBJ): ../zig/bin/build.stamp ;

../zig/bin/build.stamp: ../zig/abi.zig ../src/zig_abi.h ../zig/geometry.zig ../zig/message.zig ../zig/view.zig ../zig/util.zig ../zig/rule.zig ../zig/window.zig ../zig/display.zig ../zig/mouse.zig ../zig/build.sh ../zig/ZIG_VERSION
	../zig/build.sh
	touch $@

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
