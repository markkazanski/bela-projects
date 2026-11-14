#pragma once

#include <algorithm>
#include <cmath>

inline int barPosToLed(float pos) {
	pos = std::max(0.f, std::min(1.f, pos));
	return std::max(0, std::min(9, (int)std::floor(pos * 10.f)));
}