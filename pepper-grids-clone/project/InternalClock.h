#pragma once

class InternalClock {
public:
	void setup(float analogSampleRate);
	void setRunning(bool run) { mRunning = run; }
	bool running() const { return mRunning; }
	void requestReset() { mResetPending = true; }

	void setTempoBpm(float bpm);
	float tempoBpm() const { return mTempoBpm; }
	void setSwing(float swing01) { mSwing = swing01; }

	void process(int analogFrames);
	bool consumeStepTick();
	void afterTick();
	int step() const { return mStep; }
	float stepPeriodFrames() const { return mSamplesPerStep; }
	float subStepPhase() const { return mSubPhase; }
	int swingDelayFrames() const { return mSwingDelay; }

private:
	float mRate = 22050.f;
	float mTempoBpm = 120.f;
	float mSwing = 0.f;
	float mSamplesPerStep = 1000.f;
	float mPhase = 0.f;
	float mSubPhase = 0.f;
	int mStep = 0;
	int mSwingDelay = 0;
	bool mRunning = true;
	bool mTickPending = false;
	bool mResetPending = false;

	void onStepBoundary();
};
