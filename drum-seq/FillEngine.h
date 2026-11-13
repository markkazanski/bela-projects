#pragma once

#include "PepperIO.h"
#include "TrillControls.h"
#include "TriggerOut.h"
#include <Bela.h>

class FillEngine {
public:
	void setTrillState(const TrillFillState& state);
	void setButtonHeld(unsigned int drum, bool held);
	void process(float beatPhase, TriggerOut& triggers, int* ledFlashFrames = nullptr);

private:
	TrillFillState mTrill;
	bool mButtonHeld[PepperIO::kNumDrums] = {};

	bool mTrillWasActive = false;
	int mLastIdx[PepperIO::kNumDrums] = {-1, -1, -1, -1};
	int mLastHits[PepperIO::kNumDrums] = {-1, -1, -1, -1};
	bool mWaitingNextGrid[PepperIO::kNumDrums] = {};
	float mRollProgress[PepperIO::kNumDrums] = {};
	float mPrevBeatPhase = 0.f;
	bool mBeatPhaseInit = false;

	static constexpr float kRollSweepBeats = 4.f;

	int hitsPerBeatForDrum(unsigned int drum) const;
	bool drumActive(unsigned int drum) const;
	void onDeactivate(unsigned int drum);
	void tryFire(unsigned int drum, int idx, float beatPhase, int hits,
		TriggerOut& triggers, int* ledFlashFrames);
	float snareAccent(int idx, int hits) const;
	float hatAccent() const;
	bool shouldPlayHatGrid(int idx) const;
	bool shouldPlaySnareGrid(int idx) const;
	bool chaosSkip(unsigned int drum) const;
};
