#include "GridsEngine.h"
#include "Patterns.h"
#include <cassert>
#include <cstdio>

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
	in.mapX = 255;
	in.mapY = 255;
	in.chaos = 0;
	in.density[0] = 255;
	in.density[1] = 0;
	in.density[2] = 0;
	const uint8_t expect = Patterns::nodeData(22)[0];
	StepResult r = e.tick(0, in);
	assert(r.level[0] == expect || r.trig[0]);
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

int main() {
	test_patterns_meta();
	test_density_zero_silent();
	test_corner_node_bd();
	test_deterministic();
	std::printf("All tests passed.\n");
	return 0;
}
