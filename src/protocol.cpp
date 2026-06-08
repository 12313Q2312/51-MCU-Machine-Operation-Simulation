#include "protocol.h"
#include <algorithm>
#include <iostream>

uint16_t quantizeCoord(double val, double min, double max) {
	if (max - min < 1e-9) return 0;
	double clamped = std::max(min, std::min(max, val));
	double normalized = (clamped - min) / (max - min);
	return static_cast<uint16_t>(normalized * 65535.0);
}

uint8_t computeChecksum(const uint8_t* frame, size_t len) {
	uint8_t cs = 0;
	for (size_t i = 0; i < len - 1; i++) {
		cs ^= frame[i];
	}
	return cs;
}

std::vector<uint8_t> encodeFrames(const std::vector<Point>& trajectory,
                                   MotionType motionType) {
	if (trajectory.empty()) return {};

	// 自动检测坐标范围
	double minX = trajectory[0].x, maxX = trajectory[0].x;
	double minY = trajectory[0].y, maxY = trajectory[0].y;
	double minVx = trajectory[0].vx, maxVx = trajectory[0].vx;
	double minVy = trajectory[0].vy, maxVy = trajectory[0].vy;
	for (auto& p : trajectory) {
		minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
		minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
		minVx = std::min(minVx, p.vx); maxVx = std::max(maxVx, p.vx);
		minVy = std::min(minVy, p.vy); maxVy = std::max(maxVy, p.vy);
	}

	std::cout << "[DEBUG encodeFrames] bounds x=[" << minX << ", " << maxX
	          << "] y=[" << minY << ", " << maxY
	          << "] vx=[" << minVx << ", " << maxVx
	          << "] vy=[" << minVy << ", " << maxVy << "]" << std::endl;

	std::vector<uint8_t> frames;
	frames.reserve(trajectory.size() * FRAME_SIZE);

	for (auto& p : trajectory) {
		uint8_t frame[FRAME_SIZE];
		frame[0] = FRAME_HEADER;
		frame[1] = static_cast<uint8_t>(motionType);

		uint16_t qx = quantizeCoord(p.x, minX, maxX);
		uint16_t qy = quantizeCoord(p.y, minY, maxY);
		frame[2] = static_cast<uint8_t>((qx >> 8) & 0xFF);
		frame[3] = static_cast<uint8_t>(qx & 0xFF);
		frame[4] = static_cast<uint8_t>((qy >> 8) & 0xFF);
		frame[5] = static_cast<uint8_t>(qy & 0xFF);

		// 速度量化到 int8: 自动缩放到 [-127, 127]
		double vAbsMax = std::max({std::abs(minVx), std::abs(maxVx),
		                           std::abs(minVy), std::abs(maxVy), 1e-9});
		double vScale = 127.0 / vAbsMax;
		frame[6] = static_cast<uint8_t>(static_cast<int8_t>(
			std::max(-128.0, std::min(127.0, p.vx * vScale))));
		frame[7] = static_cast<uint8_t>(static_cast<int8_t>(
			std::max(-128.0, std::min(127.0, p.vy * vScale))));

		frame[8] = 0x00;

		frame[9] = computeChecksum(frame, FRAME_SIZE);

		frames.insert(frames.end(), frame, frame + FRAME_SIZE);
	}

	std::cout << "[DEBUG encodeFrames] total bytes=" << frames.size()
	          << " frames=" << trajectory.size() << std::endl;

	return frames;
}
