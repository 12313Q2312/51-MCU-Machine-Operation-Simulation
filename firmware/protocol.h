/**
 * firmware/protocol.h — MCU 端帧解码协议
 *
 * 与 PC 端 protocol.cpp 对应。解析 10 字节帧，提取状态向量。
 */
#ifndef FW_PROTOCOL_H
#define FW_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* 解析后的单帧数据 */
typedef struct {
    uint8_t  motion_type;    /* 0=line, 1=circle, 2=sin */
    uint16_t x;              /* 量化坐标 (0-65535) */
    uint16_t y;
    int8_t   vx;             /* 量化速度 */
    int8_t   vy;
} FrameData;

/**
 * 解码一帧。返回解析后的结构体。
 * 若帧头不是 0xAA 或校验失败，返回 0；成功返回 1。
 */
uint8_t protocol_decode(const uint8_t frame[10], FrameData* out);

/**
 * 单字节接收状态机。用于从 UART 逐字节喂入。
 * 当完整接收一帧时调用 callback。
 */
typedef void (*frame_callback_t)(const FrameData* data);

void protocol_reset(void);
void protocol_feed(uint8_t byte, frame_callback_t on_frame);

#endif /* FW_PROTOCOL_H */
