#include "ai.h"
#include "executor.h"
#include "json_io.h"
#include "preview.h"
#include "demo.h"
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unordered_map>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
namespace fs = std::filesystem;
volatile std::sig_atomic_t stopRequested = 0;
void onInterrupt(int) { stopRequested = 1; }
fs::path utf8Path(const std::string& text) { return fs::path(std::u8string(text.begin(), text.end())); }
class RunClock final : public control::Clock {
	bool virtualTime;
	uint64_t time = 0;
public:
	explicit RunClock(bool simulate) : virtualTime(simulate) {}
	uint64_t nowMs() override {
		return virtualTime ? time : static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count());
	}
	void sleepMs(uint32_t ms) override {
		if (virtualTime) time += ms;
		else std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}
	bool cancelled() override { return stopRequested != 0; }
};
std::string readFile(const std::string& path) {
	std::ifstream in(utf8Path(path), std::ios::binary);
	if (!in) throw control::Error("无法读取指定文件");
	std::string content(control::maxResponseBytes + 1, '\0');
	in.read(content.data(), static_cast<std::streamsize>(content.size()));
	content.resize(static_cast<size_t>(in.gcount()));
	if (in.bad() || content.size() > control::maxResponseBytes) throw control::Error("文件读取失败或超过 64KiB");
	return content;
}
template<class Writer> void saveFile(const fs::path& path, Writer write) {
	std::ofstream out(path, std::ios::binary);
	if (!out) throw control::Error("无法创建输出文件");
	write(out); out.close();
	if (!out) throw control::Error("输出文件写入失败");
}
const char* help = R"HELP(MotionControl — 中文指令控制 MCU 点阵运动

离线演示（固定样例，不调用 AI）：
  MotionControl --demo --simulate --output demo-run
读取计划文件并预览：
  MotionControl --plan plan.json --output my-preview
在线 AI 理解文字（Windows；需要本机 DEEPSEEK_API_KEY）：
  MotionControl --text "从左下角走到右上角，用三秒" --output ai-preview
连接开发板（Windows；预览后输入 EXECUTE 才打开端口）：
  MotionControl --text "画一个圆" --execute --port COM3 --output board-run

