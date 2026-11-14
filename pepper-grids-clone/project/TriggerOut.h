#pragma once

#include "PepperPins.h"

class TriggerOut {
public:
	void setup(float analogSampleRate);
	void setStepPeriodFrames(float f) { mStepPeriod = f; }

	void scheduleHit(unsigned part, int delayFrames, float accent01, bool hasAccent,
		float decay01 = -1.f);
	void fireNow(unsigned part, float accent01, bool hasAccent, float decay01 = -1.f);

	void process(int analogFrames);
	void write(BelaContext* ctx, unsigned aFrame) const;

private:
	float mStepPeriod = 1000.f;
	int mPulseW = 44;
	int mGapFrames = 22;
	int mPulseLeft[PepperPins::kNumParts] = {};
	int mDelayLeft[PepperPins::kNumParts] = {};
	float mAccentHeld[PepperPins::kNumParts] = {};
	bool mAccentOn[PepperPins::kNumParts] = {};
	bool mPendingTrig[PepperPins::kNumParts] = {};
	float mPendingAccent[PepperPins::kNumParts] = {};
	bool mPendingHasAccent[PepperPins::kNumParts] = {};
	float mPendingDecay[PepperPins::kNumParts] = {};

	float mHhDecayLive = 0.f;
	bool mHhDecayPrimed = false;

	void tryStartPulse(unsigned part);
	int adaptivePulseWidth() const;
};