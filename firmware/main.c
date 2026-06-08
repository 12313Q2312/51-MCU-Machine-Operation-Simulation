/**
 * firmware/main.c — MCU 固件主程序
 *
 * 平台: 51 MCU (STC89C52, 11.0592MHz) / STM32F103 (通过 HAL 宏切换)
 * 编译器: SDCC (51) / arm-none-eabi-gcc (STM32)
 *
 * 主循环:
 *   1. 从 UART 接收 PC 发来的帧数据
 *   2. 解码后放入环形队列
 *   3. 逐帧取出, 映射到 8x8 LED 点阵
 *   4. 2ms 定时中断刷新一行 (逐行动态扫描)
 *
 * 移植到新平台: 只需修改 hal_map.h 和中断初始化。
 */
#include "hal_map.h"
#include "protocol.h"
#include "led_matrix.h"
#include "queue.h"

/* ---- 全局状态 ---- */
static FrameQueue frame_queue;
static volatile uint8_t system_running = 1;  /* 急停标志 */
static volatile uint32_t tick_counter = 0;   /* 2ms tick 计数 */

/* ---- 帧就绪回调: 由 protocol_feed 在 ISR 中调用 ---- */
static void on_frame_received(const FrameData* data) {
    if (!queue_push(&frame_queue, data)) {
        HAL_LED_ERR_ON();  /* 队列满 → 错误指示 */
    }
}

/* ---- 急停中断 (51: INT0; STM32: EXTI) ---- */
#ifdef MCU_8051
void INT0_ISR(void) __interrupt(0) {
    system_running = 0;
}
#elif defined(MCU_STM32)
void EXTI0_IRQHandler(void) {
    if (__HAL_GPIO_EXTI_GET_IT(ESTOP_PIN)) {
        __HAL_GPIO_EXTI_CLEAR_IT(ESTOP_PIN);
        system_running = 0;
    }
}
#endif

/* ---- 定时器中断: 2ms 间隔, 刷新 LED 行扫描 ---- */
#ifdef MCU_8051
/*
 * T0 中断, 模式 1 (16 位自动重载)
 *
 * 11.0592MHz, 12T 模式:
 *   机器周期 = 12/11059200 ≈ 1.085us
 *   2ms = 2000us → 2000/1.085 ≈ 1843 个计数
 *   重载值 = 65536 - 1843 = 63693 = 0xF8CD
 */
void T0_ISR(void) __interrupt(1) {
    TH0 = 0xF8;
    TL0 = 0xCD;
    led_scan_row();
    tick_counter++;
}

/*
 * 软件延时 (11.0592MHz, 12T)
 * 内层循环约 8 机器周期/次 → 约 8.68us/次
 * 115 次 ≈ 1ms
 */
void _delay_ms(uint16_t ms) {
    while (ms--) {
        volatile uint16_t i;
        for (i = 0; i < 115; i++) { /* ~1ms */ }
    }
}

#elif defined(MCU_STM32)
void SysTick_Handler(void) {
    tick_counter++;
    if ((tick_counter % 2) == 0) {
        led_scan_row();
    }
    HAL_IncTick();
}
#endif

/* ---- 初始化 ---- */
static void system_init(void) {
    queue_init(&frame_queue);
    led_clear();
    protocol_reset();

#ifdef MCU_8051
    HAL_UART_INIT();

    /* T0: 模式 1, 16 位定时器, 2ms 周期 */
    TMOD = (TMOD & 0xF0) | 0x01;
    TH0 = 0xF8;   /* 2ms @ 11.0592MHz */
    TL0 = 0xCD;
    ET0 = 1;      /* 使能 T0 中断 */
    TR0 = 1;      /* 启动 T0 */

    /* INT0: 下降沿触发 */
    IT0 = 1;
    EX0 = 1;

    EA = 1;       /* 全局中断 */

    HAL_LED_RUN_ON();
#endif
}

/* ---- 主循环 ---- */
#ifdef MCU_8051
void main(void) {
#else
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
#endif

    system_init();

    /* 坐标范围: 与 PC 端 protocol.cpp 同步 */
    const uint16_t x_min = 0, x_max = 65535;
    const uint16_t y_min = 0, y_max = 65535;
    uint32_t frame_seq = 0;

    while (system_running) {
        /* 1. 检查 UART 接收 */
        while (HAL_UART_RX_AVAIL()) {
            uint8_t byte = HAL_UART_READ();
            protocol_feed(byte, on_frame_received);
        }

        /* 2. 处理队列中的帧 */
        FrameData data;
        if (queue_pop(&frame_queue, &data)) {
            frame_seq++;
            /* 清屏后绘制新点 (单点模式) */
            led_clear();
            led_plot_point(data.x, data.y, x_min, x_max, y_min, y_max);

            /* 发送 ACK 回 PC */
            HAL_UART_WRITE(0xCC);  /* ACK */
            HAL_UART_WRITE((uint8_t)(frame_seq & 0xFF));

            HAL_LED_RUN_ON();
        }

        /* 3. 急停检测 */
        if (HAL_ESTOP_READ()) {
            system_running = 0;
            HAL_LED_ERR_ON();
            led_clear();
            led_scan_row();
        }
    }

    /* 停机: 全灭 */
    HAL_LED_RUN_OFF();
    HAL_LED_ERR_ON();
    led_clear();

    while (1) {
        led_scan_row();
    }

#ifdef MCU_STM32
    return 0;
#endif
}
