/******************************************************************************/
/*
 * File:    StartupText.h
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   Prints a realistic embedded Linux boot log as hexadecimal bytes,
 *          including vulnerable debug information when enabled.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#ifndef STARTUP_TEXT_H
#define STARTUP_TEXT_H
#include "Arduino.h"

#define STARTUP_BYTES_PER_LINE  20

/* Declare functions, because it is not a class */
bool printStartupText(const char* hiddenMessage, bool isSerialConsoleEnabled);

#endif
