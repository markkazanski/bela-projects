#pragma once

#include <algorithm>
#include <cmath>

inline int barPosToLed(float pos) {
	pos = std::max(0.f, std::min(1.f, pos));
	return std::max(0, std::min(9, (int)std::floor(pos * 10.f)));
}

// LEDs 4–7: low X/low Y .. high X/high Y (mapped pad 0..1).
inline int squarePosToQuadLed(float x, float y) {
	x = std::max(0.f, std::min(1.f, x));
	y = std::max(0.f, std::min(1.f, y));
	const int hiX = x >= 0.5f ? 1 : 0;
	const int hiY = y >= 0.5f ? 2 : 0;
	return 4 + hiX + hiY;
}