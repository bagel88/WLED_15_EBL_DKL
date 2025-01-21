#include "wled.h"
/*
 * This v1 usermod file allows you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 * EEPROM bytes 2750+ are reserved for your custom use case. (if you extend #define EEPSIZE in const.h)
 * If you just need 8 bytes, use 2551-2559 (you do not need to increase EEPSIZE)
 *
 * Consider the v2 usermod API if you need a more advanced feature set!
 */

//Use userVar0 and userVar1 (API calls &U0=,&U1=, uint16_t)

//gets called once at boot. Do all initialization that doesn't depend on network here'

#define FIRST_BOOT_FLAG_FILE "/first_boot_flag.txt"

// Function to count presets in the JSON file, ignoring the "0" preset
int BootcountPresetsFromFile(const char* filename) {
    if (!WLED_FS.begin()) {
        Serial.println("WLED_FS mount failed!");
        return -1;
    }

    File file = WLED_FS.open(filename, "r");
    if (!file) {
        Serial.println("Failed to open file!");
        return -1;
    }

    size_t size = file.size();
    if (size == 0) {
        Serial.println("Empty file!");
        file.close();
        return 0;
    }

    DynamicJsonDocument doc(32768); // Adjust size based on expected JSON size
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.f_str());
        return -1;
    }

    JsonObject root = doc.as<JsonObject>();  // Extract the root object

    int count = 0;
    for (JsonObject::iterator it = root.begin(); it != root.end(); ++it) {
        if (strcmp(it->key().c_str(), "0") != 0) {
            count++;
        }
    }

    // Update "P1=3&P2=9&PL=~" with the new count
    for (JsonObject::iterator it = root.begin(); it != root.end(); ++it) {
        JsonObject preset = it->value().as<JsonObject>();
        if (preset.containsKey("win")) {
            String winValue = preset["win"].as<String>();
            int pos = winValue.indexOf("&P2=");
            if (pos != -1) {
                String before = winValue.substring(0, pos + 4); // "P1=3&P2="
                String after = winValue.substring(winValue.indexOf("&PL=")); // "&PL=~"
                String newWinValue = before + count + after;
                preset["win"] = newWinValue;
            }
        }
    }

    // Save the updated JSON back to the file
    file = WLED_FS.open(filename, "w");
    if (!file) {
        Serial.println("Failed to open file for writing!");
        return -1;
    }

    serializeJson(root, file);
    file.close();
    updateFSInfo();

    return count;
}

void userSetup()
{
    // Initialize WLED_FS filesystem
    if (!WLED_FS.begin()) {
        Serial.println("WLED_FS mount failed. Rebooting...");
        return;
    }

        // Update presets in file and count them every boot
    int presetCount = BootcountPresetsFromFile("/presets.json");
    if (presetCount >= 0) {
        Serial.print("Number of presets (excluding '0'): ");
        Serial.println(presetCount);
    } else {
        Serial.println("Error updating presets.");
    }

    bool firstBoot = false;

    // Open the flag file
    File flagFile = WLED_FS.open(FIRST_BOOT_FLAG_FILE, "r");
    if (!flagFile) {
        // File doesn't exist, it is the first boot
        firstBoot = true;
    } else {
        // If file exists, read the content (0 or 1)
        firstBoot = flagFile.read() == '0';  // Assuming the flag is stored as a single character (0 or 1)
        flagFile.close();
    }

    if (firstBoot) {
        // It's the first boot, set the flag to indicate it's not the first boot anymore
        flagFile = WLED_FS.open(FIRST_BOOT_FLAG_FILE, "w");  // Open for writing
        if (flagFile) {
            flagFile.print('1');  // Write '1' to indicate it's no longer the first boot
            flagFile.close();
        } else {
            Serial.println("Failed to write the flag file.");
        }

        // Reboot the device after the first boot
        doReboot = true;
    }
}

//gets called every time WiFi is (re-)connected. Initialize own network interfaces here
void userConnected()
{

}

//loop. You can use "if (WLED_CONNECTED)" to check for successful connection
void userLoop()
{

}
