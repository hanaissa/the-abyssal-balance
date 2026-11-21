# Experimental block-handwriting OCR model

This is a trained character classifier, not a fixed-font matcher or a canned
MIX decoder. It runs in plain C on the Pico, using flash-resident weights and
static scratch memory. It accepts separated block handwriting and normalizes
letters to uppercase. Output alphabet: `0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ`.
Spaces/newlines come from layout; colons require two observed aligned dots.
Cursive, connected characters, other punctuation and arbitrary scripts are
outside the implemented scope. `O/0` and `I/1` remain inherently ambiguous
without language/context information. No dictionary correction is performed.

## Data and training

Source: [NIST EMNIST Balanced](https://www.nist.gov/itl/products-and-services/emnist-dataset),
downloaded directly from the official NIST archive. See Cohen, Afshar, Tapson
and van Schaik, *EMNIST: an extension of MNIST*, 2017,
[paper](https://arxiv.org/abs/1702.05373). The 47 balanced labels are mapped to
36 uppercase/digit labels, merging lowercase variants. Dataset rasters are
transposed into upright orientation, thresholded and aspect-preserving
box-averaged into a centered 16x16 glyph (maximum extent 12). Training uses
rotation/scale/shear, low-resolution and stroke-width augmentation.

Training: 104800 characters. Validation: 8000 held out from the original
training partition. Test: all 18800 characters in the separate official test
partition. Seed 701; 45 epochs; 256-input / 128-ReLU / 36-output MLP, Adam.
The model is selected using validation, not test accuracy.

Weights occupy 150160 bytes of flash as const float arrays. The production
pipeline adds about 45 KB total BSS in the camera preview build, including
other preview/library state, alongside the existing 153600-byte heap frame.
The RP2040 has no hardware floating point, so latency needs physical testing.
Training happens on a computer; inference needs no Python, PC or network.

## Measured results — not production-ready

Official isolated-character test accuracy: **83.40%** before rejection.
Confidence >=0.80 and best/second probability margin >=0.35 accept **65.30%**
of test glyphs, at **96.30%** accuracy among those accepted. These probabilities
are not calibrated guarantees, especially on out-of-distribution camera input.
The actual C inference matched 668 correct of 688 accepted among the first
1024 held-out normalized test glyphs (all 1024 checked).

The complete production C pipeline was also tested on 40 composed lines made
from fixed, unselected held-out handwritten glyphs (`HELLO`, `WATER`,
`MIX 9:5:2`, `AB12`, 10 samples each): **3 accepted, 2 exactly correct**.
This low acceptance and one wrong accepted line show that the current model
is experimental. Composition tests do not establish camera-page accuracy.
Earlier notebook camera frames were rejected; no successful real-camera OCR,
UV handwriting accuracy, or on-device inference latency has been verified.
Do not use OCR success alone to authorize a reward until the recognizer is
evaluated on representative camera/UV samples and improved.

`evaluation.json` records full isolated-character results and the SHA256 of
the exported C model. The report for composed lines is generated at
`build/ocr-pipeline-evaluation.json`. Keep the model/header with the driver;
no production code depends on tests or training data.

## Reproduce

Use the existing camera Python environment (NumPy/OpenCV) for:

```powershell
& .\.venv-camera\Scripts\python.exe tools/ocr/fetch_emnist.py
& .\.venv-camera\Scripts\python.exe tools/ocr/train.py --epochs 45
cmake -S tools/ocr -B build/ocr-recognizer -G Ninja
cmake --build build/ocr-recognizer
& .\.venv-camera\Scripts\python.exe tools/ocr/evaluate_pipeline.py
ctest --test-dir build/ocr-recognizer --output-on-failure
```

Host CMake needs the installed compiler/Ninja settings used elsewhere in this
workspace. `OCR_Read` runs exactly the production C pipeline on saved raw
320x240 `.rgb565` frames. Optional CLI arguments are rotation (0..3), ROI
`x y width height`, and polarity (1 for bright text). Coordinates are after
rotation. Example:

```powershell
& .\build\ocr-recognizer\OCR_Read.exe build/camera-latest.rgb565 0 70 105 140 45
```

`APP_NOT_READY` means the pipeline rejected the image, not that it read blank
text successfully. `APP_ERROR` covers malformed frames/ROI and insufficient
output capacity. Output is cleared on failure; no partial guessed phrase is
returned. Bounds cover noise/component overflow, blank/dark images, NULL,
malformed lengths, rotation, cropping and too-small output buffers.
