# Pepper Grids clone

Independent Bela + Pepper project: Grids-style sequencer (BD / SD / HH) plus a 4th FM voice driven from the mirrored map. Control with Pepper knobs and buttons, Trill Square (pattern X/Y) and Trill Bar (fills). **Internal clock only** — CV input jacks are not used (Eurorack +12 V, knob-only mode).

## Deploy

```sh
./deploy.sh
```

(`rsync --delete` of `project/` only — do **not** sync the repo root onto Bela.)

Open `pepper-grids-clone` in the Bela IDE and run.

## Pattern view (browser)

While the project is running, open **http://bela.local/gui** (or use the **GUI** button in the Bela IDE). The view is read-only: a **5×5 map** with the current pattern position and a **4×32** grid (BD / SD / HH / FM) showing step levels, density gating, accents, playhead, freeze loop, and Bar fill state. Set `GRIDS_GUI` to `0` in [PepperPins.h](project/PepperPins.h) to disable the GUI task.

## Hardware bring-up

### 1. I/O test mode

In [PepperPins.h](project/PepperPins.h) set `GRIDS_HW_TEST` to `1`, deploy, run.

- CV outs 0–7 pulse in order (~0.5 s each); the LED with the same index should flash. Wrong order → fix `kLedPins[]`.
- Adjust `trigHigh` in [PepperPins.h](project/PepperPins.h) if CV pulses are low.
- Console prints `k0..k7` each step; knobs should read ~0.00 CCW and ~1.00 CW on +12 V.
- Button lines show `button N raw=0|1`; if press appears as `low`, set `kButtonActiveHigh` to `false`.

Set `GRIDS_HW_TEST` back to `0` when done.

### 2. Trill calibration

In [TrillInput.h](project/TrillInput.h) set `GRIDS_TRILL_DEBUG` to `1`, deploy, run. Console (~5 Hz) shows raw Bar/Square values.

Constants below reflect the Nov 2025 bring-up log (edit `namespace Cfg` in [TrillInput.cpp](project/TrillInput.cpp) if your sensors differ):

| Constant | Set from |
|----------|----------|
| `kBarSizeMin` / `kBarSizeMax` | Bar `size` light vs firm touch |
| `kKickDropOn` / `kKickDropOff` | Firm press vs release (0.70 / 0.50 norm; hysteresis) |
| `kSquareSizeMin` / `kSquareSizeMax` | Fingertip vs palm graze `sz` |
| `kSquareXMin` / `kSquareXMax` | Left vs right corner `xR` (unit: 0.13 / 0.91) |
| `kSquareYMin` / `kSquareYMax` | Top vs bottom corner `yR` (unit: 0.08 / 0.90) |
| `kSquareInvertX` / `kSquareInvertY` / `kSquareSwapXY` | Pad orientation vs `map` |

Set `GRIDS_TRILL_DEBUG` to `0` when done.

**Square corners:** Grids nodes sit on the pad corners. Slide into a corner — the firmware keeps the last good axis while the sensor drops to one dimension (`mode=edgeX` or `edgeY` in debug). A cold tap that only registers on one axis is ignored to avoid ghosts. While dragging, each axis follows the reading nearest the previous position (same idea as the Bar), so a resting hand or phantom blob does not pull the map away.

**Bar tracking:** With one finger held, the Bar follows the contact nearest the previous position (debug `fi=` index) instead of jumping to the largest blob. Two-finger pinch/spread enters density mode instead (see below).

**Bar density macro:** Two fingers on the Bar (minimum separation `kBarPinchMinSep` in [TrillInput.cpp](project/TrillInput.cpp)) enter **density mode**. Pinch (closer) thins all four voices evenly; spread (wider) makes them busier, relative to the finger distance at touchdown. Knob mix is preserved (silent voices stay silent). Lift both fingers to snap back to knob densities. One-finger holds still need ~25 ms (`kBarFillOnsetUs`) before fills engage so a quick second finger does not flicker a fill.

**Bar LED display:** While any finger is on the Bar, all ten Pepper LEDs show touch position instead of run/fill/status: one finger lights a single LED that tracks left/right along the row; two fingers light two LEDs at each contact so the gap between lit LEDs matches finger spacing. The display follows live touch (not the fill release tail). If the LED direction is opposite the Bar, set `kBarLedInvert` to `true` in `namespace Cfg` in [TrillInput.cpp](project/TrillInput.cpp).

**Bar fills:** While the Bar is held with one finger, alternating snare and hi-hat hits fire at the zone rate. Snare loudness **crescendos** over about one bar (`kFillRampSteps` in [FillRamp.h](project/FillRamp.h)) with a slight end-weighted curve; light pressure adds a small boost on top. Fill hi-hats stay at `hhAccentBase` (no ramp) so the snare build stays forward. Releasing the Bar resets the ramp. Firm pressure still triggers kick drop (BD muted).

