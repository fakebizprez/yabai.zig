#include <Foundation/Foundation.h>
#include <assert.h>
#include <ptrauth.h>

// The offline suite never initializes Dock or calls authenticated targets.
#undef ptrauth_sign_unauthenticated
#define ptrauth_sign_unauthenticated(pointer, key, data) (pointer)
#include "../../src/osax/payload.m"

@interface TestSpace : NSObject
- (uint64_t)spid;
@end
@implementation TestSpace
- (uint64_t)spid { return 42; }
@end

@interface TestDisplaySpace : NSObject {
@public
    id _currentSpace;
    id _displayUUID;
    id fallback;
}
- (id)currentSpace;
@end
@implementation TestDisplaySpace
- (id)currentSpace { return fallback; }
@end

@interface TestLegacyDock : NSObject
- (id)currentSpaceforDisplayUUID:(CFStringRef)uuid;
@end
@implementation TestLegacyDock
- (id)currentSpaceforDisplayUUID:(CFStringRef)uuid { assert(CFEqual(uuid, CFSTR("test-display"))); return @42; }
@end

@interface TestSequoiaDock : NSObject
- (id)currentSpaceForDisplayUUID:(CFStringRef)uuid;
@end
@implementation TestSequoiaDock
- (id)currentSpaceForDisplayUUID:(CFStringRef)uuid { assert(CFEqual(uuid, CFSTR("test-display"))); return @42; }
@end

@interface TestNewDock : NSObject {
@public
    NSArray *_displaySpaces;
}
- (uint32_t)displayForSPID:(uint64_t)sid;
- (id)currentSpaceForDisplay:(uint32_t)display;
@end
@implementation TestNewDock
- (uint32_t)displayForSPID:(uint64_t)sid { assert(sid == 42); return 7; }
- (id)currentSpaceForDisplay:(uint32_t)display { assert(display == 7); return @42; }
@end

// The payload must answer every request with exactly one status byte.
static char reply_status(const char *frame, size_t length, bool framed)
{
    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    if (framed) {
        assert(write(sockets[1], frame, length) == (ssize_t) length);
        shutdown(sockets[1], SHUT_WR);
        handle_client(sockets[0]);
    } else {
        char message[SA_SOCKET_BUFF_LEN] = {0};
        memcpy(message, frame, length);
        handle_message(sockets[0], message);
    }
    close(sockets[0]);
    char response[2];
    assert(recv(sockets[1], response, sizeof(response), MSG_DONTWAIT) == 1);
    close(sockets[1]);
    return response[0];
}

static void test_unavailable_command(enum sa_opcode op)
{
    char message[1 + 3 * sizeof(uint64_t) + 1] = { op };
    assert(reply_status(message, sizeof(message), false) == SA_RESPONSE_FAILURE);
}

// Same token rules hex_find_seq relies on: upper-case hex pairs or "??", single spaces.
static size_t pattern_tokens(const char *pattern)
{
    size_t count = 0;
    for (const char *p = pattern;; p += 3) {
        bool wild = p[0] == '?' && p[1] == '?';
        bool hex = strchr("0123456789ABCDEF", p[0]) && p[0] && strchr("0123456789ABCDEF", p[1]) && p[1];
        if (!wild && !hex) return 0;
        ++count;
        if (p[2] == '\0') return count;
        if (p[2] != ' ') return 0;
    }
}

static void test_patterns_well_formed(NSOperatingSystemVersion version)
{
    const char *(*getters[])(NSOperatingSystemVersion) = {
        get_dock_spaces_pattern, get_dppm_pattern, get_fix_animation_pattern, get_add_space_pattern,
        get_remove_space_pattern, get_move_space_pattern, get_set_front_window_pattern,
    };
    for (size_t i = 0; i < sizeof(getters) / sizeof(getters[0]); ++i) {
        const char *pattern = getters[i](version);
        if (pattern) assert(pattern_tokens(pattern) > 0);
    }
}

