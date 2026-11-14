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
- Sweep index **8**: FM trigger on **both** audio outs (no accent) — quiet hit.
- Sweep index **9**: same audio trigger **plus CV 7 high** (~1 ms before and through the pulse) — loud accented hit. Patch **audio R → FM TRIGGER**, **CV 7 → FM ACCENT** to hear the difference.
- Adjust `audioTrigHigh` / `fmAccentCvHigh` in [PepperPins.h](project/PepperPins.h) if levels are low.
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

**Bar tracking:** With two simultaneous touches, the Bar follows the contact nearest the previous position (debug `fi=` index) instead of jumping to the largest blob.

### I2C errors

A single failed read (`Remote I/O error`) used to disable the Trill library until restart. The aux task now clears the touch state, prints `Trill Bar read error, retrying`, and calls `setup()` again every 500 ms. On success you see `Trill Bar recovered` (confirmed on hardware). Keep fingers off the sensor briefly after recovery so baseline settles. If errors are frequent, shorten Hub cabling and keep it away from the Eurorack power bus.

### 3. Normal run checklist

- [ ] Knob 6 centre ≈ 140 BPM; CV 6 = clock 16ths; CV 7 high on accented FM hits
- [ ] Density knobs CCW = silent drum; FM density follows HH knob (4) unless unlinked
- [ ] Buttons: run/stop, reset, clear latch / FM shift, hold freeze
- [ ] Bar: all rate zones; light touch keeps BD; hard press kick drop
- [ ] Square: latch, knob takeover, palm ignored
- [ ] FM drum: audio R (or L) → TRIGGER, CV 7 → ACCENT
- [ ] No underruns; CPU stable

Record any pin or polarity changes in the `PepperPins.h` comment block.

## Control map

| Knob | Function |
|------|----------|
| 0 / 1 | Map X / Y |
| 2 / 3 / 4 | BD / SD / HH density (CCW = silent) |
| 4 (shift) | Hold button 2 + turn: FM density (unlinks from HH); HH frozen while held |
| 5 | Chaos |
| 6 | Tempo 40–240 BPM |
| 7 | Swing |

| Button | Function |
|--------|----------|
| 0 | Run / stop (toggle) |
| 1 | Reset pattern |
| 2 | Hold + knob 4: FM density layer; **release without moving knob** clears Square latch |
| 3 | Hold: freeze 4-step loop |

| CV out | Signal |
|--------|--------|
| 0 / 1 / 2 | BD / SD / HH trigger |
| 3 / 4 / 5 | BD / SD / HH accent |
| 6 | Clock out (16ths) |
| 7 | FM drum accent gate (+5 V style; `fmAccentCvHigh`, lead/tail in [PepperPins.h](project/PepperPins.h)) |

| Audio out | Signal |
|-----------|--------|
| 0 / 1 | FM drum trigger (same pulse on L and R; AC-coupled; `audioTrigHigh`) |

**FM drum patching:** audio out **R** → TRIGGER (use whichever channel fires on your unit), **CV 7** → ACCENT. Accented sequencer steps should sound louder; unaccented steps stay quieter.

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