/**
 * firmware/queue.c — 环形缓冲区实现
 */
#include "queue.h"

void queue_init(FrameQueue* q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

uint8_t queue_push(FrameQueue* q, const FrameData* item) {
    if (q->count >= QUEUE_CAPACITY) return 0;
    q->buffer[q->head] = *item;
    q->head = (q->head + 1) % QUEUE_CAPACITY;
    q->count++;
    return 1;
}

uint8_t queue_pop(FrameQueue* q, FrameData* item) {
    if (q->count == 0) return 0;
    *item = q->buffer[q->tail];
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count--;
    return 1;
}

uint8_t queue_is_empty(const FrameQueue* q) {
    return q->count == 0;
}

uint8_t queue_is_full(const FrameQueue* q) {
    return q->count >= QUEUE_CAPACITY;
}
