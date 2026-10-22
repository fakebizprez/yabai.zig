static const int macos_test_versions[] = { 11, 12, 13, 14, 15, 26, 27, 28 };

static struct {
    int phase;
    uint64_t hid_timestamp;
    uint64_t posted_at;
} macos_swipe_samples[3];
static int macos_swipe_sample_count;
static bool macos_capture_swipe;
static int macos_swipe_allocations_before_failure = -1;

static uint64_t macos_mach_ticks_to_ns(uint64_t ticks)
{
    mach_timebase_info_data_t timebase;
    assert(mach_timebase_info(&timebase) == KERN_SUCCESS);
    return (uint64_t)((long double) ticks * timebase.numer / timebase.denom);
}

static CGEventRef test_create_event(CGEventSourceRef source)
{
    if (macos_swipe_allocations_before_failure == 0) {
        macos_swipe_allocations_before_failure = -1;
        return NULL;
    }
    if (macos_swipe_allocations_before_failure > 0) --macos_swipe_allocations_before_failure;
    return CGEventCreate(source);
}

static void test_capture_event_post(CGEventTapLocation tap, CGEventRef event)
{
    assert(macos_capture_swipe && macos_swipe_sample_count < 3);
    assert(tap == kCGSessionEventTap);
    int index = macos_swipe_sample_count++;
    macos_swipe_samples[index].posted_at = macos_mach_ticks_to_ns(mach_absolute_time());
    macos_swipe_samples[index].phase = (int) CGEventGetIntegerValueField(event, (CGEventField)132);
    CFDataRef data = CGEventCreateData(NULL, event);
    assert(data);
    ISSCGEventParsedData parsed;
    bool valid = iss_parse_event_data(CFDataGetBytePtr(data), CFDataGetLength(data), &parsed);
    assert(valid);
    macos_swipe_samples[index].hid_timestamp = 0;
    for (size_t i = 0; i < parsed.field_count; ++i) {
        if (parsed.fields[i].field_id == 4205) {
            assert(parsed.fields[i].payload_length >= sizeof(IOHIDSystemQueueElementHeader));
            memcpy(&macos_swipe_samples[index].hid_timestamp, parsed.fields[i].payload, sizeof(uint64_t));
        }
    }
    assert(macos_swipe_samples[index].hid_timestamp);
    // Nonzero CGEvent timestamps are nanoseconds; the HID fallback uses Mach ticks.
    if (CGEventGetTimestamp(event) == 0) {
        macos_swipe_samples[index].hid_timestamp = macos_mach_ticks_to_ns(macos_swipe_samples[index].hid_timestamp);
    }
    iss_parsed_event_data_free(&parsed);
    CFRelease(data);
}

TEST_FUNC(macos_swipe_timing,
{
    for (int right = 0; right <= 1; ++right) {
        macos_swipe_sample_count = 0;
        macos_capture_swipe = true;
        bool posted = space_manager_post_swipe(right);
        macos_capture_swipe = false;
        TEST_CHECK(posted, true);
        TEST_CHECK(macos_swipe_sample_count, 3);
        for (int p = 0; p < macos_swipe_sample_count; ++p) {
            TEST_CHECK(macos_swipe_samples[p].phase, 1 << p);
            if (p > 0) {
                TEST_CHECK(macos_swipe_samples[p].hid_timestamp >= macos_swipe_samples[p-1].posted_at, true);
            }
        }
    }
})

TEST_FUNC(macos_swipe_failure,
{
    for (int failure = 0; failure < 3; ++failure) {
        macos_swipe_sample_count = 0;
        macos_capture_swipe = true;
        macos_swipe_allocations_before_failure = failure;
        bool posted = space_manager_post_swipe(true);
        macos_capture_swipe = false;
        macos_swipe_allocations_before_failure = -1;
        TEST_CHECK(posted, false);
        TEST_CHECK(macos_swipe_sample_count, failure == 0 ? 0 : failure + 1);
        if (failure > 0 && macos_swipe_sample_count > 0) {
            TEST_CHECK(macos_swipe_samples[macos_swipe_sample_count-1].phase, 8);
        }
    }
})

