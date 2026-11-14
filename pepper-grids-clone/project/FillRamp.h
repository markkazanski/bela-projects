#pragma once

#include <algorithm>
#include <cmath>

constexpr float kFillRampSteps = 16.f;

inline float fillRamp01(float elapsedSteps) {
	float t = elapsedSteps / kFillRampSteps;
	t = std::max(0.f, std::min(1.f, t));
	return std::pow(t, 1.5f);
}