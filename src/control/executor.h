#pragma once
#include "plan.h"
#include <array>
#include <memory>
#include <span>

namespace control {
struct ScheduledFrame {
	uint32_t timeMs;
	std::array<uint8_t, 10> bytes;
};
std::vector<ScheduledFrame> encodePlan(const std::vector<TimedPoint>& points);
class Transport {
public:
	virtual ~Transport() = default;
	virtual bool resetInput() = 0;
	virtual size_t write(std::span<const uint8_t> bytes) = 0;
	virtual int readByte() = 0; // Nonblocking; -1 means no byte available.
};
class Clock {
public:
	virtual ~Clock() = default;
	virtual uint64_t nowMs() = 0;
	virtual void sleepMs(uint32_t ms) = 0;
	virtual bool cancelled() = 0;
};
struct ExecutionReport {
	std::string status, detail;
	size_t framesWritten = 0, framesAcknowledged = 0, bytesWritten = 0;
	uint64_t elapsedMs = 0;
};
ExecutionReport executeFrames(const std::vector<ScheduledFrame>& frames, Transport& transport, Clock& clock,
	uint32_t ackTimeoutMs = 1000);
std::unique_ptr<Transport> openSerial(const std::string& port);
std::unique_ptr<Transport> makeFirmwareSimulator();
}
