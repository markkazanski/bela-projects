#include "TrillControls.h"
#include "PepperIO.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <libraries/Trill/Trill.h>
#include <unistd.h>

struct TrillBarSnapshot {
	bool touching = false;
	float position = 0.f;
	float sizeRaw = 0.f;
};

struct TrillSquareSnapshot {
	bool validTouch = false;
	float xRaw = 0.f;
	float yRaw = 0.f;
	float sizeRaw = 0.f;
};

static Trill gTrillBar;
static Trill gTrillSquare;
static TrillBarSnapshot gBarBuffers[2];
static TrillSquareSnapshot gSquareBuffers[2];
static std::atomic<int> gBarFrontIndex{0};
static std::atomic<int> gSquareFrontIndex{0};
static bool gBarOk = false;
static bool gSquareOk = false;

#if DRUM_SEQ_TRILL_DEBUG
static unsigned gDebugCounter = 0;
#endif

static void readBarInto(TrillBarSnapshot& snap) {
	if (!gBarOk) {
		snap = {};
		return;
	}
	gTrillBar.readI2C();
	const unsigned n = gTrillBar.getNumTouches();
	if (n == 0) {
		snap.touching = false;
		snap.position = 0.f;
		snap.sizeRaw = 0.f;
	} else {
		float bestSize = -1.f;
		float bestPos = 0.f;
		float bestRaw = 0.f;
		for (unsigned i = 0; i < n; ++i) {
			const float sz = gTrillBar.touchSize((uint8_t)i);
			if (sz > bestSize) {
				bestSize = sz;
				bestPos = gTrillBar.touchLocation(i);
				bestRaw = sz;
			}
		}
		snap.touching = true;
		snap.position = std::max(0.f, std::min(1.f, bestPos));
		snap.sizeRaw = bestRaw;
	}
}

static void readSquareInto(TrillSquareSnapshot& snap) {
	snap = {};
	if (!gSquareOk)
		return;
	gTrillSquare.readI2C();
	const unsigned n = gTrillSquare.getNumTouches();
	const unsigned nh = gTrillSquare.getNumHorizontalTouches();
	if (n != 1 || nh < 1)
		return;
	const float szY = gTrillSquare.touchSize(0);
	const float szX = gTrillSquare.touchHorizontalSize(0);
	const float size = std::max(szY, szX);
	if (size < TrillCfg::kSquareSizeMin)
		return;
	snap.validTouch = true;
	snap.yRaw = std::max(0.f, std::min(1.f, gTrillSquare.touchLocation(0)));
	snap.xRaw = std::max(0.f, std::min(1.f, gTrillSquare.touchHorizontalLocation(0)));
	snap.sizeRaw = size;
}

static void trillAuxLoop(void*) {
	while (!Bela_stopRequested()) {
		if (gBarOk) {
			const int back = 1 - gBarFrontIndex.load(std::memory_order_relaxed);
			readBarInto(gBarBuffers[back]);
			gBarFrontIndex.store(back, std::memory_order_release);
		}
		if (gSquareOk) {
			const int back = 1 - gSquareFrontIndex.load(std::memory_order_relaxed);
			readSquareInto(gSquareBuffers[back]);
			gSquareFrontIndex.store(back, std::memory_order_release);
		}

#if DRUM_SEQ_TRILL_DEBUG
		if (gBarOk || gSquareOk) {
			if (++gDebugCounter >= 2) {
				gDebugCounter = 0;
				const int bi = gBarFrontIndex.load(std::memory_order_acquire);
				const int si = gSquareFrontIndex.load(std::memory_order_acquire);
				if (gBarOk)
					rt_printf("TrillBar touching=%d pos=%.3f size=%.3f\n",
						(int)gBarBuffers[bi].touching, gBarBuffers[bi].position,
						gBarBuffers[bi].sizeRaw);
				if (gSquareOk)
					rt_printf("TrillSq valid=%d x=%.3f y=%.3f size=%.3f\n",
						(int)gSquareBuffers[si].validTouch, gSquareBuffers[si].xRaw,
						gSquareBuffers[si].yRaw, gSquareBuffers[si].sizeRaw);
			}
		}
#endif
		usleep(TrillCfg::kReadIntervalUs);
	}
}

bool TrillControls::setup() {
	mBarPresent = (gTrillBar.setup(1, Trill::BAR, TrillCfg::kBarAddress) == 0);
	if (mBarPresent) {
		gBarOk = true;
		gTrillBar.printDetails();
	} else {
		fprintf(stderr, "drum-seq: Trill Bar not found at 0x%02x\n", TrillCfg::kBarAddress);
	}

	mSquarePresent = (gTrillSquare.setup(1, Trill::SQUARE, TrillCfg::kSquareAddress) == 0);
	if (mSquarePresent) {
		gSquareOk = true;
		gTrillSquare.printDetails();
	} else {
		fprintf(stderr, "drum-seq: Trill Square not found at 0x%02x\n",
			TrillCfg::kSquareAddress);
	}

	if (hardwarePresent())
		Bela_runAuxiliaryTask(trillAuxLoop);
	return true;
}

float TrillControls::mapSquareAxis(float raw, bool invert) const {
	const float span = TrillCfg::kSquareEdgeMax - TrillCfg::kSquareEdgeMin;
	float v = 0.5f;
	if (span > 0.f)
		v = (raw - TrillCfg::kSquareEdgeMin) / span;
	v = std::max(0.f, std::min(1.f, v));
	if (invert)
		v = 1.f - v;
	return v;
}

void TrillControls::pollSquare() {
	mPad.present = mSquarePresent;
	if (!mSquarePresent) {
		mPad.touching = false;
		return;
	}
	const int front = gSquareFrontIndex.load(std::memory_order_acquire);
	const TrillSquareSnapshot& snap = gSquareBuffers[front];
	mPad.touching = snap.validTouch;
	if (snap.validTouch) {
		mPad.x = mapSquareAxis(snap.xRaw, TrillCfg::kSquareInvertX);
		mPad.y = mapSquareAxis(snap.yRaw, TrillCfg::kSquareInvertY);
	}
}

void TrillControls::pollBar(BelaContext* context) {
	if (!mBarPresent) {
		mFill = {};
		return;
	}

	const int front = gBarFrontIndex.load(std::memory_order_acquire);
	const TrillBarSnapshot& snap = gBarBuffers[front];

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
			mReleaseFramesLeft -= std::max(1, (int)context->analogFrames);
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

int TrillControls::hitsForZone(int zone) const {
	zone = std::max(0, std::min(TrillCfg::kNumRateZones - 1, zone));
	return TrillCfg::kRateHitsPerBeat[zone];
}

int TrillControls::zoneFromPosition(float pos, int prevZone) const {
	pos = std::max(0.f, std::min(1.f, pos));
	const float w = 1.f / (float)TrillCfg::kNumRateZones;
	prevZone = std::max(0, std::min(TrillCfg::kNumRateZones - 1, prevZone));
	const float lowerEdge = (float)prevZone * w;
	const float upperEdge = (float)(prevZone + 1) * w;
	if (pos < lowerEdge - TrillCfg::kZoneHysteresis && prevZone > 0)
		return prevZone - 1;
	if (pos > upperEdge + TrillCfg::kZoneHysteresis
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
	pollBar(context);
	pollSquare();
}
