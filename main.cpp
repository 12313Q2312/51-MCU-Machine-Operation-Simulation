#include<iostream>
#include"Point.h"
#include<fstream>
#include<vector>
#include"AI.h"
#include"motion.h"
#include<cstdlib>

using namespace std;

int main() {
    // ===== 1. 读取 MATLAB 生成的状态向量 =====
    ifstream fin("trajectory.txt");
    if (fin.is_open()) {
        cout << "文件打开成功" << endl << endl;
    }
    else {
        cout << "文件打开失败" << endl;
        return -1;
    }

    vector<Point> trajectory0;
    Point a;
    while (fin >> a.x >> a.y >> a.vx >> a.vy) {
        trajectory0.push_back(a);
    }
    fin.close();

    // 测试数据是否传输正确
    cout << "=== MATLAB 状态向量 (" << trajectory0.size() << " points) ===" << endl;
    for (auto& i : trajectory0) {
        cout << "  (" << i.x << ", " << i.y << ", " << i.vx << ", " << i.vy << ")" << endl;
    }
    cout << endl;

    // ===== 2. 轨迹生成 =====
    cout << "=== 轨迹生成 ===" << endl;

    Point start;
    start.x = 1;
    start.y = 1;
    start.vx = 1;
    start.vy = 0;

    Point end;
    end.x = 7;
    end.y = 5;

    cout << "起点: (" << start.x << ", " << start.y << ")" << endl;
    cout << "终点: (" << end.x << ", " << end.y << ")" << endl << endl;

    vector<Point> trajectory;

    MotionType decision = fakeAI(start, end);

    if (decision == line)
    {
        cout << "AI choose LINE" << endl;
        trajectory = generateLine(start, end, 0.2);
    }
    else if (decision == Sin)
    {
        cout << "AI choose SIN" << endl;
        trajectory = generateSin(start, end, 0.2);
    }
    else if (decision == circle)
    {
        cout << "AI choose CIRCLE" << endl;
        trajectory = generateCircle(start, end, 0.2);
    }

    // ===== 3. 输出轨迹点 =====
    cout << endl;
    cout << "=== 轨迹点 (" << trajectory.size() << " points) ===" << endl;
    if (trajectory.empty()) {
        cout << "  [WARNING] trajectory is empty!" << endl;
    }
    else {
        for (size_t i = 0; i < trajectory.size(); i++) {
            auto& p = trajectory[i];
            cout << "  [" << i << "] "
                 << "pos=(" << p.x << ", " << p.y << ") "
                 << "vel=(" << p.vx << ", " << p.vy << ")"
                 << endl;
        }
    }

    cout << endl << "Done. Press Enter to exit..." << endl;
    cin.get();
    return 0;
}
