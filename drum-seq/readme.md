# Pepper Grids drum sequencer

C++ Bela project for Pepper: 4-track Grids-style patterns, CV triggers and accents for kick, snare, hi-hat and FM drum. Optional Trill Bar touch layer for fills and clock-synced ratchets.

## Deploy

```sh
rsync -avz ./ root@bela.local:/root/Bela/projects/drum-seq/
```

Open `drum-seq` in the Bela IDE and run.

## Trill Bar

- Touch anywhere on the bar to start a fill (snare + hi-hat ratchets). Touch position sets rate (left = slower, right = faster). Touch size sets intensity (chaos, hat density, accents). Above ~50% size mutes the kick until release.
- Pepper buttons still roll their drum while held, using the same clock-synced grid (kick 2/beat, snare/hat 8/beat, FM 6/beat).
- I2C reads run only on a Bela auxiliary task; `render` never calls I2C.

### Wiring check

On the Bela shell:

```sh
i2cdetect -y -r 1
```

You should see `20` at address `0x20`. If the bar is missing intermittently, reseat the cable and shielding (SDA/SCL to the Trill, 3.3 V, GND, I2C bus 1).

### Touch-size calibration

1. Set `DRUM_SEQ_TRILL_DEBUG` to `1` in `TrillControls.h` and rebuild.
2. Touch lightly with a fingertip and note `size` in the console; set `TrillCfg::kTouchSizeMin` in the same header to that value (or slightly below).
3. Press with a flat finger firmly and set `kTouchSizeMax` from that reading.
4. Set debug back to `0` and redeploy.

### Definition of done (v1)

- **Rate:** moving along the bar changes ratchet speed in distinct zones.
- **Intensity:** harder/larger touch increases accents, occasional skips, and hi-hat density.
- **Kick drop:** kick pattern stops when size crosses the threshold; resumes on release.
- **Clean exit:** releasing the bar stops ratchets immediately; pattern drums resume on the next step.
- **Sync:** ratchets stay on the beat grid with internal tempo and with external clock on pin 11.

LED 6 = fill active. LED 7 = kick drop.

## Calibration (CV outputs)

Set `DRUM_SEQ_CALIBRATION_MODE` to `1` in `PepperIO.h` and rebuild. Outputs cycle every 0.5 s:

1. Triggers on CV 1–4  
2. Accent minimum on CV 5, 6, 8  
3. Accent maximum on CV 5, 6, 8  
4. Hat decay closed on CV 7  
5. Hat decay open on CV 7  

Adjust `PepperIO::gCalib` in `PepperIO.cpp`, then set the mode back to `0`.

## Patching

See the project plan for CV routing and panel controls.
