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
constexpr float kZoneHysteresis = 0.04f;
constexpr float kChaosMax = 0.35f;
constexpr int kReleaseHoldMs = 30;
constexpr unsigned kReadIntervalUs = 5000;

constexpr uint8_t kBarAddress = 0x20;
constexpr uint8_t kSquareAddress = 0x28;
constexpr float kSquareSizeMin = 0.05f;
constexpr float kSquareEdgeMin = 0.06f;
constexpr float kSquareEdgeMax = 0.94f;
constexpr bool kSquareInvertX = false;
constexpr bool kSquareInvertY = true;
constexpr float kSquareSlewMs = 120.f;
constexpr float kPotTakeoverDelta = 0.06f;

} // namespace TrillCfg

struct TrillFillState {
	bool active = false;
	int hitsPerBeat = 4;
	float intensity = 0.f;
	bool kickDrop = false;
};

struct TrillPadState {
	bool present = false;
	bool touching = false;
	float x = 0.f;
	float y = 0.f;
};

class TrillControls {
public:
	bool setup();
	void poll(BelaContext* context);

	TrillFillState fillState() const { return mFill; }
	TrillPadState padState() const { return mPad; }
	bool barPresent() const { return mBarPresent; }
	bool squarePresent() const { return mSquarePresent; }
	bool hardwarePresent() const { return mBarPresent || mSquarePresent; }

private:
	bool mBarPresent = false;
	bool mSquarePresent = false;
	TrillFillState mFill;
	TrillPadState mPad;
	int mRateZone = 2;
	bool mKickDrop = false;
	int mReleaseFramesLeft = 0;
	float mLastIntensity = 0.f;

	int hitsForZone(int zone) const;
	int zoneFromPosition(float pos, int prevZone) const;
	float normalizeIntensity(float sizeRaw) const;
	float mapSquareAxis(float raw, bool invert) const;
	void pollBar(BelaContext* context);
	void pollSquare();
};
