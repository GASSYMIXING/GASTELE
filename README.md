# GASTELE 1.2 — Gassymixing

Windows x64 VST3 telephone / lo-fi effect. The approved art is embedded in the binary; no external image installation is needed.

Release 1.2 adds an optional Realphone EQ after the entire legacy style/MIX/OUTPUT chain. Default OFF preserves 1.1 audio exactly. Global bypass bypasses this EQ too. The installer supports staged-copy verification, backup, locked-file rejection and rollback.

## Build

Requirements: Windows 10/11 x64, Visual Studio 2022 C++ Build Tools and Windows SDK, CMake 3.24+, VST3 SDK **v3.8.0_build_66** with `base`, `cmake`, `pluginterfaces`, and `public.sdk` submodules.

```powershell
git clone --branch v3.8.0_build_66 --depth 1 https://github.com/steinbergmedia/vst3sdk.git third_party/vst3sdk
git -C third_party/vst3sdk submodule update --init base cmake pluginterfaces public.sdk
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVST3_SDK_ROOT="C:/absolute/path/to/vst3sdk"
cmake --build build --config Release --target GASTELE gastele_dsp_tests gastele_integration_tests gastele_realphone_tests validator
ctest --test-dir build -C Release --output-on-failure
```

Build output: `build/VST3/Release/GASTELE.vst3`. The SDK and runtime are statically linked. There is no network access, licensing server, account, or telemetry in the plugin.

The bundled SDK revision list is in `sdk-revisions.json`. Upstream: https://github.com/steinbergmedia/vst3sdk/tree/v3.8.0_build_66 . SDK license notices are in `THIRD-PARTY-NOTICES.txt`.

For the Chinese MSVC installation used for this build, the Ninja dependency scanner required `-DGASTELE_MSVC_INCLUDE_PREFIX="注意: 包含文件:"`. A local MSVC optimizer issue in moduleinfotool is avoided by compiling that build utility with `/Od`; the plugin remains Release-optimized.

## Implementation

- `src/Dsp.h`: no allocation, locking or file access in sample processing; independent stereo channels; mono supported. Biquad band limiting, soft saturation, sample-and-hold, quantization and input-gated noise. Three parallel style paths crossfade smoothly.
- `src/Plugin.cpp`: VST3 processor/controller, stable parameter IDs and class IDs, 32/64-bit sample buffers, versioned transactional state serialization and sample-position parameter events.
- `src/Editor.cpp`: embedded Win32 child window, drag/wheel/double-click/keyboard controls, host edit gestures, repaint timer and size constraints.
- `src/Panel.h`: shared production renderer. The original approved PNG is retained and composited onto white. The three knob faces use fixed circular geometry with radial pink pointers; neither the original perspective artwork nor readouts are rotated. The main readout is a fixed foreground layer. Live numeric values, style selection, bypass and metering retain their original behavior.
- `tests/`: DSP, real processor/controller state and automation, native editor lifecycle and interactions, and UI rendering tests.

## Parameters

| ID | Name | Range | Default |
|---|---|---|---|
| 0 | Bypass | Off / On | Off (effect enabled) |
| 1 | Character | 0–100% | 65% |
| 2 | Style | Landline / Mobile / Radio | Landline |
| 3 | Mix | 0–100% | 100% |
| 4 | Output | −24 to +24 dB | 0 dB |
| 5 | Realphone | Off / On | Off |
| 100 | Output Level | 0–1, read only | 0 |

Parameter state format: little-endian uint32 version (2), then six float64 normalized values in parameter-ID order. Legacy format 1 with five parameters is accepted with Realphone OFF. Meter is not persisted. Out-of-range, nonfinite and truncated state is rejected without partially changing the active state.

Audio smoothing time constant: 12 ms. Style choice is discrete, audio transitions crossfade. Bypass includes Output gain; settled bypass and Mix=0 / Output=0 dB null against the input. Algorithmic latency is zero samples; IIR phase shift is intentional. UI defaults to 1000×700 and can be resized proportionally from 750 to 1499 px width by a host that supports plugin resizing. No standalone app, macOS build, VST2 or AAX is included.

## Verification and limits

1.0.1 fixes only the three knobs' visual geometry. DSP, parameters, component IDs, original artwork and interaction code are unchanged. Nine render positions from 0% to 100% pass pointer-centre and stray-marker checks. Pixels outside the three knob regions match 1.0.0 exactly in the default and alternate-state comparisons. Existing saved sessions retain the same plugin identity and parameter format.

The delivered build passed Steinberg validator (47/47), DSP tests at 8/22.05/44.1/48/96/192 kHz, and the native integration tests. See the release's `verification/` directory. The tests do not establish compatibility with every commercial DAW; no named commercial DAW session was tested. Actual meter refresh depends on the host forwarding VST3 output parameters.

The GUI uses the user's supplied artwork through the approved generated design. No new ownership or publication rights in the supplied artwork are asserted by this source delivery.

## Realphone

See `GASTELE-1.2-技术文档.md` for the full signal flow, estimated EQ bands, screenshot-fit method and limitations. `src/Realphone.h` implements seven serial biquads per channel. `tests/RealphoneTests.cpp` compares against a frozen 1.1 DSP implementation and checks serial placement across styles, mix values and sample rates.
