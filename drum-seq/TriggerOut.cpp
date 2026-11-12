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

int TriggerOut::rollIntervalForDrum(unsigned int drum) const {
	float step = mStepPeriodFrames;
	switch (drum) {
	case PepperIO::kHat:
	case PepperIO::kSnare:
		return (int)(step * 0.5f); // 32nd
	case PepperIO::kFm:
		return (int)(step * (2.f / 3.f)); // triplet 16th
	case PepperIO::kKick:
	default:
		return (int)step; // 8th
	}
}

void TriggerOut::startRoll(unsigned int drum) {
	if (mRollActive[drum])
		return;
	mRollActive[drum] = true;
	mRollCounter[drum] = 0;
	mRollPhase[drum] = 0;
}

void TriggerOut::stopRoll(unsigned int drum) {
	mRollActive[drum] = false;
}

void TriggerOut::fireTrigger(unsigned int drum, unsigned int /*frame*/, BelaContext* /*context*/) {
	mPulseRemaining[drum] = mPulseWidth;
}

void TriggerOut::processRoll(unsigned int drum, BelaContext* context, unsigned int frame) {
	if (!mRollActive[drum])
		return;
	mRollCounter[drum]++;
	int interval = std::max(1, rollIntervalForDrum(drum));
	if (mRollCounter[drum] >= interval) {
		mRollCounter[drum] = 0;
		mRollPhase[drum]++;
		float ramp = std::min(1.f, mRollPhase[drum] * 0.15f);
		if (drum == PepperIO::kSnare)
			ramp = std::min(1.f, 0.3f + mRollPhase[drum] * 0.1f);
		setAccentNow(drum, ramp);
		fireTrigger(drum, frame, context);
	}
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
		processRoll(d, context, frame);
	}
	analogWriteOnce(context, frame, PepperIO::kOutHatDecay, mHeldHatDecay);
}
