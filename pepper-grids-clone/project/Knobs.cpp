#include "Knobs.h"
#include "PepperPins.h"
#include <algorithm>
#include <cmath>

// Measured full-CW on +12 V (Nov 2025 unit); each max minus 0.01 headroom.
static const float kKnobMax[8] = {0.94f, 0.94f, 0.90f, 0.92f, 0.90f, 0.90f, 0.93f,
	0.97f};

static float scaleKnob(int ch, float raw) {
	if (ch < 0 || ch > 7 || kKnobMax[ch] <= 0.f)
		return 0.f;
	return std::max(0.f, std::min(1.f, raw / kKnobMax[ch]));
}

bool Knobs::setup(BelaContext* ctx) {
	for (int i = 0; i < 8; ++i)
		mFilter[i].setup(8.f, ctx->analogSampleRate, OnePole::LOWPASS);
	for (unsigned i = 0; i < 4; ++i) {
		BelaDebounce::Settings s{ctx, PepperPins::kButtonPins[i], 3.0};
		if (mDebounce[i].setup(s))
			return false;
	}
	return true;
}

void Knobs::processAnalog(BelaContext* ctx, unsigned aFrame) {
	for (unsigned ch = 0; ch < 8 && ch < (unsigned)ctx->analogInChannels; ++ch) {
		mRaw[ch] = mFilter[ch].process(analogRead(ctx, aFrame, ch));
	}
	const float target = 40.f + scaleKnob(6, mRaw[6]) * 200.f;
	if (std::fabs(target - mTempo) >= 0.5f)
		mTempo = std::round(target);
}

void Knobs::processButtons(BelaContext* ctx, unsigned dFrame) {
	for (unsigned i = 0; i < 4; ++i) {
		bool v = mDebounce[i].process(ctx, dFrame);
		if (!PepperPins::kButtonActiveHigh)
			v = !v;
		mBtnEdge[i] = v && !mBtnLast[i];
		mBtnLast[i] = v;
		mBtn[i] = v;
	}
}

float Knobs::swing01() const {
	const float k7 = knob(7);
	return std::min(1.f, k7 * 2.f);
}

float Knobs::feelLoose01() const {
	const float k7 = knob(7);
	return std::max(0.f, (k7 - 0.5f) * 2.f);
}

uint8_t Knobs::partDensity(int part) const {
	if (part < 0 || part > 3)
		return 0;
	return (uint8_t)(knob(part) * 255.f + 0.5f);
}

float Knobs::knob(int ch) const {
	if (ch < 0 || ch > 7)
		return 0.f;
	return scaleKnob(ch, mRaw[ch]);
}

float Knobs::tempoBpm() const { return mTempo; }

bool Knobs::buttonPressed(int i) const {
	return (i >= 0 && i < 4) ? mBtn[i] : false;
}

bool Knobs::buttonRising(int i) const {
	return (i >= 0 && i < 4) ? mBtnEdge[i] : false;
}

bool Knobs::buttonHeld(int i) const { return buttonPressed(i); }