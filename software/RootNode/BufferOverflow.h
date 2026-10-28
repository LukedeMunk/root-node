/******************************************************************************/
/*
 * File:    BufferOverflow.h
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   A buffer overflow simulator based on the Linux CLI.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#ifndef BUFFER_OVERFLOW_H
#define BUFFER_OVERFLOW_H
#include "Arduino.h"
#include "stdint.h"                                                             //For defining bits per integer

#define OVERFLOW_BEGIN    16                                                    //Because array is in byte resolution, 10 becomes 16. Then the return address pointer starts
#define ADDRESS_LENGTH    4                                                     //Address is 32 bits long, so 4 bytes
#define OVERFLOW_LENGTH   20                                                    //OVERFLOW_BEGIN + ADDRESS_LENGTH
#define RETURN_ADDRESS    "00010488"                                            //0x00010488 == address of login function.
#define MAX_NUM_CHARS     256

class BufferOverflow
{
    public:
        BufferOverflow();
        void objectDump();
        bool runCProgram(String arg);
    
    private:
        bool _checkBufferOverflow();
        void _printOverflowError();
        void _formatInput(String input);
        String _getOverflowPortion(bool print = false);
        void _clearInput();
        String _generateRandomBytes(uint8_t numberOfBytes);
                 
        String _formattedInput[MAX_NUM_CHARS];
        uint16_t _numChars;
        bool _inputTooLong;
};
#endif
