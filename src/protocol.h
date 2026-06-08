#pragma once
#include <vector>
#include <cstdint>
#include "Point.h"
#include "motion.h"

constexpr uint8_t FRAME_HEADER = 0xAA;
constexpr size_t FRAME_SIZE = 10;

// 将轨迹点序列编码为二进制帧
std::vector<uint8_t> encodeFrames(const std::vector<Point>& trajectory,
                                   MotionType motionType);

// 将浮点坐标量化到 uint16 (0-65535)
uint16_t quantizeCoord(double val, double min, double max);

// 计算 XOR 校验和 (对 frame[0..len-2] 异或, frame[len-1] 为校验位)
uint8_t computeChecksum(const uint8_t* frame, size_t len);
