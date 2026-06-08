#include <cassert>
#include <cstdlib>
#include <iostream>
#include "../src/Point.h"
#include "../src/motion.h"

int main() {
	Point start = {1.0, 1.0, 0.0, 0.0};
	Point end = {7.0, 5.0, 0.0, 0.0};
	double step = 0.2;

	// 测 generateLine
	{
		auto path = generateLine(start, end, step);
		assert(!path.empty());
		assert(path.size() >= 2);
		assert(std::abs(path.front().x - start.x) < 1e-9);
		assert(std::abs(path.front().y - start.y) < 1e-9);
		assert(std::abs(path.back().x - end.x) < 1e-9);
		assert(std::abs(path.back().y - end.y) < 1e-9);
		std::cout << "[PASS] generateLine: " << path.size() << " points" << std::endl;
	}

	// 测 generateSin
	{
		auto path = generateSin(start, end, step);
		assert(!path.empty());
		assert(path.size() >= 2);
		assert(std::abs(path.front().x - start.x) < 1e-9);
		assert(std::abs(path.back().x - end.x) < 1e-9);
		assert(std::abs(path.back().y - end.y) < 1e-9);
		std::cout << "[PASS] generateSin: " << path.size() << " points" << std::endl;
	}

	// 测 generateCircle
	{
		auto path = generateCircle(start, end, step);
		assert(!path.empty());
		assert(path.size() >= 2);
		assert(std::abs(path.front().x - start.x) < 1e-9);
		assert(std::abs(path.back().x - end.x) < 1e-9);
		assert(std::abs(path.back().y - end.y) < 1e-9);
		std::cout << "[PASS] generateCircle: " << path.size() << " points" << std::endl;
	}

	// 测边界: start == end
	{
		auto path = generateLine(start, start, step);
		assert(!path.empty());
		assert(path.size() == 1);
		std::cout << "[PASS] generateLine (zero length): " << path.size() << " point" << std::endl;
	}

	std::cout << "\nAll motion tests passed!" << std::endl;
	return 0;
}
