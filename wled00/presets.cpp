#include "wled.h"

/*
 * Methods to handle saving and loading presets to/from the filesystem
 */

#ifdef ARDUINO_ARCH_ESP32
static char *tmpRAMbuffer = nullptr;
#endif

static volatile byte presetToApply = 0;
static volatile byte callModeToApply = 0;
static volatile byte presetToSave = 0;
static volatile int8_t saveLedmap = -1;
static char *quickLoad = nullptr;
static char *saveName = nullptr;
static bool includeBri = true, segBounds = true, selectedOnly = false, playlistSave = false;;

static const char presets_json[] PROGMEM = "/presets.json";
static const char tmp_json[] PROGMEM = "/tmp.json";
const char *getPresetsFileName(bool persistent) {
  return persistent ? presets_json : tmp_json;
}

static void doSaveState() {
  bool persist = (presetToSave < 251);

  unsigned long start = millis();
  while (strip.isUpdating() && millis()-start < (2*FRAMETIME_FIXED)+1) yield(); // wait 2 frames
  if (!requestJSONBufferLock(10)) return;

  initPresetsFile(); // just in case if someone deleted presets.json using /edit
  JsonObject sObj = pDoc->to<JsonObject>();

  DEBUG_PRINTLN(F("Serialize current state"));
  if (playlistSave) {
    serializePlaylist(sObj);
    if (includeBri) sObj["on"] = true;
  } else {
    serializeState(sObj, true, includeBri, segBounds, selectedOnly);
  }
  if (saveName) sObj["n"] = saveName;
  else          sObj["n"] = F("Unkonwn preset"); // should not happen, but just in case...
  if (quickLoad && quickLoad[0]) sObj[F("ql")] = quickLoad;
  if (saveLedmap >= 0) sObj[F("ledmap")] = saveLedmap;
/*
  #ifdef WLED_DEBUG
    DEBUG_PRINTLN(F("Serialized preset"));
    serializeJson(*pDoc,Serial);
    DEBUG_PRINTLN();
  #endif
*/
  #if defined(ARDUINO_ARCH_ESP32)
  if (!persist) {
    if (tmpRAMbuffer!=nullptr) free(tmpRAMbuffer);
    size_t len = measureJson(*pDoc) + 1;
    DEBUG_PRINTLN(len);
    // if possible use SPI RAM on ESP32
    if (psramSafe && psramFound())
      tmpRAMbuffer = (char*) ps_malloc(len);
    else
      tmpRAMbuffer = (char*) malloc(len);
    if (tmpRAMbuffer!=nullptr) {
      serializeJson(*pDoc, tmpRAMbuffer, len);
    } else {
      writeObjectToFileUsingId(getPresetsFileName(persist), presetToSave, pDoc);
    }
  } else
  #endif
  writeObjectToFileUsingId(getPresetsFileName(persist), presetToSave, pDoc);

  if (persist) presetsModifiedTime = toki.second(); //unix time
  releaseJSONBufferLock();
  updateFSInfo();

  // clean up
  saveLedmap   = -1;
  presetToSave = 0;
  delete[] saveName;
  delete[] quickLoad;
  saveName = nullptr;
  quickLoad = nullptr;
  playlistSave = false;
}

bool getPresetName(byte index, String& name)
{
  if (!requestJSONBufferLock(19)) return false;
  bool presetExists = false;
  if (readObjectFromFileUsingId(getPresetsFileName(), index, pDoc)) {
    JsonObject fdo = pDoc->as<JsonObject>();
    if (fdo["n"]) {
      name = (const char*)(fdo["n"]);
      presetExists = true;
    }
  }
  releaseJSONBufferLock();
  return presetExists;
}

