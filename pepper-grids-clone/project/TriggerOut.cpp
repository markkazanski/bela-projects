#include "TriggerOut.h"
#include <algorithm>
#include <cmath>

void TriggerOut::setup(float analogSampleRate) {
	mPulseW = PepperPins::gCalib.trigPulseFrames;
	mGapFrames = PepperPins::gCalib.trigGapFrames;
	(void)analogSampleRate;
}

int TriggerOut::adaptivePulseWidth() const {
	const float sub = (mStepPeriod > 0.f) ? mStepPeriod / 8.f : mPulseW;
	return std::max(2, std::min(mPulseW, (int)(sub * 0.5f)));
}

void TriggerOut::tryStartPulse(unsigned part) {
	if (mPulseLeft[part] > 0) {
		mPendingTrig[part] = true;
		return;
	}
	mPulseLeft[part] = adaptivePulseWidth();
	mAccentHeld[part] = mPendingAccent[part];
	if (part != PepperPins::kFm)
		mAccentOn[part] = mPendingHasAccent[part];
}

void TriggerOut::scheduleHit(unsigned part, int delayFrames, float accent01, bool hasAccent) {
	mPendingAccent[part] = accent01;
	mPendingHasAccent[part] = hasAccent;
	mDelayLeft[part] = std::max(mGapFrames, delayFrames);
}

void TriggerOut::pulseClock(int frames) {
	mClockPulse = frames > 0 ? frames : adaptivePulseWidth();
}

void TriggerOut::fireNow(unsigned part, float accent01, bool hasAccent) {
	mPendingAccent[part] = accent01;
	mPendingHasAccent[part] = hasAccent;
	mDelayLeft[part] = mGapFrames;
}

void TriggerOut::process(int analogFrames) {
	const unsigned fm = PepperPins::kFm;
	const int lead = PepperPins::gCalib.fmAccentLeadFrames;
	const int tail = PepperPins::gCalib.fmAccentTailFrames;

	for (int n = 0; n < analogFrames; ++n) {
		if (mPendingHasAccent[fm] && mFmAccentLeft == 0 && mDelayLeft[fm] > 0
			&& mDelayLeft[fm] <= lead)
			mFmAccentLeft = mDelayLeft[fm] + adaptivePulseWidth() + tail;

		for (unsigned p = 0; p < PepperPins::kNumParts; ++p) {
			if (mDelayLeft[p] > 0) {
				mDelayLeft[p]--;
				if (mDelayLeft[p] == 0) {
					tryStartPulse(p);
					mPendingTrig[p] = false;
				}
			}
			if (mPulseLeft[p] > 0) {
				mPulseLeft[p]--;
				if (mPulseLeft[p] == 0) {
					if (p != fm)
						mAccentOn[p] = false;
				}
			}
			if (mPendingTrig[p] && mPulseLeft[p] <= 0 && mDelayLeft[p] <= 0) {
				tryStartPulse(p);
				mPendingTrig[p] = false;
			}
		}
		if (mClockPulse > 0)
			mClockPulse--;
		if (mFmAccentLeft > 0)
			mFmAccentLeft--;
	}
}

void TriggerOut::write(BelaContext* ctx, unsigned aFrame) const {
	const float hi = PepperPins::gCalib.trigHigh;
	for (unsigned p = 0; p < PepperPins::kNumCvParts; ++p) {
		const float trig = mPulseLeft[p] > 0 ? hi : 0.f;
		analogWriteOnce(ctx, aFrame, PepperPins::kOutTrig[p], trig);
		if (mAccentOn[p]) {
			const float a = PepperPins::gCalib.accentLow
				+ (PepperPins::gCalib.accentHigh - PepperPins::gCalib.accentLow)
					* mAccentHeld[p];
			analogWriteOnce(ctx, aFrame, PepperPins::kOutAccent[p], a);
		} else {
			analogWriteOnce(ctx, aFrame, PepperPins::kOutAccent[p], 0.f);
		}
	}
	analogWriteOnce(ctx, aFrame, PepperPins::kOutClock,
		mClockPulse > 0 ? hi : 0.f);
	const float fmAcc = mFmAccentLeft > 0 ? PepperPins::gCalib.fmAccentCvHigh : 0.f;
	analogWriteOnce(ctx, aFrame, PepperPins::kOutFmAccent, fmAcc);
}

void TriggerOut::writeAudio(BelaContext* ctx, unsigned audioFrame) const {
	const unsigned p = PepperPins::kFm;
	const float hi = PepperPins::gCalib.audioTrigHigh;
	const float trig = mPulseLeft[p] > 0 ? hi : 0.f;
	for (unsigned ch = 0; ch < PepperPins::kNumAudioOuts; ++ch)
		audioWrite(ctx, (int)audioFrame, (int)ch, trig);
}