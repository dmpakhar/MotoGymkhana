#pragma once

#include <Arduino.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config/Config.h"
#include "config/FirmwareIdentity.h"
#include "config/Version.h"
#include "ota/FirmwareTargetValidator.h"

class OtaUpdateServer
{
public:
    OtaUpdateServer(const char *deviceName, const IPAddress &address)
        : deviceName_(deviceName), address_(address), targetValidator_(FirmwareIdentity::kMarker)
    {
    }

    void begin(uint32_t nowMs)
    {
        if (active_)
        {
            expiresAtMs_ = nowMs + Config::kOtaMaintenanceTimeoutMs;
            return;
        }

        active_ = true;
        expiresAtMs_ = nowMs + Config::kOtaMaintenanceTimeoutMs;
        uploadStarted_ = false;
        uploadSucceeded_ = false;
        restartAtMs_ = 0;
        serverStarted_ = false;

        const IPAddress gateway(192, 168, 4, 1);
        const IPAddress subnet(255, 255, 255, 0);
        WiFi.config(address_, gateway, subnet);
        WiFi.begin(Config::kWifiAccessPointName, Config::kWifiAccessPointPassword);
        Serial.print(deviceName_);
        Serial.println(" OTA maintenance mode requested.");
    }

    void service(uint32_t nowMs)
    {
        if (!active_)
        {
            return;
        }

        if (!serverStarted_ && WiFi.status() == WL_CONNECTED)
        {
            configureServer();
            server_.begin();
            serverStarted_ = true;
            Serial.print(deviceName_);
            Serial.print(" OTA ready at http://");
            Serial.print(address_);
            Serial.println("/update");
        }

        if (serverStarted_)
        {
            server_.handleClient();
        }

        if (restartAtMs_ != 0 && static_cast<int32_t>(nowMs - restartAtMs_) >= 0)
        {
            ESP.restart();
        }

        if (!uploadStarted_ && static_cast<int32_t>(nowMs - expiresAtMs_) >= 0)
        {
            stop();
        }
    }

    bool active() const { return active_; }

private:
    bool authenticate()
    {
        if (server_.authenticate(Config::kOtaAdminUser, Config::kOtaAdminPassword))
        {
            return true;
        }
        server_.requestAuthentication();
        return false;
    }

    void configureServer()
    {
        server_.on("/", HTTP_GET, [this]() {
            server_.sendHeader("Location", "/update");
            server_.send(302, "text/plain", "");
        });
        server_.on("/update", HTTP_GET, [this]() {
            if (!authenticate())
                return;
            String page = F("<!doctype html><meta name=viewport content='width=device-width'><style>body{font:18px system-ui;max-width:620px;margin:40px auto;padding:20px;background:#10151b;color:#edf3f7}section{background:#1a232d;padding:22px;border-radius:16px}input,button{display:block;width:100%;box-sizing:border-box;margin:16px 0;padding:14px;font-size:16px}button{background:#83df9c;border:0;border-radius:10px;font-weight:700}</style><section><h1>");
            page += deviceName_;
            page += F(" OTA</h1><p>Current version: ");
            page += Version::kString;
            page += F("</p><p>Select the firmware.bin built for this device.</p><form method=POST action=/update enctype=multipart/form-data><input type=file name=firmware accept=.bin required><button>Install firmware</button></form></section>");
            server_.send(200, "text/html", page);
        });
        server_.on(
            "/update", HTTP_POST,
            [this]() {
                if (!authenticate())
                    return;
                if (uploadSucceeded_)
                {
                    server_.send(200, "text/plain", "Update installed. Device is restarting.");
                    restartAtMs_ = millis() + 750;
                }
                else
                {
                    server_.send(500, "text/plain", "Update failed. Existing firmware is unchanged.");
                    uploadStarted_ = false;
                }
            },
            [this]() {
                if (!server_.authenticate(Config::kOtaAdminUser, Config::kOtaAdminPassword))
                    return;
                HTTPUpload &upload = server_.upload();
                if (upload.status == UPLOAD_FILE_START)
                {
                    targetValidator_.reset();
                    uploadStarted_ = Update.begin(UPDATE_SIZE_UNKNOWN);
                    uploadSucceeded_ = false;
                }
                else if (upload.status == UPLOAD_FILE_WRITE && uploadStarted_)
                {
                    targetValidator_.consume(upload.buf, upload.currentSize);
                    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
                    {
                        Update.abort();
                        uploadStarted_ = false;
                    }
                }
                else if (upload.status == UPLOAD_FILE_END && uploadStarted_)
                {
                    uploadSucceeded_ = targetValidator_.found() && Update.end(true);
                    if (!uploadSucceeded_)
                    {
                        Update.abort();
                    }
                    uploadStarted_ = false;
                }
                else if (upload.status == UPLOAD_FILE_ABORTED)
                {
                    Update.abort();
                    uploadStarted_ = false;
                }
            });
    }

    void stop()
    {
        if (serverStarted_)
        {
            server_.stop();
        }
        WiFi.disconnect(false, true);
        active_ = false;
        serverStarted_ = false;
        Serial.print(deviceName_);
        Serial.println(" OTA maintenance mode timed out.");
    }

    const char *deviceName_;
    IPAddress address_;
    WebServer server_{80};
    bool active_ = false;
    bool serverStarted_ = false;
    bool uploadStarted_ = false;
    bool uploadSucceeded_ = false;
    uint32_t expiresAtMs_ = 0;
    uint32_t restartAtMs_ = 0;
    FirmwareTargetValidator targetValidator_;
};
