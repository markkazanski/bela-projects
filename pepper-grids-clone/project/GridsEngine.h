#pragma once

#include "Patterns.h"
#include "Rng.h"
#include <cstdint>

constexpr int kEngineParts = Patterns::kParts;
constexpr int kEngineVoices = 4;
constexpr int kFmSourcePart = 1;

struct EngineInputs {
	uint8_t mapX;
	uint8_t mapY;
	uint8_t density[kEngineVoices];
	uint8_t chaos;
	bool muteBd = false;
	bool muteSd = false;
	bool muteHh = false;
	bool muteFm = false;
};

struct StepResult {
	bool trig[kEngineVoices];
	bool accent[kEngineVoices];
	uint8_t level[kEngineVoices];
};

class GridsEngine {
public:
	void resetPattern();
	StepResult tick(int clockStep, const EngineInputs& in);
	int hhGapSteps(int clockStep, const EngineInputs& in, int maxLook = 4) const;

	void setFreeze(bool on, int loopStartStep);
	bool frozen() const { return mFreeze; }
	int freezeStart() const { return mFreezeStart; }
	int playStep(int clockStep) const;

	uint8_t perturbation(int part) const;

	static uint8_t levelAt(uint8_t step, uint8_t part, uint8_t x, uint8_t y);
	static uint8_t fmLevelAt(uint8_t step, uint8_t x, uint8_t y);

	void seed(uint32_t s) { mRng.seed(s); }

private:
	int mFreezeStart = 0;
	bool mFreeze = false;
	uint8_t mPerturbation[kEngineVoices] = {};
	Rng mRng;

	int audibleStep(int clockStep) const;
	bool wouldTrigger(int part, int clockStep, const EngineInputs& in) const;
	static uint8_t readDrumMap(uint8_t step, uint8_t part, uint8_t x, uint8_t y);
	static uint8_t u8Mix(uint8_t a, uint8_t b, uint8_t balance);
};