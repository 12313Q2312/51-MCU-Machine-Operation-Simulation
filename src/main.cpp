/**
 * main.cpp — 机器运动模拟控制层主程序
 *
 * 流程:
 *   1. 读取 MATLAB 生成的 trajectory.txt
 *   2. DeepSeek AI 路径规划 (降级: fakeAI + A*)
 *   3. 轨迹生成 (直线/正弦/圆弧)
 *   4. 协议编码 (protocol)
 *   5. 串口发送至 MCU
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "Point.h"
#include "motion.h"
#include "AI.h"
#include "DSAI.h"
#include "protocol.h"
#include <windows.h>
#include "serial.h"
#include <stdlib.h>

using namespace std;

int main() {
    // ???????? UTF-8??? Windows ?????
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    // ===== 1. 读取 MATLAB 状态向量 =====
    // 优先当前目录, 其次 matlab/ (CMake 未复制时)
    ifstream fin("trajectory.txt");
    if (!fin.is_open()) {
        fin.open("matlab/trajectory.txt");
    }
    if (!fin.is_open()) {
        cout << "[ERROR] 无法打开 trajectory.txt" << endl;
        cout << "请确保 MATLAB 已运行 day2_state_generation.m 生成数据文件" << endl;
        cout << "或手动复制 matlab/trajectory.txt 到当前目录" << endl;
        return -1;
    }

    vector<Point> trajectory0;
    Point a;
    while (fin >> a.x >> a.y >> a.vx >> a.vy) {
        trajectory0.push_back(a);
    }
    fin.close();

    cout << "=== MATLAB 状态向量 (" << trajectory0.size() << " points) ===" << endl;
    for (auto& i : trajectory0) {
        cout << "  (" << i.x << ", " << i.y << ", " << i.vx << ", " << i.vy << ")" << endl;
    }
    cout << endl;

    // ===== 2. 路径规划 =====
    cout << "=== 路径规划 ===" << endl;

    PathRequest req;
    req.start = {1.0, 1.0, 1.0, 0.0};
    req.end = {7.0, 5.0, 0.0, 0.0};

    PathResult plan = generatePathWithAI(req);
    cout << "Motion: " << (plan.motionType == line ? "LINE" :
                           plan.motionType == Sin ? "SIN" : "CIRCLE") << endl;
    cout << "Source: " << (plan.fromAI ? "DeepSeek" : "本地降级") << endl;
    cout << "Reason: " << plan.reasoning << endl << endl;

    // ===== 3. 轨迹生成 =====
    cout << "=== 轨迹生成 ===" << endl;
    vector<Point> trajectory;

    if (plan.motionType == line) {
        trajectory = generateLine(req.start, req.end, 0.2);
    } else if (plan.motionType == Sin) {
        trajectory = generateSin(req.start, req.end, 0.2);
    } else if (plan.motionType == circle) {
        trajectory = generateCircle(req.start, req.end, 0.2);
    }

    cout << "轨迹点数: " << trajectory.size() << endl;
    if (trajectory.empty()) {
        cout << "[WARNING] 轨迹为空!" << endl;
        return -1;
    }
    for (size_t i = 0; i < trajectory.size(); i++) {
        auto& p = trajectory[i];
        cout << "  [" << i << "] pos=(" << p.x << ", " << p.y
             << ") vel=(" << p.vx << ", " << p.vy << ")" << endl;
    }
    cout << endl;

    // ===== 4. 协议编码 =====
    cout << "=== 协议编码 ===" << endl;
    auto frames = encodeFrames(trajectory, plan.motionType);
    cout << "帧数: " << trajectory.size()
         << ", 总字节: " << frames.size() << endl << endl;

    // ===== 5. 串口发送 (可选, 需要硬件连接) =====
    const char* port = getenv("MCU_SERIAL_PORT");
    if (port && strlen(port) > 0) {
        cout << "=== 串口发送 → " << port << " ===" << endl;
        SerialPort sp;
        if (sp.open(port, 9600)) {
            uint32_t sent = sp.write(frames);
            cout << "已发送: " << sent << " bytes" << endl;

            // 等待 MCU ACK
            int ack = sp.waitForAck(5000);
            if (ack >= 0) {
                cout << "MCU ACK: seq=" << ack << endl;
            } else {
                cout << "[WARN] MCU ACK 超时" << endl;
            }
            sp.close();
        } else {
            cerr << "[ERROR] 无法打开串口 " << port << endl;
        }
    } else {
        cout << "[INFO] 未设置 MCU_SERIAL_PORT, 跳过串口发送" << endl;
        cout << "用法: set MCU_SERIAL_PORT=COM3 && TrajectorySystem.exe" << endl;
    }

    cout << endl << "Done. Press Enter to exit..." << endl;
    cin.get();
    system("pause");
    
    return 0;
}
