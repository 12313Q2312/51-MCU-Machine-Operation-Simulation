/**
 * firmware/led_matrix.c — 8x8 LED 点阵驱动实现
 */
#include "led_matrix.h"

static uint8_t framebuffer[MATRIX_ROWS];
static uint8_t current_row = 0;

void led_clear(void) {
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        framebuffer[i] = 0;
    }
}

void led_set_framebuffer(const uint8_t fb[MATRIX_ROWS]) {
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        framebuffer[i] = fb[i];
    }
}

void led_plot_point(uint16_t qx, uint16_t qy,
                    uint16_t x_min, uint16_t x_max,
                    uint16_t y_min, uint16_t y_max) {
    /* 量化坐标 → 0..7 网格索引 */
    uint16_t range;
    uint8_t col, row;

    range = (x_max > x_min) ? (x_max - x_min) : 1;
    col = (uint8_t)(((uint32_t)(qx - x_min) * (MATRIX_COLS - 1)) / range);
    if (col >= MATRIX_COLS) col = MATRIX_COLS - 1;

    range = (y_max > y_min) ? (y_max - y_min) : 1;
    row = (uint8_t)(((uint32_t)(qy - y_min) * (MATRIX_ROWS - 1)) / range);
    if (row >= MATRIX_ROWS) row = MATRIX_ROWS - 1;

    /* 注意: 屏幕坐标系 Y 轴向下, 执行层翻转 */
    row = (MATRIX_ROWS - 1) - row;

    framebuffer[row] |= (1 << col);
}

void led_scan_row(void) {
    /* 先关所有行，避免鬼影 */
    HAL_WRITE_ROW(0x00);

    /* 输出当前行列数据 */
    HAL_WRITE_COL(framebuffer[current_row]);
    HAL_WRITE_ROW((uint8_t)(1 << current_row));

    current_row++;
    if (current_row >= MATRIX_ROWS) {
        current_row = 0;
    }
}
