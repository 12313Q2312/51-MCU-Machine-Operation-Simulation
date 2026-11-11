/**
 * firmware/hal_map.h — 硬件抽象层 (HAL)
 *
 * 通过编译宏切换目标平台。移植到新 MCU 只需修改本文件。
 *
 * 晶振假设: 51 = 11.0592MHz, STM32 = 8MHz HSE / 72MHz SYSCLK
 *
 * 编译示例:
 *   SDCC:   sdcc -DMCU_8051 ...
 *   Keil:   在 Options → C51 → Define 中添加 MCU_KEIL
 *   STM32:  arm-none-eabi-gcc -DMCU_STM32 -DSTM32F103xB ...
 */

#ifndef HAL_MAP_H
#define HAL_MAP_H

/* ================================================================
 * 整数类型: Keil C51 老版本可能没有 stdint.h, 手动定义
 * ================================================================ */
#if defined(MCU_KEIL)
  /* Keil C51: 手动定义精确宽度类型, 避免依赖 stdint.h */
  typedef unsigned char  uint8_t;
  typedef unsigned int   uint16_t;
  typedef unsigned long  uint32_t;
  typedef signed char    int8_t;
  typedef signed int     int16_t;
#else
  #include <stdint.h>
#endif

/* size_t (queue.h 等用到) */
#include <stddef.h>

/* ================================================================
 * 平台选择 (三选一: MCU_8051 / MCU_KEIL / MCU_STM32)
 * ================================================================ */
#if defined(MCU_HOST_TEST)
  /* Host-only adapter for tests of the real LED driver. Never use on a board. */
  void hal_host_write_row(uint8_t value);
  void hal_host_write_col(uint8_t value);
  #define HAL_WRITE_ROW(value) hal_host_write_row(value)
  #define HAL_WRITE_COL(value) hal_host_write_col(value)
#elif defined(MCU_STM32)
  /* ---- STM32 (HAL 库) ---- */
  #include "stm32f1xx_hal.h"

  /* LED 矩阵引脚 (示例: STM32F103C8, 可按实际接线修改) */
  #define LED_ROW_PORT       GPIOA
  #define LED_ROW_PIN_START  0       /* PA0-PA7 → 8 行 */
  #define LED_COL_PORT       GPIOB
  #define LED_COL_PIN_START  0       /* PB0-PB7 → 8 列 */

  /* 串口 */
  #define MCU_UART           USART1

  /* 状态 LED */
  #define STATUS_LED_RUN_PORT  GPIOC
  #define STATUS_LED_RUN_PIN   GPIO_PIN_13
  #define STATUS_LED_ERR_PORT  GPIOC
  #define STATUS_LED_ERR_PIN   GPIO_PIN_14

  /* 急停按钮 */
  #define ESTOP_PORT           GPIOA
  #define ESTOP_PIN            GPIO_PIN_8

  /* ---- HAL 宏封装 ---- */
  #define HAL_WRITE_ROW(val)   GPIOA->ODR = ((GPIOA->ODR & ~0xFF) | ((val) & 0xFF))
  #define HAL_WRITE_COL(val)   GPIOB->ODR = ((GPIOB->ODR & ~0xFF) | ((val) & 0xFF))

  #define HAL_UART_RX_AVAIL()  __HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)
  #define HAL_UART_READ()      (uint8_t)(USART1->DR & 0xFF)
  #define HAL_UART_WRITE(b)    do { while(!(USART1->SR & USART_SR_TXE)); USART1->DR = (b); } while(0)

  #define HAL_DELAY_MS(ms)     HAL_Delay(ms)
  #define HAL_GET_TICK()       HAL_GetTick()

  #define HAL_ESTOP_READ()     ((GPIOA->IDR & ESTOP_PIN) == 0)  /* 低电平有效 */

  #define HAL_LED_RUN_ON()     HAL_GPIO_WritePin(STATUS_LED_RUN_PORT, STATUS_LED_RUN_PIN, GPIO_PIN_RESET)
  #define HAL_LED_RUN_OFF()    HAL_GPIO_WritePin(STATUS_LED_RUN_PORT, STATUS_LED_RUN_PIN, GPIO_PIN_SET)
  #define HAL_LED_ERR_ON()     HAL_GPIO_WritePin(STATUS_LED_ERR_PORT, STATUS_LED_ERR_PIN, GPIO_PIN_RESET)
  #define HAL_LED_ERR_OFF()    HAL_GPIO_WritePin(STATUS_LED_ERR_PORT, STATUS_LED_ERR_PIN, GPIO_PIN_SET)

