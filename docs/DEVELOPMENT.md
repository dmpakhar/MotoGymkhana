# Development and releases

See [Build and flash Gymio](BUILD.md) for device-specific commands.

## PlatformIO environments

| Environment | Application |
| --- | --- |
| `esp32-s3-n8r8` | Main/start gate, web UI, riders and timer |
| `finish-gate-s3-mini` | Remote finish sensor and battery OLED |
| `tft-c3-mini-1` | Large ST7796S display-only node |

Build the complete supported matrix:

```sh
pio run -e esp32-s3-n8r8 -e finish-gate-s3-mini -e tft-c3-mini-1
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

## Versioning

Gymio follows Semantic Versioning. The prerelease version appears in `VERSION`,
`src/config/Version.h`, and `CHANGELOG.md`. For a release:

1. update both version files;
2. move changelog entries from Unreleased to the release section;
3. build all three environments;
4. commit the release;
5. create and push annotated tag `v<version>`.
