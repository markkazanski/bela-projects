# Pepper Grids drum sequencer

C++ Bela project for Pepper: 4-track Grids-style patterns, CV triggers and accents for kick, snare, hi-hat and FM drum.

## Deploy

```sh
rsync -avz ./ root@bela.local:/root/Bela/projects/drum-seq/
```

Open `drum-seq` in the Bela IDE and run.

## Calibration

Set `DRUM_SEQ_CALIBRATION_MODE` to `1` in `PepperIO.h` and rebuild. Outputs cycle every 0.5 s:

1. Triggers on CV 1–4  
2. Accent minimum on CV 5, 6, 8  
3. Accent maximum on CV 5, 6, 8  
4. Hat decay closed on CV 7  
5. Hat decay open on CV 7  

Adjust `PepperIO::gCalib` in `PepperIO.cpp`, then set the mode back to `0`.

## Patching

See the project plan for CV routing and panel controls.
