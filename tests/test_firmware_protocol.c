/* Host-side tests for the real firmware frame parser, LED driver and queue.
 * main.c is not compiled here: its 8051 interrupt syntax is not host-portable.
 * The accept path mirrors main.c: only a protocol_feed callback may plot or enqueue.
 */
#include "led_matrix.h"
#include "protocol.h"
#include "queue.h"

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int frames_seen = 0;
static FrameData last_frame;
static uint8_t col_latch = 0;
static uint8_t shown[MATRIX_ROWS];

static int verify(int ok, const char *message) {
    ++checks;
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

void hal_host_write_col(uint8_t value) { col_latch = value; }

void hal_host_write_row(uint8_t value) {
    uint8_t row;
    for (row = 0; row < MATRIX_ROWS; row++) {
        if (value == (uint8_t)(1u << row)) shown[row] = col_latch;
    }
}

static void on_frame(const FrameData *data) {
    last_frame = *data;
    frames_seen++;
}

static void build_frame(uint8_t frame[FRAME_SIZE], uint8_t motion,
                        uint16_t x, uint16_t y, int8_t vx, int8_t vy,
                        uint8_t reserved) {
    uint8_t i;
    frame[0] = FRAME_HEADER;
    frame[1] = motion;
    frame[2] = (uint8_t)(x >> 8);
    frame[3] = (uint8_t)x;
    frame[4] = (uint8_t)(y >> 8);
    frame[5] = (uint8_t)y;
    frame[6] = (uint8_t)vx;
    frame[7] = (uint8_t)vy;
    frame[8] = reserved;
    frame[9] = 0;
    for (i = 0; i < FRAME_SIZE - 1; i++) frame[9] ^= frame[i];
}

static void feed_bytes(const uint8_t *bytes, size_t count) {
    size_t i;
    for (i = 0; i < count; i++) protocol_feed(bytes[i], on_frame);
}

static int fields_match(uint8_t motion, uint16_t x, uint16_t y, int8_t vx, int8_t vy) {
    return last_frame.motion_type == motion && last_frame.x == x && last_frame.y == y &&
           last_frame.vx == vx && last_frame.vy == vy;
}

static uint8_t checksum(const uint8_t *frame) {
    uint8_t cs = 0;
    uint8_t i;
    for (i = 0; i < FRAME_SIZE - 1; i++) cs ^= frame[i];
    return cs;
}

static void capture_matrix(uint8_t out[MATRIX_ROWS]) {
    uint8_t row;
    memset(shown, 0, sizeof shown);
    for (row = 0; row < MATRIX_ROWS; row++) led_scan_row();
    memcpy(out, shown, MATRIX_ROWS);
}

static int one_led(const uint8_t *matrix, uint8_t row, uint8_t col) {
    uint8_t i;
    for (i = 0; i < MATRIX_ROWS; i++) {
        uint8_t expect = (i == row) ? (uint8_t)(1u << col) : 0;
        if (matrix[i] != expect) return 0;
    }
    return 1;
}

static int test_frame_format(void) {
    uint8_t frame[FRAME_SIZE];
    FrameData out;
    FrameData sentinel;
    memset(&sentinel, 0x5A, sizeof sentinel);

    build_frame(frame, MOTION_LINE, 0x1234, 0xABCD, -1, 2, 0);
    out = sentinel;
    if (!verify(protocol_decode(frame, &out) == 1, "line frame rejected") ||
        !verify(out.motion_type == MOTION_LINE && out.x == 0x1234 && out.y == 0xABCD &&
                    out.vx == -1 && out.vy == 2,
                "line fields mismatch")) return 0;

    build_frame(frame, MOTION_CIRCLE, 0, 65535, 0, -128, 0);
    if (!verify(protocol_decode(frame, &out) == 1, "circle frame rejected") ||
        !verify(out.motion_type == MOTION_CIRCLE && out.x == 0 && out.y == 65535 &&
                    out.vx == 0 && out.vy == -128,
                "circle/int8 fields mismatch")) return 0;

    build_frame(frame, MOTION_SIN, 7, 8, 9, 10, 0x5A);
    if (!verify(protocol_decode(frame, &out) == 1, "sin frame with nonzero reserved byte rejected") ||
        !verify(out.motion_type == MOTION_SIN && out.x == 7 && out.y == 8 && out.vx == 9 && out.vy == 10,
                "sin fields mismatch")) return 0;

    build_frame(frame, MOTION_LINE, 1, 2, 3, 4, 0);
    frame[0] = 0x00;
    frame[9] = checksum(frame);
    out = sentinel;
    if (!verify(protocol_decode(frame, &out) == 0, "bad header accepted") ||
        !verify(memcmp(&out, &sentinel, sizeof out) == 0, "bad header wrote output")) return 0;

    build_frame(frame, MOTION_LINE, 1, 2, 3, 4, 0);
    frame[9] ^= 0xFF;
    out = sentinel;
    if (!verify(protocol_decode(frame, &out) == 0, "bad checksum accepted") ||
        !verify(memcmp(&out, &sentinel, sizeof out) == 0, "bad checksum wrote output")) return 0;

    build_frame(frame, 3, 1, 2, 3, 4, 0);
    out = sentinel;
    if (!verify(protocol_decode(frame, &out) == 0, "unknown motion 3 accepted") ||
        !verify(memcmp(&out, &sentinel, sizeof out) == 0, "unknown motion wrote output")) return 0;
    build_frame(frame, 0xFF, 1, 2, 3, 4, 0);
    if (!verify(protocol_decode(frame, &out) == 0, "unknown motion 0xFF accepted")) return 0;
    return 1;
}

static int test_stream_lengths(void) {
    uint8_t frame[FRAME_SIZE];
    uint8_t second[FRAME_SIZE];
    uint8_t noise[64];
    uint8_t long_run[200];
    uint8_t aa_run[30];
    size_t i;

    build_frame(frame, MOTION_LINE, 0x1111, 0x2222, 1, -2, 0);
    protocol_reset();
    frames_seen = 0;
    feed_bytes(frame, FRAME_SIZE - 1);
    if (!verify(frames_seen == 0, "short frame emitted a command")) return 0;
    protocol_feed(frame[FRAME_SIZE - 1], on_frame);
    if (!verify(frames_seen == 1 && fields_match(MOTION_LINE, 0x1111, 0x2222, 1, -2),
                "tenth byte did not complete the frame")) return 0;

    build_frame(second, MOTION_CIRCLE, 9, 8, 0, 0, 0);
    frames_seen = 0;
    feed_bytes(frame, FRAME_SIZE);
    feed_bytes(second, FRAME_SIZE);
    if (!verify(frames_seen == 2 && fields_match(MOTION_CIRCLE, 9, 8, 0, 0),
                "two exact frames were not both accepted")) return 0;

    memset(noise, 0x11, sizeof noise);
    frames_seen = 0;
    feed_bytes(noise, sizeof noise);
    feed_bytes(frame, FRAME_SIZE);
    protocol_feed(0x00, on_frame);
    if (!verify(frames_seen == 1 && fields_match(MOTION_LINE, 0x1111, 0x2222, 1, -2),
                "noise or one trailing byte changed the accepted frame")) return 0;

    memset(long_run, 0x00, sizeof long_run);
    frames_seen = 0;
    feed_bytes(long_run, sizeof long_run);
    feed_bytes(second, FRAME_SIZE);
    if (!verify(frames_seen == 1 && fields_match(MOTION_CIRCLE, 9, 8, 0, 0),
                "overlong non-header input swallowed the next frame")) return 0;

    memset(aa_run, FRAME_HEADER, sizeof aa_run);
    frames_seen = 0;
    feed_bytes(aa_run, sizeof aa_run);
    if (!verify(frames_seen == 0, "all-0xAA run was accepted as a command")) return 0;

    frames_seen = 0;
    protocol_reset();
    feed_bytes(frame, 4);
    protocol_reset();
    feed_bytes(second, FRAME_SIZE);
    if (!verify(frames_seen == 1 && fields_match(MOTION_CIRCLE, 9, 8, 0, 0),
                "reset did not discard a partial frame")) return 0;

    frames_seen = 0;
    for (i = 0; i < FRAME_SIZE; i++) protocol_feed(frame[i], 0);
    feed_bytes(second, FRAME_SIZE);
    if (!verify(frames_seen == 1 && fields_match(MOTION_CIRCLE, 9, 8, 0, 0),
                "null callback broke the following frame")) return 0;
    return 1;
}

static int test_abnormal_commands(void) {
    uint8_t unknown[FRAME_SIZE];
    uint8_t good[FRAME_SIZE];
    uint8_t bad[FRAME_SIZE];
    uint8_t stream[4 + FRAME_SIZE];
    uint8_t window[FRAME_SIZE];
    uint8_t y;

    build_frame(unknown, 0x7F, 0x0102, 0x0304, 5, 6, 0);
    build_frame(good, MOTION_SIN, 0x0A0B, 0x0C0D, -4, 7, 0);
    protocol_reset();
    frames_seen = 0;
    feed_bytes(unknown, FRAME_SIZE);
    feed_bytes(good, FRAME_SIZE);
    if (!verify(frames_seen == 1 && fields_match(MOTION_SIN, 0x0A0B, 0x0C0D, -4, 7),
                "unknown command was acknowledged or shifted the next frame")) return 0;

    build_frame(bad, MOTION_LINE, 1, 2, 3, 4, 0);
    bad[9] ^= 0x01;
    frames_seen = 0;
    feed_bytes(bad, FRAME_SIZE);
    feed_bytes(good, FRAME_SIZE);
    if (!verify(frames_seen == 1 && fields_match(MOTION_SIN, 0x0A0B, 0x0C0D, -4, 7),
                "checksum failure consumed the next aligned frame")) return 0;

    /* A spurious header, three noise bytes, then a real frame. The first
     * 10-byte window must fail checksum so the parser can resync. */
    for (y = 0; y < 32; y++) {
        build_frame(good, MOTION_LINE, 0x3210, (uint16_t)(0x1000 + y), 2, 3, 0);
        window[0] = FRAME_HEADER;
        window[1] = 0x10;
        window[2] = 0x20;
        window[3] = 0x30;
        memcpy(window + 4, good, 6);
        if (checksum(window) != window[9]) break;
    }
    if (!verify(y < 32, "could not build a checksum-failing resync window")) return 0;
    stream[0] = FRAME_HEADER;
    stream[1] = 0x10;
    stream[2] = 0x20;
    stream[3] = 0x30;
    memcpy(stream + 4, good, FRAME_SIZE);
    protocol_reset();
    frames_seen = 0;
    feed_bytes(stream, 4 + FRAME_SIZE);
    if (!verify(frames_seen == 1 && fields_match(MOTION_LINE, 0x3210, (uint16_t)(0x1000 + y), 2, 3),
                "overlong misaligned input did not recover the inner frame")) return 0;
    return 1;
}

static int test_led(void) {
    uint8_t matrix[MATRIX_ROWS];
    uint8_t again[MATRIX_ROWS];
    uint8_t unknown[FRAME_SIZE];
    uint8_t bad[FRAME_SIZE];
    uint8_t good[FRAME_SIZE];

    led_clear();
    capture_matrix(matrix);
    if (!verify(matrix[0] == 0 && matrix[1] == 0 && matrix[2] == 0 && matrix[3] == 0 &&
                    matrix[4] == 0 && matrix[5] == 0 && matrix[6] == 0 && matrix[7] == 0,
                "clear left an LED on"))
        return 0;

    led_clear();
    led_plot_point(0, 0, 0, 65535, 0, 65535);
    capture_matrix(matrix);
    if (!verify(one_led(matrix, 7, 0), "origin did not light row 7 col 0")) return 0;

    led_clear();
    led_plot_point(65535, 65535, 0, 65535, 0, 65535);
    capture_matrix(matrix);
    if (!verify(one_led(matrix, 0, 7), "max coordinate did not light row 0 col 7")) return 0;

    led_clear();
    led_plot_point(32768, 32768, 0, 65535, 0, 65535);
    capture_matrix(matrix);
    if (!verify(one_led(matrix, 3, 4), "midpoint LED position mismatch")) return 0;

    build_frame(good, MOTION_LINE, 0, 0, 0, 0, 0);
    build_frame(unknown, 9, 65535, 65535, 0, 0, 0);
    build_frame(bad, MOTION_LINE, 65535, 65535, 0, 0, 0);
    bad[3] ^= 0xFF;
    protocol_reset();
    frames_seen = 0;
    feed_bytes(good, FRAME_SIZE);
    if (!verify(frames_seen == 1, "LED setup frame was not accepted")) return 0;
    led_clear();
    led_plot_point(last_frame.x, last_frame.y, 0, 65535, 0, 65535);
    capture_matrix(matrix);
    frames_seen = 0;
    feed_bytes(unknown, FRAME_SIZE);
    feed_bytes(bad, FRAME_SIZE);
    if (!verify(frames_seen == 0, "abnormal command reached the LED path")) return 0;
    capture_matrix(again);
    if (!verify(memcmp(matrix, again, MATRIX_ROWS) == 0, "abnormal command changed the LED image"))
        return 0;
    return 1;
}

static int test_queue_rejects_extra_frame(void) {
    FrameQueue queue;
    FrameData got;
    unsigned int i;
    uint8_t frame[FRAME_SIZE];

    queue_init(&queue);
    protocol_reset();
    frames_seen = 0;
    for (i = 0; i < QUEUE_CAPACITY + 1; i++) {
        build_frame(frame, MOTION_LINE, (uint16_t)(100 + i), (uint16_t)(200 + i), 0, 0, 0);
        feed_bytes(frame, FRAME_SIZE);
        if (frames_seen == (int)i + 1) {
            if (!queue_push(&queue, &last_frame) && i < QUEUE_CAPACITY) {
                fprintf(stderr, "FAIL: push rejected before capacity\n");
                return 0;
            }
        }
    }
    if (!verify(frames_seen == QUEUE_CAPACITY + 1, "parser dropped an in-range frame") ||
        !verify(queue.count == QUEUE_CAPACITY, "ninth frame was stored") ||
        !verify(queue_is_full(&queue), "queue not full after eight frames")) return 0;
    for (i = 0; i < QUEUE_CAPACITY; i++) {
        if (!verify(queue_pop(&queue, &got), "pop failed") ||
            !verify(got.x == (uint16_t)(100 + i) && got.y == (uint16_t)(200 + i),
                    "queued frame was not FIFO")) return 0;
    }
    if (!verify(queue_is_empty(&queue), "queue not empty after draining eight")) return 0;
    return 1;
}

int main(void) {
    if (!test_frame_format() || !test_stream_lengths() || !test_abnormal_commands() ||
        !test_led() || !test_queue_rejects_extra_frame()) {
        fprintf(stderr, "firmware protocol tests failed after %d checks\n", checks);
        return 1;
    }
    printf("PASS: firmware protocol/LED/abnormal (%d checks)\n", checks);
    return 0;
}