#elif defined(MCU_KEIL)
  /* ---- 51 MCU (Keil C51 / µVision, STC89C52, 11.0592MHz) ---- */
  #include <reg52.h>

  /*
   * 注意: 使用直接端口位操作而非 sbit 声明。
   * sbit 放在头文件中会导致多文件链接时 MULTIPLE PUBLIC DEFINITIONS 错误。
   */

  /* 引脚位掩码: P3.2=急停(输入,低有效), P3.4=运行LED, P3.5=错误LED */
  #define MASK_ESTOP   0x04   /* P3.2 */
  #define MASK_LED_RUN 0x10   /* P3.4 */
  #define MASK_LED_ERR 0x20   /* P3.5 */

  /* LED 矩阵: P0=行选 (经 74HC595), P1=列数据 (经 ULN2803) */
  #define HAL_WRITE_ROW(val)   (P0 = (val))
  #define HAL_WRITE_COL(val)   (P1 = (val))

  /* ---- 串口 (9600 baud @ 11.0592MHz, 模式 1, 8-N-1) ----
   * SMOD=0 → TH1 = 256 - 11059200/(384*9600) = 0xFD
   */
  #define HAL_UART_RX_AVAIL()  (RI)
  #define HAL_UART_READ()      (RI = 0, SBUF)
  #define HAL_UART_WRITE(b)    do { while(!TI); TI = 0; SBUF = (b); } while(0)
  #define HAL_UART_INIT()      do { SCON = 0x50; TMOD |= 0x20; TH1 = 0xFD; TI = 1; TR1 = 1; } while(0)

  /* ---- 延时 (11.0592MHz, 12T) ----
   * 内层循环约 8 机器周期 → ~8.68us/次, 115 次 ≈ 1ms
   */
  #define HAL_DELAY_MS(ms)     _delay_ms(ms)
  void _delay_ms(uint16_t ms);

  /* 无系统 tick */
  #define HAL_GET_TICK()       0

  /* ---- 急停/状态 LED (直接端口位操作, 无 sbit) ---- */
  #define HAL_ESTOP_READ()     ((P3 & MASK_ESTOP) == 0)   /* P3.2 低电平=按下 */

  #define HAL_LED_RUN_ON()     (P3 &= ~MASK_LED_RUN)      /* P3.4 → 0 */
  #define HAL_LED_RUN_OFF()    (P3 |= MASK_LED_RUN)       /* P3.4 → 1 */
  #define HAL_LED_ERR_ON()     (P3 &= ~MASK_LED_ERR)      /* P3.5 → 0 */
  #define HAL_LED_ERR_OFF()    (P3 |= MASK_LED_ERR)       /* P3.5 → 1 */

#elif defined(MCU_8051)
  /* ---- 51 MCU (SDCC 编译, STC89C52, 11.0592MHz) ---- */
  #include <8052.h>

  /* LED 矩阵: 直接使用 8052.h 定义的 P0/P1 */
  #define HAL_WRITE_ROW(val)   (P0 = (val))   /* 行选 (经 74HC595) */
  #define HAL_WRITE_COL(val)   (P1 = (val))   /* 列数据 (经 ULN2803) */

  /* 串口: 9600 baud @ 11.0592MHz, 模式 1 (8-N-1)
   *   SMOD=0 → TH1 = 256 - 11059200/(384*9600) = 0xFD
   */
  #define HAL_UART_RX_AVAIL()  (RI)
  static uint8_t hal_uart_read(void) { uint8_t byte = SBUF; RI = 0; return byte; }
  #define HAL_UART_READ()      hal_uart_read()
  #define HAL_UART_WRITE(b)    do { while(!TI); TI = 0; SBUF = (b); } while(0)
  #define HAL_UART_INIT()      do { SCON = 0x50; TMOD |= 0x20; TH1 = 0xFD; TI = 1; TR1 = 1; } while(0)

  /* 定时器 T0: 模式 1, 16 位, 每 2ms 中断一次
   *   12T 模式 @ 11.0592MHz: 2ms 需要 1843 个机器周期
   *   重载值 = 65536 - 1843 = 63693 = 0xF8CD
   */
  #define HAL_DELAY_MS(ms)     _delay_ms(ms)
  void _delay_ms(uint16_t ms);

  /* 无 tick, 用软件延时替代 */
  #define HAL_GET_TICK()       0

  /* 急停: P3.2 (INT0), 低电平有效 */
  #define HAL_ESTOP_READ()     (!P3_2)

  /* 状态 LED: P3.4 (运行), P3.5 (错误), 低电平点亮 (位定义来自 8052.h) */
  #define HAL_LED_RUN_ON()     (P3_4 = 0)
  #define HAL_LED_RUN_OFF()    (P3_4 = 1)
  #define HAL_LED_ERR_ON()     (P3_5 = 0)
  #define HAL_LED_ERR_OFF()    (P3_5 = 1)

#else
  #error "请定义目标平台: -DMCU_8051 (SDCC) 或 -DMCU_KEIL (Keil) 或 -DMCU_STM32"
#endif

/* ---- 帧协议常量 (与 C++ 端一致) ---- */
#define FRAME_HEADER  0xAA
#define FRAME_SIZE    10

/* 运动模式 */
#define MOTION_LINE    0
#define MOTION_CIRCLE  1
#define MOTION_SIN     2

#endif /* HAL_MAP_H */
