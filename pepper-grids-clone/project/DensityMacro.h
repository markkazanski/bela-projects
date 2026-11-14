#pragma once

#include <algorithm>
#include <cstdint>

// e in [-1, 1]; d is knob density 0-255. Preserves mix: silent stays silent.
inline uint8_t applyDensityMacro(uint8_t d, float e) {
	if (d == 0)
		return 0;
	e = std::max(-1.f, std::min(1.f, e));
	float out;
	if (e >= 0.f)
		out = (float)d + (255.f - (float)d) * e;
	else
		out = (float)d * (1.f + e);
	out = std::max(0.f, std::min(255.f, out));
	return (uint8_t)(out + 0.5f);
}