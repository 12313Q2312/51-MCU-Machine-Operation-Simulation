#include "executor.h"
#include <deque>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
extern "C" {
#include "protocol.h"
}

namespace control {
namespace {
class FirmwareSimulator final : public Transport {
	std::deque<uint8_t> input;
	uint8_t sequence = 0;
public:
	bool resetInput() override { input.clear(); return true; }
	size_t write(std::span<const uint8_t> bytes) override {
		FrameData data{};
		if (bytes.size() != 10 || !protocol_decode(bytes.data(), &data)) return 0;
		input.push_back(0xCC); input.push_back(++sequence);
		return bytes.size();
	}
	int readByte() override {
		if (input.empty()) return -1;
		int result = input.front(); input.pop_front(); return result;
	}
};
#ifdef _WIN32
class WindowsSerial final : public Transport {
	HANDLE handle = INVALID_HANDLE_VALUE;
public:
	explicit WindowsSerial(const std::string& port) {
		if (port.size() < 4 || port.size() > 8 || port.substr(0, 3) != "COM" || port[3] == '0' ||
			port.find_first_not_of("0123456789", 3) != std::string::npos)
			throw Error("串口名应为 COM3、COM10 等");
		handle = CreateFileA(("\\\\.\\" + port).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (handle == INVALID_HANDLE_VALUE) throw Error("无法打开指定串口，请检查占用和接线");
		DCB config{}; config.DCBlength = sizeof(config);
		COMMTIMEOUTS timeouts{};
		timeouts.ReadIntervalTimeout = MAXDWORD; // Return immediately, deadline is owned by executor.
		timeouts.WriteTotalTimeoutConstant = 100;
		bool ok = GetCommState(handle, &config) != 0;
		config.BaudRate = CBR_9600; config.ByteSize = 8; config.Parity = NOPARITY; config.StopBits = ONESTOPBIT;
		config.fBinary = TRUE; config.fParity = FALSE;
		config.fOutxCtsFlow = FALSE; config.fOutxDsrFlow = FALSE;
		config.fDtrControl = DTR_CONTROL_DISABLE; config.fRtsControl = RTS_CONTROL_DISABLE;
		config.fDsrSensitivity = FALSE; config.fOutX = FALSE; config.fInX = FALSE;
		config.fErrorChar = FALSE; config.fNull = FALSE; config.fAbortOnError = FALSE;
		ok = ok && SetCommState(handle, &config) && SetCommTimeouts(handle, &timeouts);
		if (!ok) { CloseHandle(handle); handle = INVALID_HANDLE_VALUE; throw Error("串口配置失败"); }
	}
	~WindowsSerial() override { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
	bool resetInput() override { return PurgeComm(handle, PURGE_RXCLEAR) != 0; }
	size_t write(std::span<const uint8_t> bytes) override {
		DWORD written = 0;
		return WriteFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) ? written : 0;
	}
	int readByte() override {
		uint8_t byte = 0; DWORD read = 0;
		if (!ReadFile(handle, &byte, 1, &read, nullptr)) throw Error("串口读取失败，已停止");
		return read == 1 ? byte : -1;
	}
};
#endif
}
std::unique_ptr<Transport> makeFirmwareSimulator() { return std::make_unique<FirmwareSimulator>(); }
std::unique_ptr<Transport> openSerial(const std::string& port) {
#ifdef _WIN32
	return std::make_unique<WindowsSerial>(port);
#else
	(void)port;
	throw Error("真实串口执行目前支持 Windows；本平台可运行预览和 --simulate");
#endif
}
}
