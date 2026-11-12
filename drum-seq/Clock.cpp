#include "Clock.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

void Clock::setup(BelaContext* context) {
	mSamplesPerStep = stepPeriodSamples(context->analogSampleRate);
	mExtTimeout = (unsigned int)(context->analogSampleRate * 2.0);
}

void Clock::setTempoBpm(float bpm) {
	mTempoBpm = std::max(40.f, std::min(240.f, bpm));
}

void Clock::setFeelSwing(float feel01) {
	mFeel = std::max(0.f, std::min(1.f, feel01));
}

float Clock::stepPeriodSamples(float sampleRate) const {
	return sampleRate * 60.f / (mTempoBpm * 4.f);
}

void Clock::advanceStep() {
	mStep++;
	if (mStep >= PepperIO::kStepsPerBar) {
		mStep = 0;
		mBar++;
	}
}

void Clock::onRisingEdge(BelaContext* context) {
	double now = mSampleClock;
	if (mLastEdgeTime > 0.) {
		double period = now - mLastEdgeTime;
		if (period > 0.) {
			if (mPredictedPeriod <= 0.)
				mPredictedPeriod = period;
			else
				mPredictedPeriod = mPredictedPeriod * 0.7 + period * 0.3;
			mSamplesPerStep = (float)mPredictedPeriod;
		}
	}
	mLastEdgeTime = now;
	mExternalActive = true;
	mExtMissFrames = 0;
	mPhase = 0.f;
	mTickPending = true;
	prepareStepTiming(context);
}

void Clock::prepareStepTiming(BelaContext* context) {
	float swing = std::min(mFeel, 0.5f) * 2.f;
	mSwingDelayThisStep = (mStep % 2 == 1) ? swing * mSamplesPerStep * 0.33f : 0.f;
	float loose = std::max(0.f, (mFeel - 0.5f) * 2.f);
	int maxLoose = (int)(loose * context->analogSampleRate * 0.003f);
	for (unsigned int d = 0; d < PepperIO::kNumDrums; ++d)
		mLooseDelay[d] = 0;
	if (maxLoose > 0) {
		mLooseDelay[PepperIO::kHat] = rand() % (maxLoose * 2 + 1) - maxLoose;
		mLooseDelay[PepperIO::kFm] = rand() % (maxLoose * 2 + 1) - maxLoose;
	}
}

void Clock::processDigital(BelaContext* context, unsigned int frame) {
	bool clk = digitalRead(context, frame, PepperIO::kClockInPin);
	bool rst = digitalRead(context, frame, PepperIO::kResetInPin);
	if (rst && !mResetLast) {
		mStep = 0;
		mBar = 0;
		mPhase = 0.f;
	}
	mResetLast = rst;
	if (clk && !mExtLast)
		onRisingEdge(context);
	mExtLast = clk;
}

void Clock::processAnalog(BelaContext* context, unsigned int frame) {
	mSampleClock += 1.0;
	if (mExternalActive) {
		mExtMissFrames++;
		if (mExtMissFrames > mExtTimeout)
			mExternalActive = false;
	}
	if (!mExternalActive) {
		mSamplesPerStep = stepPeriodSamples(context->analogSampleRate);
		mPhase += 1.f;
		if (mPhase >= mSamplesPerStep) {
			mPhase -= mSamplesPerStep;
			mTickPending = true;
			prepareStepTiming(context);
		}
	}
	(void)frame;
}

bool Clock::consumeTick() {
	if (!mTickPending)
		return false;
	mTickPending = false;
	return true;
}

void Clock::afterTickProcessed() {
	advanceStep();
}

void Clock::looseDelayFrames(int* out4) const {
	for (unsigned int d = 0; d < PepperIO::kNumDrums; ++d)
		out4[d] = mLooseDelay[d];
}
