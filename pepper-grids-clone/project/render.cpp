#include <Bela.h>
#include <algorithm>
#include <cmath>

#include "FillBar.h"
#include "GridsEngine.h"
#include "InternalClock.h"
#include "Knobs.h"
#include "PatternXY.h"
#include "PepperPins.h"
#include "TrillInput.h"
#include "TriggerOut.h"

static InternalClock gClock;
static Knobs gKnobs;
static GridsEngine gEngine;
static TriggerOut gTriggers;
static PatternXY gPattern;
static TrillInput gTrill;
static FillBar gFill;

static int gLedFlash[3] = {};
static bool gLeds[10] = {};
static bool gLedDirty = false;

static void setLed(int i, bool on) {
	if (i >= 0 && i < 10 && gLeds[i] != on) {
		gLeds[i] = on;
		gLedDirty = true;
	}
}

static void applyLeds(BelaContext* ctx, unsigned dFrame) {
	if (!gLedDirty)
		return;
	for (int i = 0; i < 10; ++i)
		PepperPins::setLed(ctx, dFrame, i, gLeds[i]);
	gLedDirty = false;
}

#if GRIDS_HW_TEST
static void renderHwTest(BelaContext* context) {
	const unsigned digN = context->digitalFrames;
	if (digN == 0)
		return;
	unsigned anaRatio = 1;
	if (context->analogFrames > 0 && digN >= context->analogFrames)
		anaRatio = digN / context->analogFrames;

	static int sHoldFrames = 0;
	static int sPhase = 0;
	static int sActiveOut = 0;
	static int sBtnRawLast[4] = {-1, -1, -1, -1};

	if (sHoldFrames == 0)
		sHoldFrames = (int)(context->analogSampleRate * 0.5f);
	const int pulseW = PepperPins::msToFrames(context->analogSampleRate, 5.f);

	for (unsigned n = 0; n < digN; ++n) {
		for (unsigned b = 0; b < 4; ++b) {
			const int raw = digitalRead(context, n, PepperPins::kButtonPins[b]);
			if (raw != sBtnRawLast[b]) {
				const bool nowPressed = PepperPins::kButtonActiveHigh
					? (raw != 0) : (raw == 0);
				rt_printf("button %u raw=%d %s\n", b, raw,
					nowPressed ? "pressed" : "released");
				sBtnRawLast[b] = raw;
			}
		}

		const bool doAna = context->analogFrames > 0 && (n % anaRatio) == 0;
		const unsigned aFrame = doAna ? n / anaRatio : 0;
		if (doAna) {
			gKnobs.processAnalog(context, aFrame);
			sPhase++;
			if (sPhase >= sHoldFrames) {
				sPhase = 0;
				sActiveOut = (sActiveOut + 1) % 8;
				rt_printf("knobs k0=%.2f k1=%.2f k2=%.2f k3=%.2f k4=%.2f k5=%.2f k6=%.2f k7=%.2f\n",
					gKnobs.knob(0), gKnobs.knob(1), gKnobs.knob(2), gKnobs.knob(3),
					gKnobs.knob(4), gKnobs.knob(5), gKnobs.knob(6), gKnobs.knob(7));
			}
			const bool pulseOn = sPhase < pulseW;
			const float hi = PepperPins::gCalib.trigHigh;
			for (unsigned ch = 0; ch < 8; ++ch)
				analogWriteOnce(context, aFrame, ch, 0.f);
			if (pulseOn)
				analogWriteOnce(context, aFrame, sActiveOut, hi);
			for (int i = 0; i < 10; ++i)
				setLed(i, i == sActiveOut);
		}
		if (n == 0)
			applyLeds(context, n);
	}
}
#endif

bool setup(BelaContext* context, void*) {
	if (context->analogInChannels < 8) {
		rt_printf("pepper-grids-clone: need 8 analog inputs\n");
		return false;
	}
	PepperPins::setupDigital(context);
	PepperPins::gCalib.trigPulseFrames =
		PepperPins::msToFrames(context->analogSampleRate, 5.f);
	PepperPins::gCalib.trigGapFrames =
		PepperPins::msToFrames(context->analogSampleRate, 1.f);

	if (!gKnobs.setup(context))
		return false;

#if GRIDS_HW_TEST
	rt_printf("pepper-grids-clone HW TEST (CV/LED sweep, knob print, raw buttons)\n");
	return true;
#else
	gClock.setup(context->analogSampleRate);
	gTriggers.setup(context->analogSampleRate);
	gTrill.setup();
	gEngine.seed(0x12345678u);
	rt_printf("pepper-grids-clone ready\n");
#endif
	return true;
}

