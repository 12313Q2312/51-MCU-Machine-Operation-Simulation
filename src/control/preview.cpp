#include "preview.h"
#include <iomanip>
#include <locale>
#include <cmath>

namespace control {
void writeCsv(std::ostream& out, const std::vector<TimedPoint>& points) {
	out.imbue(std::locale::classic());
	out << "time_ms,x_grid,y_grid,motion_type\n" << std::setprecision(12);
	for (const auto& p : points) out << p.timeMs << ',' << p.position.x << ',' << p.position.y << ',' << int(p.motionType) << '\n';
}
void writeSvg(std::ostream& out, const std::vector<TimedPoint>& points) {
	if (points.size() < 2) throw Error("预览至少需要两个点");
	out.imbue(std::locale::classic());
	out << std::fixed << std::setprecision(5);
	out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"640\" height=\"680\" viewBox=\"0 0 640 680\">\n"
		"<title>MCU Motion Plan Preview</title><rect width=\"640\" height=\"680\" rx=\"24\" fill=\"#101827\"/>"
		"<g fill=\"#e2e8f0\" font-family=\"sans-serif\"><text x=\"40\" y=\"45\" font-size=\"24\">MCU Motion Plan</text>"
		"<text x=\"40\" y=\"72\" font-size=\"14\">LED grid simulation | preview loops for viewing</text></g>\n";
	for (int y = 0; y <= 7; ++y) for (int x = 0; x <= 7; ++x)
		out << "<circle cx=\"" << 75 + x * 70 << "\" cy=\"" << 590 - y * 70 << "\" r=\"5\" fill=\"#334155\"/>\n";
	for (int i = 0; i <= 7; ++i) {
		out << "<text x=\"" << 71 + i * 70 << "\" y=\"620\" fill=\"#94a3b8\" font-family=\"sans-serif\">" << i << "</text>";
		out << "<text x=\"45\" y=\"" << 595 - i * 70 << "\" fill=\"#94a3b8\" font-family=\"sans-serif\">" << i << "</text>";
	}
	out << "<polyline fill=\"none\" stroke=\"#38bdf8\" stroke-opacity=\"0.65\" stroke-width=\"3\" points=\"";
	for (const auto& p : points) out << 75 + p.position.x * 70 << ',' << 590 - p.position.y * 70 << ' ';
	out << "\"/>\n<circle r=\"10\" fill=\"#fbbf24\" cx=\"" << 75 + points.front().position.x * 70
		<< "\" cy=\"" << 590 - points.front().position.y * 70 << "\">\n";
	auto ledIndex = [](double value) {
		auto q = static_cast<uint32_t>(std::lround(value / 7 * 65535));
		return (q * 7 + 32767) / 65535; // Same rounding as the firmware LED driver.
	};
	for (bool xAxis : {true, false}) {
		out << "<animate attributeName=\"" << (xAxis ? "cx" : "cy") << "\" dur=\"" << points.back().timeMs / 1000.0
			<< "s\" repeatCount=\"indefinite\" calcMode=\"discrete\" values=\"";
		for (size_t i = 0; i < points.size(); ++i) {
			if (i) out << ';';
			out << (xAxis ? 75 + ledIndex(points[i].position.x) * 70 : 590 - ledIndex(points[i].position.y) * 70);
		}
		out << "\" keyTimes=\"";
		for (size_t i = 0; i < points.size(); ++i) {
			if (i) out << ';';
			out << static_cast<double>(points[i].timeMs) / points.back().timeMs;
		}
		out << "\"/>\n";
	}
	out << "</circle><text x=\"40\" y=\"655\" fill=\"#94a3b8\" font-family=\"sans-serif\">"
		<< points.back().timeMs / 1000.0 << " seconds | " << points.size() << " samples | 50 ms tick</text></svg>\n";
}
}
