/******************************************************************************/
/*
 * File:    Debugger.h
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   Class for handling the debug prints.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#ifndef DEBUGGER_H
#define DEBUGGER_H
#include "Arduino.h"
#include "PersistentMemory.h"

void debug(String text, bool forcePrint = false);
void debugln(String text, bool forcePrint = false);

#endif
