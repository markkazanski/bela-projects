#include "FillBar.h"
#include "FillRamp.h"
#include "PepperPins.h"
#include <algorithm>
#include <cmath>

void FillBar::configure(int /*releaseFrames*/) {}

void FillBar::setBar(const BarSample& b) {
	const bool was = mActive;
	mActive = b.touching;
	mKickDrop = b.kickDrop;
	mHits = std::max(1, b.hitsPerStep);
	mIntensity = b.size;
	if (was && !mActive) {
		mLastIdx = -1;
		mLastPhase = 0.f;
		mElapsedSteps = 0.f;
	}
	if (!was && mActive) {
		mLastIdx = -1;
		mElapsedSteps = 0.f;
	}
}

void FillBar::process(float subPhase, TriggerOut& out, int* ledFlash) {
	if (!mActive)
		return;
	subPhase = std::max(0.f, std::min(0.9999f, subPhase));
	const int h = mHits;
	const int idx = (int)std::floor(subPhase * (float)h);
	if (subPhase < mLastPhase - 0.5f) {
		mLastIdx = -1;
		mElapsedSteps += 1.f;
	}
	mLastPhase = subPhase;
	if (idx == mLastIdx)
		return;
	if (idx % 2 == 1) {
		const float ramp = fillRamp01(mElapsedSteps + subPhase);
		const float sdAcc = std::min(1.f, 0.2f + 0.8f * ramp + 0.15f * mIntensity);
		out.fireNow(PepperPins::kSd, sdAcc, true);
		if (ledFlash)
			ledFlash[1] = 800;
	} else {
		out.fireNow(PepperPins::kHh, PepperPins::gCalib.hhAccentBase, true, 0.f);
		if (ledFlash)
			ledFlash[2] = 600;
	}
	mLastIdx = idx;
}