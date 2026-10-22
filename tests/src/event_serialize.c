/* No events are posted: exercise CoreGraphics serialization and the raw HID payload. */
static CGEventRef event_serialize_test_event(int phase, double velocity)
{
    CGEventRef event = CGEventCreate(NULL);
    if (!event) return NULL;
    CGEventSetTimestamp(event, 123456789);
    CGEventSetIntegerValueField(event, (CGEventField)55, 30);
    CGEventSetIntegerValueField(event, (CGEventField)110, 23);
    CGEventSetIntegerValueField(event, (CGEventField)115, 2);
    CGEventSetIntegerValueField(event, (CGEventField)123, 1);
    CGEventSetIntegerValueField(event, (CGEventField)132, phase);
    CGEventSetDoubleValueField(event, (CGEventField)124, -0.5);
    CGEventSetDoubleValueField(event, (CGEventField)125, 0.25);
    CGEventSetDoubleValueField(event, (CGEventField)126, -0.25);
    CGEventSetDoubleValueField(event, (CGEventField)129, velocity);
    CGEventSetDoubleValueField(event, (CGEventField)130, -0.5);
    return event;
}

static bool event_serialize_test_parse(CGEventRef event, ISSCGEventParsedData *parsed)
{
    CFDataRef data = CGEventCreateData(kCFAllocatorDefault, event);
    if (!data) return false;
    bool ok = iss_parse_event_data(CFDataGetBytePtr(data), (size_t)CFDataGetLength(data), parsed);
    CFRelease(data);
    return ok;
}

TEST_FUNC(event_serialize_roundtrip,
{
    TEST_CHECK(iss_augment_dock_swipe_event(NULL) == NULL, true);
    for (int phase = 1; phase <= 4; phase += 3) {
        CGEventRef event = event_serialize_test_event(phase, 9999.0);
        TEST_CHECK(event != NULL, true);
        if (!event) continue;
        CGEventRef augmented = iss_augment_dock_swipe_event(event);
        TEST_CHECK(augmented != NULL, true);
        if (augmented) {
            TEST_CHECK(CGEventGetTimestamp(augmented) == 123456789, true);
            TEST_CHECK(CGEventGetIntegerValueField(augmented, (CGEventField)132) == phase, true);
            TEST_CHECK(CGEventGetDoubleValueField(augmented, (CGEventField)129) == 9999.0, true);
            ISSCGEventParsedData before = {0};
            ISSCGEventParsedData after = {0};
            bool parsed = event_serialize_test_parse(event, &before) && event_serialize_test_parse(augmented, &after);
            TEST_CHECK(parsed, true);
            int raw_count = 0;
            if (parsed) {
                for (size_t i = 0; i < before.field_count; ++i) {
                    ISSCGEventParsedField *field = &before.fields[i];
                    if (field->field_id == 4205) continue;
                    bool preserved = false;
                    for (size_t j = 0; j < after.field_count; ++j) {
                        ISSCGEventParsedField *other = &after.fields[j];
                        if (field->field_id == other->field_id) {
                            preserved = field->tag == other->tag && field->size_words == other->size_words &&
                                        field->payload_length == other->payload_length &&
                                        (!field->payload_length || memcmp(field->payload, other->payload, field->payload_length) == 0);
                            break;
                        }
                    }
                    TEST_CHECK(preserved, true);
                }
                for (size_t i = 0; i < after.field_count; ++i) {
                    ISSCGEventParsedField *field = &after.fields[i];
                    if (field->field_id != 4205) continue;
                    ++raw_count;
                    TEST_CHECK(field->payload_length == 96, true);
                    if (field->payload_length != 96) continue;
                    IOHIDSystemQueueElementHeader header;
                    IOHIDFluidTouchGestureData fluid;
                    IOHIDVelocityEventData velocity;
                    memcpy(&header, field->payload, sizeof(header));
                    memcpy(&fluid, field->payload + 28, sizeof(fluid));
                    memcpy(&velocity, field->payload + 68, sizeof(velocity));
                    TEST_CHECK(header.timestamp == 123456789, true);
                    TEST_CHECK(header.event_count == 2, true);
                    TEST_CHECK(fluid.base.size == 40 && fluid.base.type == 23, true);
                    TEST_CHECK(fluid.base.options == (uint32_t)phase << 24, true);
                    TEST_CHECK(fluid.position_x == 16384 && fluid.position_y == -16384, true);
                    TEST_CHECK(fluid.swipe_mask == 2 && fluid.gesture_motion == 1 && fluid.gesture_flavor == 3, true);
                    TEST_CHECK(fluid.swipe_progress == -32768, true);
                    TEST_CHECK(velocity.base.size == 28 && velocity.base.type == 9 && velocity.base.depth == 1, true);
                    TEST_CHECK(velocity.velocity_x == 655294464 && velocity.velocity_y == -32768, true);
                }
                TEST_CHECK(raw_count, 1);
            }
            iss_parsed_event_data_free(&before);
            iss_parsed_event_data_free(&after);
            CFRelease(augmented);
        }
        CFRelease(event);
    }
})

