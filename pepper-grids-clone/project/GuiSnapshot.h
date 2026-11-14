#pragma once

#include <cstdint>

namespace GuiSnapshot {

void publish(uint8_t mapX, uint8_t mapY, const uint8_t density[3],
	const uint8_t perturb[3], int clockStep, int audibleStep, int freezeStart,
	bool running, bool frozen, bool fillActive, bool kickDrop, bool squareSource,
	float bpm, uint8_t swing255, uint8_t chaos, bool muteBd, bool muteSd,
	bool muteHh);

constexpr int kLevels = 96;
constexpr int kBufferFloats = 114;

void copyTo(float* out, unsigned maxFloats);

} // namespace GuiSnapshot
