/******************************************************************************/
/*
 * File:    BufferOverflow.cpp
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   A buffer overflow simulator based on the Linux CLI.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#include "BufferOverflow.h"

/******************************************************************************/
/*!
  @brief    Constructor.
*/
/******************************************************************************/
BufferOverflow::BufferOverflow() {
    _clearInput();                                                              //First time call is the declaration of the array.
}

/******************************************************************************/
/*!
  @brief    Prints the disassembled code of the vulnerable testprogram.
*/
/******************************************************************************/
void BufferOverflow::objectDump() {
    Serial.println(F("testprogram:     file format elf32-littlearm"));
    Serial.println(F(""));
    Serial.println(F("Disassembly of section .init:"));
    Serial.println(F(""));
    Serial.println(F("00010438 <main>:"));
    Serial.println(F("   10438: e92d4800  push  {fp, lr}"));
    Serial.println(F("   1043c: e28db004  add fp, sp, #4"));
    Serial.println(F("   10440: e24dd018  sub sp, sp, #24"));
    Serial.println(F("   10444: e50b0018  str r0, [fp, #-24]  ; 0xffffffe8"));
    Serial.println(F("   10448: e50b101c  str r1, [fp, #-28]  ; 0xffffffe4"));
    Serial.println(F("   1044c: e51b301c  ldr r3, [fp, #-28]  ; 0xffffffe4"));
    Serial.println(F("   10450: e2833004  add r3, r3, #4"));
    Serial.println(F("   10454: e5932000  ldr r2, [r3]"));
    Serial.println(F("   10458: e24b3010  sub r3, fp, #16"));
    Serial.println(F("   1045c: e1a01002  mov r1, r2"));
    Serial.println(F("   10460: e1a00003  mov r0, r3"));
    Serial.println(F("   10464: ebffffab  bl  10318 <strcpy@plt>"));
    Serial.println(F("   10468: e55b3010  ldrb  r3, [fp, #-16]"));
    Serial.println(F("   1046c: e1a00003  mov r0, r3"));
    Serial.println(F("   10470: eb000004  bl  10488 <login>"));
    Serial.println(F("   10474: eb00000d  bl  104b0 <logout>"));
    Serial.println(F("   10478: e3a03000  mov r3, #0"));
    Serial.println(F("   1047c: e1a00003  mov r0, r3"));
    Serial.println(F("   10480: e24bd004  sub sp, fp, #4"));
    Serial.println(F("   10484: e8bd8800  pop {fp, pc}"));
    Serial.println(F(""));
    Serial.println(F("00010488 <login>:"));
    Serial.println(F("   10488: e92d4800  push  {fp, lr}"));
    Serial.println(F("   1048c: e28db004  add fp, sp, #4"));
    Serial.println(F("   10490: e24dd008  sub sp, sp, #8"));
    Serial.println(F("   10494: e50b0008  str r0, [fp, #-8]"));
    Serial.println(F("   10498: e59f000c  ldr r0, [pc, #12] ; 104ac <login+0x24>"));
    Serial.println(F("   1049c: ebffff9a  bl  1030c <printf@plt>"));
    Serial.println(F("   104a0: e1a00000  nop     ; (mov r0, r0)"));
    Serial.println(F("   104a4: e24bd004  sub sp, fp, #4"));
    Serial.println(F("   104a8: e8bd8800  pop {fp, pc}"));
    Serial.println(F("   104ac: 0001053c  .word 0x0001053c"));
    Serial.println(F(""));
    Serial.println(F("000104b0 <logout>:"));
    Serial.println(F("   104b0: e92d4800  push  {fp, lr}"));
    Serial.println(F("   104b4: e28db004  add fp, sp, #4"));
    Serial.println(F("   104b8: e59f0008  ldr r0, [pc, #8]  ; 104c8 <logout+0x18>"));
    Serial.println(F("   104bc: ebffff92  bl  1030c <printf@plt>"));
    Serial.println(F("   104c0: e1a00000  nop     ; (mov r0, r0)"));
    Serial.println(F("   104c4: e8bd8800  pop {fp, pc}"));
    Serial.println(F("   104c8: 00010548  .word 0x00010548"));
    Serial.println(F(""));
}

/******************************************************************************/
/*!
  @brief    Simulates the vulnerable testprogram.
  @param    arg                 Given argument
  @return   bool                True if the buffer overflow attack is done
                                correctly
*/
/******************************************************************************/
bool BufferOverflow::runCProgram(String arg) {
    _formatInput(arg);

    if (_inputTooLong) {
        _printOverflowError();
        return false;
    }
    
    if (_numChars < OVERFLOW_BEGIN) {
        Serial.println("You are now super user.");
        Serial.print("Hello ");
        Serial.println(arg);
        Serial.println("You are not longer super user.");
    } else if (_checkBufferOverflow()) {
        return true;
    }

    return false;
}

