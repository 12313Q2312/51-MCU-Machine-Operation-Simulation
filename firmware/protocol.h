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
 * 解码一帧。frame 必须是完整的 10 字节。
 * 成功返回 1。帧头不是 0xAA、校验失败，或运动类型不是 0/1/2 时返回 0，且不写 out。
 */
uint8_t protocol_decode(const uint8_t frame[10], FrameData* out);

/**
 * 单字节接收状态机。用于从 UART 逐字节喂入。
 * 凑满 10 字节且校验通过、运动类型为 0/1/2 时调用 callback。
 * 不足 10 字节不会回调。校验失败时在本窗口内重新寻找 0xAA。
 * 未知运动类型整帧丢弃，不回调。不可重入，只在主循环调用。
 */
typedef void (*frame_callback_t)(const FrameData* data);

void protocol_reset(void);
void protocol_feed(uint8_t byte, frame_callback_t on_frame);

#endif /* FW_PROTOCOL_H */
