#include <Bela.h>
#include <cmath>
#include <cstdlib>

#include "Clock.h"
#include "Controls.h"
#include "PatternEngine.h"
#include "PepperIO.h"
#include "TriggerOut.h"

static Clock gClock;
static Controls gControls;
static PatternEngine gEngine;
static TriggerOut gTriggers;

static int gLedFlash[PepperIO::kNumDrums] = {};
static bool gBeatLed = false;

#if DRUM_SEQ_CALIBRATION_MODE
static int gCalibPhase = 0;
static int gCalibCounter = 0;
static float gCalibOut[8] = {};

static void runCalibration(BelaContext* context, unsigned int aFrame) {
	const int hold = (int)(context->analogSampleRate * 0.5f);
	gCalibCounter++;
	if (gCalibCounter < hold)
		return;
	gCalibCounter = 0;

	for (unsigned ch = 0; ch < 8; ++ch)
		gCalibOut[ch] = 0.f;

	switch (gCalibPhase % 5) {
	case 0:
		gCalibOut[0] = gCalibOut[1] = gCalibOut[2] = gCalibOut[3] =
			PepperIO::gCalib.trigHigh;
		break;
	case 1:
		gCalibOut[4] = gCalibOut[5] = gCalibOut[7] = PepperIO::gCalib.accentMin;
		break;
	case 2:
		gCalibOut[4] = gCalibOut[5] = gCalibOut[7] = PepperIO::gCalib.accentMax;
		break;
	case 3:
		gCalibOut[6] = PepperIO::gCalib.hatDecayClosed;
		break;
	case 4:
		gCalibOut[6] = PepperIO::gCalib.hatDecayOpen;
		break;
	}
	gCalibPhase++;
	(void)aFrame;
}

static void writeCalibOutputs(BelaContext* context, unsigned int aFrame) {
	if (context->analogFrames == 0)
		return;
	for (unsigned ch = 0; ch < 8; ++ch)
		analogWriteOnce(context, aFrame, ch, gCalibOut[ch]);
}
#endif

bool setup(BelaContext* context, void* userData) {
	if (context->analogFrames == 0 || context->analogInChannels < 8) {
		rt_printf("drum-seq: need 8 analog channels\n");
		return false;
	}

	PepperIO::setupDigital(context);
	PepperIO::gCalib.trigPulseFrames = PepperIO::msToFrames(context, 2.f);

	gEngine.setup();
	gTriggers.setup(context);
	gClock.setup(context);
	if (!gControls.setup(context))
		return false;

	srand(42);
	rt_printf("Pepper Grids drum sequencer ready\n");
	return true;
}

void render(BelaContext* context, void* userData) {
	const unsigned int digFrames = context->digitalFrames;
	if (digFrames == 0)
		return;

	const unsigned int anaFrames = context->analogFrames;
	unsigned int anaRatio = 1;
	if (anaFrames > 0 && digFrames >= anaFrames)
		anaRatio = digFrames / anaFrames;

	for (unsigned int n = 0; n < digFrames; ++n) {
#if DRUM_SEQ_CALIBRATION_MODE
		if (anaFrames > 0 && (n % anaRatio) == 0) {
			unsigned int aFrame = n / anaRatio;
			runCalibration(context, aFrame);
			writeCalibOutputs(context, aFrame);
		}
#else
		gClock.processDigital(context, n);
		gControls.processDigital(context, n);

		const bool doAnalog = (anaFrames > 0 && (n % anaRatio) == 0);
		const unsigned int aFrame = doAnalog ? (n / anaRatio) : 0;

		if (doAnalog) {
			gControls.processAnalog(context, aFrame);
			gClock.processAnalog(context, aFrame);

			gTriggers.setStepPeriodFrames(gClock.stepPeriodFrames());

			for (unsigned int d = 0; d < PepperIO::kNumDrums; ++d) {
				gEngine.setDensity(d, gControls.density(d));
				gEngine.setMuted(d, gControls.muted(d));
				if (gControls.rollHeld(d))
					gTriggers.startRoll(d);
				else
					gTriggers.stopRoll(d);
			}
			gEngine.setPatternPosition(gControls.patternX(), gControls.patternY());
			if (!gClock.externalActive())
				gClock.setTempoBpm(gControls.tempoBpm());
			gClock.setFeelSwing(gControls.feel());
			gEngine.setFeel(gControls.feel());

			if (gClock.consumeTick()) {
				if (gClock.step() == 0)
					gEngine.onNewBar(gClock.bar());
				int loose[4];
				gClock.looseDelayFrames(loose);
				gBeatLed = (gClock.step() % 4 == 0);
				gEngine.onStep(gClock.step(), gClock.bar(), gTriggers,
					gClock.swingDelayFrames(), loose, gLedFlash);
				gClock.afterTickProcessed();
			}

			gTriggers.process(context, aFrame);
		}

		for (int i = 0; i < PepperIO::kNumDrums; ++i) {
			if (gLedFlash[i] > 0)
				gLedFlash[i]--;
			gControls.setLedState(i, gLedFlash[i] > 0);
		}
		gControls.setLedState(4, gBeatLed);
		gControls.setLedState(5, gClock.externalActive());
		gControls.applyLeds(context, n);
#endif
	}
}

void cleanup(BelaContext* context, void* userData) {}
