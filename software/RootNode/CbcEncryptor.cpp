/******************************************************************************/
/*
 * File:    CbcEncryptor.cpp
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   Handles encryption and decryption of files. Uses AES encryption.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#include "CbcEncryptor.h"

/******************************************************************************/
/*!
  @brief    Constructor.
*/
/******************************************************************************/
CbcEncryptor::CbcEncryptor() {
    _aesInitVecInt = CBC_INIT_VECTOR;
    _aesKeyString = AES_KEY;

    /* Convert key string to bytes */
    for (uint8_t i = 0; i < AES_KEY_SIZE; i++) {
        _aesKey[i] = (byte) _aesKeyString[i];
    }
}

/******************************************************************************/
/*!
  @brief    Encrypts the SPIFFS file if it exists.
  @param    filename            Name of the file to encrypt
  @return   bool                True if encryption is successfull
*/
/******************************************************************************/
bool CbcEncryptor::encryptFile(String filename) {
    String temporaryFilename = filename + ".tmp";

    if (!SPIFFS.exists(filename)) {
        return false;
    }
    
    setKey(AES_KEY);                                                            //Reset key
    _aesKeyString = AES_KEY;

    File inputFile = SPIFFS.open(filename, "r");                                //Plaintext file
    File tmpFile = SPIFFS.open(temporaryFilename, "w");                         //Temporary file to store encrypted part
    if (!inputFile || !tmpFile) {
        inputFile.close();
        tmpFile.close();
        SPIFFS.remove(temporaryFilename);
        return false;
    }

    String format = inputFile.readStringUntil('\n');
    if (format.endsWith("\r")) {
        format.remove(format.length() - 1);
    }

    /* If already encrypted, don't do anything */
    if (format == "format: encrypted") {
        inputFile.close();
        tmpFile.close();
        SPIFFS.remove(temporaryFilename);
        return true;
    }
    
    tmpFile.println("format: encrypted");                                       //Save format
    _aes.iv_inc();                                                              //Unique initialization vector every encryption
    
    /* Encrypt the file line by line */
    while (inputFile.available()) {
        String line = inputFile.readStringUntil('\n');                          //Read a line from the file
        if (line.endsWith("\r")) {
            line.remove(line.length() - 1);
        }
        if (line.length() == 0) {
            continue;
        }
        tmpFile.println(encryptLine(line));
    }
    
    inputFile.close();
    tmpFile.close();
    
    SPIFFS.remove(filename);                                                    //Remove plain text file
    if (!SPIFFS.rename(temporaryFilename, filename)) {                          //Rename tmp file
        return false;
    }
    
    return true;
}

/******************************************************************************/
/*!
  @brief    Decrypts the SPIFFS file if it exists.
  @param    filename            Name of the file to decrypt
  @return   bool                True if decryption is successfull
*/
/******************************************************************************/
bool CbcEncryptor::decryptFile(String filename) {
    String temporaryFilename = filename + ".tmp";

    if (!SPIFFS.exists(filename)) {
        return false;
    }
    
    setKey(AES_KEY);                                                            //Reset key
    _aesKeyString = AES_KEY;
    
    File inputFile = SPIFFS.open(filename, "r");
    File tmpFile = SPIFFS.open(temporaryFilename, "w");
    if (!inputFile || !tmpFile) {
        inputFile.close();
        tmpFile.close();
        SPIFFS.remove(temporaryFilename);
        return false;
    }

    String format = inputFile.readStringUntil('\n');
    if (format.endsWith("\r")) {
        format.remove(format.length() - 1);
    }

    /* If already plaintext, don't do anything */
    if (format == "format: plaintext") {
        inputFile.close();
        tmpFile.close();
        SPIFFS.remove(temporaryFilename);
        return true;
    }

    tmpFile.println("format: plaintext");                                       //Save format
    /* Decrypt the file line by line */
    while (inputFile.available()) {
        String line = inputFile.readStringUntil('\n');                          //Read a line from the file
        if (line.endsWith("\r")) {
            line.remove(line.length() - 1);
        }
        if (line.length() == 0) {
            continue;
        }
        tmpFile.println(decryptLine(line));
    }
    
    inputFile.close();
    tmpFile.close();
    
    SPIFFS.remove(filename);                                                    //Remove plain text file
    if (!SPIFFS.rename(temporaryFilename, filename)) {                          //Rename tmp file
        return false;
    }

    return true;
}

