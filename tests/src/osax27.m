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

static void test_unavailable_command(enum sa_opcode op)
{
    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    char message = op;
    handle_message(sockets[0], &message);
    char response = 0;
    assert(recv(sockets[1], &response, sizeof(response), MSG_DONTWAIT) == 1);
    assert(response == SA_RESPONSE_FAILURE);
    close(sockets[0]);
    close(sockets[1]);
}

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

        // Both arm64e prologues must match the same macOS 27 add-space signature.
        unsigned char instruction[] = {0x7f,0x23,0x03,0xd5,0xe1,0x03,0x1e,0xaa,0,0,0,0x97,0xfe,0x03,0x01,0xaa,0,0,0,0xa9,0xfd,0x43,0x01,0x91,0xf3,0x03,0x14,0xaa,0xf5,0x03,0x00,0xaa,0x96,0x8e,0x43,0xf8};
        assert(hex_find_seq((uint64_t)instruction, get_add_space_pattern(v27)) == (uint64_t)instruction);
        instruction[0] = 0xfe;
        instruction[1] = 0xa7;
        instruction[2] = 0xc1;
        instruction[3] = 0xda;
        assert(hex_find_seq((uint64_t)instruction, get_add_space_pattern(v27)) == (uint64_t)instruction);
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

        dock_spaces = nil;
        test_unavailable_command(SA_OPCODE_SPACE_FOCUS);
        test_unavailable_command(SA_OPCODE_SPACE_MOVE);
        test_unavailable_command(SA_OPCODE_WINDOW_FOCUS);
    }
    puts("macOS 27 payload helpers passed");
    return 0;
}