### I2C errors

A single failed read (`errno 121`, Remote I/O error) usually means the sensor did not ACK: polling too fast while a scan is in progress, a stale I2C file descriptor, or wiring/noise. Square and Bar run in **separate aux tasks** at **12 ms** and **10 ms** (`kSquarePollUs` / `kBarPollUs` in [TrillInput.cpp](project/TrillInput.cpp)), matching Bela’s Trill examples instead of the old 5 ms shared loop.

At **startup**, each sensor is initialised up to **5** times (`kSetupAttempts`, 300 ms apart). If one still fails, the project runs anyway: knobs 4/5 drive the map until the Square comes up; the aux task keeps retrying until `Trill … recovered` and `printDetails()` appear.

On **read failure** while running, the firmware clears touch state, prints `Trill … read error, retrying`, closes the I2C fd, and calls `setup()` again (500 ms between attempts; after 3 failed attempts, 2 s until the device comes back). Keep fingers off the sensor briefly after recovery so baseline settles.

On **firmware 2** (Bar/Square), `reset` / `setScanTrigger` “failed to write” lines during setup are normal library noise. Treat **`Unable to identify device`** as the real failure (sensor did not answer).

If errors persist:

- Run `i2cdetect -y -r 1` several times; `0x20` and `0x28` should always appear.
- Shorten Hub cabling, reseat connectors, route away from the Eurorack ribbon/PSU.
- Try raising `kSquarePollUs` to `20000` to rule out timing vs. hardware.

### 3. Normal run checklist

- [ ] Knob 6 centre ≈ 140 BPM
- [ ] Knobs 0–3: per-voice density; knobs 4/5: map X/Y
- [ ] Knob 7: Feel (CCW half = swing; CW half = looseness + chaos)
- [ ] Buttons: run/stop, reset, clear Square latch, hold freeze
- [ ] Bar: all rate zones; light touch keeps BD; hard press kick drop
- [ ] Bar: one finger — one LED tracks along the row; two fingers — two LEDs show spacing
- [ ] Bar: two-finger pinch thins, spread fattens; release restores knob densities
- [ ] Square: latch, knob takeover, palm ignored
- [ ] FM drum: **CV 3** → trigger on your FM voice
- [ ] Hi-hat: **CV 6** level on every hit (`hhAccentBase` / `hhAccentPeak`); **CV 7** → decay (one open hat per bar; tune `hhDecayOpen`)
- [ ] No underruns; CPU stable

Record any pin or polarity changes in the `PepperPins.h` comment block.

## Control map

| Knob | Function |
|------|----------|
| 0 / 1 / 2 / 3 | BD / SD / HH / FM density (CCW = silent) |
| 4 / 5 | Map X / Y |
| 6 | Tempo 40–240 BPM |
| 7 | Feel: 0–50% swing; 50–100% looseness + chaos (Trill Bar still adds chaos on touch) |

| Button | Function |
|--------|----------|
| 0 | Run / stop (toggle) |
| 1 | Reset pattern |
| 2 | Clear Square latch |
| 3 | Hold: freeze 4-step loop |

| CV out | Signal |
|--------|--------|
| 0 / 1 / 2 / 3 | BD / SD / HH / FM trigger (~5 ms) |
| 4 / 5 / 6 | BD / SD / HH accent (BD/SD gate-style; HH is a level on every hit via `hhAccentBase` / `hhAccentPeak`) |
| 7 | HH decay CV (~one open hat per 16-step bar; `hhDecayClosed` / `hhDecayOpen` in [PepperPins.h](project/PepperPins.h)) |

**Hi-hat accent (CV 6):** Every HH trigger sends a voltage in a narrow range—unaccented steps use `hhAccentBase`, accented steps use `hhAccentPeak` (defaults 0.35 / 0.55). Trill Bar fill hats are fixed at `hhAccentBase`. Set `hhAccentBase` to `0` if your module needs a gate-only accent input.

**Hi-hat decay (CV 7):** At the start of each bar (steps 0 and 16) the firmware picks at most one open-hat step: an HH hit with room to ring (gap ≥ 2 to the next hat), preferring off-beat “and” positions. All other hats are closed. CV 7 updates ~1 ms before the trigger and holds until the next HH hit. Patch **CV 7 → hat DECAY CV**.

FM pattern lane reads the Grids map at the mirrored position `(255−X, 255−Y)` using snare (SD) node data — related groove, opposite corner of the pad.

## Trill Hub

Bus 1: Bar `0x20`, Square `0x28`.

```sh
i2cdetect -y -r 1
```

## Host tests

```sh
cmake -S test -B build && cmake --build build && ctest --test-dir build
```

Regenerate patterns: `python3 tools/gen_patterns.py`

## Licence

GPLv3 — see [LICENSE](LICENSE). Pattern tables from Mutable Instruments Grids.