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

    // Presets 3 to 9
    for (int i = 3; i <= 9; i++) {
        JsonObject preset = doc.createNestedObject(String(i).c_str());
        preset["on"] = true;
        preset["bri"] = 255;
        preset["transition"] = 7;
        preset["mainseg"] = 0;

        // Create the seg array
        JsonArray segArray = preset.createNestedArray("seg");
        JsonObject segment = segArray.createNestedObject();
        segment["id"] = 0;
        segment["start"] = 0;
        segment["stop"] = 18;
        segment["grp"] = 1;
        segment["spc"] = 0;
        segment["of"] = 0;
        segment["on"] = true;
        segment["frz"] = false;
        segment["bri"] = 255;
        segment["cct"] = 127;
        segment["set"] = 0;
        segment["n"] = "";

        // Colors vary by preset
        JsonArray colors = segment.createNestedArray("col");
        if (i == 3) { // BlueGreen
            JsonArray col1 = colors.createNestedArray();
            col1.add(0); col1.add(0); col1.add(255);
            JsonArray col2 = colors.createNestedArray();
            col2.add(8); col2.add(255); col2.add(0);
            JsonArray col3 = colors.createNestedArray();
            col3.add(0); col3.add(0); col3.add(0);
            segment["pal"] = 3;
        } else if (i == 4) { // C9
            JsonArray col1 = colors.createNestedArray();
            col1.add(0); col1.add(0); col1.add(255);
            JsonArray col2 = colors.createNestedArray();
            col2.add(8); col2.add(255); col2.add(0);
            JsonArray col3 = colors.createNestedArray();
            col3.add(0); col3.add(0); col3.add(0);
            segment["pal"] = 48;
        } else if (i == 5) { // BlueRed
            JsonArray col1 = colors.createNestedArray();
            col1.add(255); col1.add(0); col1.add(0);
            JsonArray col2 = colors.createNestedArray();
            col2.add(0); col2.add(0); col2.add(255);
            JsonArray col3 = colors.createNestedArray();
            col3.add(0); col3.add(0); col3.add(0);
            segment["pal"] = 3;
        } else if (i == 6) { // Synth
            JsonArray col1 = colors.createNestedArray();
            col1.add(255); col1.add(0); col1.add(255);
            JsonArray col2 = colors.createNestedArray();
            col2.add(0); col2.add(255); col2.add(200);
            JsonArray col3 = colors.createNestedArray();
            col3.add(0); col3.add(0); col3.add(0);
            segment["pal"] = 3;
        } else if (i == 7) { // PurpleGreen
            JsonArray col1 = colors.createNestedArray();
            col1.add(255); col1.add(0); col1.add(255);
            JsonArray col2 = colors.createNestedArray();
            col2.add(8); col2.add(255); col2.add(0);
            JsonArray col3 = colors.createNestedArray();
            col3.add(0); col3.add(0); col3.add(0);
            segment["pal"] = 3;
        } else if (i == 8) { // BlueOrange
            JsonArray col1 = colors.createNestedArray();
            col1.add(0); col1.add(0); col1.add(255);
            JsonArray col2 = colors.createNestedArray();
            col2.add(255); col2.add(149); col2.add(0);
            JsonArray col3 = colors.createNestedArray();
            col3.add(0); col3.add(0); col3.add(0);
            segment["pal"] = 3;
        } else if (i == 9) { // OrangeCyan
            JsonArray col1 = colors.createNestedArray();
            col1.add(255); col1.add(160); col1.add(0);
            JsonArray col2 = colors.createNestedArray();
            col2.add(0); col2.add(255); col2.add(200);
            JsonArray col3 = colors.createNestedArray();
            col3.add(0); col3.add(0); col3.add(0);
            segment["pal"] = 3;
        }

        segment["fx"] = 65;
        segment["sx"] = 128;
        segment["ix"] = 112;
        segment["c1"] = 0;
        segment["c2"] = 128;
        segment["c3"] = 16;
        segment["sel"] = true;
        segment["rev"] = false;
        segment["mi"] = false;
        segment["o1"] = true;
        segment["o2"] = false;
        segment["o3"] = true;
        segment["si"] = 0;
        segment["m12"] = 0;

        // Add 32 "stop": 0 objects
        for (int j = 0; j < 32; j++) {
            JsonObject stopObj = segArray.createNestedObject();
            stopObj["stop"] = 0;
        }

        // Set preset names
        if (i == 3) preset["n"] = "BlueGreen";
        else if (i == 4) preset["n"] = "C9";
        else if (i == 5) preset["n"] = "BlueRed";
        else if (i == 6) preset["n"] = "Synth";
        else if (i == 7) preset["n"] = "PurpleGreen";
        else if (i == 8) preset["n"] = "BlueOrange";
        else if (i == 9) preset["n"] = "OrangeCyan";
    }

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
void savePreset(byte index, const char* pname, JsonObject sObj)
{
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
}

void deletePreset(byte index) {
  StaticJsonDocument<24> empty;
  writeObjectToFileUsingId(getPresetsFileName(), index, &empty);
  presetsModifiedTime = toki.second(); //unix time
  updateFSInfo();
}