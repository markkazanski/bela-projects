#pragma once

#include <Bela.h>
#include <atomic>
#include <cstdint>

#ifndef GRIDS_TRILL_DEBUG
#define GRIDS_TRILL_DEBUG 0
#endif

struct BarSample {
	bool touching = false;
	float position = 0.f;
	float size = 0.f;
	int hitsPerStep = 1;
	bool kickDrop = false;
	bool densityMode = false;
	float densityMacro = 0.f;
};

struct BarTouches {
	int count = 0;
	float pos[2] = {0.f, 0.f};
};

struct PadSample;

class TrillInput {
public:
	bool setup();
	void publishToPattern(PadSample& out) const;
	BarSample bar() const;
	BarTouches barTouches() const;

	bool anyTouch() const;
	bool fillActive() const { return bar().touching; }
	bool squareLinked() const;
};
