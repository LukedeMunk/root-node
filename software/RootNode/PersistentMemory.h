/******************************************************************************/
/*
 * File:    PersistentMemory.h
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   Manages all EEPROM reads and writes.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H
#include "Arduino.h"
#include "configuration.h"
#include "Debugger.h"
#include "EEPROM.h"

#define LED_SETTINGS_MAGIC          0xA5
#define LED_SETTINGS_MAGIC_ADDRESS  (HOSTNAME_ADDRESS + MAX_HOSTNAME_LENGTH)
#define LED_BRIGHTNESS_ADDRESS      (LED_SETTINGS_MAGIC_ADDRESS + 1)
#define LED_POWER_STATE_ADDRESS     (LED_BRIGHTNESS_ADDRESS + sizeof(uint16_t))
#define EEPROM_STORAGE_SIZE         (LED_POWER_STATE_ADDRESS + 1)

class PersistentMemory
{
    public:
        static String getHostname();
        static void writeHostname(const char hostname[MAX_HOSTNAME_LENGTH]);
        static uint16_t getBrightness();
        static void writeBrightness(uint16_t brightness);
        static uint8_t getPowerState();
        static void writePowerState(uint8_t powerState);
        static void setEepromToNull(uint8_t writeLength, uint8_t startAdress);
        static void setDebugEnabled(bool isEnabled);
        static bool getDebugEnabled();

    private:
        static bool _checkEepromCommit();

        static bool _debugEnabled;
};

#endif
