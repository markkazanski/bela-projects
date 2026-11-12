#pragma once

#include "PepperIO.h"
#include <Bela.h>
#include <libraries/Debounce/BelaDebounce.h>
#include <libraries/OnePole/OnePole.h>

class Controls {
public:
	bool setup(BelaContext* context);
	void processAnalog(BelaContext* context, unsigned int frame);
	void processDigital(BelaContext* context, unsigned int frame);

	float density(unsigned int drum) const;
	float patternX() const;
	float patternY() const;
	float tempoBpm() const;
	float feel() const;
	bool muted(unsigned int drum) const;
	bool rollHeld(unsigned int drum) const;

	void setLedState(int index, bool on);
	void applyLeds(BelaContext* context, unsigned int frame);

private:
	int mHoldFrames = 11025; // 250 ms, set in setup() from digitalSampleRate

	OnePole mFilter[8];
	BelaDebounce mButtons[4];
	float mPot[8] = {};
	bool mMuted[PepperIO::kNumDrums] = {};
	bool mRollHeld[PepperIO::kNumDrums] = {};
	bool mButtonDown[4] = {};
	int mHoldCounter[4] = {};
	bool mLedState[10] = {};

	float mapDensity(float in) const;
	float mapTempo(float in) const;
};
