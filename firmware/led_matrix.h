/**
 * firmware/led_matrix.h — 8x8 LED 点阵驱动 (抽象接口)
 *
 * 采用逐行动态扫描。每 2ms 刷新一行，60Hz 整体刷新率。
 * 通过 hal_map.h 的 HAL_WRITE_ROW/HAL_WRITE_COL 抽象硬件。
 */
#ifndef FW_LED_MATRIX_H
#define FW_LED_MATRIX_H

#include <stdint.h>
#include "hal_map.h"

/* LED 矩阵尺寸 */
#define MATRIX_ROWS 8
#define MATRIX_COLS 8

/**
 * 设置 framebuffer[8] — 每位对应一个 LED (1=亮, 0=灭)。
 * 例如: fb[0] = 0b10000001 表示第0行最左和最右亮。
 */
void led_set_framebuffer(const uint8_t fb[MATRIX_ROWS]);

/**
 * 将量化坐标 (0-65535) 映射到 8x8 网格并点亮对应 LED。
 * x_range: [x_min, x_max] 对应列范围。
 * y_range: [y_min, y_max] 对应行范围。
 * 调用后需在主循环中调用 led_scan_row() 逐行刷新。
 */
void led_plot_point(uint16_t qx, uint16_t qy,
                    uint16_t x_min, uint16_t x_max,
                    uint16_t y_min, uint16_t y_max);

/** 清空 framebuffer */
void led_clear(void);

/**
 * 扫描一行 (由定时器中断每 2ms 调用一次)。
 * 自动循环 0-7 行。
 */
void led_scan_row(void);

#endif /* FW_LED_MATRIX_H */
