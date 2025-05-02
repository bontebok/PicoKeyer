#include <Arduino.h>
#include <LittleFS.h>
#include "main.h"

// Function to initialize LittleFS
bool initLittleFS()
{
    if (!LittleFS.begin())
        return false;

    return true;
}

// Function to read settings from LittleFS
bool readSettings(Settings_t &settings)
{
    File file = LittleFS.open(SETTINGS_FILE, "r");
    if (!file)
        return false;

    size_t bytesRead = file.readBytes((char *)&settings, sizeof(Settings_t));
    file.close();

    if (bytesRead != sizeof(Settings_t))
        return false;

    return true;
}

// Function to write settings to LittleFS
bool writeSettings(const Settings_t &settings)
{
    File file = LittleFS.open(SETTINGS_FILE, "w");
    if (!file)
        return false;

    size_t bytesWritten = file.write((char *)&settings, sizeof(Settings_t));
    file.close();

    if (bytesWritten != sizeof(Settings_t))
        return false;
    return true;
}

// Function to load settings (with default fallback)
bool loadSettings(Settings_t &settings)
{
    if (!initLittleFS())
        return false;

    if (!LittleFS.exists(SETTINGS_FILE))
        return writeSettings(settings);

    Settings_t storedSettings;

    if (!readSettings(storedSettings))
        return writeSettings(settings);
    else {
        if (storedSettings.version != VERSION) // Version mismatch
            return writeSettings(settings);
        else
            settings = storedSettings;
    }
    return true;
}

void init(Settings_t &settings)
{
    loadSettings(settings);
}

void save(Settings_t &settings)
{
    writeSettings(settings);
}