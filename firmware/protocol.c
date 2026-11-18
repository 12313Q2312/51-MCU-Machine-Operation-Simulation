/**
 * firmware/protocol.c — MCU 端帧解码实现
 */
#include "protocol.h"
#include "hal_map.h"

/* Called only from protocol_feed. protocol_decode inlines the same loop so
 * its pointer can stay in overlay RAM instead of the last direct bytes. */
static uint8_t checksum_matches(const uint8_t *frame) {
    uint8_t cs = 0;
    uint8_t i;
    for (i = 0; i < FRAME_SIZE - 1; i++) cs ^= frame[i];
    return cs == frame[FRAME_SIZE - 1];
}

uint8_t protocol_decode(const uint8_t frame[10], FrameData* out) {
    uint8_t cs = 0;
    uint8_t i;
    if (frame[0] != FRAME_HEADER) return 0;
    for (i = 0; i < FRAME_SIZE - 1; i++) cs ^= frame[i];
    if (cs != frame[FRAME_SIZE - 1]) return 0;
    /* 0=line, 1=circle, 2=sin. Any other motion byte is not a command. */
    if (frame[1] > MOTION_SIN) return 0;

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
    FrameData data;

    if (!rx_synced) {
        /* 搜索帧头 0xAA */
        if (byte == FRAME_HEADER) {
            rx_buf[0] = byte;
            rx_pos = 1;
            rx_synced = 1;
        }
        return;
    }

    /* 长度保护：10 字节窗口未退出时不再写入。 */
    if (rx_pos >= FRAME_SIZE) {
        rx_pos = 0;
        rx_synced = 0;
        if (byte == FRAME_HEADER) {
            rx_buf[0] = byte;
            rx_pos = 1;
            rx_synced = 1;
        }
        return;
    }

    rx_buf[rx_pos++] = byte;
    if (rx_pos < FRAME_SIZE) return;

    if (protocol_decode(rx_buf, &data)) {
        rx_pos = 0;
        rx_synced = 0;
        if (on_frame) on_frame(&data);
        return;
    }

    /*
     * 校验正确但运动类型未知：这 10 字节是对齐的，整帧丢弃。
     * 不能在窗口内部重同步，否则会把下一帧拆开。
     * 校验失败：帧头可能落在本窗口内部，从下一个 0xAA 继续。
     */
    if (checksum_matches(rx_buf)) {
        rx_pos = 0;
        rx_synced = 0;
        return;
    }

    {
        uint8_t start;
        uint8_t n;
        uint8_t j;
        for (start = 1; start < FRAME_SIZE; start++) {
            if (rx_buf[start] == FRAME_HEADER) break;
        }
        if (start >= FRAME_SIZE) {
            rx_pos = 0;
            rx_synced = 0;
            return;
        }
        n = (uint8_t)(FRAME_SIZE - start);
        for (j = 0; j < n; j++) {
            rx_buf[j] = rx_buf[start + j];
        }
        rx_pos = n;
        rx_synced = 1;
    }
}
