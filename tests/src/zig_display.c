/* ref_display_serialize() (frozen dd84572 src/display.c, in
 * tests/src/ref_upstream_rule.c) vs yabai_display_serialize (zig/display.zig).
 *
 * Seam: the six manager-derived inputs arrive lifted in so the export
 * stays pure and headless-testable: uuid_str (NULL when the
 * ts_cfstring_copy result would be NULL), arrange_index (replaces
 * display_manager_display_id_arrangement), label_str (NULL when
 * display_manager_get_label_for_display yields NULL), the frame rect as
 * four f64s (replaces CGDisplayBounds), the space mission-index int
 * array + count (replaces display_space_list plus
 * space_manager_mission_control_index), and has_focus (replaces
 * did == g_display_manager.current_display_id). Bools reuse
 * yabai_json_bool; emission is libc fwrite/snprintf exactly (%d id,
 * %.4f frame, did_output comma chain; the trailing has-focus branch
 * omits did_output=true, exactly like C).
 *
 * Oracle scope: INDEX is SLS-backed (SLSCopyManagedDisplays on
 * g_connection) and SPACES is SLS-backed (display_space_list on
 * g_connection), so with g_connection == 0 the C oracle only covers the
 * SLS-free flags (ID, UUID, LABEL, FRAME, HAS_FOCUS). INDEX, SPACES,
 * all-flags, and flags==0 are pinned with hand vectors transcribed from
 * the C format strings. Must FAIL against the stub (TDD red) and pass
 * on the real port (green).
 */

/* Temp-pool init for the C uuid oracle (mirrors zig_view.c). */
static void ensure_ts_for_display_serialize(void)
{
    static bool ts_ready = false;
    if (!ts_ready) {
        ts_init(MEGABYTES(8));
        ts_ready = true;
    }
}

/* C-oracle parity over SLS-free flags. The lifted args must mirror what
 * C derives: uuid_str echoes ts_cfstring_copy(display_uuid(did)) (or
 * NULL), label_str echoes the registered label (or NULL), frame echoes
 * CGDisplayBounds(did), has_focus echoes did == current_display_id. */
static int serialize_display_parity(uint32_t did, uint64_t flags, const char *uuid_str, const char *label_str, CGRect frame, bool has_focus)
{
    char *c_msg = NULL;
    size_t c_len = 0;
    char *z_msg = NULL;
    size_t z_len = 0;
    FILE *c_rsp = open_memstream(&c_msg, &c_len);
    FILE *z_rsp = open_memstream(&z_msg, &z_len);
    if (!c_rsp || !z_rsp) {
        if (c_rsp) fclose(c_rsp);
        if (z_rsp) fclose(z_rsp);
        free(c_msg);
        free(z_msg);
        return 0;
    }
    ref_display_serialize(c_rsp, did, flags);
    yabai_display_serialize(z_rsp, did, flags, uuid_str, 0, label_str, frame.origin.x, frame.origin.y, frame.size.width, frame.size.height, NULL, 0, has_focus);
    fflush(c_rsp);
    fflush(z_rsp);
    fclose(c_rsp);
    fclose(z_rsp);
    int ok = 1;
    if (c_len != z_len) ok = 0;
    else if (c_len > 0 && memcmp(c_msg, z_msg, c_len) != 0) ok = 0;
    if (!ok) {
        printf("display serialize mismatch flags 0x%llx c_len %zu z_len %zu\n---C---\n%.*s\n---Z---\n%.*s\n",
               (unsigned long long) flags, c_len, z_len,
               (int) c_len, c_msg ? c_msg : "(null)",
               (int) z_len, z_msg ? z_msg : "(null)");
    }
    free(c_msg);
    free(z_msg);
    return ok;
}

/* Zig-only hand vector for SLS-backed flags, all-flags, flags==0, and
 * the NULL/empty edges. */
static int serialize_display_zig_only(uint32_t did, uint64_t flags, const char *uuid_str, int arrange_index, const char *label_str, double fx, double fy, double fw, double fh, const int *space_list, int space_count, bool has_focus, const char *expected)
{
    char *z_msg = NULL;
    size_t z_len = 0;
    FILE *z_rsp = open_memstream(&z_msg, &z_len);
    if (!z_rsp) {
        free(z_msg);
        return 0;
    }
    yabai_display_serialize(z_rsp, did, flags, uuid_str, arrange_index, label_str, fx, fy, fw, fh, space_list, space_count, has_focus);
    fflush(z_rsp);
    fclose(z_rsp);
    int ok = z_msg && strcmp(z_msg, expected) == 0;
    if (!ok) {
        printf("display serialize zig-only mismatch flags 0x%llx\n---got---\n%s\n---want---\n%s\n",
               (unsigned long long) flags, z_msg ? z_msg : "(null)", expected);
    }
    free(z_msg);
    return ok;
}

