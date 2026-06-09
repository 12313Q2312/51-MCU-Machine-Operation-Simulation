/**
 * DSAI.cpp — DeepSeek AI + A* 路径规划实现
 *
 * 平台: Windows (WinHTTP) / Linux (libcurl 可选)
 * 编译器: MSVC / GCC / Clang, C++20
 */
#include "DSAI.h"
#include "AI.h"         /* fakeAI 降级 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

/* ---- Forward declarations ---- */
static std::optional<std::string> queryDeepSeek(const PathRequest& req);
static MotionType chooseMotionType(double dist);

/* ================================================================
 * 主入口
 * ================================================================ */
PathResult generatePathWithAI(const PathRequest& req) {
    /* 第一选择: DeepSeek API */
    auto raw = queryDeepSeek(req);
    if (raw.has_value()) {
        auto parsed = parseDeepSeekResponse(raw.value());
        if (parsed.has_value()) {
            return parsed.value();
        }
    }

    /* 降级: 本地 A* + 运动模式选择 */
    std::cerr << "[WARN] DeepSeek failed or returned invalid, "
              << "falling back to A* + fakeAI" << std::endl;

    double dx = req.end.x - req.start.x;
    double dy = req.end.y - req.start.y;
    double dist = std::sqrt(dx * dx + dy * dy);

    PathResult result;
    result.waypoints = astarSearch(req);
    result.motionType = chooseMotionType(dist);
    result.reasoning = "本地 A* 降级 (DeepSeek 不可用)";
    result.fromAI = false;
    return result;
}

/* ================================================================
 * A* 搜索: 100x100 栅格, 8 方向, 分辨率 0.1
 *
 * 节点池使用预分配数组 + 索引, 避免 vector 扩容导致指针悬空。
 * 安全上限: GRID_SIZE*GRID_SIZE + 8*GRID_SIZE (每格最多 8 方向推入)
 * ================================================================ */
static const double GRID_RES = 0.1;
static const int GRID_SIZE = 100;
static const double SQRT2 = 1.414213562;
static const size_t MAX_NODES = GRID_SIZE * GRID_SIZE * 9;  /* 安全上限 */

struct AStarNode {
    int x, y;
    double g, h;
    size_t parent;  /* SIZE_MAX = 无父节点 */

    double f() const { return g + h; }
};

struct CompareNode {
    bool operator()(size_t a, size_t b) const {
        return (*pool)[a].f() > (*pool)[b].f();
    }
    const std::vector<AStarNode>* pool;
};

/* 将坐标哈希为字符串键 */
static std::string gridKey(int x, int y) {
    return std::to_string(x) + "," + std::to_string(y);
}

/* 将世界坐标映射到栅格索引 */
static int worldToGrid(double val) {
    int g = static_cast<int>(std::round(val / GRID_RES));
    return std::max(0, std::min(GRID_SIZE - 1, g));
}

static double gridToWorld(int g) {
    return g * GRID_RES;
}

