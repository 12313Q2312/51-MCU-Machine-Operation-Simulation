/**
 * serial.h — 串口通信 (PC → MCU)
 *
 * 平台: Windows (CreateFile/SetCommState)
 * 移植: Linux 替换为 termios/open/read/write, 接口保持一致
 *
 * 用于将 control frames 通过 USB-TTL 发送到 MCU。
 */

#pragma once
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
  #include <windows.h>
#else
  /* Linux: 用 int fd 替代 HANDLE */
  using HANDLE = int;
  #define INVALID_HANDLE_VALUE (-1)
#endif

class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    /* 禁止拷贝 */
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    /**
     * 打开串口
     * @param portName  "COM3" (Win) 或 "/dev/ttyUSB0" (Linux)
     * @param baudRate  默认 9600
     * @return 成功 true
     */
    bool open(const char* portName, uint32_t baudRate = 9600);

    /** 关闭串口 */
    void close();

    /** 是否已打开 */
    bool isOpen() const;

    /**
     * 写入数据
     * @return 实际写入字节数
     */
    uint32_t write(const uint8_t* data, uint32_t size);

    /** 写入 vector */
    uint32_t write(const std::vector<uint8_t>& data);

    /**
     * 读取数据 (非阻塞, 超时 100ms)
     * @return 实际读取字节数
     */
    uint32_t read(uint8_t* buffer, uint32_t size);

    /**
     * 等待并读取 MCU 的 ACK (2 字节: 0xCC + seq)
     * @param timeoutMs  超时毫秒
     * @return ACK 序号, 超时返回 -1
     */
    int waitForAck(uint32_t timeoutMs = 5000);

private:
    HANDLE hComm_;
    bool isOpen_;
};