// Function to count presets in the JSON file, ignoring the "0" preset
int countPresetsFromFile(const char* filename) {
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

    int actualCount = count + 1; // Adds 1 to count

    // Update "P1=3&P2=9&PL=~" with the new count
    for (JsonObject::iterator it = root.begin(); it != root.end(); ++it) {
        JsonObject preset = it->value().as<JsonObject>();
        if (preset.containsKey("win")) {
            String winValue = preset["win"].as<String>();
            int pos = winValue.indexOf("&P2=");
            if (pos != -1) {
                String before = winValue.substring(0, pos + 4); // "P1=3&P2="
                String after = winValue.substring(winValue.indexOf("&PL=")); // "&PL=~"
                String newWinValue = before + actualCount + after;
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

    return count;
}

int countPresetsFromFileOnDelete(const char* filename) {
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

    //int actualCount = count + 1; // Adds 1 to count

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

    return count;
}

bool acquireJSONBufferLock(uint8_t retries = 5, uint16_t delayMs = 50) {
  for (uint8_t attempt = 0; attempt < retries; attempt++) {
    if (requestJSONBufferLock(9)) {
      return true; // Lock acquired
    }
    delay(delayMs); // Wait before retrying
  }
  return false; // Failed to acquire lock
}

void writeHardcodedPresetJson() {
    // Nieuwe functie voor het hardcoden van presets in WLED
    
    // Maak JSON document
        // Check if the file already exists
    if (WLED_FS.exists("/presets.json")) {
        return; // Exit the function if the file exists
    }
    DynamicJsonDocument doc(8192); // Adjust size if necessary

    // Preset 0: Empty
    doc.createNestedObject("0");

    // Preset 1
    JsonObject preset1 = doc.createNestedObject("1");
    preset1["win"] = "T=2";
    preset1["n"] = "OnOff";

    // Preset 2
    JsonObject preset2 = doc.createNestedObject("2");
    preset2["win"] = "P1=3&P2=9&PL=~";
    preset2["n"] = "PresetSwitch";

    // Preset 3: BlueGreen
    JsonObject preset3 = doc.createNestedObject("3");
    preset3["on"] = true;
    preset3["bri"] = 255;
    preset3["transition"] = 7;
    preset3["mainseg"] = 0;

    JsonArray segArray3 = preset3.createNestedArray("seg");
    JsonObject seg3 = segArray3.createNestedObject();
    seg3["id"] = 0;
    seg3["start"] = 0;
    seg3["stop"] = 18;
    seg3["grp"] = 1;
    seg3["spc"] = 0;
    seg3["of"] = 0;
    seg3["on"] = true;
    seg3["frz"] = false;
    seg3["bri"] = 255;
    seg3["cct"] = 127;
    seg3["set"] = 0;
    seg3["n"] = "";

    JsonArray colArray3 = seg3.createNestedArray("col");

    JsonArray color1_3 = colArray3.createNestedArray();
    color1_3.add(0); color1_3.add(0); color1_3.add(255); color1_3.add(0); // RGBW: [0, 0, 255, 0]

    JsonArray color2_3 = colArray3.createNestedArray();
    color2_3.add(8); color2_3.add(255); color2_3.add(0); color2_3.add(0); // RGBW: [8, 255, 0, 0]

    JsonArray color3_3 = colArray3.createNestedArray();
    color3_3.add(0); color3_3.add(0); color3_3.add(0); color3_3.add(0);   // RGBW: [0, 0, 0, 0]

    seg3["fx"] = 65;
    seg3["sx"] = 128;
    seg3["ix"] = 112;
    seg3["pal"] = 3;
    seg3["c1"] = 0;
    seg3["c2"] = 128;
    seg3["c3"] = 16;
    seg3["sel"] = true;
    seg3["rev"] = false;
    seg3["mi"] = false;
    seg3["o1"] = true;
    seg3["o2"] = false;
    seg3["o3"] = true;
    seg3["si"] = 0;
    seg3["m12"] = 0;

    for (int j = 0; j < 32; j++) {
        JsonObject stopObj = segArray3.createNestedObject();
        stopObj["stop"] = 0;
    }
    preset3["n"] = "BlueGreen";

    // Preset 4: C9
    JsonObject preset4 = doc.createNestedObject("4");
    preset4["on"] = true;
    preset4["bri"] = 255;
    preset4["transition"] = 7;
    preset4["mainseg"] = 0;

    JsonArray segArray4 = preset4.createNestedArray("seg");
    JsonObject seg4 = segArray4.createNestedObject();
    seg4["id"] = 0;
    seg4["start"] = 0;
    seg4["stop"] = 18;
    seg4["grp"] = 1;
    seg4["spc"] = 0;
    seg4["of"] = 0;
    seg4["on"] = true;
    seg4["frz"] = false;
    seg4["bri"] = 255;
    seg4["cct"] = 127;
    seg4["set"] = 0;
    seg4["n"] = "";

    JsonArray colArray4 = seg4.createNestedArray("col");

    JsonArray color1_4 = colArray4.createNestedArray();
    color1_4.add(0); color1_4.add(0); color1_4.add(255); color1_4.add(0); // RGBW: [0, 0, 255, 0]

    JsonArray color2_4 = colArray4.createNestedArray();
    color2_4.add(8); color2_4.add(255); color2_4.add(0); color2_4.add(0); // RGBW: [8, 255, 0, 0]

    JsonArray color3_4 = colArray4.createNestedArray();
    color3_4.add(0); color3_4.add(0); color3_4.add(0); color3_4.add(0);   // RGBW: [0, 0, 0, 0]

    seg4["fx"] = 65;
    seg4["sx"] = 128;
    seg4["ix"] = 112;
    seg4["pal"] = 48;
    seg4["c1"] = 0;
    seg4["c2"] = 128;
    seg4["c3"] = 16;
    seg4["sel"] = true;
    seg4["rev"] = false;
    seg4["mi"] = false;
    seg4["o1"] = true;
    seg4["o2"] = false;
    seg4["o3"] = true;
    seg4["si"] = 0;
    seg4["m12"] = 0;

    for (int j = 0; j < 32; j++) {
        JsonObject stopObj = segArray4.createNestedObject();
        stopObj["stop"] = 0;
    }
    preset4["n"] = "C9";

    // Repeat similarly for presets 5-9...

    // Write the JSON to presets.json
    File presetJsonFile = WLED_FS.open("/presets.json", "w");
    if (!presetJsonFile) {
        Serial.println("Failed to open presets.json for writing");
        return;
    }

    if (serializeJson(doc, presetJsonFile) == 0) {
        Serial.println("Failed to write to presets.json");
    } else {
        Serial.println("Preset data written to presets.json");
    }

    presetJsonFile.close();
}

void initPresetsFile() {
    // Dit is een default WLED functie
    // Onderstaande toevoeging roept bovenstaande twee nieuwe functies aan
    writeHardcodedPresetJson();

  char fileName[33]; strncpy_P(fileName, getPresetsFileName(), 32); fileName[32] = 0; //use PROGMEM safe copy as FS.open() does not
  if (WLED_FS.exists(fileName)) return;

  StaticJsonDocument<64> doc;
  JsonObject sObj = doc.to<JsonObject>();
  sObj.createNestedObject("0");
  File f = WLED_FS.open(fileName, "w");
  if (!f) {
    errorFlag = ERR_FS_GENERAL;
    return;
  }
  serializeJson(doc, f);
  f.close();
}

bool applyPresetFromPlaylist(byte index)
{
  DEBUG_PRINTF_P(PSTR("Request to apply preset: %d\n"), index);
  presetToApply = index;
  callModeToApply = CALL_MODE_DIRECT_CHANGE;
  return true;
}

bool applyPreset(byte index, byte callMode)
{
  unloadPlaylist(); // applying a preset unloads the playlist (#3827)
  DEBUG_PRINTF_P(PSTR("Request to apply preset: %u\n"), index);
  presetToApply = index;
  callModeToApply = callMode;
  return true;
}

// apply preset or fallback to a effect and palette if it doesn't exist
void applyPresetWithFallback(uint8_t index, uint8_t callMode, uint8_t effectID, uint8_t paletteID)
{
  applyPreset(index, callMode);
  //these two will be overwritten if preset exists in handlePresets()
  effectCurrent = effectID;
  effectPalette = paletteID;
}

void handlePresets()
{
  byte presetErrFlag = ERR_NONE;
  if (presetToSave) {
    strip.suspend();
    doSaveState();
    strip.resume();
    return;
  }

  if (presetToApply == 0 || !requestJSONBufferLock(9)) return; // no preset waiting to apply, or JSON buffer is already allocated, return to loop until free

  bool changePreset = false;
  uint8_t tmpPreset = presetToApply; // store temporary since deserializeState() may call applyPreset()
  uint8_t tmpMode   = callModeToApply;

  JsonObject fdo;

  presetToApply = 0; //clear request for preset
  callModeToApply = 0;

  DEBUG_PRINTF_P(PSTR("Applying preset: %u\n"), (unsigned)tmpPreset);

  #ifdef ARDUINO_ARCH_ESP32
  if (tmpPreset==255 && tmpRAMbuffer!=nullptr) {
    deserializeJson(*pDoc,tmpRAMbuffer);
  } else
  #endif
  {
  presetErrFlag = readObjectFromFileUsingId(getPresetsFileName(tmpPreset < 255), tmpPreset, pDoc) ? ERR_NONE : ERR_FS_PLOAD;
  }
  fdo = pDoc->as<JsonObject>();

  // only reset errorflag if previous error was preset-related
  if ((errorFlag == ERR_NONE) || (errorFlag == ERR_FS_PLOAD)) errorFlag = presetErrFlag;

  //HTTP API commands
  const char* httpwin = fdo["win"];
  if (httpwin) {
    String apireq = "win"; // reduce flash string usage
    apireq += F("&IN&"); // internal call
    apireq += httpwin;
    handleSet(nullptr, apireq, false); // may call applyPreset() via PL=
    setValuesFromFirstSelectedSeg(); // fills legacy values
    changePreset = true;
  } else {
    if (!fdo["seg"].isNull() || !fdo["on"].isNull() || !fdo["bri"].isNull() || !fdo["nl"].isNull() || !fdo["ps"].isNull() || !fdo[F("playlist")].isNull()) changePreset = true;
    if (!(tmpMode == CALL_MODE_BUTTON_PRESET && fdo["ps"].is<const char *>() && strchr(fdo["ps"].as<const char *>(),'~') != strrchr(fdo["ps"].as<const char *>(),'~')))
      fdo.remove("ps"); // remove load request for presets to prevent recursive crash (if not called by button and contains preset cycling string "1~5~")
    deserializeState(fdo, CALL_MODE_NO_NOTIFY, tmpPreset); // may change presetToApply by calling applyPreset()
  }
  if (!errorFlag && tmpPreset < 255 && changePreset) currentPreset = tmpPreset;

  #if defined(ARDUINO_ARCH_ESP32)
  //Aircoookie recommended not to delete buffer
  if (tmpPreset==255 && tmpRAMbuffer!=nullptr) {
    free(tmpRAMbuffer);
    tmpRAMbuffer = nullptr;
  }
  #endif

  releaseJSONBufferLock();
  if (changePreset) notify(tmpMode); // force UDP notification
  stateUpdated(tmpMode);  // was colorUpdated() if anything breaks
  updateInterfaces(tmpMode);
}

//called from handleSet(PS=) [network callback (sObj is empty), IR (irrational), deserializeState, UDP] and deserializeState() [network callback (filedoc!=nullptr)]
void savePreset(byte index, const char* pname, JsonObject sObj) {
    // Prevent saving presets with ID 1 and 2
    if (index == 1 || index == 2) {
        return;
  }
  
  if (!saveName) saveName = new char[33];
  if (!quickLoad) quickLoad = new char[9];
  if (!saveName || !quickLoad) return;

  if (index == 0 || (index > 250 && index < 255)) return;
  if (pname) strlcpy(saveName, pname, 33);
  else {
    if (sObj["n"].is<const char*>()) strlcpy(saveName, sObj["n"].as<const char*>(), 33);
    else                             sprintf_P(saveName, PSTR("Preset %d"), index);
  }

  DEBUG_PRINTF_P(PSTR("Saving preset (%d) %s\n"), index, saveName);

  presetToSave = index;
  playlistSave = false;
  if (sObj[F("ql")].is<const char*>()) strlcpy(quickLoad, sObj[F("ql")].as<const char*>(), 9); // client limits QL to 2 chars, buffer for 8 bytes to allow unicode
  else quickLoad[0] = 0;

  const char *bootPS = PSTR("bootps");
  if (!sObj[FPSTR(bootPS)].isNull()) {
    bootPreset = sObj[FPSTR(bootPS)] | bootPreset;
    sObj.remove(FPSTR(bootPS));
    doSerializeConfig = true;
  }

  if (sObj.size()==0 || sObj["o"].isNull()) { // no "o" means not a playlist or custom API call, saving of state is async (not immediately)
    includeBri   = sObj["ib"].as<bool>() || sObj.size()==0 || index==255; // temporary preset needs brightness
    segBounds    = sObj["sb"].as<bool>() || sObj.size()==0 || index==255; // temporary preset needs bounds
    selectedOnly = sObj[F("sc")].as<bool>();
    saveLedmap   = sObj[F("ledmap")] | -1;
  } else {
    // this is a playlist or API call
    if (sObj[F("playlist")].isNull()) {
      // we will save API call immediately (often causes presets.json corruption)
      presetToSave = 0;
      if (index <= 250) { // cannot save API calls to temporary preset (255)
        sObj.remove("o");
        sObj.remove("v");
        sObj.remove("time");
        sObj.remove(F("error"));
        sObj.remove(F("psave"));
        if (sObj["n"].isNull()) sObj["n"] = saveName;
        initPresetsFile(); // just in case if someone deleted presets.json using /edit
        writeObjectToFileUsingId(getPresetsFileName(), index, pDoc);
        presetsModifiedTime = toki.second(); //unix time
        updateFSInfo();
      }
      delete[] saveName;
      delete[] quickLoad;
      saveName = nullptr;
      quickLoad = nullptr;
    } else {
      // store playlist
      // WARNING: playlist will be loaded in json.cpp after this call and will have repeat counter increased by 1
      includeBri   = true; // !sObj["on"].isNull();
      playlistSave = true;
    }
  }
      // Initialize WLED_FS filesystem
    if (!WLED_FS.begin()) {
        Serial.println("WLED_FS mount failed. Rebooting...");
        return;
    }

        // Update presets in file and count them every boot
    int presetCount = countPresetsFromFile("/presets.json");
    if (presetCount >= 0) {
        Serial.print("Number of presets (excluding '0'): ");
        Serial.println(presetCount);
        presetsModifiedTime = toki.second(); //unix time
        updateFSInfo();
    } else {
        Serial.println("Error updating presets.");
    }
}

void deletePreset(byte index) {
    // Prevent deletion of presets with ID 1 and 2
    if (index == 1 || index == 2) {
        Serial.printf("Error: Preset ID %d cannot be deleted\n", index);
        return;
  }
  File file = WLED_FS.open("/presets.json", "r");
  if (!file) {
    Serial.println("Error: Failed to open presets.json for reading");
    return;
  }

  // Parse the JSON file into a DynamicJsonDocument
  DynamicJsonDocument doc(32768); // Adjust size based on presets.json size
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("Error: Failed to parse presets.json");
    return;
  }

  // Get the root JSON object
  JsonObject root = doc.as<JsonObject>();

  // Check if the preset exists and delete it
  String targetKey = String(index);
  if (!root.containsKey(targetKey)) {
    Serial.println("Error: Preset not found");
    return;
  }
  root.remove(targetKey);
  Serial.printf("Deleted preset ID %d\n", index);

  // Shift IDs down for presets with higher IDs
  for (byte i = index + 1; i < 255; i++) { // Assuming max ID is 254
    String currentKey = String(i);
    String newKey = String(i - 1);

    if (root.containsKey(currentKey)) {
      // Move the preset to the new key
      JsonObject preset = root[currentKey].as<JsonObject>();
      JsonObject newPreset = root.createNestedObject(newKey);
      for (JsonPair p : preset) {
        newPreset[p.key()] = p.value();
      }
      root.remove(currentKey); // Remove the old key
      Serial.printf("Shifted preset ID %d to %d\n", i, i - 1);
    } else {
      break; // Stop if there are no more presets to shift
    }
  }

  // Open the file for writing to save the changes
  file = WLED_FS.open("/presets.json", "w");
  if (!file) {
    Serial.println("Error: Failed to open presets.json for writing");
    return;
  }

  // Serialize the modified JSON object back into the file
  if (serializeJson(doc, file) == 0) {
    Serial.println("Error: Failed to write presets.json");
  } else {
    Serial.println("Preset deleted and IDs shifted successfully");
  }

  file.close();

  // Update presets in file and count them every boot
  int presetCount = countPresetsFromFileOnDelete("/presets.json");
  if (presetCount >= 0) {
    Serial.print("Number of presets (excluding '0'): ");
    Serial.println(presetCount);
    presetsModifiedTime = toki.second(); //unix time
    updateFSInfo();
  } else {
    Serial.println("Error updating presets.");
  }

  // Update filesystem metadata
  presetsModifiedTime = toki.second(); // Update with the current Unix time
  updateFSInfo();
}