#pragma once

#include <cstdint>

struct Rng {
	uint32_t state = 0xA5A5A5A5u;

	void seed(uint32_t s) { state = s ? s : 0xA5A5A5A5u; }

	uint8_t nextU8() {
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return (uint8_t)(state & 0xFFu);
	}

	uint8_t mulShift8(uint8_t a, uint8_t b) {
		return (uint8_t)(((uint16_t)a * b) >> 8);
	}
};
