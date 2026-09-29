# Liquid Glass Recording Demo

Build `P3_Interactive_Smoke`, then run:

```powershell
.\build\Debug\P3_Interactive_Smoke.exe --recording-demo
```

The demo opens a fixed 1280 × 720 client area and loops every **59 seconds**.
Capture that window in OBS at 1280 × 720 and 60 fps. Begin recording near the
`BEFORE` cue; let one full cycle play. Press **Esc** to close the demo. Mouse
input is ignored in this mode so the sequence remains repeatable.

| Time | Scene |
| --- | --- |
| 0–4 s | Static baseline screenshot: dark background, original white spot |
| 4–8 s | Live current shader, same background, glass size and position |
| 8–23 s | Slow moving glass over text, shapes, fine lines, grid and contrast blocks |
| 23–33 s | Button: normal, hover, pressed, disabled, focused; 2 s each |
| 33–39 s | Toggle: off, on, hover, pressed; 1.5 s each |
| 39–41 s | Slider track, fill, thumb and a slow drag |
| 41–47 s | Large panel, card, input-sized rect and vertical card |
| 47–53 s | Button, toggle, slider, pill, 22 px knob and extreme radius input |
| 53–59 s | Same glass on dark, light and high-contrast backgrounds; 2 s each |

The `BEFORE` cue uses `assets/recording_before_dark.png`, a captured frame from
the original white-spot shader. The build copies it beside the executable.
The `AFTER` cue and every other glass frame are rendered live through the
current `GlassSurface`; the old shader is never loaded or restored.

For a fast review of each cue without waiting through the timeline:

```powershell
.\build\Debug\P3_Interactive_Smoke.exe --recording-stills --out .\build\recording-stills
```

This writes 24 PNGs and exits. It is a visual review path, not a video file.
The existing interactive and `--visual` modes remain available separately.
