#pragma once
#include "plan.h"
#include <ostream>

namespace control {
void writeCsv(std::ostream& out, const std::vector<TimedPoint>& points);
void writeSvg(std::ostream& out, const std::vector<TimedPoint>& points);
}
