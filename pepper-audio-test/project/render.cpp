#include <Bela.h>
#include <cmath>

// Pepper LED pins left→right (same as pepper-grids-clone kLedPins[]).
static constexpr unsigned kLedPins[10] = {6, 7, 10, 2, 3, 0, 1, 4, 5, 8};
static constexpr unsigned kButtonPin = 15;
static constexpr bool kButtonActiveHigh = true;
static constexpr int kDebThresh = 15;
static constexpr float kPi = 3.14159265f;
static constexpr float kToneAmp = 0.5f;
static constexpr float kToneHz = 440.f;

enum Mode { kModePulse = 0, kModeTone = 1, kModeDc = 2, kNumModes = 3 };

static int gMode = kModePulse;
static int gPhase = 0;

static int gPulsePeriod = 0;
static int gPulseW = 0;
static int gLastPulseSlot = -1;

static int gTone2s = 0;
static double gSinePhase = 0.0;
static int gLastToneHalf = -1;

static int gDcStepPeriod = 0;
static int gLastDcStep = -1;

static int gDebCount = 0;
static bool gBtnDebounced = false;
static float gSampleRate = 44100.f;

static void printMode(int mode) {
	switch (mode) {
	case kModePulse:
		rt_printf("mode 0 pulse: button cycles modes; L+/L-/R+/R- slots 0.5 s\n");
		break;
	case kModeTone:
		rt_printf("mode 1 tone: 440 Hz, 2 s L then 2 s R (amp 0.5)\n");
		break;
	case kModeDc:
		rt_printf("mode 2 DC: 4 s/step — 0 L+0.5, 1 L-0.5, 2 R+0.5, 3 both 0\n");
		break;
	default:
		break;
	}
}

static void onModeChange() {
	gPhase = 0;
	gSinePhase = 0.0;
	gLastPulseSlot = -1;
	gLastToneHalf = -1;
	gLastDcStep = -1;
	printMode(gMode);
}

static void processButton(BelaContext* ctx, unsigned digN) {
	for (unsigned d = 0; d < digN; ++d) {
		const int raw = digitalRead(ctx, d, kButtonPin);
		const bool pressed = kButtonActiveHigh ? (raw != 0) : (raw == 0);
		if (pressed) {
			if (gDebCount < kDebThresh)
				gDebCount++;
		} else
			gDebCount = 0;
		const bool deb = gDebCount >= kDebThresh;
		if (deb && !gBtnDebounced) {
			gMode = (gMode + 1) % kNumModes;
			onModeChange();
		}
		gBtnDebounced = deb;
	}
}

static void updateLeds(BelaContext* ctx, unsigned dFrame, int pulseSlot, int toneHalf,
	int dcStep) {
	for (int i = 0; i < 10; ++i)
		digitalWrite(ctx, dFrame, kLedPins[i], 0);

	if (gMode == kModePulse) {
		if (pulseSlot >= 0 && pulseSlot < 4)
			digitalWrite(ctx, dFrame, kLedPins[pulseSlot], 1);
		return;
	}
	if (gMode == kModeTone) {
		digitalWrite(ctx, dFrame, kLedPins[4], 1);
		if (toneHalf == 0)
			digitalWrite(ctx, dFrame, kLedPins[0], 1);
		else if (toneHalf == 1)
			digitalWrite(ctx, dFrame, kLedPins[2], 1);
		return;
	}
	digitalWrite(ctx, dFrame, kLedPins[5], 1);
	if (dcStep >= 0 && dcStep < 4)
		digitalWrite(ctx, dFrame, kLedPins[dcStep], 1);
}

static void printDcStep(int step) {
	switch (step) {
	case 0:
		rt_printf("mode 2 DC step 0: L=+0.5 (expect ~+2.5 V) R=0\n");
		break;
	case 1:
		rt_printf("mode 2 DC step 1: L=-0.5 (expect ~-2.5 V) R=0\n");
		break;
	case 2:
		rt_printf("mode 2 DC step 2: L=0 R=+0.5 (expect ~+2.5 V on R)\n");
		break;
	case 3:
		rt_printf("mode 2 DC step 3: L=0 R=0\n");
		break;
	default:
		break;
	}
}

