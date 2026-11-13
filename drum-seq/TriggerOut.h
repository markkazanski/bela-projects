#pragma once

#include "PepperIO.h"
#include <Bela.h>

class TriggerOut {
public:
	void setup(BelaContext* context);
	void resetOutputs(BelaContext* context, unsigned int frame);

	void setAccentNow(unsigned int drum, float norm);
	void setHatDecayNow(float norm);
	void scheduleTrigger(unsigned int drum, int delayFrames, float accentLevel01);
	void scheduleTriggerWithDelay(unsigned int drum, int extraDelayFrames,
		float accentLevel01);

	void fireNow(unsigned int drum, float accentLevel01);
	void setStepPeriodFrames(float period);
	void process(BelaContext* context, unsigned int frame);

private:
	float mStepPeriodFrames = 1000.f;
	int mPreTriggerFrames = 22;
	int mPulseWidth = 22;
	float mHeldAccent[PepperIO::kNumDrums] = {};
	float mHeldHatDecay = 0.f;
	int mPulseRemaining[PepperIO::kNumDrums] = {};
	int mPendingDelay[PepperIO::kNumDrums] = {};
	float mPendingAccent[PepperIO::kNumDrums] = {};

	void fireTrigger(unsigned int drum, unsigned int frame, BelaContext* context);
};
