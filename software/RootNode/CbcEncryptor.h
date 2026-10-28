/******************************************************************************/
/*
 * File:    CbcEncryptor.h
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   Handles encryption and decryption of files. Uses AES encryption.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#ifndef ENCRYPTOR_H
#define ENCRYPTOR_H
#include "FS.h"                                                                 //For SPIFFS
#include "AES.h"                                                                //For the encryption
#include "configuration.h"                                                      //For the configuration

class CbcEncryptor
{
    public:
        CbcEncryptor();
        bool encryptFile(String filename);
        bool decryptFile(String filename);
        String encryptLine(String line);
        String decryptLine(String line);
        bool setKey(String key);
    
    private:
        byte _hexCharToByte(char hexChar);

        String _aesKeyString;
        byte _aesKey[AES_KEY_SIZE];
        byte _aesInitVector[N_BLOCK];
        unsigned long long int _aesInitVecInt;
        AES _aes;
};
#endif
