#include "plan.h"
#include "json_io.h"
#include <cmath>
#include <initializer_list>
#include <numbers>
#include <sstream>
#include <unordered_set>

namespace control {
using Json = nlohmann::json;
namespace {
void require(bool condition, const char* message) {
	if (!condition) throw Error(message);
}
void keys(const Json& value, std::initializer_list<const char*> expected) {
	require(value.is_object() && value.size() == expected.size(), "对象字段数量或类型错误");
	for (auto name : expected) require(value.contains(name), "缺少必要字段或包含未知字段");
}
uint32_t integer(const Json& value, uint32_t min, uint32_t max) {
	require(value.is_number_integer(), "需要整数");
	// Compare before conversion, including unsigned values larger than int64_t.
	require(value >= min && value <= max, "整数超出允许范围");
	return value.get<uint32_t>();
}
double number(const Json& value) {
	require(value.is_number(), "需要数值");
	double result = value.get<double>();
	require(std::isfinite(result), "数值必须有限");
	return result;
}
void bounds(Position p) {
	require(std::isfinite(p.x) && std::isfinite(p.y) && p.x >= 0 && p.x <= 7 && p.y >= 0 && p.y <= 7,
		"坐标超出 LED 工作区 [0,7]");
}
Position position(const Json& value) {
	keys(value, {"x", "y"});
	Position p{number(value.at("x")), number(value.at("y"))};
	bounds(p);
	return p;
}
std::string textField(const Json& value, size_t limit) {
	require(value.is_string(), "需要文本字段");
	auto s = value.get<std::string>();
	validateText(s, limit);
	return s;
}
uint32_t validatePlan(const MotionPlan& plan) {
	bounds(plan.start);
	require(plan.repeat >= 1 && plan.repeat <= 10, "重复次数必须为 1–10");
	require(!plan.actions.empty() && plan.actions.size() <= 16, "动作数量必须为 1–16");
	uint64_t total = 0;
	Position cursor = plan.start;
	for (const auto& action : plan.actions) {
		require(action.durationMs > 0 && action.durationMs % tickMs == 0, "动作时长必须是 50ms 的正整数倍");
		total += action.durationMs;
		require(total * plan.repeat <= 60000, "计划总时长不能超过 60 秒");
		switch (action.type) {
		case ActionType::line:
			bounds(action.to); cursor = action.to; break;
		case ActionType::circle:
			bounds(action.center);
			require(std::isfinite(action.radius) && action.radius > 0, "圆半径必须为有限正数");
			require(action.center.x - action.radius >= 0 && action.center.x + action.radius <= 7 &&
				action.center.y - action.radius >= 0 && action.center.y + action.radius <= 7, "圆周超出工作区");
			require(std::abs(std::hypot(cursor.x - action.center.x, cursor.y - action.center.y) - action.radius) <= 1e-6,
				"画圆前当前位置必须在圆周上，请增加连接直线");
			break;
		case ActionType::wait: break;
		default: throw Error("不支持的动作类型");
		}
	}
	require(plan.repeat == 1 || std::hypot(cursor.x - plan.start.x, cursor.y - plan.start.y) <= 1e-6,
		"重复计划的终点必须回到起点");
	require(total * plan.repeat / tickMs + 1 <= 4096, "采样点数量超出限制");
	return static_cast<uint32_t>(total * plan.repeat);
}
}

void validateText(const std::string& text, size_t maxCharacters) {
	if (text.empty() || text.size() > maxCharacters * 4) throw Error("文本为空或过长");
	size_t count = 0;
	bool visible = false;
	for (size_t i = 0; i < text.size();) {
		auto c = static_cast<unsigned char>(text[i]);
		size_t length = 0;
		uint32_t cp = 0;
		if (c < 0x80) { length = 1; cp = c; }
		else if (c >= 0xC2 && c <= 0xDF) { length = 2; cp = c & 0x1F; }
		else if (c >= 0xE0 && c <= 0xEF) { length = 3; cp = c & 0x0F; }
		else if (c >= 0xF0 && c <= 0xF4) { length = 4; cp = c & 0x07; }
		else throw Error("文本不是有效 UTF-8");
		if (i + length > text.size()) throw Error("UTF-8 字符不完整");
		for (size_t j = 1; j < length; ++j) {
			auto b = static_cast<unsigned char>(text[i + j]);
			if ((b & 0xC0) != 0x80) throw Error("UTF-8 字符无效");
			cp = (cp << 6) | (b & 0x3F);
		}
		if ((length == 3 && cp < 0x800) || (length == 4 && cp < 0x10000) || cp > 0x10FFFF ||
			(cp >= 0xD800 && cp <= 0xDFFF) || cp < 0x20 || (cp >= 0x7F && cp <= 0x9F))
			throw Error("文本包含无效字符或控制字符，请使用单行输入");
		visible |= cp != 0x20 && cp != 0x3000;
		i += length;
		if (++count > maxCharacters) throw Error("文本超过字符数量限制");
	}
	if (!visible) throw Error("文本不能只有空格");
}

Json parseStrictJson(const std::string& text) {
	if (text.empty() || text.size() > maxResponseBytes) throw Error("JSON 为空或超过 64KiB");
	std::vector<std::unordered_set<std::string>> objects;
	auto callback = [&objects](int depth, Json::parse_event_t event, Json& parsed) {
		if (depth > 16) throw Error("JSON 嵌套超过 16 层");
		if (event == Json::parse_event_t::object_start) objects.emplace_back();
		if (event == Json::parse_event_t::key && !objects.back().insert(parsed.get<std::string>()).second)
			throw Error("JSON 包含重复字段");
		if (event == Json::parse_event_t::object_end) objects.pop_back();
		return true;
	};
	try { return Json::parse(text, callback); }
	catch (const Json::exception&) { throw Error("JSON 格式或数值无效"); }
}

Command parseCommand(const std::string& text) {
	auto value = parseStrictJson(text);
	try {
		require(value.is_object() && value.contains("status") && value.at("status").is_string(), "缺少计划状态");
		Command command;
		if (value.at("status") == "clarification") {
			keys(value, {"status", "question"});
			command.question = textField(value.at("question"), 500);
			return command;
		}
		require(value.at("status") == "ready", "不支持的计划状态");
		keys(value, {"status", "summary", "defaults", "plan"});
		command.summary = textField(value.at("summary"), 1000);
		auto defaults = value.at("defaults");
		require(defaults.is_array() && defaults.size() <= 16, "默认值列表无效");
		for (const auto& item : defaults) command.defaults.push_back(textField(item, 300));
		const auto& p = value.at("plan");
		keys(p, {"version", "workspace", "start", "tickMs", "repeat", "actions"});
		require(integer(p.at("version"), 1, 1) == 1 && p.at("workspace") == "led8x8", "不支持的计划版本或工作区");
		integer(p.at("tickMs"), tickMs, tickMs);
		MotionPlan plan;
		plan.start = position(p.at("start"));
		plan.repeat = integer(p.at("repeat"), 1, 10);
		const auto& actions = p.at("actions");
		require(actions.is_array() && !actions.empty() && actions.size() <= 16, "动作数量必须为 1–16");
		for (const auto& a : actions) {
			require(a.is_object() && a.contains("type") && a.at("type").is_string(), "缺少动作类型");
			Action action;
			const auto type = a.at("type").get<std::string>();
			if (type == "line") {
				keys(a, {"type", "to", "durationMs"});
				action.type = ActionType::line; action.to = position(a.at("to"));
			} else if (type == "circle") {
				keys(a, {"type", "center", "radius", "clockwise", "durationMs"});
				action.type = ActionType::circle; action.center = position(a.at("center"));
				action.radius = number(a.at("radius"));
				require(a.at("clockwise").is_boolean(), "clockwise 必须为布尔值");
				action.clockwise = a.at("clockwise").get<bool>();
			} else if (type == "wait") keys(a, {"type", "durationMs"});
			else throw Error("不支持的动作，只支持 line/circle/wait");
			action.durationMs = integer(a.at("durationMs"), tickMs, 60000);
			plan.actions.push_back(action);
		}
		validatePlan(plan);
		command.plan = plan;
		return command;
	} catch (const Json::exception&) { throw Error("计划字段类型或结构无效"); }
}

std::vector<TimedPoint> compilePlan(const MotionPlan& plan) {
	const auto duration = validatePlan(plan);
	std::vector<TimedPoint> result;
	result.reserve(duration / tickMs + 1);
	Position cursor = plan.start;
	uint32_t time = 0;
	result.push_back({0, cursor, 0});
	for (uint32_t r = 0; r < plan.repeat; ++r) {
		for (const auto& a : plan.actions) {
			const auto start = cursor;
			const auto steps = a.durationMs / tickMs;
			const auto angle = std::atan2(start.y - a.center.y, start.x - a.center.x);
			for (uint32_t i = 1; i <= steps; ++i) {
				double f = static_cast<double>(i) / steps;
				Position p = start;
				if (a.type == ActionType::line) {
					p = i == steps ? a.to : Position{start.x + (a.to.x - start.x) * f, start.y + (a.to.y - start.y) * f};
				} else if (a.type == ActionType::circle && i != steps) {
					double theta = angle + (a.clockwise ? -1 : 1) * 2 * std::numbers::pi * f;
					p = {a.center.x + a.radius * std::cos(theta), a.center.y + a.radius * std::sin(theta)};
				}
				bounds(p);
				time += tickMs;
				result.push_back({time, p, static_cast<uint8_t>(a.type == ActionType::circle ? 1 : 0)});
			}
			cursor = result.back().position;
		}
	}
	return result;
}

std::string describePlan(const MotionPlan& plan) {
	auto duration = validatePlan(plan);
	std::ostringstream out;
	out << "工作区：8×8 LED 格坐标，初始位置 (" << plan.start.x << ',' << plan.start.y << ")\n";
	size_t index = 0;
	for (const auto& a : plan.actions) {
		out << ++index << ". ";
		if (a.type == ActionType::line) out << "直线到 (" << a.to.x << ',' << a.to.y << ')';
		else if (a.type == ActionType::circle) out << "整圆，圆心 (" << a.center.x << ',' << a.center.y << ")，半径 " << a.radius << (a.clockwise ? "，顺时针" : "，逆时针");
		else out << "保持当前位置";
		out << "，用时 " << a.durationMs << "ms\n";
	}
	out << "重复 " << plan.repeat << " 次；总时长 " << duration << "ms；采样点 " << duration / tickMs + 1 << '\n';
	return out.str();
}
}
