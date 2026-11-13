#include "FillEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

// Kick, Snare, Hat, FM
static constexpr int kButtonHitsPerBeat[PepperIO::kNumDrums] = {2, 4, 8, 6};

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
		mRollProgress[drum] = 0.f;
	}
	if (mButtonHeld[drum] && !held)
		onDeactivate(drum);
	mButtonHeld[drum] = held;
}

void FillEngine::onDeactivate(unsigned int drum) {
	mLastIdx[drum] = -1;
	mLastHits[drum] = -1;
	mWaitingNextGrid[drum] = false;
	mRollProgress[drum] = 0.f;
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
	return (idx % 2) == 0;
}

bool FillEngine::shouldPlaySnareGrid(int idx) const {
	if (!mTrill.active)
		return true;
	return (idx % 2) == 1;
}

static int lastOddSlot(int hits) {
	if (hits <= 1)
		return 1;
	if ((hits % 2) == 0)
		return hits - 1;
	return std::max(1, hits - 2);
}

bool FillEngine::chaosSkip(unsigned int drum) const {
	if (!mTrill.active)
		return false;
	const float p = TrillCfg::kChaosMax * mTrill.intensity;
	if (p <= 0.f)
		return false;
	return ((float)rand() / (float)RAND_MAX) < p;
}

float FillEngine::snareAccent(int idx, int hits) const {
	if (idx == lastOddSlot(hits))
		return std::min(1.f, 0.65f + 0.35f * mTrill.intensity);
	float ghost = 0.12f + 0.15f * mTrill.intensity;
	const float jitter = 0.1f * mTrill.intensity;
	return std::max(0.f, std::min(1.f,
		ghost + jitter * (((float)rand() / RAND_MAX) - 0.5f)));
}

float FillEngine::hatAccent() const {
	return std::min(1.f, 0.35f + 0.65f * mTrill.intensity);
}

static bool trillControlsSnareHat(unsigned int drum, bool trillActive) {
	return trillActive
		&& (drum == PepperIO::kSnare || drum == PepperIO::kHat);
}

void FillEngine::tryFire(unsigned int drum, int idx, float beatPhase, int hits,
	TriggerOut& triggers, int* ledFlashFrames) {
	if (chaosSkip(drum))
		return;

	float accent = 0.7f;
	const bool trillLane = trillControlsSnareHat(drum, mTrill.active);
	if (trillLane && drum == PepperIO::kSnare) {
		accent = snareAccent(idx, hits);
	} else if (trillLane && drum == PepperIO::kHat) {
		accent = hatAccent();
		triggers.setHatDecayNow(0.f);
	} else if (mButtonHeld[drum]) {
		accent = mRollProgress[drum];
		if (drum == PepperIO::kHat)
			triggers.setHatDecayNow(accent);
	} else {
		accent = 0.7f;
	}

	triggers.fireNow(drum, accent);
	if (ledFlashFrames)
		ledFlashFrames[drum] = 1100;
}

void FillEngine::process(float beatPhase, TriggerOut& triggers, int* ledFlashFrames) {
	beatPhase = std::max(0.f, std::min(0.9999f, beatPhase));

	if (!mBeatPhaseInit) {
		mPrevBeatPhase = beatPhase;
		mBeatPhaseInit = true;
	} else {
		float delta = beatPhase - mPrevBeatPhase;
		if (delta < -0.5f)
			delta += 1.f;
		if (delta > 0.f) {
			for (unsigned d = 0; d < PepperIO::kNumDrums; ++d) {
				if (!mButtonHeld[d])
					continue;
				mRollProgress[d] = std::min(1.f,
					mRollProgress[d] + delta / kRollSweepBeats);
			}
		}
		mPrevBeatPhase = beatPhase;
	}

	if (mTrill.active && !mTrillWasActive) {
		mTrillWasActive = true;
		for (unsigned d = 0; d < PepperIO::kNumDrums; ++d) {
			if (d == PepperIO::kSnare || d == PepperIO::kHat) {
				mLastIdx[d] = -1;
				mWaitingNextGrid[d] = true;
			}
		}
		const int h = std::max(1, mTrill.hitsPerBeat);
		mLastHits[PepperIO::kSnare] = h;
		mLastHits[PepperIO::kHat] = h;
		const int idx0 = (int)std::floor(beatPhase * (float)h);
		const float frac = beatPhase * (float)h - (float)idx0;
		if (frac < 0.1f) {
			if (shouldPlayHatGrid(idx0))
				tryFire(PepperIO::kHat, idx0, beatPhase, h, triggers, ledFlashFrames);
			if (shouldPlaySnareGrid(idx0))
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

		if (mLastHits[d] >= 0 && hits != mLastHits[d]) {
			mLastIdx[d] = idx;
			mLastHits[d] = hits;
			continue;
		}
		if (mLastHits[d] < 0)
			mLastHits[d] = hits;

		if (mWaitingNextGrid[d]) {
			if (mLastIdx[d] < 0) {
				mLastIdx[d] = idx;
				continue;
			}
			if (idx != mLastIdx[d]) {
				if (d == PepperIO::kHat && !shouldPlayHatGrid(idx)) {
					mLastIdx[d] = idx;
					continue;
				}
				if (d == PepperIO::kSnare && !shouldPlaySnareGrid(idx)) {
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
			if (d == PepperIO::kSnare && !shouldPlaySnareGrid(idx)) {
				mLastIdx[d] = idx;
				continue;
			}
			tryFire(d, idx, beatPhase, hits, triggers, ledFlashFrames);
			mLastIdx[d] = idx;
		}
	}
}
