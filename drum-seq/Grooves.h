#pragma once

#include "PepperIO.h"
#include <cstdint>

namespace Grooves {

enum Lane {
	kLaneKick = 0,
	kLaneSnare,
	kLaneHat,
	kLaneHatOpen,
	kLaneFm,
	kNumLanes
};

struct Groove {
	const char* name;
	uint8_t lane[kNumLanes][PepperIO::kStepsPerBar];
};

constexpr int kGridSize = 3;

// Filled by parseAllGrooves()
extern Groove gGrid[kGridSize][kGridSize];

void parseAllGrooves();

uint8_t parseLaneChar(char c);

} // namespace Grooves
