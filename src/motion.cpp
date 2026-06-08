#include<vector>
#include<iostream>
#include<cmath>
#include"Point.h"

// ========== 直线运动 ==========
std::vector<Point> generateLine(Point start, Point end, double step) {
    std::vector<Point> path;

    double dx = end.x - start.x;
    double dy = end.y - start.y;
    double length = sqrt(dx * dx + dy * dy);

    std::cout << "[DEBUG generateLine] start=(" << start.x << "," << start.y
              << ") end=(" << end.x << "," << end.y
              << ") length=" << length << " step=" << step << std::endl;

    if (length < 1e-9) {
        start.vx = 0; start.vy = 0;
        path.push_back(start);
        std::cout << "[DEBUG generateLine] start==end, return 1 point" << std::endl;
        return path;
    }

    // 单位方向向量 (也作为速度方向)
    double vx = dx / length;
    double vy = dy / length;

    Point current = start;
    current.vx = vx;
    current.vy = vy;

    double traveled = 0.0;
    int iter = 0;
    while (traveled + step < length) {
        path.push_back(current);
        traveled += step;
        current.x = start.x + vx * traveled;
        current.y = start.y + vy * traveled;
        current.vx = vx;
        current.vy = vy;
        iter++;
    }

    // 压入终点
    end.vx = vx;
    end.vy = vy;
    path.push_back(end);

    std::cout << "[DEBUG generateLine] done, iterations=" << iter
              << " total_points=" << path.size() << std::endl;
    return path;
}

// ========== 正弦运动 ==========
std::vector<Point> generateSin(Point start, Point end, double step) {
    std::vector<Point> path;

    double dx = end.x - start.x;
    double dy = end.y - start.y;
    double length = sqrt(dx * dx + dy * dy);

    std::cout << "[DEBUG generateSin] start=(" << start.x << "," << start.y
              << ") end=(" << end.x << "," << end.y
              << ") length=" << length << " step=" << step << std::endl;

    if (length < 1e-9) {
        start.vx = 0; start.vy = 0;
        path.push_back(start);
        return path;
    }

    // 沿路径方向的单位向量
    double ux = dx / length;
    double uy = dy / length;
    // 垂直方向的单位向量
    double px = -uy;
    double py = ux;

    const double PI = 3.141592653589793;
    double amplitude = length * 0.3;          // 振幅 = 30% 路径长度
    double omega = 2.0 * PI / length * 3.0;   // 3 个完整正弦波

    double t = 0.0;
    int iter = 0;
    while (t < length) {
        Point p;
        double sin_val = sin(omega * t);
        double cos_val = cos(omega * t);

        p.x = start.x + ux * t + px * amplitude * sin_val;
        p.y = start.y + uy * t + py * amplitude * sin_val;

        // 速度 = 位置对 t 的导数
        p.vx = ux + px * amplitude * omega * cos_val;
        p.vy = uy + py * amplitude * omega * cos_val;

        path.push_back(p);
        t += step;
        iter++;
    }

    // 压入终点
    double cos_end = cos(omega * length);
    end.vx = ux + px * amplitude * omega * cos_end;
    end.vy = uy + py * amplitude * omega * cos_end;
    path.push_back(end);

    std::cout << "[DEBUG generateSin] done, iterations=" << iter
              << " total_points=" << path.size() << std::endl;
    return path;
}

// ========== 圆弧运动 (半圆弧) ==========
std::vector<Point> generateCircle(Point start, Point end, double step) {
    std::vector<Point> path;

    double dx = end.x - start.x;
    double dy = end.y - start.y;
    double length = sqrt(dx * dx + dy * dy);

    std::cout << "[DEBUG generateCircle] start=(" << start.x << "," << start.y
              << ") end=(" << end.x << "," << end.y
              << ") length=" << length << " step=" << step << std::endl;

    if (length < 1e-9) {
        start.vx = 0; start.vy = 0;
        path.push_back(start);
        return path;
    }

    // 圆心 = 起点和终点连线的中点
    double cx = (start.x + end.x) / 2.0;
    double cy = (start.y + end.y) / 2.0;
    double radius = length / 2.0;

    const double PI = 3.141592653589793;

    // 起点相对圆心的角度
    double start_angle = atan2(start.y - cy, start.x - cx);
    double angular_step = step / radius;     // 弧长 = 半径 * 角度
    double total_angle = PI;                  // 半圆弧

    double angle = start_angle;
    double end_angle = start_angle + total_angle;

    std::cout << "[DEBUG generateCircle] center=(" << cx << "," << cy
              << ") radius=" << radius
              << " start_angle=" << start_angle
              << " end_angle=" << end_angle
              << " angular_step=" << angular_step << std::endl;

    int iter = 0;
    while (angle < end_angle) {
        Point p;
        p.x = cx + radius * cos(angle);
        p.y = cy + radius * sin(angle);

        // 切线速度
        p.vx = -radius * sin(angle);
        p.vy = radius * cos(angle);

        path.push_back(p);
        angle += angular_step;
        iter++;

        // 安全阀: 防止意外死循环
        if (iter > 100000) {
            std::cerr << "[ERROR generateCircle] exceeded max iterations!" << std::endl;
            break;
        }
    }

    // 压入终点
    end.vx = -radius * sin(end_angle);
    end.vy = radius * cos(end_angle);
    path.push_back(end);

    std::cout << "[DEBUG generateCircle] done, iterations=" << iter
              << " total_points=" << path.size() << std::endl;
    return path;
}
