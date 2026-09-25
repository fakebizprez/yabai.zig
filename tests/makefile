ZIG = zig
ZIG_PINNED_VERSION = 0.16.0
ZIG_OBJ = ./bin/zig_geometry.o ./bin/zig_message.o

.PHONY: clean build run all

all: clean build run

clean:
	rm -rf ./bin

build: $(ZIG_OBJ)
	mkdir -p ./bin
	clang ./src/tests.m $(ZIG_OBJ) -o ./bin/tests -DTESTS -DPROFILE=1 -F/System/Library/PrivateFrameworks -framework Carbon -framework Cocoa -framework CoreServices -framework CoreVideo -framework SkyLight

./bin/zig_geometry.o:
	mkdir -p ./bin
	@if [ "$$($(ZIG) version)" != "$(ZIG_PINNED_VERSION)" ]; then echo "error: pinned zig $(ZIG_PINNED_VERSION) required"; exit 1; fi
	$(ZIG) build-obj ../zig/geometry.zig -O Debug --name geometry -femit-bin=$@

./bin/zig_message.o:
	mkdir -p ./bin
	@if [ "$$($(ZIG) version)" != "$(ZIG_PINNED_VERSION)" ]; then echo "error: pinned zig $(ZIG_PINNED_VERSION) required"; exit 1; fi
	$(ZIG) build-obj ../zig/message.zig -O Debug --name message -femit-bin=$@

run:
	./bin/tests
