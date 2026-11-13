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
	bool rollHeld(unsigned int drum) const;

	void setLedState(int index, bool on);
	void applyLeds(BelaContext* context, unsigned int frame);

private:
	OnePole mFilter[8];
	BelaDebounce mButtons[4];
	float mPot[8] = {};
	bool mRollHeld[PepperIO::kNumDrums] = {};
	bool mLedState[10] = {};

	float mapDensity(float in) const;
	float mapTempo(float in) const;
};