/* Hand-pinned full-JSON golden for BOTH oracles. Expectations transcribed
 * by hand from the fprintf format in src/display.c (open brace,
 * tab-quoted fields in ID/UUID/INDEX/LABEL/FRAME/SPACES/HAS_FOCUS order
 * joined with ",\n", closing "\n}"): id %d of the did, uuid quoted
 * verbatim ("<unknown>" when NULL), index %d, label quoted verbatim
 * ("" when NULL), frame %.4f x/y/w/h, spaces [%d, ...] (empty when the
 * list is NULL), has-focus from json_bool. The degenerate did (7) has
 * no system UUID, label, frame, or spaces, so C derives NULL/0/""/zero
 * without touching SLS-backed managers; both outputs must match these
 * exact bytes. */
static int serialize_display_golden_both(uint32_t did, uint64_t flags, const char *uuid_str, int arrange_index, const char *label_str, double fx, double fy, double fw, double fh, const int *space_list, int space_count, bool has_focus, const char *expected)
{
    char *c_msg = NULL;
    size_t c_len = 0;
    char *z_msg = NULL;
    size_t z_len = 0;
    FILE *c_rsp = open_memstream(&c_msg, &c_len);
    FILE *z_rsp = open_memstream(&z_msg, &z_len);
    if (!c_rsp || !z_rsp) {
        if (c_rsp) fclose(c_rsp);
        if (z_rsp) fclose(z_rsp);
        free(c_msg);
        free(z_msg);
        return 0;
    }
    ref_display_serialize(c_rsp, did, flags);
    yabai_display_serialize(z_rsp, did, flags, uuid_str, arrange_index, label_str, fx, fy, fw, fh, space_list, space_count, has_focus);
    fflush(c_rsp);
    fflush(z_rsp);
    fclose(c_rsp);
    fclose(z_rsp);
    size_t exp_len = strlen(expected);
    int ok = 1;
    if (!c_msg || c_len != exp_len || strcmp(c_msg, expected) != 0) {
        printf("display serialize C golden mismatch flags 0x%llx\n---got---\n%s\n---want---\n%s\n",
               (unsigned long long) flags, c_msg ? c_msg : "(null)", expected);
        ok = 0;
    }
    if (!z_msg || z_len != exp_len || strcmp(z_msg, expected) != 0) {
        printf("display serialize Zig golden mismatch flags 0x%llx\n---got---\n%s\n---want---\n%s\n",
               (unsigned long long) flags, z_msg ? z_msg : "(null)", expected);
        ok = 0;
    }
    free(c_msg);
    free(z_msg);
    return ok;
}

static const int ser_spaces[3] = { 3, 4, 5 };

