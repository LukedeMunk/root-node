/******************************************************************************/
/*
 * File:    UserHandler.h
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   Class for the http authentication process.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#ifndef USER_HANDLER_H
#define USER_HANDLER_H
#include "stdint.h"                                                             //For defining bits per integer
#include "ESP8266WebServer.h"                                                   //For running the webserver
#include "FS.h"                                                                 //For SPIFFS
#include "configuration.h"                                                      //For the configuration
#include "CbcEncryptor.h"                                                       //For decrypting file to obtain users

class UserHandler
{
    public:
        UserHandler();
        void updateUsers();
        String* getUsers();
        uint8_t getNumberOfUsers();
        String createSession(String username, String password, uint8_t* permissionLevel);
        void clearSession(ESP8266WebServer *server);
        uint8_t getPermissionLevel(ESP8266WebServer *server);
        bool checkPermission(uint8_t permissionLevel, ESP8266WebServer *server);
    
    private:
        String* _parseLine(String line);
        String _getSessionToken(ESP8266WebServer *server);
        String _users[MAX_NUMBER_USERS*USER_INFO_LENGTH];
        String _sessionTokens[MAX_NUMBER_USERS];
        uint8_t _numberUsers;
        CbcEncryptor cryptor;
};
#endif
