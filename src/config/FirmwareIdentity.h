#pragma once

namespace FirmwareIdentity
{
#if defined(GYMIO_TARGET_MAIN_S3_MINI)
#define GYMIO_FIRMWARE_TARGET_TEXT "main-gate-s3-mini"
#elif defined(GYMIO_TARGET_MAIN_S3_N8R8)
#define GYMIO_FIRMWARE_TARGET_TEXT "esp32-s3-n8r8"
#elif defined(GYMIO_TARGET_FINISH_S3_MINI)
#define GYMIO_FIRMWARE_TARGET_TEXT "finish-gate-s3-mini"
#elif defined(GYMIO_TARGET_TFT_C3_MINI)
#define GYMIO_FIRMWARE_TARGET_TEXT "tft-c3-mini-1"
#else
#define GYMIO_FIRMWARE_TARGET_TEXT "unknown"
#endif

constexpr char kTarget[] = GYMIO_FIRMWARE_TARGET_TEXT;
constexpr char kMarker[] = "GYMIO_TARGET=" GYMIO_FIRMWARE_TARGET_TEXT;
} // namespace FirmwareIdentity