输入方式恰选一个：--demo、--plan FILE、--text TEXT、--provider-response FILE。
--provider-response 读取提供方响应的离线样例，不代表在线 AI 实测。
--model NAME 可指定 DeepSeek 模型，默认 deepseek-flash。
--simulate 使用真实固件解码函数及虚拟时钟，不连接硬件。
--execute 必须与 --port 同用，不能和 --simulate 同用。
--output 必须是尚不存在的目录，默认 motion-preview。
输出 plan.json、trajectory.csv、preview.svg、run.json。用浏览器打开 SVG 看动画。
真实执行前请复位并检查开发板，保持空队列；Ctrl+C 停止后续发送。
停止发送不是硬件急停，ACK 不代表物理位置反馈。机器人本版不支持。
)HELP";
int run(const std::vector<std::string>& args) {
	using namespace control;
	std::unordered_map<std::string, std::string> options;
	for (size_t i = 1; i < args.size(); ++i) {
		const auto& key = args[i];
		if (key == "--help" && args.size() == 2) { std::cout << help; return 0; }
		bool flag = key == "--demo" || key == "--simulate" || key == "--execute";
		bool value = key == "--plan" || key == "--text" || key == "--provider-response" || key == "--port" || key == "--output" || key == "--model";
		if ((!flag && !value) || options.contains(key)) throw Error("未知或重复的参数，请查看 --help");
		if (value && (i + 1 == args.size() || args[i + 1].starts_with("--"))) throw Error("参数缺少值");
		options.emplace(key, flag ? "" : args[++i]);
	}
	int inputs = options.contains("--demo") + options.contains("--plan") + options.contains("--text") + options.contains("--provider-response");
	if (inputs != 1) { std::cout << help; throw Error("请选择一种输入方式"); }
	bool execute = options.contains("--execute"), simulate = options.contains("--simulate");
	if (execute != options.contains("--port") || (execute && simulate)) throw Error("--execute 必须与 --port 同用，且不能与 --simulate 同用");
	if (options.contains("--model") && !options.contains("--text")) throw Error("--model 仅用于在线文字输入");
	fs::path output = utf8Path(options.contains("--output") ? options.at("--output") : "motion-preview");
	if (output.empty() || fs::exists(output)) throw Error("输出目录已存在或为空，请选择新的 --output 目录");
	std::string json, source;
	if (options.contains("--demo")) { json = demoJson; source = "fixture"; }
	else if (options.contains("--plan")) { json = readFile(options.at("--plan")); source = "file"; }
	else if (options.contains("--provider-response")) { json = parseProviderResponse(readFile(options.at("--provider-response"))); source = "provider-fixture"; }
	else {
		std::cout << "正在请求 AI 解析；当前尚未打开任何串口。\n";
		json = requestAiPlan(options.at("--text"), options.contains("--model") ? options.at("--model") : "deepseek-flash");
		source = "ai";
	}
	auto command = parseCommand(json);
	if (!command.plan) { std::cout << "需要补充：" << command.question << '\n'; return 3; }
	auto points = compilePlan(*command.plan);
	auto frames = encodePlan(points);
	std::cout << "来源：" << source << (source == "ai" ? "（在线 AI 响应）" : "（离线输入，未调用 AI）") << '\n';
	std::cout << "AI/计划说明：" << command.summary << '\n';
	for (const auto& item : command.defaults) std::cout << "采用默认值：" << item << '\n';
	std::cout << "程序核对的动作：\n" << describePlan(*command.plan);
	if (!fs::create_directories(output)) throw Error("无法创建新的输出目录");
	saveFile(output / "plan.json", [&](auto& out) { out << parseStrictJson(json).dump(2) << '\n'; });
	saveFile(output / "trajectory.csv", [&](auto& out) { writeCsv(out, points); });
	saveFile(output / "preview.svg", [&](auto& out) { writeSvg(out, points); });
	nlohmann::json report{{"version", 1}, {"source", source}, {"status", "previewed"}, {"samples", points.size()},
		{"durationMs", points.back().timeMs}, {"mode", "preview"}, {"framesWritten", 0}, {"framesAcknowledged", 0}};
	auto saveReport = [&] { saveFile(output / "run.json", [&](auto& out) { out << report.dump(2) << '\n'; }); };
	saveReport();
	std::cout << "已生成 preview.svg 动画和 trajectory.csv。\n";
	if (!execute && !simulate) return 0;
	if (execute) {
		std::cout << "请确认：已复位开发板、空队列、接线正确；将按上面的计划更新 LED 点阵。\n"
			"输入 EXECUTE 开始；其他输入取消：" << std::flush;
		std::string confirmation;
		if (!std::getline(std::cin, confirmation) || confirmation != "EXECUTE") {
			report["status"] = "cancelled"; saveReport(); std::cout << "已取消，未打开串口。\n"; return 4;
		}
	}
	report["mode"] = simulate ? "firmware-decoder-simulation" : "serial";
	try {
		std::signal(SIGINT, onInterrupt);
		auto transport = simulate ? makeFirmwareSimulator() : openSerial(options.at("--port"));
		RunClock clock(simulate);
		auto result = executeFrames(frames, *transport, clock);
		report["status"] = result.status; report["detail"] = result.detail;
		report["framesWritten"] = result.framesWritten; report["framesAcknowledged"] = result.framesAcknowledged;
		report["bytesWritten"] = result.bytesWritten; report["elapsedMs"] = result.elapsedMs;
		saveReport();
		std::cout << result.status << ": " << result.detail << '\n';
		if (simulate) std::cout << "此结果来自固件协议解码模拟与虚拟时钟，未连接真实硬件。\n";
		return result.status == "completed" ? 0 : 5;
	} catch (const std::exception&) {
		report["status"] = "io_error"; saveReport(); throw;
	}
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
	SetConsoleOutputCP(CP_UTF8); SetConsoleCP(CP_UTF8);
	std::vector<std::string> args;
	for (int i = 0; i < argc; ++i) {
		int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, nullptr, 0, nullptr, nullptr);
		if (!size) { std::cerr << "参数编码错误\n"; return 2; }
		std::string value(size, '\0');
		WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, value.data(), size, nullptr, nullptr);
		value.pop_back(); args.push_back(value);
	}
#else
int main(int argc, char** argv) {
	std::vector<std::string> args(argv, argv + argc);
#endif
	try { return run(args); }
	catch (const std::exception& e) { std::cerr << "错误：" << e.what() << '\n'; return 2; }
}
