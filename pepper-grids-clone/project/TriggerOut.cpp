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
	const bool useAccentCv = part < PepperPins::kNumAccentParts;
	if (useAccentCv)
		mAccentOn[part] = mPendingHasAccent[part];
	if (part == PepperPins::kHh && mPendingDecay[part] >= 0.f) {
		mHhDecayLive = mPendingDecay[part];
		mHhDecayPrimed = false;
	}
}

void TriggerOut::scheduleHit(unsigned part, int delayFrames, float accent01,
	bool hasAccent, float decay01) {
	mPendingAccent[part] = accent01;
	mPendingHasAccent[part] = hasAccent;
	mPendingDecay[part] = decay01;
	if (part == PepperPins::kHh)
		mHhDecayPrimed = false;
	mDelayLeft[part] = std::max(mGapFrames, delayFrames);
}

void TriggerOut::fireNow(unsigned part, float accent01, bool hasAccent, float decay01) {
	mPendingAccent[part] = accent01;
	mPendingHasAccent[part] = hasAccent;
	mPendingDecay[part] = decay01;
	if (part == PepperPins::kHh)
		mHhDecayPrimed = false;
	mDelayLeft[part] = mGapFrames;
}

void TriggerOut::process(int analogFrames) {
	const int hh = PepperPins::kHh;
	const int lead = PepperPins::gCalib.hhDecayLeadFrames;

	for (int n = 0; n < analogFrames; ++n) {
		if (!mHhDecayPrimed && mPendingDecay[hh] >= 0.f && mDelayLeft[hh] > 0
			&& mDelayLeft[hh] <= lead) {
			mHhDecayLive = mPendingDecay[hh];
			mHhDecayPrimed = true;
		}

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
					if (p < PepperPins::kNumAccentParts)
						mAccentOn[p] = false;
				}
			}
			if (mPendingTrig[p] && mPulseLeft[p] <= 0 && mDelayLeft[p] <= 0) {
				tryStartPulse(p);
				mPendingTrig[p] = false;
			}
		}
	}
}

void TriggerOut::write(BelaContext* ctx, unsigned aFrame) const {
	const float hi = PepperPins::gCalib.trigHigh;
	for (unsigned p = 0; p < PepperPins::kNumParts; ++p) {
		const float trig = mPulseLeft[p] > 0 ? hi : 0.f;
		analogWriteOnce(ctx, aFrame, PepperPins::kOutTrig[p], trig);
	}
	for (unsigned p = 0; p < PepperPins::kNumAccentParts; ++p) {
		if (mAccentOn[p]) {
			const float a = PepperPins::gCalib.accentLow
				+ (PepperPins::gCalib.accentHigh - PepperPins::gCalib.accentLow)
					* mAccentHeld[p];
			analogWriteOnce(ctx, aFrame, PepperPins::kOutAccent[p], a);
		} else {
			analogWriteOnce(ctx, aFrame, PepperPins::kOutAccent[p], 0.f);
		}
	}
	const float& c = PepperPins::gCalib.hhDecayClosed;
	const float& o = PepperPins::gCalib.hhDecayOpen;
	analogWriteOnce(ctx, aFrame, PepperPins::kOutHhDecay, c + (o - c) * mHhDecayLive);
}