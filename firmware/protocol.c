/**
 * firmware/protocol.c — MCU 端帧解码实现
 */
#include "protocol.h"
#include "hal_map.h"

uint8_t protocol_decode(const uint8_t frame[10], FrameData* out) {
    if (frame[0] != FRAME_HEADER) return 0;

    /* XOR 校验 */
    uint8_t cs = 0;
    for (int i = 0; i < FRAME_SIZE - 1; i++) {
        cs ^= frame[i];
    }
    if (cs != frame[FRAME_SIZE - 1]) return 0;

    out->motion_type = frame[1];
    out->x = ((uint16_t)frame[2] << 8) | frame[3];
    out->y = ((uint16_t)frame[4] << 8) | frame[5];
    out->vx = (int8_t)frame[6];
    out->vy = (int8_t)frame[7];

    return 1;
}

/* ---- 逐字节接收状态机 ---- */
static uint8_t rx_buf[FRAME_SIZE];
static uint8_t rx_pos = 0;
static uint8_t rx_synced = 0;  /* 是否已找到帧头 */

void protocol_reset(void) {
    rx_pos = 0;
    rx_synced = 0;
}

void protocol_feed(uint8_t byte, frame_callback_t on_frame) {
    if (!rx_synced) {
        /* 搜索帧头 0xAA */
        if (byte == FRAME_HEADER) {
            rx_buf[0] = byte;
            rx_pos = 1;
            rx_synced = 1;
        }
        return;
    }

    rx_buf[rx_pos++] = byte;
    if (rx_pos >= FRAME_SIZE) {
        rx_pos = 0;
        rx_synced = 0;

        FrameData data;
        if (protocol_decode(rx_buf, &data) && on_frame) {
            on_frame(&data);
        }
    }
}
