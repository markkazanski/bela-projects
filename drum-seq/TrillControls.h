#pragma once

#include <Bela.h>
#include <atomic>
#include <cstdint>

// Set to 1 for aux-thread raw touch prints (~10 Hz) during calibration.
#ifndef DRUM_SEQ_TRILL_DEBUG
#define DRUM_SEQ_TRILL_DEBUG 0
#endif

namespace TrillCfg {

// Calibrate on hardware: lightest fingertip vs flat firm finger (see readme).
constexpr float kTouchSizeMin = 0.02f;
constexpr float kTouchSizeMax = 0.60f;
constexpr float kKickDropOn = 0.55f;
constexpr float kKickDropOff = 0.45f;
constexpr int kRateHitsPerBeat[] = {2, 4, 6, 8, 12};
constexpr int kNumRateZones = 5;
constexpr float kZoneHysteresis = 0.03f;
constexpr float kChaosMax = 0.35f;
constexpr int kReleaseHoldMs = 30;
constexpr unsigned kReadIntervalUs = 5000;

} // namespace TrillCfg

struct TrillFillState {
	bool active = false;
	int hitsPerBeat = 4;
	float intensity = 0.f;
	bool kickDrop = false;
};

class TrillControls {
public:
	bool setup();
	void poll(BelaContext* context);

	TrillFillState fillState() const { return mFill; }
	bool hardwarePresent() const { return mPresent; }

private:
	bool mPresent = false;
	TrillFillState mFill;
	int mRateZone = 2;
	bool mKickDrop = false;
	int mReleaseFramesLeft = 0;
	float mLastIntensity = 0.f;

	int hitsForZone(int zone) const;
	int zoneFromPosition(float pos, int prevZone) const;
	float normalizeIntensity(float sizeRaw) const;
};
