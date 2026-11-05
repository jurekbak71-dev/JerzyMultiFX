# Jerzy MultiFX GUI3D validation

## Compatibility contract

- VST3 processor ID: `4A4D4658-710148A5-B81D27AE-C3D64001`
- VST3 controller ID: `4A4D4658-710148A5-B81D27AE-C3D64002`
- Parameter count: 957
- Ordered parameter-key FNV-1a digest: `7ef0e24299ef3c0b`
- Design surface: 1280×720; default host viewport: 1024×576
- DSP, parameter ranges, parameter order and preset/state format: unchanged

`tests/CoreTests.cpp` verifies the parameter count and digest. `GUI_BASELINE.json` records the full ordered key list and fixed GUI geometry.

## Input and host behavior

Controls are real JUCE child components. Knob drags use a single begin/perform/end VST3 automation gesture. Plain right-click resets a knob to its declared default; Shift+right-click keeps MIDI Learn. The routing buttons commit their real parameter on mouse-down.

The Windows editor smoke test uses `SendInput`, so Windows and JUCE see the physical mouse-button state. It checks knob drag, right-click reset, routing selection, automation gestures, independent meters, resize and editor recreation against the built VST3 DLL.

## Rendering

Knobs use 101-frame sprite strips with separate shadows. Text is rendered live by JUCE. Decorative assets never intercept input. The host supplies the final pixel viewport and the JUCE child surface uses scale factor 1.0 to avoid duplicate FL Studio DPI scaling.

## Verification status

- Native shared DSP/state/RPi regression suite: PASS locally.
- PNG integrity and manifest generation: PASS locally.
- Native Windows x64 MSVC build, real DLL editor smoke test, Steinberg validator and installable package: required in GitHub Actions for every pull request.
- Physical FL Studio and physical Raspberry Pi/HeadRush: AWAITING HARDWARE VALIDATION.
