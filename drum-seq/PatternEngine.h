#pragma once

#include "Grooves.h"
#include "PepperIO.h"
#include "TriggerOut.h"
#include <cstdint>

class PatternEngine {
public:
	void setup();
	void setPatternPosition(float x, float y);
	void setDensity(unsigned int drum, float d);
	void setFeel(float feel01);

	void onNewBar(int bar);
	void onStep(int step, int bar, TriggerOut& triggers, float swingDelayFrames,
		const int* extraDelayFrames, int* ledFlashFrames = nullptr);

	float blendLane(int lane, int step) const;

private:
	float mPatternX = 0.f;
	float mPatternY = 0.f;
	float mDensity[PepperIO::kNumDrums] = {0.7f, 0.7f, 0.7f, 0.5f};
	float mFeel = 0.f;
	int8_t mChaos[PepperIO::kNumDrums][PepperIO::kStepsPerBar] = {};

	float laneValue(int gx, int gy, int lane, int step) const;
	float fillBoost(int bar, unsigned int drum) const;
	bool shouldPlay(unsigned int drum, int step, int bar, float level) const;
};
