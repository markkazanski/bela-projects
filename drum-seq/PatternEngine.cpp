#include "PatternEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

void PatternEngine::setup() {
	Grooves::parseAllGrooves();
}

void PatternEngine::setPatternPosition(float x, float y) {
	mPatternX = std::max(0.f, std::min(1.f, x));
	mPatternY = std::max(0.f, std::min(1.f, y));
}

void PatternEngine::setDensity(unsigned int drum, float d) {
	if (drum < PepperIO::kNumDrums)
		mDensity[drum] = std::max(0.f, std::min(1.f, d));
}

void PatternEngine::setFeel(float feel01) {
	mFeel = std::max(0.f, std::min(1.f, feel01));
}

void PatternEngine::setMuted(unsigned int drum, bool muted) {
	if (drum < PepperIO::kNumDrums)
		mMuted[drum] = muted;
}

float PatternEngine::laneValue(int gx, int gy, int lane, int step) const {
	gx = std::max(0, std::min(Grooves::kGridSize - 1, gx));
	gy = std::max(0, std::min(Grooves::kGridSize - 1, gy));
	return (float)Grooves::gGrid[gy][gx].lane[lane][step];
}

float PatternEngine::blendLane(int lane, int step) const {
	float fx = mPatternX * 2.f;
	float fy = mPatternY * 2.f;
	int ix = std::min((int)fx, Grooves::kGridSize - 2);
	int iy = std::min((int)fy, Grooves::kGridSize - 2);
	float tx = fx - (float)ix;
	float ty = fy - (float)iy;
	auto v = [&](int gx, int gy) { return laneValue(gx, gy, lane, step); };
	float top = v(ix, iy) * (1.f - tx) + v(ix + 1, iy) * tx;
	float bottom = v(ix, iy + 1) * (1.f - tx) + v(ix + 1, iy + 1) * tx;
	return top * (1.f - ty) + bottom * ty;
}

void PatternEngine::onNewBar(int bar) {
	float chaosAmount = std::max(0.f, (mFeel - 0.5f) * 2.f);
	int maxJitter = (int)(chaosAmount * 40.f);
	for (unsigned int d = 0; d < PepperIO::kNumDrums; ++d) {
		for (int s = 0; s < PepperIO::kStepsPerBar; ++s)
			mChaos[d][s] = (maxJitter > 0) ? (int8_t)((rand() % (maxJitter * 2 + 1)) - maxJitter) : 0;
	}
	(void)bar;
}

float PatternEngine::fillBoost(int bar, unsigned int drum) const {
	if ((bar + 1) % 4 != 0)
		return 0.f;
	if (drum == PepperIO::kSnare || drum == PepperIO::kFm)
		return 0.12f;
	return 0.f;
}

bool PatternEngine::shouldPlay(unsigned int drum, int step, int bar, float level) const {
	if (mMuted[drum] || level <= 0.f)
		return false;
	float density = mDensity[drum] + fillBoost(bar, drum);
	density = std::min(1.f, density);
	float threshold = 255.f * (1.f - density);
	return level > threshold;
}

void PatternEngine::onStep(int step, int bar, TriggerOut& triggers, float swingDelayFrames,
	const int* extraDelayFrames, int* ledFlashFrames) {
	static const int kLaneForDrum[PepperIO::kNumDrums] = {
		Grooves::kLaneKick, Grooves::kLaneSnare, Grooves::kLaneHat, Grooves::kLaneFm};

	float kickLevel = blendLane(Grooves::kLaneKick, step)
		+ mChaos[PepperIO::kKick][step];
	float snareLevel = blendLane(Grooves::kLaneSnare, step)
		+ mChaos[PepperIO::kSnare][step];

	bool playKick = shouldPlay(PepperIO::kKick, step, bar, kickLevel);
	bool playSnare = shouldPlay(PepperIO::kSnare, step, bar, snareLevel);

	if (playKick && playSnare && mDensity[PepperIO::kKick] < 0.35f
		&& kickLevel < snareLevel * 0.6f)
		playKick = false;

	auto hit = [&](unsigned int drum, float level, int laneOpen) {
		if (!shouldPlay(drum, step, bar, level))
			return;
		int delay = (int)swingDelayFrames;
		if (extraDelayFrames)
			delay += extraDelayFrames[drum];
		float accent01 = level / 255.f;
		if (drum == PepperIO::kHat) {
			float open = blendLane(laneOpen, step) / 255.f;
			triggers.setHatDecayNow(open);
		}
		triggers.scheduleTriggerWithDelay(drum, delay, accent01);
		if (ledFlashFrames)
			ledFlashFrames[drum] = 1100;
	};

	if (playKick)
		hit(PepperIO::kKick, kickLevel, Grooves::kLaneKick);
	if (playSnare)
		hit(PepperIO::kSnare, snareLevel, Grooves::kLaneSnare);

	float hatLevel = blendLane(Grooves::kLaneHat, step) + mChaos[PepperIO::kHat][step];
	float fmLevel = blendLane(Grooves::kLaneFm, step) + mChaos[PepperIO::kFm][step];
	hit(PepperIO::kHat, hatLevel, Grooves::kLaneHatOpen);
	hit(PepperIO::kFm, fmLevel, Grooves::kLaneFm);
}
