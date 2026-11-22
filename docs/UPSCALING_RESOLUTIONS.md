# Fixed-resolution upscaling

FX3.DX12 exposes fixed 16:9 render presets:

| Preset | Resolution |
|---|---:|
| HD720 | 1280x720 |
| HD900 | 1600x900 |
| FullHD1080 | 1920x1080 |
| QHD1440 | 2560x1440 |
| UHD4K | 3840x2160 |

The output size remains the client/swap-chain size. The automatic policy selects:
- output up to 1280x720: native rendering;
- output up to 1920x1080: render at 1280x720;
- output up to 2560x1440: render at 1920x1080;
- larger output: render at 2560x1440.

The current raster path renders into a lower-resolution off-screen color target and
uses a full-screen Direct3D 12 pass with linear-clamp sampling to scale it to the
swap-chain. This is a functional spatial upscaler/fallback, not AMD FSR 3.
FSR3 DLL loading remains separate; AMD temporal upscaling and frame generation are
not enabled until the exact SDK headers and API structures are integrated.

The C API exposes the following XWeApplicationDesc values:
- XWE_UPSCALING_AUTOMATIC (default);
- XWE_UPSCALING_NATIVE;
- XWE_UPSCALING_RENDER_720P;
- XWE_UPSCALING_RENDER_900P;
- XWE_UPSCALING_RENDER_1080P;
- XWE_UPSCALING_RENDER_1440P.

For example, set desc.upscaling_mode to XWE_UPSCALING_RENDER_900P before
calling XWeCreateApplication to force a 1600x900 internal render target when
the output is larger than 1600x900.

The public configuration is in:
- include/XWe/Rendering/Upscaling/UpscalingSettings.hpp
- include/XWe/Rendering/Upscaling/FSR3Loader.hpp
