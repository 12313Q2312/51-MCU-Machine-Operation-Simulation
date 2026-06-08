/**
 * DSAI.h — DeepSeek AI 路径规划 + A* 降级
 *
 * 两层级联:
 *   1. 首选 DeepSeek API (需 DEEPSEEK_API_KEY 环境变量)
 *   2. 降级为本地 A* 搜索 (无网络依赖)
 *
 * 可移植性: 本模块运行在 PC 控制层，不依赖 MCU 硬件。
 *   Windows: WinHTTP 实现 HTTP 请求
 *   Linux:   需替换为 libcurl (见代码中 USE_LIBCURL 宏)
 */

#pragma once
#include <vector>
#include <string>
#include <optional>
#include "Point.h"
#include "motion.h"

/* 路径规划请求 */
struct PathRequest {
    Point start;
    Point end;
    /* 未来扩展: 障碍物列表 */
    /* std::vector<Point> obstacles; */
};

/* 路径规划结果 */
struct PathResult {
    std::vector<Point> waypoints;
    MotionType motionType;
    std::string reasoning;   /* AI 决策理由 (仅 DeepSeek 模式) */
    bool fromAI;             /* true=DeepSeek, false=本地算法 */
};

/**
 * 主入口: 根据请求生成路径。
 * 自动尝试 DeepSeek API，失败则降级为 A* + 运动模式选择。
 */
PathResult generatePathWithAI(const PathRequest& req);

/**
 * A* 搜索: 8 方向栅格, 100x100, 分辨率 0.1
 */
std::vector<Point> astarSearch(const PathRequest& req);

/**
 * 简易 JSON 提取: 从 DeepSeek 返回 content 中解析 waypoints
 */
std::optional<PathResult> parseDeepSeekResponse(const std::string& json);
