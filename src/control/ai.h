#pragma once
#include "plan.h"

namespace control {
std::string buildAiRequest(const std::string& text, const std::string& model);
std::string parseProviderResponse(const std::string& response);
std::string requestAiPlan(const std::string& text, const std::string& model);
}
