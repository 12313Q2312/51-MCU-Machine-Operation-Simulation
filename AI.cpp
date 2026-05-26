#include"motion.h"
#include"Point.h"
#include<cmath>
#include<iostream>

// 简易AI函数：根据距离选择运动模式
MotionType fakeAI(Point start, Point end) {
    double dx = end.x - start.x;
    double dy = end.y - start.y;

    double dist = sqrt(dx * dx + dy * dy);

    std::cout << "[DEBUG fakeAI] start=(" << start.x << "," << start.y
              << ") end=(" << end.x << "," << end.y
              << ") distance=" << dist << std::endl;

    if (dist < 3)
    {
        std::cout << "[DEBUG fakeAI] -> LINE" << std::endl;
        return line;
    }

    if (dist < 6)
    {
        std::cout << "[DEBUG fakeAI] -> SIN" << std::endl;
        return Sin;
    }

    std::cout << "[DEBUG fakeAI] -> CIRCLE" << std::endl;
    return circle;
}
