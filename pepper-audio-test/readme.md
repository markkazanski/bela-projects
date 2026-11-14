# Pepper audio L/R test

Standalone Bela project to test **audio out L** and **R** on Pepper: polarity pulses, continuous tone, and **DC hold** for multimeter tracing. Does not change `pepper-grids-clone`.

## Confirmed finding (Nov 2026, this unit)

- **Mode 0 (pulse):** R fires the drum TRIGGER on +; L never fires on + or − (not polarity).
- **Mode 1 (tone):** tone on R; no tone on L.
- **Mode 2 (DC):** R ≈ **+2.5 V** on step 2; L ≈ **0 V** (~−70 mV) on step 0.

**Conclusion:** hardware fault in the audio **L** path (Cape DAC, J9 line-out header, or Pepper trace/jack). Firmware is not muting L; **do not use audio out L** for triggers or audio. R is healthy.

### J9 trace (pending)

In **mode 2**, identify pins on the Cape **J9** 1×4 line-out header:

1. On **step 2** (LED 2), find the pin that reads **~+2.5 V** → that is **R** (and a pin at ~0 V on every step is likely ground).
2. On **step 0** (LED 0), one of the other pins should read **~+2.5 V** → that is **L**.

| J9 L on step 0 | Interpretation |
|----------------|----------------|
| **~+2.5 V** at J9, **0 V** at jack | Pepper-side fault: reseat header, check solder, trace, jack tip. Power off: continuity **J9 L pin → audio out L jack tip**. |
| **~0 V** at J9, R still **+2.5 V** on step 2 | Cape-side fault: reflow J9 on Cape; if still dead, left DAC channel on Cape. |

## Deploy and run

```sh
./deploy.sh
```

Open **pepper-audio-test** in the Bela IDE and run.

**Button 0** (left Pepper button) **cycles modes**. Console prints the active mode and step changes.

## Mode 0 — Pulse (default)

Every **0.5 s** one slot runs:

| Slot | LED | Audio | Pulse |
|------|-----|-------|--------|
| 0 | 0 | L | +1.0 |
| 1 | 1 | L | −1.0 |
| 2 | 2 | R | +1.0 |
| 3 | 3 | R | −1.0 |

Console: `slot N: L/R +/-`. LEDs 0–3 = slot.

| Result | Meaning |
|--------|---------|
| L fires on **−** only | L inverted (not seen on this unit) |
| L never, R on **+** | Level or **hardware** on L (current result) |
| L on **+** | L OK for triggers |

## Mode 1 — Tone

**440 Hz** sine, amplitude **0.5**, **2 s** on **L** then **2 s** on **R**. LED **4** on; LED **0** or **2** shows which side is sounding. Listen in a mixer or scope — confirms whether L carries any AC at all.

## Mode 2 — DC hold (multimeter)

Steady DC **4 s** per step. LED **5** on; LEDs **0–3** = step. Software ±0.5 ≈ **±2.5 V** on the jack (DC-coupled line out).

| Step | LED | L | R | Expect at jack tip (vs ground) |
|------|-----|---|---|--------------------------------|
| 0 | 0 | +0.5 | 0 | L ≈ +2.5 V |
| 1 | 1 | −0.5 | 0 | L ≈ −2.5 V |
| 2 | 2 | 0 | +0.5 | R ≈ +2.5 V (reference) |
| 3 | 3 | 0 | 0 | both ~0 V |

**Trace** (Bela powered; meter ground on rack ground or jack sleeve):

1. **Pepper audio out L jack tip** on step 0. ~+2.5 V → path OK to the jack (earlier trigger issue was input/cable). ~0 V → continue.
2. **Bela Cape J9 line-out L pin** (socket on Pepper side). Voltage here but not at jack → Pepper fault (header, trace, jack). Power off: continuity J9 L pin → jack tip. Reseat/reflow the 1×4 header on Pepper/Cape per [bela-pepper-wiring.md](../bela-pepper-wiring.md).
3. **No voltage at J9 L** while step 2 shows **~+2.5 V on R** → Cape left DAC channel or J9 solder on the Cape.

## After testing

Stop the project and switch back to **pepper-grids-clone**. FM trigger is on **CV 3**, not audio.