/******************************************************************************/
/*
 * File:    configuration.h
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   Configuration options can be configured here.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#ifndef CONFIG_H
#define CONFIG_H

/* Hostname */
#define DEFAULT_HOSTNAME                "smartlight"
#define HOSTNAME_ADDRESS                1
#define MAX_HOSTNAME_LENGTH             32
#define NETWORK_CONFIG_AP_SSID          "Config RootNode WiFi "

/* Filesystem */
#define EXTERNAL_FILE_FOLDER            "/external_files"
#define VIRTUAL_UPLOAD_DIRECTORY        "/home/user/uploads"
#define EXTERNAL_FILE_MAX_NAME_LENGTH   40
#define EXTERNAL_FILE_MAX_UPLOAD_SIZE   65536UL
#define EXT_FILE_WEBUSERS_DB            "/webusers.db"
#define EXT_FILE_TEST_PROGRAM           "/testprogram.c"
#define EXT_FILE_TEST_PROGRAM_OBJ       "/testprogram"
#define EXT_FILE_NOTES                  "/notes.txt"

#define FIRMWARE_VERSION                "1.0.0"
#define DEVICE_CHIP_MODEL               "ESP8266"
#define SESSION_COOKIE_NAME             "RootNodeSession"

/* LED */
#define ON                              HIGH
#define OFF                             LOW
#define MIN_BRIGHTNESS                  1                                       //analogWrite() on ESP8266 D1 Mini board is inverted
#define MAX_BRIGHTNESS                  1023

/* Permission levels */
#define PERMISSION_LEVEL_ALL        0
#define PERMISSION_LEVEL_USER       1
#define PERMISSION_LEVEL_ADMIN      2

#define USER_INFO_LENGTH            3                                           //Username, password and permission level
#define MAX_NUMBER_USERS            10

/* Serial CLI */
#define SERIAL_PASSWORD             "102030201"
#define SERIAL_ROOT_PASSWORD        "IamSuperUsah!"

/* Encryption */
#define AES_KEY                     "ThisIsNotTheKey!"                          //Key to encrypt and decrypt
#define CBC_INIT_VECTOR             36753562                                    //For CBC (encryption) padding

#define AES_KEY_SIZE                16
#define AES_BIT_SIZE                128

/* HTTP codes */
#define HTTP_CODE_OK        	    200
#define HTTP_CODE_FOUND             302
#define HTTP_CODE_BAD_REQUEST       400
#define HTTP_CODE_UNAUTHORIZED      401
#define HTTP_CODE_FORBIDDEN         403
#define HTTP_CODE_NOT_FOUND         404
#define HTTP_CODE_PAYLOAD_TOO_LARGE 413
#define HTTP_CODE_INTERNAL_ERROR    500
#endif
