#include "FillBar.h"
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
	}
	if (!was && mActive)
		mLastIdx = -1;
}

void FillBar::process(float subPhase, TriggerOut& out, int* ledFlash) {
	if (!mActive)
		return;
	subPhase = std::max(0.f, std::min(0.9999f, subPhase));
	const int h = mHits;
	const int idx = (int)std::floor(subPhase * (float)h);
	if (subPhase < mLastPhase - 0.5f)
		mLastIdx = -1;
	mLastPhase = subPhase;
	if (idx == mLastIdx)
		return;
	if (idx % 2 == 1) {
		const float acc = std::min(1.f, 0.35f + 0.65f * mIntensity);
		out.fireNow(PepperPins::kSd, acc, true);
		if (ledFlash)
			ledFlash[1] = 800;
	} else {
		const float acc = std::min(1.f, 0.3f + 0.7f * mIntensity);
		out.fireNow(PepperPins::kHh, acc, true, 0.f);
		if (ledFlash)
			ledFlash[2] = 600;
	}
	mLastIdx = idx;
}
