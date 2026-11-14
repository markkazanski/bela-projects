#pragma once

#include "PepperPins.h"

class TriggerOut {
public:
	void setup(float analogSampleRate);
	void setStepPeriodFrames(float f) { mStepPeriod = f; }

	void scheduleHit(unsigned part, int delayFrames, float accent01, bool hasAccent);
	void fireNow(unsigned part, float accent01, bool hasAccent);
	void pulseClock(int frames = 0);

	void process(int analogFrames);
	void write(BelaContext* ctx, unsigned aFrame) const;
	void writeAudio(BelaContext* ctx, unsigned audioFrame) const;

private:
	float mStepPeriod = 1000.f;
	int mPulseW = 44;
	int mGapFrames = 22;
	int mPulseLeft[PepperPins::kNumParts] = {};
	int mDelayLeft[PepperPins::kNumParts] = {};
	float mAccentHeld[PepperPins::kNumParts] = {};
	bool mAccentOn[PepperPins::kNumParts] = {};
	int mClockPulse = 0;
	int mFmAccentLeft = 0;
	bool mPendingTrig[PepperPins::kNumParts] = {};
	float mPendingAccent[PepperPins::kNumParts] = {};
	bool mPendingHasAccent[PepperPins::kNumParts] = {};

	void tryStartPulse(unsigned part);
	int adaptivePulseWidth() const;
};