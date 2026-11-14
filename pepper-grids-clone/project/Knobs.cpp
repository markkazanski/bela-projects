#include "Knobs.h"
#include "PepperPins.h"
#include <algorithm>
#include <cmath>

// Measured full-CW on +12 V (Nov 2025 unit); each max minus 0.01 headroom.
static const float kKnobMax[8] = {0.94f, 0.94f, 0.90f, 0.92f, 0.90f, 0.90f, 0.93f,
	0.97f};

static constexpr float kKnobMoveEps = 0.02f;
static constexpr float kPickupEps = 0.03f;

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
	if (mHhPickup && std::fabs(knob(4) - mHhPickupRef) > kPickupEps)
		mHhPickup = false;
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

void Knobs::onButton2Shift(bool rising, bool falling, bool held) {
	if (rising) {
		mKnob4AtPress = knob(4);
		if (!mHhPickup)
			mHhFrozen = (uint8_t)(knob(4) * 255.f + 0.5f);
	}
	mShiftHeld = held;
	if (held && std::fabs(knob(4) - mKnob4AtPress) > kKnobMoveEps)
		mFmDensity = (uint8_t)(knob(4) * 255.f + 0.5f);
	if (falling) {
		const float moved = std::fabs(knob(4) - mKnob4AtPress);
		if (moved < kKnobMoveEps)
			mClearLatch = true;
		else {
			mFmUnlinked = true;
			mFmDensity = (uint8_t)(knob(4) * 255.f + 0.5f);
		}
		mHhPickup = true;
		mHhPickupRef = knob(4);
	}
}

uint8_t Knobs::hhDensityRaw() const {
	if (mShiftHeld)
		return mHhFrozen;
	if (mHhPickup)
		return mHhFrozen;
	return (uint8_t)(knob(4) * 255.f + 0.5f);
}

uint8_t Knobs::partDensity(int part) const {
	if (part == 0 || part == 1)
		return (uint8_t)(knob(2 + part) * 255.f + 0.5f);
	if (part == PepperPins::kHh)
		return hhDensityRaw();
	if (part == PepperPins::kFm) {
		if (mFmUnlinked)
			return mFmDensity;
		if (mShiftHeld)
			return (uint8_t)(knob(4) * 255.f + 0.5f);
		return hhDensityRaw();
	}
	return 0;
}

bool Knobs::consumeClearLatchRequest() {
	if (!mClearLatch)
		return false;
	mClearLatch = false;
	return true;
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