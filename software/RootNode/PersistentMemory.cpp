/******************************************************************************/
/*
 * File:    PersistentMemory.cpp
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   Manages all EEPROM reads and writes.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#include "PersistentMemory.h"

bool PersistentMemory::_debugEnabled = false;

/******************************************************************************/
/*!
  @brief    Gets the hostname from the EEPROM.
  @return   String              Current hostname
*/
/******************************************************************************/
String PersistentMemory::getHostname() {
    char hostname[MAX_HOSTNAME_LENGTH + 1] = {0};
    bool isTerminated = false;

    EEPROM.begin(EEPROM_STORAGE_SIZE);
    for (uint8_t i = 0; i < MAX_HOSTNAME_LENGTH; i++) {
        uint8_t storedValue = EEPROM.read(HOSTNAME_ADDRESS + i);

        if (storedValue == 0xFF || storedValue == '\0') {
            hostname[i] = '\0';
            isTerminated = true;
            break;
        }

        if (!isalnum(storedValue) && storedValue != '-') {
            EEPROM.end();
            return String();
        }

        hostname[i] = (char) storedValue;
    }
    hostname[MAX_HOSTNAME_LENGTH] = '\0';
    EEPROM.end();

    if (!isTerminated || hostname[0] == '\0' || hostname[0] == '-' ||
        hostname[strlen(hostname) - 1] == '-') {
        return String();
    }

    return String(hostname);
}

/******************************************************************************/
/*!
  @brief    Writes the new hostname to the EEPROM.
  @param    hostname            String that contains the hostname to be written
*/
/******************************************************************************/
void PersistentMemory::writeHostname(const char hostname[MAX_HOSTNAME_LENGTH]) {
    EEPROM.begin(EEPROM_STORAGE_SIZE);

    uint8_t hostnameLength = 0;
    while (hostnameLength < MAX_HOSTNAME_LENGTH - 1 && hostname[hostnameLength] != '\0') {
        hostnameLength++;
    }

    bool hasChanges = false;
    for (uint8_t i = 0; i < MAX_HOSTNAME_LENGTH; i++) {
        uint8_t newValue = i < hostnameLength ? hostname[i] : '\0';
        if (EEPROM.read(HOSTNAME_ADDRESS + i) != newValue) {
            EEPROM.write(HOSTNAME_ADDRESS + i, newValue);
            hasChanges = true;
        }
        yield();
    }

    if (hasChanges) {
        _checkEepromCommit();
    }
    EEPROM.end();
}

/******************************************************************************/
/*!
  @brief    Gets the stored LED brightness or its safe default.
  @return   uint16_t            Stored brightness
*/
/******************************************************************************/
uint16_t PersistentMemory::getBrightness() {
    EEPROM.begin(EEPROM_STORAGE_SIZE);
    uint8_t settingsMagic = EEPROM.read(LED_SETTINGS_MAGIC_ADDRESS);
    uint16_t brightness;
    EEPROM.get(LED_BRIGHTNESS_ADDRESS, brightness);
    EEPROM.end();

    if (settingsMagic != LED_SETTINGS_MAGIC || brightness > MAX_BRIGHTNESS) {
        return MAX_BRIGHTNESS;
    }

    return brightness;
}

/******************************************************************************/
/*!
  @brief    Stores LED brightness when its EEPROM value changed.
  @param    brightness          Brightness to store
*/
/******************************************************************************/
void PersistentMemory::writeBrightness(uint16_t brightness) {
    if (brightness > MAX_BRIGHTNESS) {
        brightness = MAX_BRIGHTNESS;
    }

    EEPROM.begin(EEPROM_STORAGE_SIZE);
    uint16_t storedBrightness;
    EEPROM.get(LED_BRIGHTNESS_ADDRESS, storedBrightness);
    bool hasChanges = EEPROM.read(LED_SETTINGS_MAGIC_ADDRESS) != LED_SETTINGS_MAGIC ||
        storedBrightness != brightness;

    if (hasChanges) {
        EEPROM.write(LED_SETTINGS_MAGIC_ADDRESS, LED_SETTINGS_MAGIC);
        EEPROM.put(LED_BRIGHTNESS_ADDRESS, brightness);
        _checkEepromCommit();
    }
    EEPROM.end();
}

/******************************************************************************/
/*!
  @brief    Gets the stored LED power state or its safe default.
  @return   uint8_t             ON or OFF
*/
/******************************************************************************/
uint8_t PersistentMemory::getPowerState() {
    EEPROM.begin(EEPROM_STORAGE_SIZE);
    uint8_t settingsMagic = EEPROM.read(LED_SETTINGS_MAGIC_ADDRESS);
    uint8_t powerState = EEPROM.read(LED_POWER_STATE_ADDRESS);
    EEPROM.end();

    if (settingsMagic != LED_SETTINGS_MAGIC ||
        (powerState != ON && powerState != OFF)) {
        return OFF;
    }

    return powerState;
}

/******************************************************************************/
/*!
  @brief    Stores LED power state when its EEPROM value changed.
  @param    powerState          ON or OFF
*/
/******************************************************************************/
void PersistentMemory::writePowerState(uint8_t powerState) {
    powerState = powerState == ON ? ON : OFF;

    EEPROM.begin(EEPROM_STORAGE_SIZE);
    bool hasChanges = EEPROM.read(LED_SETTINGS_MAGIC_ADDRESS) != LED_SETTINGS_MAGIC ||
        EEPROM.read(LED_POWER_STATE_ADDRESS) != powerState;

    if (hasChanges) {
        EEPROM.write(LED_SETTINGS_MAGIC_ADDRESS, LED_SETTINGS_MAGIC);
        EEPROM.write(LED_POWER_STATE_ADDRESS, powerState);
        _checkEepromCommit();
    }
    EEPROM.end();
}

/******************************************************************************/
/*!
  @brief    Resets the given EEPROM adresses.
  @param    writeLength         Length to be erased
  @param    startAdress         Start adress
*/
/******************************************************************************/
void PersistentMemory::setEepromToNull(uint8_t writeLength, uint8_t startAdress) {
    EEPROM.begin(startAdress + writeLength);

    for (uint8_t i = 0; i < writeLength; i++) {
        EEPROM.write(startAdress + i, 0);
        yield();
    }

    _checkEepromCommit();

    debug("Reset Value at: ");
    debug(String(startAdress));
    debug(" till ");
    debugln(String(startAdress + writeLength));

    EEPROM.end();
}

/******************************************************************************/
/*!
  @brief    Checks if the eeprom was actually committed.
*/
/******************************************************************************/
bool PersistentMemory::_checkEepromCommit() {
    if (EEPROM.commit()) {
        //debugln("Data written!");
        return true;
    } else {
        debugln("EEPROM write error", true);
        return false;
    }
}

/******************************************************************************/
/*!
  @brief    Sets if debug is enabled for the current runtime.
  @param    isEnabled           If debug is enabled (true == enabled)
*/
/******************************************************************************/
void PersistentMemory::setDebugEnabled(bool isEnabled) {
    _debugEnabled = isEnabled;
}

/******************************************************************************/
/*!
  @brief    Gets if debug is enabled.
  @return   bool                True if debug is enabled
*/
/******************************************************************************/
bool PersistentMemory::getDebugEnabled() {
    return _debugEnabled;
}
