# Development and releases

See [Build and flash Gymio](BUILD.md) for device-specific commands.

## PlatformIO environments

| Environment | Application |
| --- | --- |
| `esp32-s3-n8r8` | Main/start gate, web UI, riders and timer |
| `main-gate-s3-mini` | Compact main/start gate with button and status LEDs |
| `finish-gate-s3-mini` | Remote finish sensor with status OLED |
| `tft-c3-mini-1` | Large ST7796S display-only node |

Build the complete supported matrix:

```sh
pio run -e esp32-s3-n8r8 -e main-gate-s3-mini -e finish-gate-s3-mini -e tft-c3-mini-1
```

## Source layout

| Path | Contents |
| --- | --- |
| `src/main.cpp` | Main gate entry point |
| `src/config` | Main-gate pins, timing, radio, and version |
| `src/tof`, `src/gate`, `src/timer` | Measurement and timer logic |
| `src/display` | Main-gate OLED |
| `src/rider` | Rider registry and browser associations |
| `src/web` | SoftAP, web UI/API, history, settings, ESP-NOW |
| `src/finish` | Finish-gate application |
| `src/tft` | Large-display application and hardware profile |
| `src/protocol` | Versioned binary radio frames |
| `src/diagnostics` | Optional guarded diagnostic programs |

Production environments exclude unrelated entry points using
`build_src_filter`. Diagnostic files are retained as development tools but no
standalone diagnostic environment is part of the supported build matrix.

## Possible improvements

- Make the manual/physical gate-setup safety margin independently configurable.
  It currently reuses the proven 150 mm automatic-calibration margin. After
  collecting real static-noise measurements, test a smaller manual margin in
  the 50–75 mm range to retain more usable gate distance without reintroducing
  false crossings. Keep the automatic-calibration margin unchanged unless it
  is evaluated separately.

## Versioning

Gymio follows Semantic Versioning. The prerelease version appears in `VERSION`,
`src/config/Version.h`, and `CHANGELOG.md`. For a release:

1. update both version files;
2. move changelog entries from Unreleased to the release section;
3. build all four environments;
4. commit the release;
5. create and push annotated tag `v<version>`.
