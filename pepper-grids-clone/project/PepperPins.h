#pragma once

#include <Bela.h>

// Hardware notes (confirm on your unit and edit below):
// LEDs C++ pins left→right: {6,7,10,2,3,0,1,4,5,8} (Pd out = pin + 11).
// Buttons: pins {15,14,13,12}; kButtonActiveHigh (default true) — use GRIDS_HW_TEST.
// Trill cal constants: TrillInput.cpp namespace Cfg — use GRIDS_TRILL_DEBUG.
// CV pulse level: gCalib.trigHigh in PepperPins.cpp (default 0.85).
// Knob full-scale: Knobs.cpp kKnobMax[] (measured CW on +12 V).
// LEDs: use digitalWrite (not digitalWriteOnce) so state persists across blocks.

#ifndef GRIDS_HW_TEST
#define GRIDS_HW_TEST 0
#endif
#ifndef GRIDS_GUI
#define GRIDS_GUI 1
#endif
// Digital clock jumpers on pins 9/11 are NOT used (internal clock only).
// Trill Hub: I2C bus 1, Bar 0x20, Square 0x28.

namespace PepperPins {

enum Part { kBd = 0, kSd = 1, kHh = 2, kFm = 3, kNumParts = 4 };
constexpr unsigned kNumCvParts = 3;

constexpr bool kButtonActiveHigh = true;
constexpr unsigned kLedPins[10] = {6, 7, 10, 2, 3, 0, 1, 4, 5, 8};
constexpr unsigned kButtonPins[4] = {15, 14, 13, 12};

constexpr unsigned kOutTrig[kNumCvParts] = {0, 1, 2};
constexpr unsigned kOutAccent[kNumCvParts] = {3, 4, 5};
constexpr unsigned kOutClock = 6;
constexpr unsigned kOutFmAccent = 7;

constexpr unsigned kNumAudioOuts = 2;

constexpr int kStepsPerPattern = 32;

struct Calib {
	float trigHigh = 0.85f;
	float accentLow = 0.15f;
	float accentHigh = 0.85f;
	float audioTrigHigh = 1.0f;
	float fmAccentCvHigh = 1.0f;
	int fmAccentLeadFrames = 22;
	int fmAccentTailFrames = 44;
	int trigPulseFrames = 44;   // set in setup (~5 ms @ 22.05 kHz)
	int trigGapFrames = 22;   // ~1 ms low before re-fire
};

extern Calib gCalib;

inline int msToFrames(float sampleRate, float ms) {
	return (int)(sampleRate * ms * 0.001f + 0.5f);
}

inline void setupDigital(BelaContext* ctx) {
	for (unsigned p : kLedPins)
		pinMode(ctx, 0, p, OUTPUT);
	for (unsigned p : kButtonPins)
		pinMode(ctx, 0, p, INPUT);
}

inline void setLed(BelaContext* ctx, unsigned frame, int index, bool on) {
	if (index < 0 || index >= 10)
		return;
	digitalWrite(ctx, frame, kLedPins[index], on ? 1 : 0);
}

} // namespace PepperPins