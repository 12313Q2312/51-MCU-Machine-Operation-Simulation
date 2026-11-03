#include "executor.h"
#include <cmath>

namespace control {
namespace {
bool validFrame(const ScheduledFrame& f) {
	const auto& b = f.bytes;
	uint8_t checksum = 0;
	for (size_t i = 0; i < 9; ++i) checksum ^= b[i];
	return b[0] == 0xAA && b[1] <= 1 && b[6] == 0 && b[7] == 0 && b[8] == 0 && checksum == b[9];
}
void validateSchedule(const std::vector<ScheduledFrame>& frames) {
	if (frames.empty() || frames.size() > 4096 || frames.back().timeMs > 60000) throw Error("无效的执行时间表");
	for (size_t i = 0; i < frames.size(); ++i)
		if (frames[i].timeMs != i * tickMs || !validFrame(frames[i])) throw Error("执行帧或节拍无效");
}
}
std::vector<ScheduledFrame> encodePlan(const std::vector<TimedPoint>& points) {
	if (points.empty() || points.size() > 4096) throw Error("轨迹点数量无效");
	std::vector<ScheduledFrame> frames;
	frames.reserve(points.size());
	for (const auto& point : points) {
		auto p = point.position;
		if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.x < 0 || p.x > 7 || p.y < 0 || p.y > 7 || point.motionType > 1)
			throw Error("无法编码越界坐标或未知动作");
		auto x = static_cast<uint16_t>(std::lround(p.x / 7 * 65535));
		auto y = static_cast<uint16_t>(std::lround(p.y / 7 * 65535));
		ScheduledFrame f{point.timeMs, {0xAA, point.motionType, static_cast<uint8_t>(x >> 8), static_cast<uint8_t>(x),
			static_cast<uint8_t>(y >> 8), static_cast<uint8_t>(y), 0, 0, 0, 0}};
		for (size_t i = 0; i < 9; ++i) f.bytes[9] ^= f.bytes[i];
		frames.push_back(f);
	}
	validateSchedule(frames);
	return frames;
}

ExecutionReport executeFrames(const std::vector<ScheduledFrame>& frames, Transport& transport, Clock& clock, uint32_t ackTimeoutMs) {
	validateSchedule(frames); // Complete validation before the first transport operation.
	if (ackTimeoutMs == 0 || ackTimeoutMs > 10000) throw Error("ACK 超时参数无效");
	ExecutionReport report;
	const auto start = clock.nowMs();
	auto finish = [&](const char* status, const char* detail) {
		report.status = status; report.detail = detail;
		report.elapsedMs = clock.nowMs() - start;
		return report;
	};
	try {
	if (clock.cancelled()) return finish("cancelled", "已取消，未发送");
	if (!transport.resetInput()) return finish("io_error", "清理接收缓冲失败");
	int previous = -1;
	for (const auto& frame : frames) {
		const auto due = start + frame.timeMs;
		while (clock.nowMs() < due) {
			if (clock.cancelled()) return finish("cancelled", "停止后续发送；LED 可能保持最后位置");
			clock.sleepMs(1);
		}
		if (clock.cancelled()) return finish("cancelled", "停止后续发送；LED 可能保持最后位置");
		if (clock.nowMs() - due > tickMs) return finish("overrun", "执行落后一拍以上，已停止且不追赶发送");
		const auto written = transport.write(frame.bytes);
		report.bytesWritten += written;
		if (written != frame.bytes.size()) return finish("io_error", "串口写入失败或只写入部分帧，需复位后重试");
		++report.framesWritten;
		const auto deadline = clock.nowMs() + ackTimeoutMs;
		bool header = false, acknowledged = false;
		while (clock.nowMs() < deadline) {
			if (clock.cancelled()) return finish("cancelled", "已停止等待和后续发送");
			int byte = transport.readByte();
			if (byte < 0) { clock.sleepMs(1); continue; }
			if (!header) { header = byte == 0xCC; }
			else {
				if (previous >= 0 && byte != ((previous + 1) & 0xFF)) return finish("ack_sequence_error", "ACK 计数不连续，需复位开发板");
				previous = byte; acknowledged = true; ++report.framesAcknowledged; break;
			}
			// A continuously noisy port must still advance the real deadline.
			clock.sleepMs(1);
		}
		if (!acknowledged) return finish("ack_timeout", "等待 ACK 超时；未自动重发，需复位后重试");
		if (clock.nowMs() - due > tickMs) return finish("overrun", "ACK 延迟超过一拍，已停止");
	}
	return finish("completed", "所有帧收到 ACK；不代表物理位置反馈验证");
	} catch (const std::exception&) {
		return finish("io_error", "设备操作异常，已停止；需复位后重试");
	}
}
}
