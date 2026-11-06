# GUI comparison

The baseline is the 1280×720 Jerzy MultiFX layout recorded in `GUI_BASELINE.json`. Its geometry, VST3 class IDs and ordered list of 957 parameter keys are locked by regression tests.

The Windows packaging job places fresh editor captures from the real VST3 DLL in this directory inside the installable package. User-supplied reference images are identified by hashes in the asset manifest and are not redistributed.

The change replaces procedural low-resolution knob drawing with independent 101-frame assets, adds separate panel, switch, LED and VU layers, and keeps the existing labels, sections, hit targets and control bindings.