void render(BelaContext* context, void*) {
#if GRIDS_HW_TEST
	renderHwTest(context);
	return;
#endif

	const unsigned digN = context->digitalFrames;
	if (digN == 0)
		return;
	unsigned anaRatio = 1;
	if (context->analogFrames > 0 && digN >= context->analogFrames)
		anaRatio = digN / context->analogFrames;

	static bool trillPolled = false;
	static bool freezeBtnLast = false;
	trillPolled = false;

	for (unsigned n = 0; n < digN; ++n) {
		gKnobs.processButtons(context, n);

		const bool doAna = context->analogFrames > 0 && (n % anaRatio) == 0;
		const unsigned aFrame = doAna ? n / anaRatio : 0;

		if (doAna) {
			gKnobs.processAnalog(context, aFrame);
			if (!trillPolled) {
				PadSample pad;
				gTrill.publishToPattern(pad);
				gPattern.update(pad, gKnobs.knob(0), gKnobs.knob(1), context);
				gFill.setBar(gTrill.bar());
				trillPolled = true;
			}

			if (gKnobs.buttonRising(0))
				gClock.setRunning(!gClock.running());
			if (gKnobs.buttonRising(1)) {
				gClock.requestReset();
				gEngine.resetPattern();
			}
			if (gKnobs.buttonRising(2))
				gPattern.clearLatch();
			const bool freezeBtn = gKnobs.buttonHeld(3);
			if (freezeBtn && !freezeBtnLast)
				gEngine.setFreeze(true, gClock.step());
			else if (!freezeBtn && freezeBtnLast)
				gEngine.setFreeze(false, 0);
			freezeBtnLast = freezeBtn;

			gClock.setTempoBpm(gKnobs.tempoBpm());
			gClock.setSwing(gKnobs.knob(7));
			gClock.process(1);
			gTriggers.setStepPeriodFrames(gClock.stepPeriodFrames());

			if (gClock.consumeStepTick()) {
				const int step = gClock.step();
				EngineInputs in;
				in.mapX = gPattern.mapX();
				in.mapY = gPattern.mapY();
				const BarSample barChaos = gTrill.bar();
				float chaos01 = gKnobs.knob(5);
				if (barChaos.touching)
					chaos01 = std::min(1.f, chaos01 + barChaos.size * 0.35f);
				in.chaos = (uint8_t)(chaos01 * 255.f + 0.5f);
				for (int p = 0; p < 3; ++p)
					in.density[p] = (uint8_t)(gKnobs.knob(2 + p) * 255.f + 0.5f);
				const BarSample bar = gTrill.bar();
				in.muteBd = bar.touching && bar.kickDrop;
				in.muteSd = bar.touching;
				in.muteHh = bar.touching;

				StepResult res = gEngine.tick(step, in);
				const int swing = gClock.swingDelayFrames();
				for (int p = 0; p < 3; ++p) {
					if (!res.trig[p])
						continue;
					const float acc = res.level[p] / 255.f;
					gTriggers.scheduleHit(p, swing, acc, res.accent[p]);
					gLedFlash[p] = 900;
				}

				gTriggers.pulseClock();
				if (step == 0)
					gTriggers.pulseReset();
				gClock.afterTick();
			}

			gFill.process(gClock.subStepPhase(), gTriggers, gLedFlash);
			gTriggers.process(1);
			gTriggers.write(context, aFrame);
		}

		for (int p = 0; p < 3; ++p) {
			if (gLedFlash[p] > 0)
				gLedFlash[p]--;
			setLed(p, gLedFlash[p] > 0);
		}
		const int s = gClock.step();
		setLed(4, s < 8);
		setLed(5, s >= 8 && s < 16);
		setLed(6, s >= 16 && s < 24);
		setLed(7, s >= 24);
		setLed(3, gTrill.anyTouch());
		setLed(8, gTrill.fillActive());
		setLed(9, gClock.running());

		if (n == 0)
			applyLeds(context, n);
	}
}

void cleanup(BelaContext*, void*) {}
