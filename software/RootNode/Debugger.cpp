/******************************************************************************/
/*
 * File:    Debugger.cpp
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   Class for handling the debug prints.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#include "Debugger.h"

/******************************************************************************/
/*!
  @brief    Prints text if debug is enabled.
  @param    text                String of text that needs to be printed
*/
/******************************************************************************/
void debug(String text, bool forcePrint) {
    if (forcePrint || PersistentMemory::getDebugEnabled()) {
        Serial.print(text);
    }
}

/******************************************************************************/
/*!
  @brief    Prints text (+'\n') if debug is enabled.
  @param    text                String of text that needs to be printed
  @param    XX
*/
/******************************************************************************/
void debugln(String text, bool forcePrint) {
    if (forcePrint || PersistentMemory::getDebugEnabled()) {
        Serial.println(text);
    }
}
