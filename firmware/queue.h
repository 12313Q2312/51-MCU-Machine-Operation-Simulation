/**
 * firmware/queue.h — 环形缓冲区 (用于缓冲接收到的帧数据)
 *
 * 无锁设计，单生产者(ISR) + 单消费者(主循环)。
 */
#ifndef FW_QUEUE_H
#define FW_QUEUE_H

#include <stdint.h>
#include "protocol.h"

#define QUEUE_CAPACITY 8    /* 8051: 小 RAM 设备；PC 串口逐帧等待 ACK */

typedef struct {
    FrameData buffer[QUEUE_CAPACITY];
    volatile uint8_t head;  /* 生产者写入位置 */
    volatile uint8_t tail;  /* 消费者读取位置 */
    volatile uint8_t count;
} FrameQueue;

void queue_init(FrameQueue* q);
uint8_t queue_push(FrameQueue* q, const FrameData* item);
uint8_t queue_pop(FrameQueue* q, FrameData* item);
uint8_t queue_is_empty(const FrameQueue* q);
uint8_t queue_is_full(const FrameQueue* q);

#endif /* FW_QUEUE_H */
