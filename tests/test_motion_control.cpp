#include "plan.h"
#include "json_io.h"
#include "ai.h"
#include "executor.h"
#include "preview.h"
#include "demo.h"
#include <cmath>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
extern "C" {
#include "protocol.h"
#include "led_matrix.h"
}

using namespace control;
namespace {
size_t checks = 0;
void check(bool condition, const char* message) {
	++checks;
	if (!condition) throw std::runtime_error(message);
}
void rejects(const std::function<void()>& action) {
	bool failed = false;
	try { action(); } catch (const Error&) { failed = true; }
	check(failed, "Expected rejection");
}
void near(double a, double b) { check(std::abs(a - b) < 1e-8, "Coordinate mismatch"); }
uint8_t columns = 0;
std::array<uint8_t, 8> displayed{};
size_t decoded = 0;
void onFrame(const FrameData*) { ++decoded; }
class FakeClock final : public Clock {
public:
	uint64_t time = 0, cancelAt = UINT64_MAX;
	uint64_t nowMs() override { return time; }
	void sleepMs(uint32_t ms) override { time += ms; }
	bool cancelled() override { return time >= cancelAt; }
};
class FakeTransport final : public Transport {
public:
	FakeClock& clock;
	std::deque<int> input;
	std::vector<uint64_t> writes;
	int sequence = 254;
	bool silence = false, noise = false, badSequence = false, partial = false, throwOnSecond = false;
	uint32_t delay = 0;
	size_t resets = 0;
	explicit FakeTransport(FakeClock& value) : clock(value) {}
	bool resetInput() override { ++resets; return true; }
	size_t write(std::span<const uint8_t> bytes) override {
		if (throwOnSecond && writes.size() == 1) throw Error("test I/O failure");
		writes.push_back(clock.time);
		if (partial) return 3;
		clock.time += delay;
		if (!silence && !noise) {
			input.push_back(0xCC); input.push_back(sequence);
			sequence = (sequence + (badSequence ? 2 : 1)) & 255;
		}
		return bytes.size();
	}
	int readByte() override {
		if (noise) return 0xAB;
		if (input.empty()) return -1;
		int result = input.front(); input.pop_front(); return result;
	}
};
void testPlan() {
	auto command = parseCommand(demoJson);
	check(command.plan.has_value(), "Demo should have a plan");
	auto p = compilePlan(*command.plan);
	check(p.size() == 121 && p.back().timeMs == 6000, "Demo time/size mismatch");
	near(p[40].position.x, 2); near(p[40].position.y, 4);
	near(p[60].position.x, 4); near(p[60].position.y, 6);
	near(p[80].position.x, 6); near(p[80].position.y, 4);
	near(p[100].position.x, 4); near(p[100].position.y, 2);
	near(p[120].position.x, 2); near(p[120].position.y, 4);
	for (size_t i = 0; i < p.size(); ++i) check(p[i].timeMs == i * 50, "Sampling gap/duplicate boundary");
	auto original = parseStrictJson(demoJson);
	std::vector<std::function<void(nlohmann::json&)>> mutations{
		[](auto& j) { j["plan"]["start"]["x"] = -1; },
		[](auto& j) { j["plan"]["start"]["x"] = "1"; },
		[](auto& j) { j["plan"]["repeat"] = 0; },
		[](auto& j) { j["plan"]["repeat"] = 11; },
		[](auto& j) { j["plan"]["repeat"] = 2; },
		[](auto& j) { j["plan"]["repeat"] = 1.0; },
		[](auto& j) { j["plan"]["actions"][0]["durationMs"] = 0; },
		[](auto& j) { j["plan"]["actions"][0]["durationMs"] = -50; },
		[](auto& j) { j["plan"]["actions"][0]["durationMs"] = 51; },
		[](auto& j) { j["plan"]["actions"][0]["durationMs"] = 60000; },
		[](auto& j) { j["plan"]["actions"][1]["radius"] = -1; },
		[](auto& j) { j["plan"]["actions"][1]["radius"] = 4; },
		[](auto& j) { j["plan"]["actions"][0]["to"]["x"] = 1; },
		[](auto& j) { j["plan"]["actions"][1]["clockwise"] = 1; },
		[](auto& j) { j["plan"]["actions"][0]["type"] = "shell"; },
		[](auto& j) { j["plan"]["actions"][0]["code"] = "anything"; },
		[](auto& j) { j["plan"]["start"].erase("y"); },
		[](auto& j) { j["plan"]["tickMs"] = 0; },
		[](auto& j) { j["plan"]["version"] = 2; },
		[](auto& j) { j["plan"]["actions"] = nlohmann::json::array(); },
		[](auto& j) { j["defaults"] = "none"; },
		[](auto& j) { j["summary"] = "\x1b[2J"; }
	};
	for (const auto& mutate : mutations) {
		auto j = original; mutate(j); rejects([&] { parseCommand(j.dump()); });
	}
	rejects([] { parseCommand(R"({"status":"clarification","status":"ready"})"); });
	rejects([] { parseStrictJson(R"({"x":{"a":1,"a":2}})"); });
	rejects([] { parseStrictJson(std::string(18, '[') + "0" + std::string(18, ']')); });
	rejects([] { parseStrictJson(std::string(65537, ' ')); });
	rejects([] { parseStrictJson("{\"x\":1e1000}"); });
	rejects([] { validateText(std::string(1001, 'a')); });
	rejects([] { validateText(std::string("\xC0\xAF")); });
	rejects([] { validateText(std::string("\xED\xA0\x80")); });
	rejects([] { validateText("   "); });
	validateText("向右移动两格");
	check(!parseCommand(R"({"status":"clarification","question":"请提供完整指令。"})").plan, "Clarification must not have a plan");
	auto loop = original;
	loop["plan"]["repeat"] = 3;
	loop["plan"]["actions"] = nlohmann::json::parse(R"([{"type":"line","to":{"x":7,"y":0},"durationMs":1000},{"type":"line","to":{"x":0,"y":0},"durationMs":1000}])");
	auto repeated = compilePlan(*parseCommand(loop.dump()).plan);
	check(repeated.size() == 121 && repeated.back().timeMs == 6000, "Repeat time/size mismatch");
	for (auto point : repeated) near(point.position.y, 0);
	auto wait = original;
	wait["plan"]["actions"] = nlohmann::json::parse(R"([{"type":"wait","durationMs":100}])");
	check(compilePlan(*parseCommand(wait.dump()).plan).size() == 3, "Wait samples mismatch");
	MotionPlan invalid = *command.plan; invalid.actions[0].durationMs = 0;
	rejects([&] { compilePlan(invalid); });
	invalid = *command.plan; invalid.start.x = std::numeric_limits<double>::quiet_NaN();
	rejects([&] { compilePlan(invalid); });
	std::ostringstream csv, svg;
	writeCsv(csv, p); writeSvg(svg, p);
	check(csv.str().find("6000,2,4,1") != std::string::npos, "CSV endpoint missing");
	check(svg.str().find("<animate") != std::string::npos, "SVG animation missing");
}
void testProvider() {
	auto body = parseStrictJson(buildAiRequest("从左下角走到右上角，用三秒", "deepseek-flash"));
	check(body.at("response_format").at("type") == "json_object", "JSON response mode missing");
	check(body.at("messages")[1].at("content") == "从左下角走到右上角，用三秒", "Chinese input corrupted");
	nlohmann::json response{{"choices", nlohmann::json::array({{{"finish_reason", "stop"}, {"message", {{"role", "assistant"}, {"content", demoJson}}}}})}};
	check(parseCommand(parseProviderResponse(response.dump())).plan.has_value(), "Provider envelope parse failed");
	response["choices"][0]["finish_reason"] = "length";
	rejects([&] { parseProviderResponse(response.dump()); });
	response["choices"][0]["finish_reason"] = "stop";
	response["choices"][0]["message"]["content"] = "   ";
	rejects([&] { parseProviderResponse(response.dump()); });
	response["choices"][0]["message"]["content"] = demoJson;
	response["choices"][0]["message"]["refusal"] = "refused";
	rejects([&] { parseProviderResponse(response.dump()); });
	rejects([] { buildAiRequest("test", "model\r\nheader"); });
}
void testProtocolAndLed() {
	auto frames = encodePlan(compilePlan(*parseCommand(demoJson).plan));
	protocol_reset(); decoded = 0;
	for (const auto& frame : frames) {
		FrameData result{};
		check(protocol_decode(frame.bytes.data(), &result) == 1, "Firmware rejected PC frame");
		for (auto byte : frame.bytes) protocol_feed(byte, onFrame);
	}
	check(decoded == 121, "Firmware stream parser lost frames");
	for (int x = 0; x <= 7; ++x) for (int y = 0; y <= 7; ++y) {
		auto one = encodePlan({{0, {double(x), double(y)}, 0}});
		FrameData data{}; check(protocol_decode(one[0].bytes.data(), &data) == 1, "Grid frame invalid");
		led_clear(); displayed.fill(0);
		led_plot_point(data.x, data.y, 0, 65535, 0, 65535);
		for (int row = 0; row < 8; ++row) led_scan_row();
		for (int row = 0; row < 8; ++row) check(displayed[row] == (row == 7 - y ? 1 << x : 0), "LED grid position shifted");
	}
	auto corrupt = frames.front().bytes; corrupt[4] ^= 1;
	FrameData data{}; check(!protocol_decode(corrupt.data(), &data), "Firmware accepted corrupt frame");
	auto mid = encodePlan({{0, {3.5, 3.5}, 0}});
	check(mid[0].bytes[2] == 0x80 && mid[0].bytes[3] == 0, "Fixed origin/scale mismatch");
	rejects([] { encodePlan({{0, {8, 0}, 0}}); });
}
void testExecution() {
	auto frames = encodePlan({{0,{0,0},0},{50,{1,0},0},{100,{2,0},0}});
	{
		FakeClock clock; FakeTransport transport(clock);
		auto r = executeFrames(frames, transport, clock);
		check(r.status == "completed" && r.framesWritten == 3 && r.framesAcknowledged == 3, "Execution/ACK wrap failed");
		check(transport.writes == std::vector<uint64_t>({0,50,100}), "Frame pacing failed");
	}
	{
		FakeClock clock; FakeTransport transport(clock); transport.silence = true;
		auto r = executeFrames(frames, transport, clock, 100);
		check(r.status == "ack_timeout" && r.framesWritten == 1 && r.framesAcknowledged == 0 && r.elapsedMs == 100, "Timeout did not stop/no retry");
	}
	{
		FakeClock clock; FakeTransport transport(clock); transport.noise = true;
		check(executeFrames(frames, transport, clock, 100).status == "ack_timeout", "Continuous noise bypassed deadline");
	}
	{
		FakeClock clock; FakeTransport transport(clock); transport.badSequence = true;
		auto r = executeFrames(frames, transport, clock);
		check(r.status == "ack_sequence_error" && r.framesWritten == 2, "Bad ACK did not stop");
	}
	{
		FakeClock clock; FakeTransport transport(clock); transport.partial = true;
		auto r = executeFrames(frames, transport, clock);
		check(r.status == "io_error" && r.bytesWritten == 3 && r.framesWritten == 0, "Partial write was hidden");
	}
	{
		FakeClock clock; FakeTransport transport(clock); transport.delay = 75;
		auto r = executeFrames(frames, transport, clock);
		check(r.status == "overrun" && r.framesWritten == 1, "Late ACK triggered catch-up");
	}
	{
		FakeClock clock; clock.cancelAt = 75; FakeTransport transport(clock);
		auto r = executeFrames(frames, transport, clock);
		check(r.status == "cancelled" && r.framesWritten == 2, "Cancellation sent another frame");
	}
	{
		FakeClock clock; FakeTransport transport(clock); transport.throwOnSecond = true;
		auto r = executeFrames(frames, transport, clock);
		check(r.status == "io_error" && r.framesWritten == 1 && r.framesAcknowledged == 1, "Exception lost partial progress");
	}
	{
		FakeClock clock; FakeTransport transport(clock); auto bad = frames; bad[1].bytes[0] = 0;
		rejects([&] { executeFrames(bad, transport, clock); });
		check(transport.resets == 0 && transport.writes.empty(), "Invalid input touched transport");
	}
}
}
extern "C" void hal_host_write_col(uint8_t value) { columns = value; }
extern "C" void hal_host_write_row(uint8_t value) {
	for (int row = 0; row < 8; ++row) if (value == (1 << row)) displayed[row] = columns;
}
int main() {
	try {
		testPlan(); testProvider(); testProtocolAndLed(); testExecution();
		std::cout << "PASS: " << checks << " checks (plan, provider, firmware, LED, execution)\n";
		return 0;
	} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
