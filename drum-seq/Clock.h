#pragma once

#include "PepperIO.h"
#include <Bela.h>

class Clock {
public:
	void setup(BelaContext* context);
	void setTempoBpm(float bpm);
	void setFeelSwing(float feel01);
	void processDigital(BelaContext* context, unsigned int frame);
	void processAnalog(BelaContext* context, unsigned int frame);

	bool consumeTick();
	void afterTickProcessed();
	int step() const { return mStep; }
	int bar() const { return mBar; }
	float swingDelayFrames() const { return mSwingDelayThisStep; }
	bool externalActive() const { return mExternalActive; }
	float stepPeriodFrames() const { return mSamplesPerStep; }
	void looseDelayFrames(int* out4) const;

private:
	float mPhase = 0.f;
	float mSamplesPerStep = 0.f;
	float mTempoBpm = 120.f;
	float mFeel = 0.f;
	int mStep = 0;
	int mBar = 0;
	bool mTickPending = false;
	float mSwingDelayThisStep = 0.f;

	bool mExtLast = false;
	bool mResetLast = false;
	double mLastEdgeTime = 0.;
	double mPredictedPeriod = 0.;
	double mSampleClock = 0.;
	unsigned int mExtMissFrames = 0;
	bool mExternalActive = false;
	static constexpr unsigned int kExtTimeoutFrames = 44100; // ~2s at 22k — scaled in setup

	unsigned int mExtTimeout = 44100;
	int mLooseDelay[PepperIO::kNumDrums] = {};

	void advanceStep();
	float stepPeriodSamples(float sampleRate) const;
	void onRisingEdge(BelaContext* context);
	void prepareStepTiming(BelaContext* context);
};
