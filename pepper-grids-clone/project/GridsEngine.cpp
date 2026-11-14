#include "GridsEngine.h"
#include <algorithm>

namespace {
constexpr int kSteps = Patterns::kSteps;
constexpr int kAccentThreshold = 192;
constexpr int kFmVoice = 3;
} // namespace

void GridsEngine::resetPattern() {
	for (int p = 0; p < kEngineVoices; ++p)
		mPerturbation[p] = 0;
}

void GridsEngine::setFreeze(bool on, int loopStartStep) {
	mFreeze = on;
	if (on)
		mFreezeStart = loopStartStep;
}

int GridsEngine::audibleStep(int clockStep) const {
	if (!mFreeze)
		return clockStep;
	int rel = clockStep - mFreezeStart;
	rel %= kSteps;
	if (rel < 0)
		rel += kSteps;
	return (mFreezeStart + (rel % 4)) % kSteps;
}

uint8_t GridsEngine::u8Mix(uint8_t a, uint8_t b, uint8_t balance) {
	return (uint8_t)((int)a + (((int)b - (int)a) * (int)balance >> 8));
}

int GridsEngine::playStep(int clockStep) const {
	return audibleStep(clockStep);
}

uint8_t GridsEngine::perturbation(int part) const {
	if (part < 0 || part >= kEngineVoices)
		return 0;
	return mPerturbation[part];
}

uint8_t GridsEngine::levelAt(uint8_t step, uint8_t part, uint8_t x, uint8_t y) {
	return readDrumMap(step, part, x, y);
}

uint8_t GridsEngine::fmLevelAt(uint8_t step, uint8_t x, uint8_t y) {
	const uint8_t mx = (uint8_t)(255 - x);
	const uint8_t my = (uint8_t)(255 - y);
	return readDrumMap(step, (uint8_t)kFmSourcePart, mx, my);
}

uint8_t GridsEngine::readDrumMap(uint8_t step, uint8_t part, uint8_t x, uint8_t y) {
	const uint8_t i = x >> 6;
	const uint8_t j = y >> 6;
	const uint8_t ni0 = Patterns::kDrumMap[i][j];
	const uint8_t ni1 = Patterns::kDrumMap[i + 1][j];
	const uint8_t ni2 = Patterns::kDrumMap[i][j + 1];
	const uint8_t ni3 = Patterns::kDrumMap[i + 1][j + 1];
	const uint8_t offset = (uint8_t)(part * kSteps + step);
	const uint8_t* a = Patterns::nodeData(ni0);
	const uint8_t* b = Patterns::nodeData(ni1);
	const uint8_t* c = Patterns::nodeData(ni2);
	const uint8_t* d = Patterns::nodeData(ni3);
	const uint8_t xa = a[offset];
	const uint8_t xb = b[offset];
	const uint8_t xc = c[offset];
	const uint8_t xd = d[offset];
	const uint8_t bx = (uint8_t)(x << 2);
	const uint8_t by = (uint8_t)(y << 2);
	return u8Mix(u8Mix(xa, xb, bx), u8Mix(xc, xd, bx), by);
}

bool GridsEngine::wouldTrigger(int part, int clockStep, const EngineInputs& in) const {
	if (part < 0 || part >= kEngineVoices)
		return false;
	if ((part == 0 && in.muteBd) || (part == 1 && in.muteSd) || (part == 2 && in.muteHh)
		|| (part == kFmVoice && in.muteFm))
		return false;
	if (in.density[part] == 0)
		return false;
	const int playStep = audibleStep(clockStep);
	uint8_t level;
	if (part == kFmVoice)
		level = fmLevelAt((uint8_t)playStep, in.mapX, in.mapY);
	else
		level = readDrumMap((uint8_t)playStep, (uint8_t)part, in.mapX, in.mapY);
	if (level == 0)
		return false;
	if (level < 255 - mPerturbation[part])
		level = (uint8_t)(level + mPerturbation[part]);
	else
		level = 255;
	const uint8_t threshold = (uint8_t)~in.density[part];
	return level > threshold;
}

int GridsEngine::hhGapSteps(int clockStep, const EngineInputs& in, int maxLook) const {
	for (int off = 1; off <= maxLook; ++off) {
		int cs = (clockStep + off) % kSteps;
		if (cs < 0)
			cs += kSteps;
		if (wouldTrigger(2, cs, in))
			return off;
	}
	return maxLook + 1;
}

int GridsEngine::hhOpenStep(int barStartStep, const EngineInputs& in) const {
	if (in.muteHh || in.density[2] == 0)
		return -1;
	constexpr int kBarLen = 16;
	int pick = -1;
	int pickGap = -1;
	int pickOff = 0;
	int pickPos = -1;
	for (int i = 0; i < kBarLen; ++i) {
		const int cs = (barStartStep + i) % kSteps;
		if (!wouldTrigger(2, cs, in))
			continue;
		const int gap = hhGapSteps(cs, in);
		if (gap < 2)
			continue;
		const int off = (cs % 4 == 2) ? 1 : 0;
		if (pick < 0 || off > pickOff || (off == pickOff && gap > pickGap)
			|| (off == pickOff && gap == pickGap && i > pickPos)) {
			pick = cs;
			pickGap = gap;
			pickOff = off;
			pickPos = i;
		}
	}
	return pick;
}

StepResult GridsEngine::tick(int clockStep, const EngineInputs& in) {
	StepResult out = {};
	const int playStep = audibleStep(clockStep);

	if (clockStep == 0) {
		const uint8_t randomness = (uint8_t)(in.chaos >> 2);
		for (int p = 0; p < kEngineVoices; ++p)
			mPerturbation[p] = mRng.mulShift8(mRng.nextU8(), randomness);
	}

	for (int p = 0; p < kEngineVoices; ++p) {
		if ((p == 0 && in.muteBd) || (p == 1 && in.muteSd) || (p == 2 && in.muteHh)
			|| (p == kFmVoice && in.muteFm)) {
			out.level[p] = 0;
			continue;
		}
		if (in.density[p] == 0) {
			out.level[p] = 0;
			continue;
		}
		uint8_t level;
		if (p == kFmVoice)
			level = fmLevelAt((uint8_t)playStep, in.mapX, in.mapY);
		else
			level = readDrumMap((uint8_t)playStep, (uint8_t)p, in.mapX, in.mapY);
		if (level == 0)
			continue;
		if (level < 255 - mPerturbation[p])
			level = (uint8_t)(level + mPerturbation[p]);
		else
			level = 255;

		const uint8_t threshold = (uint8_t)~in.density[p];
		out.level[p] = level;
		if (level > threshold) {
			out.trig[p] = true;
			if (level > kAccentThreshold)
				out.accent[p] = true;
		}
	}
	return out;
}