#ifdef __arm64__
// Byte windows at the first match of each macOS 27 pattern in the host Dock
// (macOS 27.0.1 26A434), captured offline from both arm64e slices.
static unsigned char dock27_spaces_arm64e[] = {0xc8,0x1e,0x00,0xb0,0x08,0xc1,0x2e,0x91,0x00,0x01,0x40,0xf9,0xe2,0x03,0x13,0xaa,0xee,0xed,0x0a,0x94,0xc8,0x1e,0x00,0xb0,0x08}; // +0x30a98
static unsigned char dock27_spaces_x1[] = {0x28,0x1e,0x00,0xd0,0x08,0xc1,0x2e,0x91,0x00,0x01,0x40,0xf9,0xe2,0x03,0x13,0xaa,0xfc,0x9e,0x0a,0x94,0x28,0x1e,0x00,0xd0,0x08}; // +0x2fdc0
static unsigned char dock27_animation_arm64e[] = {0x00,0x10,0x6a,0x1e,0xa8,0x63,0x01,0xd1,0x00,0x01,0x50,0xf8}; // +0x227508
static unsigned char dock27_animation_x1[] = {0x00,0x10,0x6a,0x1e,0xa8,0x63,0x01,0xd1,0x00,0x01,0x50,0xf8}; // +0x218a70
static unsigned char dock27_add_space_arm64e[] = {0x7f,0x23,0x03,0xd5,0xe1,0x03,0x1e,0xaa,0x5b,0x12,0xfc,0x97,0xfe,0x03,0x01,0xaa,0xfd,0x7b,0x05,0xa9,0xfd,0x43,0x01,0x91,0xf3,0x03,0x14,0xaa,0xf5,0x03,0x00,0xaa,0x96,0x8e,0x43,0xf8}; // +0x228cbc
static unsigned char dock27_add_space_x1[] = {0xfe,0xa7,0xc1,0xda,0xe1,0x03,0x1e,0xaa,0x99,0x31,0xfc,0x97,0xfe,0x03,0x01,0xaa,0xfd,0x7b,0x05,0xa9,0xfd,0x43,0x01,0x91,0xf3,0x03,0x14,0xaa,0xf5,0x03,0x00,0xaa,0x96,0x8e,0x43,0xf8}; // +0x21a1dc
static unsigned char dock27_remove_space_arm64e[] = {0x7f,0x23,0x03,0xd5,0xff,0x83,0x02,0xd1,0xfc,0x6f,0x04,0xa9,0xfa,0x67,0x05,0xa9,0xf8,0x5f,0x06,0xa9,0xf6,0x57,0x07,0xa9,0xf4,0x4f,0x08,0xa9,0xfd,0x7b,0x09,0xa9,0xfd,0x43,0x02,0x91,0xf3,0x03,0x03,0xaa,0xf5,0x03,0x02,0xaa,0xf4,0x03,0x01,0xaa}; // +0x18b9a0
static unsigned char dock27_remove_space_x1[] = {0xfe,0xa7,0xc1,0xda,0xff,0x83,0x02,0xd1,0xfc,0x6f,0x04,0xa9,0xfa,0x67,0x05,0xa9,0xf8,0x5f,0x06,0xa9,0xf6,0x57,0x07,0xa9,0xf4,0x4f,0x08,0xa9,0xfd,0x7b,0x09,0xa9,0xfd,0x43,0x02,0x91,0xf3,0x03,0x03,0xaa,0xf5,0x03,0x02,0xaa,0xf4,0x03,0x01,0xaa}; // +0x181838

static void test_dock27_pattern(const char *pattern, size_t tokens, unsigned char *window_a, size_t size_a, unsigned char *window_b, size_t size_b)
{
    assert(pattern && pattern_tokens(pattern) == tokens);
    assert(size_a == tokens && size_b == tokens);
    assert(hex_find_seq((uint64_t) window_a, pattern) == (uint64_t) window_a);
    assert(hex_find_seq((uint64_t) window_b, pattern) == (uint64_t) window_b);
}
#define test_dock27(pattern, tokens, name) test_dock27_pattern(pattern, tokens, name##_arm64e, sizeof(name##_arm64e), name##_x1, sizeof(name##_x1))
#endif

