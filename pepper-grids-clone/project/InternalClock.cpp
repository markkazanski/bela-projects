#include "InternalClock.h"
#include "Patterns.h"
#include <algorithm>
#include <cmath>

void InternalClock::setup(float analogSampleRate) {
	mRate = analogSampleRate;
	setTempoBpm(mTempoBpm);
}

void InternalClock::setTempoBpm(float bpm) {
	mTempoBpm = std::max(40.f, std::min(240.f, bpm));
	mSamplesPerStep = mRate * 60.f / (mTempoBpm * 4.f);
}

void InternalClock::onStepBoundary() {
	mTickPending = true;
	const float swingAmt = std::min(mSwing, 0.5f) * 2.f;
	mSwingDelay = (mStep % 2 == 1) ? (int)(swingAmt * mSamplesPerStep * 0.33f) : 0;
}

void InternalClock::afterTick() {
	mStep++;
	if (mStep >= Patterns::kSteps)
		mStep = 0;
}

void InternalClock::process(int analogFrames) {
	if (mResetPending) {
		mStep = 0;
		mPhase = 0.f;
		mSubPhase = 0.f;
		mResetPending = false;
		mTickPending = true;
		mSwingDelay = 0;
	}
	if (!mRunning) {
		mSubPhase = (mSamplesPerStep > 0.f) ? mPhase / mSamplesPerStep : 0.f;
		return;
	}
	for (int i = 0; i < analogFrames; ++i) {
		mPhase += 1.f;
		if (mPhase >= mSamplesPerStep) {
			mPhase -= mSamplesPerStep;
			onStepBoundary();
		}
	}
	mSubPhase = (mSamplesPerStep > 0.f)
		? std::min(0.9999f, mPhase / mSamplesPerStep)
		: 0.f;
}

bool InternalClock::consumeStepTick() {
	if (!mTickPending)
		return false;
	mTickPending = false;
	return true;
}
