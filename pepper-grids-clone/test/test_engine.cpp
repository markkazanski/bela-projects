#include "GridsEngine.h"
#include "Patterns.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>

static EngineInputs fullDensity() {
	EngineInputs in;
	in.mapX = 0;
	in.mapY = 0;
	in.chaos = 0;
	for (int p = 0; p < 3; ++p)
		in.density[p] = 255;
	return in;
}

static void test_patterns_meta() {
	assert(Patterns::kGridSize == 5);
	assert(Patterns::kSteps == 32);
	assert(Patterns::kNodeBytes == 96);
	const uint8_t* n = Patterns::nodeData(0);
	assert(n[0] == 255);
}

static void test_density_zero_silent() {
	GridsEngine e;
	e.seed(1);
	EngineInputs in = fullDensity();
	in.density[0] = 0;
	for (int s = 0; s < 32; ++s) {
		StepResult r = e.tick(s, in);
		assert(!r.trig[0]);
	}
}

static void test_corner_node_bd() {
	GridsEngine e;
	e.seed(1);
	EngineInputs in;
	in.mapX = 192;
	in.mapY = 192;
	in.chaos = 0;
	in.density[0] = 255;
	in.density[1] = 0;
	in.density[2] = 0;
	const int nodeId = Patterns::kDrumMap[3][3];
	const uint8_t expect = Patterns::nodeData(nodeId)[0];
	StepResult r = e.tick(0, in);
	assert(std::abs((int)r.level[0] - (int)expect) <= 1);
	assert(r.trig[0]);
}

static void test_deterministic() {
	GridsEngine a, b;
	a.seed(99);
	b.seed(99);
	EngineInputs in = fullDensity();
	in.chaos = 40;
	for (int s = 0; s < 64; ++s) {
		StepResult ra = a.tick(s % 32, in);
		StepResult rb = b.tick(s % 32, in);
		assert(ra.trig[0] == rb.trig[0]);
		assert(ra.trig[1] == rb.trig[1]);
		assert(ra.trig[2] == rb.trig[2]);
	}
}

static void test_node_reproduction() {
	const int coords[] = {0, 64, 128, 192};
	GridsEngine e;
	e.seed(1);
	EngineInputs in = fullDensity();
	for (int xi : coords) {
		for (int yi : coords) {
			const uint8_t nodeId =
				Patterns::kDrumMap[xi >> 6][yi >> 6];
			in.mapX = (uint8_t)xi;
			in.mapY = (uint8_t)yi;
			for (int p = 0; p < 3; ++p) {
				for (int s = 0; s < 32; ++s) {
					const uint8_t expect =
						Patterns::nodeData(nodeId)[p * 32 + s];
					StepResult r = e.tick(s, in);
					assert(std::abs((int)r.level[p] - (int)expect) <= 1);
				}
			}
		}
	}
}

static void test_blend_in_range() {
	GridsEngine e;
	e.seed(1);
	EngineInputs in = fullDensity();
	in.mapX = 96;
	in.mapY = 96;
	const int i = in.mapX >> 6;
	const int j = in.mapY >> 6;
	const int ni0 = Patterns::kDrumMap[i][j];
	const int ni1 = Patterns::kDrumMap[i + 1][j];
	const int ni2 = Patterns::kDrumMap[i][j + 1];
	const int ni3 = Patterns::kDrumMap[i + 1][j + 1];
	for (int p = 0; p < 3; ++p) {
		for (int s = 0; s < 32; ++s) {
			const int off = p * 32 + s;
			const uint8_t v0 = Patterns::nodeData(ni0)[off];
			const uint8_t v1 = Patterns::nodeData(ni1)[off];
			const uint8_t v2 = Patterns::nodeData(ni2)[off];
			const uint8_t v3 = Patterns::nodeData(ni3)[off];
			const uint8_t lo =
				std::min({v0, v1, v2, v3});
			const uint8_t hi =
				std::max({v0, v1, v2, v3});
			StepResult r = e.tick(s, in);
			assert(r.level[p] >= lo && r.level[p] <= hi);
		}
	}
}

static void test_density_monotonic() {
	GridsEngine e;
	e.seed(1);
	EngineInputs in = fullDensity();
	in.mapX = 128;
	in.mapY = 128;
	const int densities[] = {64, 128, 192, 255};
	for (int p = 0; p < 3; ++p) {
		int prev = -1;
		for (int d : densities) {
			in.density[p] = (uint8_t)d;
			int count = 0;
			for (int s = 0; s < 32; ++s) {
				StepResult r = e.tick(s, in);
				if (r.trig[p])
					count++;
			}
			assert(count >= prev);
			prev = count;
		}
		in.density[p] = 255;
		for (int s = 0; s < 32; ++s) {
			StepResult r = e.tick(s, in);
			if (r.level[p] > 0)
				assert(r.trig[p]);
		}
	}
}

static void test_y_affects_bd() {
	GridsEngine e;
	e.seed(1);
	EngineInputs in = fullDensity();
	in.mapX = 0;
	in.mapY = 0;
	uint8_t levelsY0[32];
	for (int s = 0; s < 32; ++s) {
		StepResult r = e.tick(s, in);
		levelsY0[s] = r.level[0];
	}
	in.mapY = 192;
	bool changed = false;
	for (int s = 0; s < 32; ++s) {
		StepResult r = e.tick(s, in);
		if (std::abs((int)r.level[0] - (int)levelsY0[s]) > 1)
			changed = true;
	}
	assert(changed);
}

int main() {
	test_patterns_meta();
	test_density_zero_silent();
	test_corner_node_bd();
	test_deterministic();
	test_node_reproduction();
	test_blend_in_range();
	test_density_monotonic();
	test_y_affects_bd();
	std::printf("All tests passed.\n");
	return 0;
}
