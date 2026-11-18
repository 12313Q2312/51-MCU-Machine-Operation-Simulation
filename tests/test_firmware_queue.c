/* Host-side boundary test for the real 8051 firmware ring buffer (C99). */
#include "queue.h"
#include <stdio.h>

static int checks = 0;
static int verify(int ok, const char *message) {
    ++checks;
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}
static FrameData make_frame(unsigned int id) {
    FrameData f = {0};
    f.motion_type = (uint8_t)(id % 3u);
    f.x = (uint16_t)(id * 257u);
    f.y = (uint16_t)(65535u - id);
    f.vx = (int8_t)(-(int)(id % 127u));
    f.vy = (int8_t)(id % 127u);
    return f;
}
static int equal_frame(const FrameData *a, const FrameData *b) {
    return a->motion_type == b->motion_type &&
           a->x == b->x && a->y == b->y &&
           a->vx == b->vx && a->vy == b->vy;
}
int main(void) {
    FrameQueue queue;
    FrameData input, output;
    unsigned int i;
    if (!verify(QUEUE_CAPACITY == 8, "configured capacity is not eight")) return 1;

    queue_init(&queue);
    if (!verify(queue_is_empty(&queue), "new queue not empty") ||
        !verify(!queue_is_full(&queue), "new queue unexpectedly full") ||
        !verify(!queue_pop(&queue, &output), "empty pop succeeded")) return 1;

    for (i = 0; i < QUEUE_CAPACITY; ++i) {
        input = make_frame(i);
        if (!verify(queue_push(&queue, &input), "push failed before capacity")) return 1;
    }
    if (!verify(queue_is_full(&queue), "capacity boundary not full")) return 1;
    input = make_frame(100);
    if (!verify(!queue_push(&queue, &input), "overflow push succeeded") ||
        !verify(queue.count == QUEUE_CAPACITY, "overflow changed queue count")) return 1;

    for (i = 0; i < QUEUE_CAPACITY / 2; ++i) {
        input = make_frame(i);
        if (!verify(queue_pop(&queue, &output), "pop after full failed") ||
            !verify(equal_frame(&input, &output), "FIFO data mismatch")) return 1;
    }
    if (!verify(!queue_is_full(&queue), "queue did not free slots")) return 1;
    for (i = QUEUE_CAPACITY; i < QUEUE_CAPACITY + QUEUE_CAPACITY / 2; ++i) {
        input = make_frame(i);
        if (!verify(queue_push(&queue, &input), "wraparound push failed")) return 1;
    }
    for (i = QUEUE_CAPACITY / 2; i < QUEUE_CAPACITY + QUEUE_CAPACITY / 2; ++i) {
        input = make_frame(i);
        if (!verify(queue_pop(&queue, &output), "wraparound pop failed") ||
            !verify(equal_frame(&input, &output), "wraparound FIFO mismatch")) return 1;
    }
    if (!verify(queue_is_empty(&queue), "queue not empty after wraparound") ||
        !verify(!queue_pop(&queue, &output), "empty pop succeeded after wraparound")) return 1;
    for (i = 0; i < 3u * QUEUE_CAPACITY; ++i) {
        input = make_frame(i + 20u);
        if (!verify(queue_push(&queue, &input), "repeated push failed") ||
            !verify(queue_pop(&queue, &output), "repeated pop failed") ||
            !verify(equal_frame(&input, &output), "repeated FIFO mismatch")) return 1;
    }
    printf("PASS: firmware queue boundary/wraparound/FIFO/overflow (%d checks)\n", checks);
    return 0;
}