/******************************************************************************/
/*!
  @brief    Checks if the buffer overflow is done correctly.
  @return   bool                True if the buffer overflow attack is done
                                correctly
*/
/******************************************************************************/
bool BufferOverflow::_checkBufferOverflow() {
    if (_getOverflowPortion() == RETURN_ADDRESS) {
        return true;
    }
    
    _printOverflowError();                                                      //If the overflow is not correctly, print value of the return address pointer
    return false;
}

/******************************************************************************/
/*!
  @brief    Prints the segmentation error when overflow is not correctly.
*/
/******************************************************************************/
void BufferOverflow::_printOverflowError() {
    Serial.println("Program received signal SIGSEGV, Segmentation fault.");
    Serial.print("0x");
    _getOverflowPortion(true);
    Serial.println(" in ?? ()");
}

/******************************************************************************/
/*!
  @brief    Converts the given argument string to an understandable format.
  @param    input               String of the given arguments
*/
/******************************************************************************/
void BufferOverflow::_formatInput(String input) {
    String tmp = "";
    
    _clearInput();
    
    /* Set every character in an element */
    for (uint16_t i = 0; i < input.length(); i++) {
        if (_numChars >= MAX_NUM_CHARS) {
            _inputTooLong = true;
            break;
        }

        if (input[i] == '\\') {
            if (i + 3 < input.length()) {
                _formattedInput[_numChars] = "\\x";                             //Move all hex chars in one element (for ex.: '\x90')
                _formattedInput[_numChars] += input[i+2];
                _formattedInput[_numChars] += input[i+3];
                i += 3;                                                         //Increase with 3, because the number of chars taken for a hex is 4 ('\x90')
            } else {
                _formattedInput[_numChars] = "\\x5c";
            }
        } else {
            _formattedInput[_numChars] = input[i];
        }
        _numChars++;
    }

    /* Turn the whole array, to simulate little endian systems */
    for (uint16_t i = 0; i < _numChars / 2; i++) {
        tmp = _formattedInput[i];
        _formattedInput[i] = _formattedInput[_numChars - i - 1];
        _formattedInput[_numChars - i - 1] = tmp;
        tmp = "";
    }
}

/******************************************************************************/
/*!
  @brief    Converts the given argument string to an understandable format.
  @param    print               If true, the overflowportion gets printed
                                directly (To avoid character glichting with HEX
                                parameter)
*/
/******************************************************************************/
String BufferOverflow::_getOverflowPortion(bool print) {
    String overflowPortion = "";
    
    if (_numChars < OVERFLOW_LENGTH) {
        uint8_t numMissingBytes = OVERFLOW_LENGTH - _numChars;
        overflowPortion += _generateRandomBytes(numMissingBytes);
        
        if (print) {
            Serial.print(overflowPortion);
        }
        
        /* To determine and print the overflow portion */
        for (uint8_t i = 0; i < ADDRESS_LENGTH - numMissingBytes; i++) {
            /* Check if is hex number, else print as hex */
            if (_formattedInput[i][0] == '\\') {
                overflowPortion += _formattedInput[i][2];
                overflowPortion += _formattedInput[i][3];
                if (print) {
                    Serial.print(_formattedInput[i][2]);
                    Serial.print(_formattedInput[i][3]);
                }
            } else {
                overflowPortion += _formattedInput[i];
                if (print) {
                    Serial.print(char(_formattedInput[i][0]), HEX);
                }
            }
        }
    } else {
        /* To print the overflow portion */
        uint8_t delta = abs(_numChars - OVERFLOW_LENGTH);
        for (uint8_t i = delta; i < delta + ADDRESS_LENGTH; i++) {
            /* Check if is hex number, else print as hex */
            if (_formattedInput[i][0] == '\\') {
                overflowPortion += _formattedInput[i][2];
                overflowPortion += _formattedInput[i][3];
                if (print) {
                    Serial.print(_formattedInput[i][2]);
                    Serial.print(_formattedInput[i][3]);
                }
            } else {
                if (print) {
                    Serial.print(char(_formattedInput[i][0]), HEX);
                }
                overflowPortion += _formattedInput[i];
            }
        }
    }

    return overflowPortion;
}

/******************************************************************************/
/*!
  @brief    Clears the input array and number of chars.
*/
/******************************************************************************/
void BufferOverflow::_clearInput() {
    for (uint16_t i = 0; i < MAX_NUM_CHARS; i++) {
        _formattedInput[i] = "";
    }

    _numChars = 0;
    _inputTooLong = false;
}

/******************************************************************************/
/*!
  @brief    Generates random bytes to fill the return register when the
            overflow bytes are too less.
  @param    numBytes            Number of needed bytes
  @return   String              String of the generated bytes
*/
/******************************************************************************/
String BufferOverflow::_generateRandomBytes(uint8_t numBytes) {
    String bytes = "";
    randomSeed(numBytes);
    
    for (uint8_t i = 0; i < numBytes; i++) {
        bytes += String(random(127), HEX);
    }

    return bytes;
}