bool setup(BelaContext* ctx, void*) {
	gSampleRate = ctx->audioSampleRate;
	gPulsePeriod = (int)(gSampleRate * 0.5f);
	gPulseW = (int)(gSampleRate * 0.005f + 0.5f);
	if (gPulseW < 1)
		gPulseW = 1;
	gTone2s = (int)(gSampleRate * 2.f);
	gDcStepPeriod = (int)(gSampleRate * 4.f);

	for (unsigned p : kLedPins)
		pinMode(ctx, 0, p, OUTPUT);
	pinMode(ctx, 0, kButtonPin, INPUT);

	printMode(gMode);
	rt_printf("pepper-audio-test: press button 0 (left) to cycle modes\n");
	return true;
}

void render(BelaContext* ctx, void*) {
	const unsigned digN = ctx->digitalFrames;
	processButton(ctx, digN);
	const unsigned dFrame = digN > 0 ? digN - 1 : 0;

	int pulseSlot = gLastPulseSlot;
	int toneHalf = gLastToneHalf;
	int dcStep = gLastDcStep;
	float outL = 0.f;
	float outR = 0.f;

	for (unsigned n = 0; n < ctx->audioFrames; ++n) {
		if (gMode == kModePulse) {
			const int inSlot = gPhase / gPulsePeriod;
			const int slot = ((inSlot % 4) + 4) % 4;
			const int pos = gPhase % gPulsePeriod;
			const bool on = pos < gPulseW;
			const float v = on ? ((slot % 2 == 0) ? 1.f : -1.f) : 0.f;
			const int ch = slot < 2 ? 0 : 1;
			outL = ch == 0 ? v : 0.f;
			outR = ch == 1 ? v : 0.f;
			pulseSlot = slot;
			if (on && pos == 0) {
				const char* side = ch == 0 ? "L" : "R";
				const char* pol = v > 0.f ? "+" : "-";
				rt_printf("slot %d: %s %s\n", slot, side, pol);
			}
		} else if (gMode == kModeTone) {
			const int half = (gPhase / gTone2s) % 2;
			toneHalf = half;
			const float s = kToneAmp
				* std::sin((float)(gSinePhase * 2.0 * kPi * kToneHz / gSampleRate));
			gSinePhase += 1.0;
			if (half == 0) {
				outL = s;
				outR = 0.f;
			} else {
				outL = 0.f;
				outR = s;
			}
		} else {
			const int step = (gPhase / gDcStepPeriod) % 4;
			dcStep = step;
			switch (step) {
			case 0:
				outL = 0.5f;
				outR = 0.f;
				break;
			case 1:
				outL = -0.5f;
				outR = 0.f;
				break;
			case 2:
				outL = 0.f;
				outR = 0.5f;
				break;
			default:
				outL = 0.f;
				outR = 0.f;
				break;
			}
		}

		audioWrite(ctx, (int)n, 0, outL);
		audioWrite(ctx, (int)n, 1, outR);
		gPhase++;
	}

	if (gMode == kModePulse && pulseSlot != gLastPulseSlot) {
		gLastPulseSlot = pulseSlot;
		updateLeds(ctx, dFrame, pulseSlot, -1, -1);
	} else if (gMode == kModeTone && toneHalf != gLastToneHalf) {
		gLastToneHalf = toneHalf;
		rt_printf("mode 1 tone: %s\n", toneHalf == 0 ? "L" : "R");
		updateLeds(ctx, dFrame, -1, toneHalf, -1);
	} else if (gMode == kModeDc && dcStep != gLastDcStep) {
		gLastDcStep = dcStep;
		printDcStep(dcStep);
		updateLeds(ctx, dFrame, -1, -1, dcStep);
	}

	if (gMode == kModeTone && gLastToneHalf < 0) {
		gLastToneHalf = toneHalf;
		rt_printf("mode 1 tone: %s\n", toneHalf == 0 ? "L" : "R");
		updateLeds(ctx, dFrame, -1, toneHalf, -1);
	}
	if (gMode == kModeDc && gLastDcStep < 0) {
		gLastDcStep = dcStep;
		printDcStep(dcStep);
		updateLeds(ctx, dFrame, -1, -1, dcStep);
	}
}

void cleanup(BelaContext*, void*) {}