/**
 * serial.cpp — 串口通信实现 (Windows API)
 *
 * 移植到 Linux: 将 WIN32 段替换为:
 *   open(portName, O_RDWR | O_NOCTTY)
 *   tcgetattr / cfsetispeed / cfsetospeed / tcsetattr
 *   read / write
 */
#include "serial.h"
#include <iostream>
#include <cstring>

SerialPort::SerialPort() : hComm_(INVALID_HANDLE_VALUE), isOpen_(false) {}

SerialPort::~SerialPort() { close(); }

#ifdef _WIN32

bool SerialPort::open(const char* portName, uint32_t baudRate) {
    if (isOpen_) close();

    /* 构造 Windows 设备名: \\.\COM3 */
    std::string devicePath = "\\\\.\\";
    devicePath += portName;

    hComm_ = CreateFileA(
        devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,                    /* 独占访问 */
        NULL,
        OPEN_EXISTING,
        0,                    /* 同步模式 */
        NULL);

    if (hComm_ == INVALID_HANDLE_VALUE) {
        std::cerr << "[ERROR] SerialPort: cannot open " << portName
                  << " (error " << GetLastError() << ")" << std::endl;
        return false;
    }

    /* 配置串口参数 */
    DCB dcb = {};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(hComm_, &dcb)) {
        std::cerr << "[ERROR] SerialPort: GetCommState failed" << std::endl;
        CloseHandle(hComm_);
        hComm_ = INVALID_HANDLE_VALUE;
        return false;
    }

    dcb.BaudRate = baudRate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(hComm_, &dcb)) {
        std::cerr << "[ERROR] SerialPort: SetCommState failed" << std::endl;
        CloseHandle(hComm_);
        hComm_ = INVALID_HANDLE_VALUE;
        return false;
    }

    /* 超时设置 */
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(hComm_, &timeouts);

    /* 清空缓冲区 */
    PurgeComm(hComm_, PURGE_RXCLEAR | PURGE_TXCLEAR);

    isOpen_ = true;
    std::cout << "[INFO] SerialPort: " << portName << " opened at "
              << baudRate << " baud" << std::endl;
    return true;
}

void SerialPort::close() {
    if (hComm_ != INVALID_HANDLE_VALUE) {
        PurgeComm(hComm_, PURGE_RXCLEAR | PURGE_TXCLEAR);
        CloseHandle(hComm_);
        hComm_ = INVALID_HANDLE_VALUE;
    }
    isOpen_ = false;
}

bool SerialPort::isOpen() const { return isOpen_; }

uint32_t SerialPort::write(const uint8_t* data, uint32_t size) {
    if (!isOpen_) return 0;
    DWORD written = 0;
    if (!WriteFile(hComm_, data, size, &written, NULL)) {
        std::cerr << "[ERROR] SerialPort: write failed (error "
                  << GetLastError() << ")" << std::endl;
        return 0;
    }
    return written;
}

uint32_t SerialPort::write(const std::vector<uint8_t>& data) {
    return write(data.data(), static_cast<uint32_t>(data.size()));
}

uint32_t SerialPort::read(uint8_t* buffer, uint32_t size) {
    if (!isOpen_) return 0;
    DWORD bytesRead = 0;
    if (!ReadFile(hComm_, buffer, size, &bytesRead, NULL)) {
        return 0;
    }
    return bytesRead;
}

int SerialPort::waitForAck(uint32_t timeoutMs) {
    if (!isOpen_) return -1;

    /* 设置临时读超时 */
    COMMTIMEOUTS oldTimeouts, newTimeouts;
    GetCommTimeouts(hComm_, &oldTimeouts);
    newTimeouts = oldTimeouts;
    newTimeouts.ReadTotalTimeoutConstant = timeoutMs;
    SetCommTimeouts(hComm_, &newTimeouts);

    uint32_t elapsed = 0;

    while (elapsed < timeoutMs) {
        uint8_t byte;
        uint32_t n = read(&byte, 1);
        if (n == 0) {
            /* 短暂等待后重试 */
            Sleep(10);
            elapsed += 10;
            continue;
        }
        if (byte == 0xCC) {
            /* 读序列号 */
            uint8_t seqByte;
            uint32_t n = read(&seqByte, 1);
            if (n == 1) {
                SetCommTimeouts(hComm_, &oldTimeouts);
                return static_cast<int>(seqByte);
            }
        }
    }

    SetCommTimeouts(hComm_, &oldTimeouts);
    return -1;
}

#else
/* ---- Linux 桩 (用户自行实现) ---- */
bool SerialPort::open(const char* portName, uint32_t baudRate) {
    (void)portName; (void)baudRate;
    std::cerr << "[ERROR] SerialPort: Linux implementation not yet provided" << std::endl;
    return false;
}
void SerialPort::close() {}
bool SerialPort::isOpen() const { return false; }
uint32_t SerialPort::write(const uint8_t*, uint32_t) { return 0; }
uint32_t SerialPort::write(const std::vector<uint8_t>&) { return 0; }
uint32_t SerialPort::read(uint8_t*, uint32_t) { return 0; }
int SerialPort::waitForAck(uint32_t) { return -1; }
#endif
