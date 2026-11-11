#pragma once
#include "plan.h"
#include <nlohmann/json.hpp>

namespace control {
// All untrusted JSON enters through this size/depth/duplicate-key gate.
nlohmann::json parseStrictJson(const std::string& text);
}
