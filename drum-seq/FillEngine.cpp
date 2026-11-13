#include "FillEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

static constexpr int kButtonHitsPerBeat[PepperIO::kNumDrums] = {2, 8, 8, 6};

void FillEngine::setTrillState(const TrillFillState& state) {
	const bool was = mTrill.active;
	mTrill = state;
	if (was && !mTrill.active) {
		onDeactivate(PepperIO::kSnare);
		onDeactivate(PepperIO::kHat);
		mTrillWasActive = false;
	}
}

void FillEngine::setButtonHeld(unsigned int drum, bool held) {
	if (drum >= PepperIO::kNumDrums)
		return;
	if (held && !mButtonHeld[drum]) {
		mLastIdx[drum] = -1;
		mWaitingNextGrid[drum] = true;
	}
	if (mButtonHeld[drum] && !held)
		onDeactivate(drum);
	mButtonHeld[drum] = held;
}

void FillEngine::onDeactivate(unsigned int drum) {
	mLastIdx[drum] = -1;
	mWaitingNextGrid[drum] = false;
	if (drum == PepperIO::kSnare)
		mSnareHitCount = 0;
}

bool FillEngine::drumActive(unsigned int drum) const {
	if (drum == PepperIO::kSnare || drum == PepperIO::kHat) {
		if (mTrill.active)
			return true;
		return mButtonHeld[drum];
	}
	return mButtonHeld[drum];
}

int FillEngine::hitsPerBeatForDrum(unsigned int drum) const {
	if (mTrill.active && (drum == PepperIO::kSnare || drum == PepperIO::kHat))
		return std::max(1, mTrill.hitsPerBeat);
	return kButtonHitsPerBeat[drum];
}

bool FillEngine::shouldPlayHatGrid(int idx) const {
	if (!mTrill.active)
		return true;
	if (mTrill.intensity > 0.5f)
		return true;
	return (idx % 2) == 0;
}

bool FillEngine::chaosSkip(unsigned int drum) const {
	if (!mTrill.active)
		return false;
	const float p = TrillCfg::kChaosMax * mTrill.intensity;
	if (p <= 0.f)
		return false;
	return ((float)rand() / (float)RAND_MAX) < p;
}

float FillEngine::snareAccent() const {
	if (!mTrill.active)
		return std::min(1.f, 0.3f + mSnareHitCount * 0.1f);
	const float base = 0.3f + 0.4f * mTrill.intensity;
	return std::min(1.f, base + mSnareHitCount * 0.08f);
}

float FillEngine::hatAccent() const {
	if (!mTrill.active)
		return std::min(1.f, 0.4f + mSnareHitCount * 0.05f);
	return std::min(1.f, 0.35f + 0.65f * mTrill.intensity);
}

void FillEngine::tryFire(unsigned int drum, int idx, float beatPhase, int hits,
	TriggerOut& triggers, int* ledFlashFrames) {
	if (chaosSkip(drum))
		return;

	float accent = 0.7f;
	if (drum == PepperIO::kSnare) {
		accent = snareAccent();
		mSnareHitCount++;
		const float jitter = mTrill.active ? (0.1f * mTrill.intensity) : 0.f;
		accent = std::max(0.f, std::min(1.f, accent + jitter * (((float)rand() / RAND_MAX) - 0.5f)));
	} else if (drum == PepperIO::kHat) {
		accent = hatAccent();
		if (mTrill.active)
			triggers.setHatDecayNow(0.f);
	} else if (drum == PepperIO::kFm) {
		accent = std::min(1.f, 0.5f + mSnareHitCount * 0.07f);
	} else {
		accent = 0.85f;
	}

	triggers.fireNow(drum, accent);
	if (ledFlashFrames)
		ledFlashFrames[drum] = 1100;
}

void FillEngine::process(float beatPhase, TriggerOut& triggers, int* ledFlashFrames) {
	beatPhase = std::max(0.f, std::min(0.9999f, beatPhase));

	if (mTrill.active && !mTrillWasActive) {
		mTrillWasActive = true;
		for (unsigned d = 0; d < PepperIO::kNumDrums; ++d) {
			if (d == PepperIO::kSnare || d == PepperIO::kHat) {
				mLastIdx[d] = -1;
				mWaitingNextGrid[d] = true;
			}
		}
		mSnareHitCount = 0;
		const int h = std::max(1, mTrill.hitsPerBeat);
		const int idx0 = (int)std::floor(beatPhase * (float)h);
		const float frac = beatPhase * (float)h - (float)idx0;
		if (frac < 0.1f) {
			if (shouldPlayHatGrid(idx0))
				tryFire(PepperIO::kHat, idx0, beatPhase, h, triggers, ledFlashFrames);
			tryFire(PepperIO::kSnare, idx0, beatPhase, h, triggers, ledFlashFrames);
			mWaitingNextGrid[PepperIO::kSnare] = false;
			mWaitingNextGrid[PepperIO::kHat] = false;
			mLastIdx[PepperIO::kSnare] = idx0;
			mLastIdx[PepperIO::kHat] = idx0;
		}
	}

	for (unsigned d = 0; d < PepperIO::kNumDrums; ++d) {
		if (!drumActive(d))
			continue;
		const int hits = hitsPerBeatForDrum(d);
		const int idx = (int)std::floor(beatPhase * (float)hits);

		if (mWaitingNextGrid[d]) {
			if (idx != mLastIdx[d] || mLastIdx[d] < 0) {
				if (d == PepperIO::kHat && !shouldPlayHatGrid(idx)) {
					mLastIdx[d] = idx;
					continue;
				}
				tryFire(d, idx, beatPhase, hits, triggers, ledFlashFrames);
				mLastIdx[d] = idx;
				mWaitingNextGrid[d] = false;
			}
			continue;
		}

		if (idx != mLastIdx[d]) {
			if (d == PepperIO::kHat && !shouldPlayHatGrid(idx)) {
				mLastIdx[d] = idx;
				continue;
			}
			tryFire(d, idx, beatPhase, hits, triggers, ledFlashFrames);
			mLastIdx[d] = idx;
		}
	}
}