TEST_FUNC(event_serialize_payload_replacement,
{
    CGEventRef event = event_serialize_test_event(1, 0.0);
    TEST_CHECK(event != NULL, true);
    if (event) {
        CGEventSetDoubleValueField(event, (CGEventField)130, 0.0);
        CGEventRef first = iss_augment_dock_swipe_event(event);
        TEST_CHECK(first != NULL, true);
        if (first) {
            ISSCGEventParsedData parsed = {0};
            bool ok = event_serialize_test_parse(first, &parsed);
            TEST_CHECK(ok, true);
            if (ok) {
                int raw_count = 0;
                for (size_t i = 0; i < parsed.field_count; ++i) {
                    if (parsed.fields[i].field_id == 4205) {
                        ++raw_count;
                        TEST_CHECK(parsed.fields[i].payload_length == 68, true);
                    }
                }
                TEST_CHECK(raw_count, 1);
            }
            iss_parsed_event_data_free(&parsed);
            CGEventSetIntegerValueField(first, (CGEventField)132, 4);
            CGEventRef second = iss_augment_dock_swipe_event(first);
            TEST_CHECK(second != NULL, true);
            if (second) {
                ok = event_serialize_test_parse(second, &parsed);
                TEST_CHECK(ok, true);
                int raw_count = 0;
                if (ok) {
                    for (size_t i = 0; i < parsed.field_count; ++i) {
                        ISSCGEventParsedField *field = &parsed.fields[i];
                        if (field->field_id != 4205) continue;
                        ++raw_count;
                        TEST_CHECK(field->payload_length == 96, true);
                        if (field->payload_length == 96) {
                            IOHIDFluidTouchGestureData fluid;
                            memcpy(&fluid, field->payload + 28, sizeof(fluid));
                            TEST_CHECK(fluid.base.options == 4u << 24, true);
                        }
                    }
                }
                TEST_CHECK(raw_count, 1);
                iss_parsed_event_data_free(&parsed);
                CFRelease(second);
            }
            CFRelease(first);
        }
        CFRelease(event);
    }
})

static const uint8_t integer_field[] = { 0, 0, 0, 2, 0, 1, 0, 10, 1, 2, 3, 4, 5, 6, 7, 8 };

static const uint8_t unknown_tag[] = { 0, 0, 0, 2, 0, 1, 0x80, 10, 0, 0, 0, 0 };

static const uint8_t zero_blob[] = { 0, 0, 0, 2, 0, 0, 0, 10 };

static const uint8_t raw[] = { 1, 2 };

TEST_FUNC(event_serialize_malformed,
{
    ISSCGEventParsedData parsed = {0};
    TEST_CHECK(iss_parse_event_data(NULL, 4, &parsed), false);
    for (size_t length = 0; length < sizeof(integer_field); ++length) {
        if (length == 4) continue; /* An empty version-2 event is valid. */
        TEST_CHECK(iss_parse_event_data(integer_field, length, &parsed), false);
        TEST_CHECK(parsed.fields == NULL, true);
    }
    TEST_CHECK(iss_parse_event_data(unknown_tag, sizeof(unknown_tag), &parsed), false);
    TEST_CHECK(iss_parse_event_data(zero_blob, sizeof(zero_blob), &parsed), false);

    /* More than 16 fields exercises realloc and cleanup beyond its original capacity. */
    uint8_t many_fields[4 + 33 * 8] = {0};
    many_fields[3] = 2;
    for (int i = 0; i < 33; ++i) {
        size_t offset = 4 + i * 8;
        many_fields[offset + 1] = 1;
        many_fields[offset + 2] = 0x40;
        many_fields[offset + 3] = i + 1;
        many_fields[offset + 7] = i;
    }
    bool ok = iss_parse_event_data(many_fields, sizeof(many_fields), &parsed);
    TEST_CHECK(ok, true);
    if (ok) {
        TEST_CHECK(parsed.field_count == 33, true);
        size_t length = 0;
        uint8_t *serialized = iss_serialize_event_data(&parsed, raw, sizeof(raw), &length);
        TEST_CHECK(serialized != NULL, true);
        if (serialized) {
            ISSCGEventParsedData roundtrip = {0};
            bool reparsed = iss_parse_event_data(serialized, length, &roundtrip);
            TEST_CHECK(reparsed, true);
            if (reparsed) {
                TEST_CHECK(roundtrip.field_count == 34, true);
                size_t replaced_length = 0;
                uint8_t *replaced = iss_serialize_event_data(&roundtrip, zero_blob, sizeof(zero_blob), &replaced_length);
                TEST_CHECK(replaced != NULL, true);
                if (replaced) {
                    ISSCGEventParsedData replacement = {0};
                    bool replaced_ok = iss_parse_event_data(replaced, replaced_length, &replacement);
                    TEST_CHECK(replaced_ok, true);
                    if (replaced_ok) {
                        TEST_CHECK(replacement.field_count == 34, true);
                        TEST_CHECK(replacement.fields[33].field_id == 4205, true);
                        TEST_CHECK(replacement.fields[33].payload_length == sizeof(zero_blob), true);
                        TEST_CHECK(memcmp(replacement.fields[33].payload, zero_blob, sizeof(zero_blob)), 0);
                    }
                    iss_parsed_event_data_free(&replacement);
                    free(replaced);
                }
            }
            iss_parsed_event_data_free(&roundtrip);
            free(serialized);
        }
        iss_parsed_event_data_free(&parsed);
    }
    TEST_CHECK(iss_parse_event_data(many_fields, sizeof(many_fields) - 1, &parsed), false);
    TEST_CHECK(parsed.fields == NULL, true);
})
