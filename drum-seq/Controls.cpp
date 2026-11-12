#include "Controls.h"
#include <algorithm>

bool Controls::setup(BelaContext* context) {
	mHoldFrames = (int)(context->digitalSampleRate * 0.25f);
	for (int i = 0; i < 8; ++i)
		mFilter[i].setup(8.f, context->analogSampleRate, OnePole::LOWPASS);
	for (unsigned int i = 0; i < 4; ++i) {
		BelaDebounce::Settings s{context, PepperIO::kButtonPins[i], 3.0};
		if (mButtons[i].setup(s))
			return false;
	}
	return true;
}

void Controls::processAnalog(BelaContext* context, unsigned int frame) {
	for (unsigned int ch = 0; ch < 8 && ch < (unsigned)context->analogInChannels; ++ch) {
		float v = analogRead(context, frame, ch);
		mPot[ch] = mFilter[ch].process(v);
	}
}

void Controls::processDigital(BelaContext* context, unsigned int frame) {
	for (unsigned int i = 0; i < 4; ++i) {
		bool pressed = mButtons[i].process(context, frame);
		if (PepperIO::kButtonActiveLow)
			pressed = !pressed;
		if (pressed && !mButtonDown[i]) {
			mButtonDown[i] = true;
			mHoldCounter[i] = 0;
		}
		if (!pressed && mButtonDown[i]) {
			if (mHoldCounter[i] < mHoldFrames)
				mMuted[i] = !mMuted[i];
			mRollHeld[i] = false;
			mButtonDown[i] = false;
			mHoldCounter[i] = 0;
		}
		if (mButtonDown[i]) {
			mHoldCounter[i]++;
			if (mHoldCounter[i] >= mHoldFrames)
				mRollHeld[i] = true;
		}
	}
}

float Controls::mapDensity(float in) const {
	return std::max(0.f, std::min(1.f, in));
}

float Controls::mapTempo(float in) const {
	return 40.f + in * 200.f;
}

float Controls::density(unsigned int drum) const {
	if (drum > 3)
		return 0.7f;
	return mapDensity(mPot[drum]);
}

bool Controls::muted(unsigned int drum) const {
	if (drum > 3)
		return false;
	return mMuted[drum];
}

bool Controls::rollHeld(unsigned int drum) const {
	if (drum > 3)
		return false;
	return mRollHeld[drum];
}

float Controls::patternX() const {
	return mapDensity(mPot[4]);
}

float Controls::patternY() const {
	return mapDensity(mPot[5]);
}

float Controls::tempoBpm() const {
	return mapTempo(mPot[6]);
}

float Controls::feel() const {
	return mapDensity(mPot[7]);
}

void Controls::setLedState(int index, bool on) {
	if (index >= 0 && index < 10)
		mLedState[index] = on;
}

void Controls::applyLeds(BelaContext* context, unsigned int frame) {
	for (int i = 0; i < 10; ++i) {
		bool show = mLedState[i];
		if (i < 4 && mMuted[i])
			show = false;
		PepperIO::setLed(context, frame, i, show);
	}
}
