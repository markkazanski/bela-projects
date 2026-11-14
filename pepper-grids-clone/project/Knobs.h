#pragma once

#include <Bela.h>
#include <libraries/Debounce/BelaDebounce.h>
#include <libraries/OnePole/OnePole.h>
#include <cstdint>

class Knobs {
public:
	bool setup(BelaContext* ctx);
	void processAnalog(BelaContext* ctx, unsigned aFrame);
	void processButtons(BelaContext* ctx, unsigned dFrame);

	float knob(int ch) const;
	float tempoBpm() const;

	uint8_t partDensity(int part) const;

	float swing01() const;
	float feelLoose01() const;

	bool buttonPressed(int i) const;
	bool buttonRising(int i) const;
	bool buttonHeld(int i) const;

private:
	OnePole mFilter[8];
	BelaDebounce mDebounce[4];
	float mRaw[8] = {};
	float mTempo = 120.f;
	bool mBtn[4] = {};
	bool mBtnLast[4] = {};
	bool mBtnEdge[4] = {};
};