/******************************************************************************/
/*!
  @brief    Sets the encryption key.
  @param    key                 Encryption key
  @return   bool                True if is successfull
*/
/******************************************************************************/
bool CbcEncryptor::setKey(String key) {
    _aesKeyString = key;

    /* Check if key is the right length */
    if (key.length() != AES_KEY_SIZE) {
        return false;
    }

    /* Convert key string to bytes */
    for (uint8_t i = 0; i < AES_KEY_SIZE; i++) {
        _aesKey[i] = (byte) key[i];
    }

    return true;
}

/******************************************************************************/
/*!
  @brief    Encrypts a line of text.
  @param    line                Line to encrypt
  @return   String              Encrypted string
*/
/******************************************************************************/
String CbcEncryptor::encryptLine(String line) {
    uint16_t len = line.length();
    byte plain[len];                                                            //To store plain bytes
    uint8_t paddedLength = len + N_BLOCK - len % N_BLOCK;                       //Calculate total length when padded
    byte encrypted[paddedLength];                                               //To store encrypted bytes
    char encryptedString[paddedLength*2+1];                                     //To store return value
  
    /* Convert string to bytes */
    for (uint8_t i = 0; i < len; i++) {
        plain[i] = (byte) line[i];
    }
  
    _aes.set_IV(_aesInitVecInt);
    _aes.get_IV(_aesInitVector);
    _aes.do_aes_encrypt(plain, len, encrypted, _aesKey, 128, _aesInitVector);
  
    /* Copy bytes and locate the end of printable plaintext */
    for (uint8_t i = 0; i < paddedLength; i++) {
        sprintf(encryptedString+2*i,"%02x",*(encrypted+i));
    }

    return encryptedString;
}

/******************************************************************************/
/*!
  @brief    Decrypts a line of text.
  @param    line                Line to decrypt
  @return   String              Decrypted string
*/
/******************************************************************************/
String CbcEncryptor::decryptLine(String line) {
    uint16_t len = line.length()/2;
    byte encrypted[len];                                                        //To store encrypted bytes
    byte decrypted[len];                                                        //To store decrypted bytes
    uint16_t outputLen = len;                                                   //To store plaintext length
    char decryptedString[len + 1];                                              //To store return value

    /* Convert hex string to bytes */
    for (uint8_t i = 0; i < len; i++) {
        encrypted[i] = _hexCharToByte(line[i*2])<<4 | _hexCharToByte(line[i*2+1]);
    }
    
    _aes.set_IV(_aesInitVecInt);
    _aes.get_IV(_aesInitVector);
    _aes.do_aes_decrypt(encrypted, len, decrypted, _aesKey, 128, _aesInitVector);

    /* Convert bytes to string */
    for (uint8_t i = 0; i < len; i++) {
        decryptedString[i] = (char) decrypted[i];

        if ((decrypted[i] >= 127 || decrypted[i] <= 32) && outputLen == len) {
            outputLen = i;
        }
    }
    decryptedString[len] = '\0';

    if (_aesKeyString != AES_KEY) {
        return String(decryptedString);                                         //If another key is used, just throw the whole string back
    }

    return String(decryptedString).substring(0, outputLen);                     //Convert to string to trim the string with substring()
}

/******************************************************************************/
/*!
  @brief    Converts a hexidecimal stored in a char to a byte
  @param    hexChar             Character to convert
  @return   byte                Value in a byte variable
*/
/******************************************************************************/
byte CbcEncryptor::_hexCharToByte(char hexChar) {
    uint8_t c = hexChar;

    if (c <= '9' && c >= '0') {
        c -= '0';
    } else if (c <= 'f' && c >= 'a') {
        c -= ('a' - 0x0a);
    } else if (c <= 'F' && c >= 'A') {
        c -= ('A' - 0x0a);
    } else {
        return(1);
    }
    
    return (c);
}