/* 8 方向偏移 */
static const int DX[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
static const int DY[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

std::vector<Point> astarSearch(const PathRequest& req) {
    int sx = worldToGrid(req.start.x);
    int sy = worldToGrid(req.start.y);
    int ex = worldToGrid(req.end.x);
    int ey = worldToGrid(req.end.y);

    /* 已访问集 */
    std::unordered_set<std::string> closed;

    /* 节点池: resize 预分配, 永不扩容, 索引稳定 */
    std::vector<AStarNode> pool;
    pool.resize(MAX_NODES);
    size_t nodeCount = 0;

    auto makeNode = [&](int x, int y, double g, double h, size_t parentIdx) -> size_t {
        if (nodeCount >= MAX_NODES) {
            std::cerr << "[ERROR] A* node pool exhausted!" << std::endl;
            return SIZE_MAX;
        }
        pool[nodeCount] = {x, y, g, h, parentIdx};
        return nodeCount++;
    };

    CompareNode cmp;
    cmp.pool = &pool;
    std::priority_queue<size_t, std::vector<size_t>, CompareNode> open(cmp);

    double h0 = std::sqrt((double)(ex - sx) * (ex - sx) + (ey - sy) * (ey - sy));
    size_t startIdx = makeNode(sx, sy, 0.0, h0, SIZE_MAX);
    if (startIdx == SIZE_MAX) return {};
    open.push(startIdx);

    size_t goalIdx = SIZE_MAX;

    while (!open.empty()) {
        size_t curIdx = open.top();
        open.pop();
        AStarNode& cur = pool[curIdx];

        if (cur.x == ex && cur.y == ey) {
            goalIdx = curIdx;
            break;
        }

        std::string key = gridKey(cur.x, cur.y);
        if (closed.count(key)) continue;
        closed.insert(key);

        for (int d = 0; d < 8; d++) {
            int nx = cur.x + DX[d];
            int ny = cur.y + DY[d];

            if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE) continue;
            if (closed.count(gridKey(nx, ny))) continue;

            double cost = (d % 2 == 0) ? 1.0 : SQRT2;
            double ng = cur.g + cost;
            double nh = std::sqrt((double)(ex - nx) * (ex - nx) + (ey - ny) * (ey - ny));

            size_t nIdx = makeNode(nx, ny, ng, nh, curIdx);
            if (nIdx == SIZE_MAX) continue;
            open.push(nIdx);
        }
    }

    /* 回溯路径 */
    std::vector<Point> waypoints;
    if (goalIdx != SIZE_MAX) {
        std::vector<size_t> reversed;
        for (size_t n = goalIdx; n != SIZE_MAX; n = pool[n].parent) {
            reversed.push_back(n);
        }
        for (auto it = reversed.rbegin(); it != reversed.rend(); ++it) {
            Point p;
            p.x = gridToWorld(pool[*it].x);
            p.y = gridToWorld(pool[*it].y);
            p.vx = 0.0;
            p.vy = 0.0;
            waypoints.push_back(p);
        }
    } else {
        /* A* 失败: 返回线性插值兜底 */
        std::cerr << "[WARN] A* no path found, using linear fallback" << std::endl;
        Point p;
        p.x = req.start.x; p.y = req.start.y; p.vx = 0.0; p.vy = 0.0;
        waypoints.push_back(p);
        p.x = req.end.x; p.y = req.end.y;
        waypoints.push_back(p);
    }

    return waypoints;
}

/* ================================================================
 * 简易 JSON 解析器 (递归下降, 约 80 行)
 *
 * 目标: 从 DeepSeek 返回的 content 字符串中提取 waypoints。
 * 格式: {"waypoints": [{"x":1.0,"y":2.0}, ...], "motionType": "line"}
 * ================================================================ */
namespace {

class JsonParser {
public:
    explicit JsonParser(const std::string& s) : s_(s), pos_(0) {}

    std::optional<PathResult> parse() {
        skipWs();
        if (!expect('{')) return std::nullopt;

        PathResult result;
        result.fromAI = true;
        bool foundContent = false;

        while (pos_ < s_.size()) {
            skipWs();
            std::string key = parseString();
            if (key.empty()) break;
            skipWs();
            if (!expect(':')) break;

            if (key == "content") {
                std::string inner = parseString();
                JsonParser innerParser(inner);
                auto innerResult = innerParser.parse();
                if (innerResult.has_value()) {
                    return innerResult;
                }
                foundContent = true;
            } else if (key == "waypoints") {
                result.waypoints = parseWaypoints();
            } else if (key == "motionType") {
                std::string mt = parseString();
                if (mt == "line" || mt == "Line") result.motionType = line;
                else if (mt == "circle" || mt == "Circle") result.motionType = circle;
                else if (mt == "sine" || mt == "Sin" || mt == "sin") result.motionType = Sin;
                else result.motionType = line;
            } else if (key == "reasoning") {
                result.reasoning = parseString();
            } else {
                skipValue();
            }

            skipWs();
            if (pos_ < s_.size() && s_[pos_] == ',') pos_++;
        }

        if (!result.waypoints.empty() || foundContent) {
            return result;
        }
        return std::nullopt;
    }

private:
    void skipWs() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' ||
               s_[pos_] == '\n' || s_[pos_] == '\r')) {
            pos_++;
        }
    }

    bool expect(char c) {
        skipWs();
        if (pos_ < s_.size() && s_[pos_] == c) { pos_++; return true; }
        return false;
    }

    std::string parseString() {
        skipWs();
        if (pos_ >= s_.size() || s_[pos_] != '"') return "";
        pos_++;
        std::string result;
        while (pos_ < s_.size() && s_[pos_] != '"') {
            if (s_[pos_] == '\\' && pos_ + 1 < s_.size()) {
                pos_++;
                switch (s_[pos_]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    default: result += s_[pos_]; break;
                }
            } else {
                result += s_[pos_];
            }
            pos_++;
        }
        if (pos_ < s_.size()) pos_++;
        return result;
    }

    double parseNumber() {
        skipWs();
        size_t start = pos_;
        if (pos_ < s_.size() && s_[pos_] == '-') pos_++;
        while (pos_ < s_.size() && (std::isdigit(s_[pos_]) || s_[pos_] == '.')) pos_++;
        return std::stod(s_.substr(start, pos_ - start));
    }

    void skipValue() {
        skipWs();
        if (pos_ >= s_.size()) return;
        char c = s_[pos_];
        if (c == '"') { parseString(); }
        else if (c == '{') { pos_++; int d = 1; while (pos_ < s_.size() && d > 0) { if (s_[pos_]=='{') d++; if (s_[pos_]=='}') d--; pos_++; } }
        else if (c == '[') { pos_++; int d = 1; while (pos_ < s_.size() && d > 0) { if (s_[pos_]=='[') d++; if (s_[pos_]==']') d--; pos_++; } }
        else { while (pos_ < s_.size() && s_[pos_] != ',' && s_[pos_] != '}' && s_[pos_] != ']') pos_++; }
    }

    std::vector<Point> parseWaypoints() {
        std::vector<Point> pts;
        skipWs();
        if (!expect('[')) return pts;

        while (pos_ < s_.size()) {
            skipWs();
            if (s_[pos_] == ']') { pos_++; break; }

            if (!expect('{')) break;

            Point p{};
            while (pos_ < s_.size()) {
                skipWs();
                std::string key = parseString();
                if (key.empty()) break;
                skipWs();
                if (!expect(':')) break;
                if (key == "x") p.x = parseNumber();
                else if (key == "y") p.y = parseNumber();
                else if (key == "vx") p.vx = parseNumber();
                else if (key == "vy") p.vy = parseNumber();
                else skipValue();
                skipWs();
                if (pos_ < s_.size() && s_[pos_] == ',') pos_++;
            }
            pts.push_back(p);
            skipWs();
            if (pos_ < s_.size() && s_[pos_] == '}') pos_++;
            skipWs();
            if (pos_ < s_.size() && s_[pos_] == ',') pos_++;
        }
        return pts;
    }

    const std::string& s_;
    size_t pos_;
};

} /* anonymous namespace */

