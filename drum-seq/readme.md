# Pepper Grids drum sequencer

C++ Bela project for Pepper: 4-track Grids-style patterns, CV triggers and accents for kick, snare, hi-hat and FM drum. Optional Trill Bar touch layer for fills and clock-synced ratchets. Optional Trill Square for latched Grids X/Y.

## Deploy

```sh
rsync -avz ./ root@bela.local:/root/Bela/projects/drum-seq/
```

Open `drum-seq` in the Bela IDE and run.

## Trill Hub (Bar + Square)

Both sensors share I2C bus 1 through a Trill Hub. The sequencer uses fixed addresses: Bar `0x20`, Square `0x28`. Either device can be absent; the project still runs.

### Wiring check

```sh
i2cdetect -y -r 1
```

Expect `20` and `28`. Reseat Hub cabling if either address is missing.

### Trill Bar (fills)

- Touch anywhere on the bar to start a fill (snare + hi-hat ratchets). Touch position sets rate (left = slower, right = faster). Touch size sets intensity (chaos, hat density, accents). Above ~50% size mutes the kick until release.
- Pepper buttons still roll their drum while held, using the same clock-synced grid (kick 2/beat, snare 4/beat, hat 8/beat, FM 6/beat). While held, that drum’s accent CV sweeps from minimum to maximum over one bar; hi-hat decay (CV 8) opens over the same bar.
- I2C reads run only on a Bela auxiliary task; `render` never calls I2C.

**Bar touch-size calibration**

1. Set `DRUM_SEQ_TRILL_DEBUG` to `1` in `TrillControls.h` and rebuild.
2. Touch lightly with a fingertip and set `TrillCfg::kTouchSizeMin` from the console `size` (or slightly below).
3. Press firmly with a flat finger and set `kTouchSizeMax`.
4. Set debug back to `0`.

LED 6 = fill active. LED 7 = kick drop.

**Bar DoD (v1)**

- Rate zones along the bar; intensity from touch size; kick drop above threshold; clean exit on release; ratchets locked to the clock grid.

### Trill Square (pattern X/Y)

- One finger sets Grids pattern position (X/Y). Position **latches on lift**.
- Touches below `kSquareSizeMin` are ignored (palm/edge noise). Two or more fingers are ignored.
- Pots 5 and 6 still work: moving either pot by more than `kPotTakeoverDelta` after a Square touch returns control to the knobs.
- Optional slew (`kSquareSlewMs`, default 120 ms) smooths Square-driven moves so grooves do not jump mid-bar.

**Square calibration** (`DRUM_SEQ_TRILL_DEBUG 1`)

1. Set `kSquareSizeMin` just above palm/accidental graze readings.
2. Touch the four corners; set `kSquareEdgeMin` / `kSquareEdgeMax` from raw x/y so corners map to 0 and 1.
3. Adjust `kSquareInvertX` / `kSquareInvertY` so the pad orientation matches the panel.
4. Set `kSquareSlewMs` to `0` to disable slew.

LED 8 = Square is the active pattern source (latched or being touched).

**Square DoD (v1)**

- All nine Grids nodes reachable (corners, edge midpoints, center match pot extremes at those blends).
- Lifting the finger keeps the pattern.
- Palm/graze does not move position; two fingers ignored.
- Moving pot 5 or 6 takes control back.
- No harsh groove jumps mid-bar with slew enabled.

## Calibration (CV outputs)

Set `DRUM_SEQ_CALIBRATION_MODE` to `1` in `PepperIO.h` and rebuild. Outputs cycle every 0.5 s:

1. Triggers on CV 1–4  
2. Accent minimum on CV 5, 6, 7  
3. Accent maximum on CV 5, 6, 7  
4. Hat decay closed on CV 8  
5. Hat decay open on CV 8  

Adjust `PepperIO::gCalib` in `PepperIO.cpp`, then set the mode back to `0`.

## Patching

Analog outputs (panel CV jacks):

| CV | Output | Signal |
|----|--------|--------|
| 1 | Kick trigger | Fixed pulse |
| 2 | Snare trigger | Fixed pulse |
| 3 | Hi-hat trigger | Fixed pulse |
| 4 | FM trigger | Fixed pulse |
| 5 | Kick accent | Held voltage (groove or button roll sweep) |
| 6 | Snare accent | Held voltage |
| 7 | Hi-hat accent | Held voltage |
| 8 | Hi-hat decay | Held voltage (open/closed) |

FM has no accent CV; re-patch CV 5 to the FM module if you want accent there instead of kick.

Panel knobs and buttons: see project plan.
