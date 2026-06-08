#pragma once
#include<vector>
#include"Point.h"

//line motion
std::vector<Point> generateLine(Point start,Point end,double step);
//sin motion
std::vector<Point> generateSin(Point start, Point end, double step);
//circle motion
std::vector<Point> generateCircle(Point start, Point end, double step);

//状态枚举
enum MotionType {
	line,
	circle,
	Sin
};