std::optional<PathResult> parseDeepSeekResponse(const std::string& json) {
    JsonParser parser(json);
    return parser.parse();
}

/* ================================================================
 * DeepSeek API 调用 (Windows: WinHTTP)
 *
 * 移植到 Linux: 将 queryDeepSeek 替换为 libcurl 实现即可。
 * 条件编译: #ifdef _WIN32 → WinHTTP / #else → libcurl
 * ================================================================ */
static MotionType chooseMotionType(double dist) {
    return fakeAI({0,0,0,0}, {dist, 0, 0, 0});
}

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

static std::optional<std::string> queryDeepSeek(const PathRequest& req) {
    /* ---- 1. ?? API Key (???? DEEPSEEK_API_KEY) ---- */
    const char* apiKey = std::getenv("DEEPSEEK_API_KEY");
    if (!apiKey || std::strlen(apiKey) == 0) {
        std::cerr << "[INFO] DEEPSEEK_API_KEY not set, skipping API call" << std::endl;
        std::cerr << "[INFO] ????: set DEEPSEEK_API_KEY=sk-xxxx   (CMD)" << std::endl;
        std::cerr << "[INFO] ????: ='sk-xxxx' (PowerShell)" << std::endl;
        return std::nullopt;
    }

    /* ---- 2. ?? JSON ??? (???? stream:false) ---- */
    std::ostringstream body;
    body << "{"
         << "\"model\":\"deepseek-chat\","
         << "\"messages\":[{"
         << "\"role\":\"system\","
         << "\"content\":\"You are a path planner. Output JSON with waypoints array "
         << "(objects with x,y) and motionType (line/circle/sine). "
         << "No markdown, pure JSON only.\""
         << "},{"
         << "\"role\":\"user\","
         << "\"content\":\"Plan from (" << req.start.x << "," << req.start.y
         << ") to (" << req.end.x << "," << req.end.y << ").\""
         << "}],"
         << "\"stream\":false"
         << "}";

    std::string bodyStr = body.str();
    std::cout << "[DEBUG DSAI] Sending DeepSeek request (body="
              << bodyStr.size() << " bytes)..." << std::endl;

    /* ---- 3. ??? WinHTTP ?? ---- */
    HINTERNET hSession = WinHttpOpen(
        L"TrajectorySystem/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) {
        std::cerr << "[ERROR] WinHttpOpen failed: " << GetLastError() << std::endl;
        return std::nullopt;
    }

    /* ---- 4. ??? api.deepseek.com:443 ---- */
    HINTERNET hConnect = WinHttpConnect(hSession,
        L"api.deepseek.com", 443, 0);
    if (!hConnect) {
        std::cerr << "[ERROR] WinHttpConnect failed: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }

    /* ---- 5. ?? HTTPS POST ?? ---- */
    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
        L"POST", L"/chat/completions",
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    if (!hRequest) {
        std::cerr << "[ERROR] WinHttpOpenRequest failed: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }

    /* ---- 6. SSL ????? (?????/????, ???? Win ?? TLS ??) ---- */
    DWORD securityFlags = 0;
    DWORD dwSize = sizeof(securityFlags);
    WinHttpQueryOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                       &securityFlags, &dwSize);
    securityFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA
                   | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
                   | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                     &securityFlags, sizeof(securityFlags));

    /* ---- 7. ????? (Content-Type + Authorization) ---- */
    std::wstring authHeader = L"Authorization: Bearer ";
    size_t keyLen = std::strlen(apiKey);
    for (size_t i = 0; i < keyLen; i++) {
        authHeader += (wchar_t)(unsigned char)apiKey[i];
    }

    /* WinHTTP ? header ? CRLF ?? */
    std::wstring allHeaders = L"Content-Type: application/json\r\n";
    allHeaders += authHeader + L"\r\n";

    if (!WinHttpAddRequestHeaders(hRequest, allHeaders.c_str(),
            (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD)) {
        std::cerr << "[ERROR] WinHttpAddRequestHeaders failed: "
                  << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }

    /* ---- 8. ???? ---- */
    BOOL ok = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)bodyStr.c_str(), (DWORD)bodyStr.size(),
        (DWORD)bodyStr.size(), 0);

    if (!ok) {
        std::cerr << "[ERROR] WinHttpSendRequest failed: "
                  << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }

    /* ---- 9. ???? ---- */
    ok = WinHttpReceiveResponse(hRequest, NULL);
    if (!ok) {
        std::cerr << "[ERROR] WinHttpReceiveResponse failed: "
                  << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }

    /* ---- 10. ?? HTTP ??? (??!) ---- */
    DWORD statusCode = 0;
    dwSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &dwSize,
        WINHTTP_NO_HEADER_INDEX);

    std::cout << "[DEBUG DSAI] HTTP status: " << statusCode << std::endl;

    /* ????? */
    std::string response;
    DWORD bytesRead = 0;
    char buffer[4096];
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead)
           && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }

    /* ? 200 ???: ??????????? */
    if (statusCode != 200) {
        std::cerr << "[ERROR] DeepSeek API returned HTTP " << statusCode << std::endl;
        if (!response.empty()) {
            std::cerr << "[ERROR] Response: " << response << std::endl;
        }
        if (statusCode == 401) {
            std::cerr << "[HINT]  API Key ??, ??? DEEPSEEK_API_KEY ????" << std::endl;
        } else if (statusCode == 429) {
            std::cerr << "[HINT]  ??????, ?????" << std::endl;
        } else if (statusCode == 402) {
            std::cerr << "[HINT]  ??????, ???" << std::endl;
        }
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }

    /* ---- 11. ?? WinHTTP ?? ---- */
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    std::cout << "[DEBUG DSAI] Response received, "
              << response.size() << " bytes" << std::endl;

    return response;
}


#else
/* Linux fallback: 返回 nullopt, 由 A* 接管 */
static std::optional<std::string> queryDeepSeek(const PathRequest& /*req*/) {
    std::cerr << "[INFO] DeepSeek API not available on this platform, "
              << "using local A*" << std::endl;
    return std::nullopt;
}
#endif /* _WIN32 */