TEST_FUNC(zig_display_serialize,
{
    ensure_ts_for_display_serialize();

    uint32_t saved_current = g_display_manager.current_display_id;
    uint32_t did = CGMainDisplayID();
    CGRect frame = CGDisplayBounds(did);

    /* Lifted uuid echoes the live display_uuid derivation (NULL when the
     * display has no UUID, mirroring the C "<unknown>" fallback). */
    CFStringRef uuid_ref = display_uuid(did);
    char *uuid_str = uuid_ref ? ts_cfstring_copy(uuid_ref) : NULL;
    if (uuid_ref) CFRelease(uuid_ref);

    /* Each SLS-free property alone. */
    TEST_CHECK(serialize_display_parity(did, DISPLAY_PROPERTY_ID, uuid_str, NULL, frame, false), 1);
    TEST_CHECK(serialize_display_parity(did, DISPLAY_PROPERTY_UUID, uuid_str, NULL, frame, false), 1);
    TEST_CHECK(serialize_display_parity(did, DISPLAY_PROPERTY_LABEL, uuid_str, NULL, frame, false), 1);
    TEST_CHECK(serialize_display_parity(did, DISPLAY_PROPERTY_FRAME, uuid_str, NULL, frame, false), 1);

    /* A display id with no UUID: C falls back to "<unknown>". */
    uint32_t bogus_did = 0x7fffff00;
    CFStringRef bogus_uuid = display_uuid(bogus_did);
    TEST_CHECK(bogus_uuid == NULL, true);
    if (bogus_uuid) CFRelease(bogus_uuid);
    TEST_CHECK(serialize_display_parity(bogus_did, DISPLAY_PROPERTY_ID | DISPLAY_PROPERTY_UUID, NULL, NULL, CGDisplayBounds(bogus_did), false), 1);

    /* has-focus both polarities via current_display_id. */
    g_display_manager.current_display_id = did;
    TEST_CHECK(serialize_display_parity(did, DISPLAY_PROPERTY_HAS_FOCUS, uuid_str, NULL, frame, true), 1);
    g_display_manager.current_display_id = did + 1;
    TEST_CHECK(serialize_display_parity(did, DISPLAY_PROPERTY_HAS_FOCUS, uuid_str, NULL, frame, false), 1);

    /* Label present vs absent. */
    display_manager_set_label_for_display(&g_display_manager, did, string_copy("disp-label"));
    TEST_CHECK(serialize_display_parity(did, DISPLAY_PROPERTY_LABEL, uuid_str, "disp-label", frame, false), 1);
    TEST_CHECK(serialize_display_parity(did, DISPLAY_PROPERTY_ID | DISPLAY_PROPERTY_UUID | DISPLAY_PROPERTY_LABEL, uuid_str, "disp-label", frame, false), 1);
    display_manager_remove_label_for_display(&g_display_manager, did);

    /* Comma chains over SLS-free flags. */
    TEST_CHECK(serialize_display_parity(did, DISPLAY_PROPERTY_ID | DISPLAY_PROPERTY_UUID, uuid_str, NULL, frame, false), 1);
    TEST_CHECK(serialize_display_parity(did, DISPLAY_PROPERTY_FRAME | DISPLAY_PROPERTY_LABEL | DISPLAY_PROPERTY_ID, uuid_str, NULL, frame, false), 1);
    g_display_manager.current_display_id = did;
    TEST_CHECK(serialize_display_parity(did,
               DISPLAY_PROPERTY_ID | DISPLAY_PROPERTY_UUID | DISPLAY_PROPERTY_LABEL |
               DISPLAY_PROPERTY_FRAME | DISPLAY_PROPERTY_HAS_FOCUS,
               uuid_str, NULL, frame, true), 1);
    g_display_manager.current_display_id = did + 1;

    /* Zig-only: each SLS-backed property alone. */
    TEST_CHECK(serialize_display_zig_only(did, DISPLAY_PROPERTY_INDEX, uuid_str, 2, NULL, 0, 0, 0, 0, NULL, 0, false,
               "{\n\t\"index\":2\n}"), 1);
    TEST_CHECK(serialize_display_zig_only(did, DISPLAY_PROPERTY_SPACES, uuid_str, 0, NULL, 0, 0, 0, 0, ser_spaces, 3, false,
               "{\n\t\"spaces\":[3, 4, 5]\n}"), 1);
    TEST_CHECK(serialize_display_zig_only(did, DISPLAY_PROPERTY_SPACES, uuid_str, 0, NULL, 0, 0, 0, 0, ser_spaces, 1, false,
               "{\n\t\"spaces\":[3]\n}"), 1);

    /* Zig-only: empty space list renders []. */
    TEST_CHECK(serialize_display_zig_only(did, DISPLAY_PROPERTY_SPACES, uuid_str, 0, NULL, 0, 0, 0, 0, NULL, 0, false,
               "{\n\t\"spaces\":[]\n}"), 1);

    /* Zig-only: NULL uuid renders <unknown>, NULL label renders "". */
    TEST_CHECK(serialize_display_zig_only(did, DISPLAY_PROPERTY_UUID, NULL, 0, NULL, 0, 0, 0, 0, NULL, 0, false,
               "{\n\t\"uuid\":\"<unknown>\"\n}"), 1);
    TEST_CHECK(serialize_display_zig_only(did, DISPLAY_PROPERTY_LABEL, uuid_str, 0, NULL, 0, 0, 0, 0, NULL, 0, false,
               "{\n\t\"label\":\"\"\n}"), 1);

    /* Zig-only: frame %.4f formatting. */
    TEST_CHECK(serialize_display_zig_only(did, DISPLAY_PROPERTY_FRAME, uuid_str, 0, NULL, 1.5, 2.25, 1512, 944, NULL, 0, false,
               "{\n\t\"frame\":{\n\t\t\"x\":1.5000,\n\t\t\"y\":2.2500,\n\t\t\"w\":1512.0000,\n\t\t\"h\":944.0000\n\t}\n}"), 1);

    /* Zig-only: has-focus both polarities. */
    TEST_CHECK(serialize_display_zig_only(did, DISPLAY_PROPERTY_HAS_FOCUS, uuid_str, 0, NULL, 0, 0, 0, 0, NULL, 0, true,
               "{\n\t\"has-focus\":true\n}"), 1);
    TEST_CHECK(serialize_display_zig_only(did, DISPLAY_PROPERTY_HAS_FOCUS, uuid_str, 0, NULL, 0, 0, 0, 0, NULL, 0, false,
               "{\n\t\"has-focus\":false\n}"), 1);

    /* Zig-only: all-flags and flags==0 expand to the same full document. */
    {
        uint64_t all = DISPLAY_PROPERTY_ID | DISPLAY_PROPERTY_UUID | DISPLAY_PROPERTY_INDEX |
                       DISPLAY_PROPERTY_LABEL | DISPLAY_PROPERTY_FRAME |
                       DISPLAY_PROPERTY_SPACES | DISPLAY_PROPERTY_HAS_FOCUS;
        const char *full = "{\n\t\"id\":7,\n\t\"uuid\":\"disp-uuid-1\",\n\t\"index\":2,\n\t\"label\":\"disp-label\",\n\t\"frame\":{\n\t\t\"x\":1.5000,\n\t\t\"y\":2.2500,\n\t\t\"w\":1512.0000,\n\t\t\"h\":944.0000\n\t},\n\t\"spaces\":[3, 4, 5],\n\t\"has-focus\":true\n}";
        TEST_CHECK(serialize_display_zig_only(7, all, "disp-uuid-1", 2, "disp-label", 1.5, 2.25, 1512, 944, ser_spaces, 3, true, full), 1);
        TEST_CHECK(serialize_display_zig_only(7, 0, "disp-uuid-1", 2, "disp-label", 1.5, 2.25, 1512, 944, ser_spaces, 3, true, full), 1);

        /* Degenerate full expansion: NULL strings, empty spaces, all false. */
        const char *bare = "{\n\t\"id\":7,\n\t\"uuid\":\"<unknown>\",\n\t\"index\":0,\n\t\"label\":\"\",\n\t\"frame\":{\n\t\t\"x\":0.0000,\n\t\t\"y\":0.0000,\n\t\t\"w\":0.0000,\n\t\t\"h\":0.0000\n\t},\n\t\"spaces\":[],\n\t\"has-focus\":false\n}";
        TEST_CHECK(serialize_display_zig_only(7, 0, NULL, 0, NULL, 0, 0, 0, 0, NULL, 0, false, bare), 1);
    }

    /* Hand-pinned full-JSON golden for BOTH oracles: fixed degenerate
     * display (NULL uuid/label, zero frame, empty spaces, focus false)
     * over all-flags and flags==0. The id must never have been a real
     * display: macOS keeps reporting a 1x1 frame for the id of a display
     * that was once attached (a small id like 7 failed on a machine that
     * had used it), so this uses the same out-of-range id as the
     * "<unknown>" check above. C derives <unknown>/0/""/zero/[] without
     * SLS; the lifted Zig inputs mirror those derivations exactly. */
    {
        uint64_t all = DISPLAY_PROPERTY_ID | DISPLAY_PROPERTY_UUID | DISPLAY_PROPERTY_INDEX |
                       DISPLAY_PROPERTY_LABEL | DISPLAY_PROPERTY_FRAME |
                       DISPLAY_PROPERTY_SPACES | DISPLAY_PROPERTY_HAS_FOCUS;
        const char *bare = "{\n\t\"id\":2147483392,\n\t\"uuid\":\"<unknown>\",\n\t\"index\":0,\n\t\"label\":\"\",\n\t\"frame\":{\n\t\t\"x\":0.0000,\n\t\t\"y\":0.0000,\n\t\t\"w\":0.0000,\n\t\t\"h\":0.0000\n\t},\n\t\"spaces\":[],\n\t\"has-focus\":false\n}";
        uint32_t saved_focus = g_display_manager.current_display_id;
        display_manager_remove_label_for_display(&g_display_manager, 0x7fffff00);
        g_display_manager.current_display_id = 1;
        TEST_CHECK(serialize_display_golden_both(0x7fffff00, all, NULL, 0, NULL, 0, 0, 0, 0, NULL, 0, false, bare), 1);
        TEST_CHECK(serialize_display_golden_both(0x7fffff00, 0, NULL, 0, NULL, 0, 0, 0, 0, NULL, 0, false, bare), 1);
        g_display_manager.current_display_id = saved_focus;
    }

    g_display_manager.current_display_id = saved_current;
})
