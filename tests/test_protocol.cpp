#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "../src/Point.h"
#include "../src/motion.h"
#include "../src/protocol.h"

int main() {
	// 测 quantizeCoord
	{
		uint16_t v = quantizeCoord(0.0, 0.0, 10.0);
		assert(v == 0);

		v = quantizeCoord(10.0, 0.0, 10.0);
		assert(v == 65535);

		v = quantizeCoord(5.0, 0.0, 10.0);
		assert(v >= 32700 && v <= 32800);

		v = quantizeCoord(-1.0, 0.0, 10.0);  // clamp to min
		assert(v == 0);

		std::cout << "[PASS] quantizeCoord" << std::endl;
	}

	// 测 computeChecksum
	{
		uint8_t frame[4] = {0xAA, 0x01, 0x02, 0x00};
		frame[3] = computeChecksum(frame, 4);
		uint8_t expected = 0xAA ^ 0x01 ^ 0x02;
		assert(frame[3] == expected);
		std::cout << "[PASS] computeChecksum" << std::endl;
	}

	// 测 encodeFrames 空输入
	{
		std::vector<Point> empty;
		auto frames = encodeFrames(empty, line);
		assert(frames.empty());
		std::cout << "[PASS] encodeFrames (empty)" << std::endl;
	}

	// 测 encodeFrames 正常输入
	{
		std::vector<Point> pts;
		pts.push_back({1.0, 1.0, 0.5, 0.0});
		pts.push_back({7.0, 5.0, 0.5, 0.5});

		auto frames = encodeFrames(pts, circle);
		assert(frames.size() == pts.size() * FRAME_SIZE);
		assert(frames[0] == FRAME_HEADER);
		assert(frames[1] == static_cast<uint8_t>(circle));

		// 校验和验证: 每帧最后一字节等于前面所有字节 XOR
		for (size_t i = 0; i < pts.size(); i++) {
			size_t offset = i * FRAME_SIZE;
			uint8_t expectedCs = 0;
			for (size_t j = 0; j < FRAME_SIZE - 1; j++) {
				expectedCs ^= frames[offset + j];
			}
			assert(frames[offset + FRAME_SIZE - 1] == expectedCs);
		}
		std::cout << "[PASS] encodeFrames: " << pts.size()
		          << " frames, " << frames.size() << " bytes" << std::endl;
	}

	std::cout << "\nAll protocol tests passed!" << std::endl;
	return 0;
}
