#include "TriggerOut.h"
#include <algorithm>
#include <cmath>

void TriggerOut::setup(BelaContext* context) {
	mPreTriggerFrames = PepperIO::msToFrames(context, (float)PepperIO::kMsBeforeTrigger);
	mPulseWidth = PepperIO::gCalib.trigPulseFrames;
	mStepPeriodFrames = context->analogSampleRate * 60.f / 120.f / 4.f;
}

void TriggerOut::setStepPeriodFrames(float period) {
	if (period > 0.f)
		mStepPeriodFrames = period;
}

void TriggerOut::resetOutputs(BelaContext* context, unsigned int frame) {
	for (unsigned int ch = 0; ch < 8; ++ch)
		analogWriteOnce(context, frame, ch, 0.f);
}

void TriggerOut::setAccentNow(unsigned int drum, float level01) {
	unsigned int ch = PepperIO::kAccentOutChannel[drum];
	if (ch == 255)
		return;
	mHeldAccent[drum] = PepperIO::accentFromLevel(level01, drum);
}

void TriggerOut::setHatDecayNow(float open01) {
	mHeldHatDecay = PepperIO::hatDecayFromOpenness(open01);
}

void TriggerOut::scheduleTrigger(unsigned int drum, int delayFrames, float accentLevel01) {
	setAccentNow(drum, accentLevel01);
	mPendingDelay[drum] = std::max(mPreTriggerFrames, delayFrames);
	mPendingAccent[drum] = accentLevel01;
}

void TriggerOut::scheduleTriggerWithDelay(unsigned int drum, int extraDelayFrames,
	float accentLevel01) {
	scheduleTrigger(drum, mPreTriggerFrames + extraDelayFrames, accentLevel01);
}

void TriggerOut::fireNow(unsigned int drum, float accentLevel01) {
	scheduleTrigger(drum, mPreTriggerFrames, accentLevel01);
}

void TriggerOut::fireTrigger(unsigned int drum, unsigned int /*frame*/, BelaContext* /*context*/) {
	mPulseRemaining[drum] = mPulseWidth;
}

void TriggerOut::process(BelaContext* context, unsigned int frame) {
	for (unsigned int d = 0; d < PepperIO::kNumDrums; ++d) {
		if (mPendingDelay[d] > 0) {
			mPendingDelay[d]--;
			if (mPendingDelay[d] == 0)
				fireTrigger(d, frame, context);
		}
		if (mPulseRemaining[d] > 0)
			mPulseRemaining[d]--;

		float trig = mPulseRemaining[d] > 0 ? PepperIO::gCalib.trigHigh : 0.f;
		analogWriteOnce(context, frame, PepperIO::kTriggerOutChannel[d], trig);

		unsigned int accentCh = PepperIO::kAccentOutChannel[d];
		if (accentCh != 255)
			analogWriteOnce(context, frame, accentCh, mHeldAccent[d]);
	}
	analogWriteOnce(context, frame, PepperIO::kOutHatDecay, mHeldHatDecay);
}