static TEST_SIG(macos_version_detection)
{
    char *test_name = "macos_version_detection";
    bool result = true;
#define SUPPORT_MACOS_VERSION(name, version) _workspace_is_macos_version_##name,
    bool saved[] = { SUPPORTED_MACOS_VERSION_LIST };
#undef SUPPORT_MACOS_VERSION

    for (int i = 0; i < array_count(macos_test_versions); ++i) {
        int major = macos_test_versions[i];
        workspace_set_macos_version(major);
        int matches = 0;
#define SUPPORT_MACOS_VERSION(name, version) \
        TEST_CHECK(workspace_is_macos_##name(), major == version); \
        matches += workspace_is_macos_##name();
        SUPPORTED_MACOS_VERSION_LIST
#undef SUPPORT_MACOS_VERSION
        TEST_CHECK(matches, major == 28 ? 0 : 1);
    }

    int index = 0;
#define SUPPORT_MACOS_VERSION(name, version) _workspace_is_macos_version_##name = saved[index++];
    SUPPORTED_MACOS_VERSION_LIST
#undef SUPPORT_MACOS_VERSION
    return result;
}

TEST_FUNC(macos_space_dictionary,
{
    CFMutableDictionaryRef space = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int64_t legacy_id = 42;
    int64_t modern_id = 43;
    CFNumberRef legacy = CFNumberCreate(NULL, kCFNumberSInt64Type, &legacy_id);
    CFNumberRef modern = CFNumberCreate(NULL, kCFNumberSInt64Type, &modern_id);
    TEST_CHECK(space_manager_space_id(NULL) == 0, true);
    TEST_CHECK(space_manager_space_id((CFDictionaryRef) CFSTR("invalid")) == 0, true);
    TEST_CHECK(space_manager_space_id(space) == 0, true);
    CFDictionarySetValue(space, CFSTR("id64"), legacy);
    TEST_CHECK(space_manager_space_id(space) == 42, true);
    CFDictionarySetValue(space, CFSTR("ManagedSpaceID"), modern);
    TEST_CHECK(space_manager_space_id(space) == 43, true);
    CFDictionaryRemoveValue(space, CFSTR("id64"));
    TEST_CHECK(space_manager_space_id(space) == 43, true);
    CFDictionarySetValue(space, CFSTR("ManagedSpaceID"), CFSTR("invalid"));
    TEST_CHECK(space_manager_space_id(space) == 0, true);
    CFRelease(modern);
    CFRelease(legacy);
    CFRelease(space);
})

TEST_FUNC(macos_swipe_events,
{
    TEST_CHECK(space_manager_focus_space_using_gesture(0, 42), false);
    TEST_CHECK(space_manager_focus_space_using_gesture(1, 0), false);
    for (int right = 0; right <= 1; ++right) {
        for (int phase = 1; phase <= 4; phase *= 2) {
            CGEventRef event = space_manager_make_swipe_event(right, phase);
            TEST_CHECK(event != NULL, true);
            if (!event) continue;
            TEST_CHECK(CGEventGetIntegerValueField(event, (CGEventField)55) == 30, true);
            TEST_CHECK(CGEventGetIntegerValueField(event, (CGEventField)110) == 23, true);
            TEST_CHECK(CGEventGetIntegerValueField(event, (CGEventField)132) == phase, true);
            TEST_CHECK(CGEventGetIntegerValueField(event, (CGEventField)134) == phase, true);
            // CoreGraphics stores swipe progress as a float.
            TEST_CHECK(CGEventGetDoubleValueField(event, (CGEventField)124) == (double)(float)(right ? -0.000016 : 0.000016), true);
            if (phase == 4) {
                TEST_CHECK(CGEventGetDoubleValueField(event, (CGEventField)129) == (right ? -500.0 : 500.0), true);
            }
            CFRelease(event);
        }
    }
})
