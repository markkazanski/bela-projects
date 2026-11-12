#pragma once

#include <Bela.h>
#include <algorithm>
#include <cmath>

// Set to 1 to cycle triggers/accent/decay on all outputs for hardware calibration.
#ifndef DRUM_SEQ_CALIBRATION_MODE
#define DRUM_SEQ_CALIBRATION_MODE 0
#endif

namespace PepperIO {

enum Drum {
	kKick = 0,
	kSnare,
	kHat,
	kFm,
	kNumDrums
};

// C++ digital pins (Pd channel = pin + 11).
constexpr bool kButtonActiveLow = false;
constexpr unsigned int kLedPins[10] = {6, 7, 10, 2, 3, 0, 1, 4, 5, 8};
constexpr unsigned int kButtonPins[4] = {15, 14, 13, 12};
constexpr unsigned int kClockInPin = 11;
constexpr unsigned int kResetInPin = 9;

// Analog outputs
constexpr unsigned int kOutKickTrig = 0;
constexpr unsigned int kOutSnareTrig = 1;
constexpr unsigned int kOutHatTrig = 2;
constexpr unsigned int kOutFmTrig = 3;
constexpr unsigned int kOutSnareAccent = 4;
constexpr unsigned int kOutHatAccent = 5;
constexpr unsigned int kOutHatDecay = 6;
constexpr unsigned int kOutFmAccent = 7;

constexpr unsigned int kTriggerOutChannel[kNumDrums] = {
	kOutKickTrig, kOutSnareTrig, kOutHatTrig, kOutFmTrig};
constexpr unsigned int kAccentOutChannel[kNumDrums] = {
	255, kOutSnareAccent, kOutHatAccent, kOutFmAccent}; // kick has no accent CV

constexpr int kStepsPerBar = 16;
constexpr int kMsBeforeTrigger = 1;

struct Calib {
	float trigHigh;       // 0..1 analog value for trigger pulse
	float accentMin;      // ghost
	float accentMax;      // full
	float hatDecayClosed;
	float hatDecayOpen;
	int trigPulseFrames;  // width at analogSampleRate
};

// Defaults (~5V scale: Bela analog out is ~0..1 maps to hardware range; tune on device)
extern Calib gCalib;

inline float voltsToNorm(float volts) { return volts / 5.0f; }

inline float accentFromLevel(float level01, unsigned int drum) {
	level01 = std::max(0.f, std::min(1.f, level01));
	if (drum == kKick)
		return 0.f;
	return gCalib.accentMin + (gCalib.accentMax - gCalib.accentMin) * level01;
}

inline float hatDecayFromOpenness(float open01) {
	open01 = std::max(0.f, std::min(1.f, open01));
	return gCalib.hatDecayClosed
		+ (gCalib.hatDecayOpen - gCalib.hatDecayClosed) * open01;
}

inline int msToFrames(BelaContext* context, float ms) {
	return (int)(context->analogSampleRate * ms * 0.001f + 0.5f);
}

inline void setupDigital(BelaContext* context) {
	for (unsigned int pin : kLedPins)
		pinMode(context, 0, pin, OUTPUT);
	for (unsigned int pin : kButtonPins)
		pinMode(context, 0, pin, INPUT);
	pinMode(context, 0, kClockInPin, INPUT);
	pinMode(context, 0, kResetInPin, INPUT);
}

inline void setLed(BelaContext* context, unsigned int frame, int index, bool on) {
	if (index < 0 || index >= 10)
		return;
	digitalWrite(context, frame, kLedPins[index], on ? 1 : 0);
}

} // namespace PepperIO