int main(void)
{
    @autoreleasepool {
        NSOperatingSystemVersion v27 = {27, 0, 1};
        NSOperatingSystemVersion v26 = {26, 4, 0};
        NSOperatingSystemVersion unknown = {28, 0, 0};
        assert(verify_os_version(v27));
        assert(macOSSequoia);
        assert(!verify_os_version(unknown));
        assert(hex_find_seq(1, NULL) == 0);
        assert(hex_find_seq(0, "00") == 0);
#ifdef __arm64__
        assert(get_dock_spaces_offset(v27) == 0x20000);
        assert(get_add_space_offset(v27) == 0x160000);
        assert(get_remove_space_offset(v27) == 0x120000);
        assert(get_fix_animation_offset(v27) == 0x160000);
        assert(get_dppm_pattern(v27) == NULL);
        assert(get_move_space_pattern(v27) == NULL);
        assert(get_set_front_window_pattern(v27) == NULL);
        assert(get_dock_spaces_pattern(v27));
        assert(get_fix_animation_pattern(v27));
        assert(get_add_space_pattern(v27));
        assert(get_remove_space_pattern(v27));
        assert(osax_required_attributes(27) == (OSAX_ATTRIB_DOCK_SPACES | OSAX_ATTRIB_ADD_SPACE | OSAX_ATTRIB_REM_SPACE | OSAX_ATTRIB_ANIM_TIME));
        assert(get_dock_spaces_offset(v26) == 0x30000);
        assert(get_move_space_pattern(v26));
        assert(get_dppm_pattern(v26));
        assert(get_set_front_window_pattern(v26));

        test_dock27(get_dock_spaces_pattern(v27), 25, dock27_spaces);
        test_dock27(get_fix_animation_pattern(v27), 12, dock27_animation);
        test_dock27(get_add_space_pattern(v27), 36, dock27_add_space);
        test_dock27(get_remove_space_pattern(v27), 48, dock27_remove_space);
#else
        assert(osax_required_attributes(27) == OSAX_ATTRIB_ALL);
#endif
        assert(osax_required_attributes(26) == OSAX_ATTRIB_ALL);
        assert(osax_required_attributes(15) == OSAX_ATTRIB_ALL);
        assert(osax_required_attributes(28) == OSAX_ATTRIB_ALL);
        assert(verify_os_version(v26));
        assert(get_ivar_value(nil, "_currentSpace") == nil);
        assert(get_current_space_for_display_space(nil) == nil);
        assert(display_space_for_display_uuid(NULL) == nil);
        assert(space_for_display_with_id(NULL, 42) == nil);

        TestSpace *space = [[[TestSpace alloc] init] autorelease];
        TestDisplaySpace *display = [[[TestDisplaySpace alloc] init] autorelease];
        display->_currentSpace = space;
        display->_displayUUID = @"test-display";
        assert(get_current_space_for_display_space(display) == space);
        display->_currentSpace = nil;
        display->fallback = space;
        assert(get_current_space_for_display_space(display) == space);

        dock_spaces = [[[TestLegacyDock alloc] init] autorelease];
        macOSSequoia = false;
        assert([current_space_for_display(CFSTR("test-display"), 42) isEqual:@42]);
        dock_spaces = [[[TestSequoiaDock alloc] init] autorelease];
        macOSSequoia = true;
        assert([current_space_for_display(CFSTR("test-display"), 42) isEqual:@42]);
        TestNewDock *new_dock = [[[TestNewDock alloc] init] autorelease];
        new_dock->_displaySpaces = @[display];
        dock_spaces = new_dock;
        assert([current_space_for_display(CFSTR("test-display"), 42) isEqual:@42]);
        assert(display_space_for_display_uuid(CFSTR("test-display")) == display);
        assert(display_space_for_space_with_id(42) == display);
        dock_spaces = [[[NSObject alloc] init] autorelease];
        assert(current_space_for_display(CFSTR("test-display"), 42) == nil);

        NSOperatingSystemVersion versions[] = {{12,0,0},{13,0,0},{13,3,0},{14,0,0},{14,1,0},{15,0,0},{15,4,0},{26,0,0},{26,4,0},{27,0,0}};
        for (size_t i = 0; i < sizeof(versions) / sizeof(versions[0]); ++i) {
            test_patterns_well_formed(versions[i]);
        }

        dock_spaces = nil;
        test_unavailable_command(SA_OPCODE_SPACE_FOCUS);
        test_unavailable_command(SA_OPCODE_SPACE_CREATE);
        test_unavailable_command(SA_OPCODE_SPACE_DESTROY);
        test_unavailable_command(SA_OPCODE_SPACE_MOVE);
        test_unavailable_command(SA_OPCODE_WINDOW_FOCUS);
        test_unavailable_command(0x7F);

        // A no-op window command (wid 0) succeeds without touching SkyLight.
        char window_move[1 + sizeof(uint32_t) + 2 * sizeof(int)] = { SA_OPCODE_WINDOW_MOVE };
        assert(reply_status(window_move, sizeof(window_move), false) == SA_RESPONSE_SUCCESS);

        // Frames the payload cannot read (empty, oversized, truncated) are failures.
        char empty_frame[] = { 0x00, 0x00 };
        assert(reply_status(empty_frame, sizeof(empty_frame), true) == SA_RESPONSE_FAILURE);
        int16_t oversized = SA_SOCKET_BUFF_LEN;
        assert(reply_status((char *) &oversized, sizeof(oversized), true) == SA_RESPONSE_FAILURE);
        char truncated[] = { 0x09, 0x00, SA_OPCODE_WINDOW_MOVE };
        assert(reply_status(truncated, sizeof(truncated), true) == SA_RESPONSE_FAILURE);
        char framed_move[2 + sizeof(window_move)] = { sizeof(window_move), 0x00, SA_OPCODE_WINDOW_MOVE };
        assert(reply_status(framed_move, sizeof(framed_move), true) == SA_RESPONSE_SUCCESS);
    }
    puts("macOS 27 payload helpers passed");
    return 0;
}
