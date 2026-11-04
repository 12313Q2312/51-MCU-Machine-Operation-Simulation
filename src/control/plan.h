#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace control {
class Error : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};
struct Position { double x = 0; double y = 0; };
enum class ActionType { line, circle, wait };
struct Action {
	ActionType type = ActionType::wait;
	Position to, center;
	double radius = 0;
	bool clockwise = true;
	uint32_t durationMs = 0;
};
struct MotionPlan {
	Position start;
	uint32_t repeat = 1;
	std::vector<Action> actions;
};
struct Command {
	std::string summary, question;
	std::vector<std::string> defaults;
	std::optional<MotionPlan> plan;
};
struct TimedPoint {
	uint32_t timeMs;
	Position position;
	uint8_t motionType;
};
inline constexpr uint32_t tickMs = 50;
inline constexpr size_t maxResponseBytes = 65536;

Command parseCommand(const std::string& text);
std::vector<TimedPoint> compilePlan(const MotionPlan& plan);
void validateText(const std::string& text, size_t maxCharacters = 1000);
std::string describePlan(const MotionPlan& plan);
}
