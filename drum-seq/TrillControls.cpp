#include "TrillControls.h"
#include "PepperIO.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <libraries/Trill/Trill.h>
#include <unistd.h>

struct TrillRawSnapshot {
	bool touching = false;
	float position = 0.f;
	float sizeRaw = 0.f;
};

static Trill gTrill;
static TrillRawSnapshot gBuffers[2];
static std::atomic<int> gFrontIndex{0};

#if DRUM_SEQ_TRILL_DEBUG
static unsigned gDebugCounter = 0;
#endif

static void trillAuxLoop(void*) {
	while (!Bela_stopRequested()) {
		gTrill.readI2C();
		const int back = 1 - gFrontIndex.load(std::memory_order_relaxed);
		TrillRawSnapshot& snap = gBuffers[back];
		const unsigned n = gTrill.getNumTouches();
		if (n == 0) {
			snap.touching = false;
			snap.position = 0.f;
			snap.sizeRaw = 0.f;
		} else {
			float bestSize = -1.f;
			float bestPos = 0.f;
			float bestRaw = 0.f;
			for (unsigned i = 0; i < n; ++i) {
				const float sz = gTrill.touchSize((uint8_t)i);
				if (sz > bestSize) {
					bestSize = sz;
					bestPos = gTrill.touchLocation(i);
					bestRaw = sz;
				}
			}
			snap.touching = true;
			snap.position = std::max(0.f, std::min(1.f, bestPos));
			snap.sizeRaw = bestRaw;
		}
		gFrontIndex.store(back, std::memory_order_release);

#if DRUM_SEQ_TRILL_DEBUG
		if (++gDebugCounter >= 2) {
			gDebugCounter = 0;
			rt_printf("Trill touches=%u pos=%.3f size=%.3f touching=%d\n",
				n, snap.position, snap.sizeRaw, (int)snap.touching);
		}
#endif
		usleep(TrillCfg::kReadIntervalUs);
	}
}

bool TrillControls::setup() {
	if (gTrill.setup(1, Trill::BAR) != 0) {
		fprintf(stderr, "drum-seq: Trill Bar not found (sequencer runs without fills)\n");
		mPresent = false;
		return true;
	}
	gTrill.printDetails();
	mPresent = true;
	Bela_runAuxiliaryTask(trillAuxLoop);
	return true;
}

int TrillControls::hitsForZone(int zone) const {
	zone = std::max(0, std::min(TrillCfg::kNumRateZones - 1, zone));
	return TrillCfg::kRateHitsPerBeat[zone];
}

int TrillControls::zoneFromPosition(float pos, int prevZone) const {
	pos = std::max(0.f, std::min(1.f, pos));
	const float w = 1.f / (float)TrillCfg::kNumRateZones;
	prevZone = std::max(0, std::min(TrillCfg::kNumRateZones - 1, prevZone));
	const float center = (prevZone + 0.5f) * w;
	if (pos < center - w * 0.5f + TrillCfg::kZoneHysteresis && prevZone > 0)
		return prevZone - 1;
	if (pos > center + w * 0.5f - TrillCfg::kZoneHysteresis
		&& prevZone < TrillCfg::kNumRateZones - 1)
		return prevZone + 1;
	return prevZone;
}

float TrillControls::normalizeIntensity(float sizeRaw) const {
	const float span = TrillCfg::kTouchSizeMax - TrillCfg::kTouchSizeMin;
	if (span <= 0.f)
		return 0.f;
	return std::max(0.f, std::min(1.f, (sizeRaw - TrillCfg::kTouchSizeMin) / span));
}

void TrillControls::poll(BelaContext* context) {
	if (!mPresent) {
		mFill = {};
		return;
	}

	const int front = gFrontIndex.load(std::memory_order_acquire);
	const TrillRawSnapshot& snap = gBuffers[front];

	const int holdFrames = PepperIO::msToFrames(context, (float)TrillCfg::kReleaseHoldMs);
	if (snap.touching) {
		if (!mFill.active) {
			const float w = 1.f / (float)TrillCfg::kNumRateZones;
			mRateZone = std::min(TrillCfg::kNumRateZones - 1,
				(int)(snap.position / w));
		}
		mReleaseFramesLeft = holdFrames;
		mFill.active = true;
		mLastIntensity = normalizeIntensity(snap.sizeRaw);
		mFill.intensity = mLastIntensity;
		mRateZone = zoneFromPosition(snap.position, mRateZone);
		mFill.hitsPerBeat = hitsForZone(mRateZone);
		if (mKickDrop) {
			if (mFill.intensity < TrillCfg::kKickDropOff)
				mKickDrop = false;
		} else {
			if (mFill.intensity >= TrillCfg::kKickDropOn)
				mKickDrop = true;
		}
		mFill.kickDrop = mKickDrop;
	} else {
		if (mReleaseFramesLeft > 0)
			mReleaseFramesLeft--;
		if (mReleaseFramesLeft > 0) {
			mFill.active = true;
			mFill.intensity = mLastIntensity;
			mFill.hitsPerBeat = hitsForZone(mRateZone);
			mFill.kickDrop = mKickDrop;
		} else {
			mFill.active = false;
			mFill.intensity = 0.f;
			mFill.kickDrop = false;
			mKickDrop = false;
		}
	}
}
