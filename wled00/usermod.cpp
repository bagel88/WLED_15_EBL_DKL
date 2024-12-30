#include "wled.h"
#include "EEPROM.h"
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

void userSetup()
{
  // Initialize LittleFS filesystem
  if (!WLED_FS.begin()) {
    Serial.println("LittleFS mount failed. Rebooting...");
    return;
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
    File flagFile = WLED_FS.open(FIRST_BOOT_FLAG_FILE, "w");  // Open for writing
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
