/******************************************************************************/
/*
 * File:    UserHandler.cpp
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   Class for the http authentication process.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#include "UserHandler.h"

/******************************************************************************/
/*!
  @brief    Constructor.
  @param    server              Webserver object
*/
/******************************************************************************/
UserHandler::UserHandler() {
    _numberUsers = 0;
    for (uint8_t i = 0; i < MAX_NUMBER_USERS; i++) {
        _sessionTokens[i] = "";
    }
}

/******************************************************************************/
/*!
  @brief    Updates the users from the configuration file in RAM.
*/
/******************************************************************************/
void UserHandler::updateUsers() {
    for (uint8_t i = 0; i < MAX_NUMBER_USERS * USER_INFO_LENGTH; i++) {
        _users[i] = "";
    }
    _numberUsers = 0;
    for (uint8_t i = 0; i < MAX_NUMBER_USERS; i++) {
        _sessionTokens[i] = "";
    }

    /* If there is no file, return 0 users */
    if (!SPIFFS.exists(EXTERNAL_FILE_FOLDER EXT_FILE_WEBUSERS_DB)) {
        Serial.println("No web users file found");
        return;
    }
    
    /* Decrypt the file before reading */
    if (!cryptor.decryptFile(EXTERNAL_FILE_FOLDER EXT_FILE_WEBUSERS_DB)) {
        Serial.println("Cannot decrypt web users file");
        return;
    }

    File configFile = SPIFFS.open(EXTERNAL_FILE_FOLDER EXT_FILE_WEBUSERS_DB, "r");
    if (!configFile) {
        Serial.println("No decrypted web users file found");
        return;
    }
    
    String line;
    String* user;
    bool parsingFailed = false;
    configFile.readStringUntil('\n');                                           //Ignore first line (format)
    
    /* Extract user information line by line */
    for (uint8_t i = 0; i < MAX_NUMBER_USERS; i++) {
        line = configFile.readStringUntil('\n');                                //Read a line from the file
        
        if (line == "") {
            break;
        }

        user = _parseLine(line);
        if (user == NULL) {
            parsingFailed = true;
            break;
        }

        uint8_t userIndex = i * USER_INFO_LENGTH;
        _users[userIndex] = user[0].c_str();
        _users[userIndex + 1] = user[1].c_str();
        _users[userIndex + 2] = user[2].c_str();
        _numberUsers++;
    }
    configFile.close();
    
    /* Encrypt the file again */
    bool encryptionSucceeded = cryptor.encryptFile(
        EXTERNAL_FILE_FOLDER EXT_FILE_WEBUSERS_DB);
    if (parsingFailed || !encryptionSucceeded) {
        for (uint8_t i = 0; i < MAX_NUMBER_USERS * USER_INFO_LENGTH; i++) {
            _users[i] = "";
        }
        _numberUsers = 0;
    }
}

/******************************************************************************/
/*!
  @brief    Gets users.
  @return   String*             String array of users
*/
/******************************************************************************/
String* UserHandler::getUsers() {
    return _users;
}

/******************************************************************************/
/*!
  @brief    Gets number of users.
  @return   Uint8_t             Number of users
*/
/******************************************************************************/
uint8_t UserHandler::getNumberOfUsers() {
    return _numberUsers;
}

/******************************************************************************/
/*!
  @brief    Creates a session for valid existing credentials.
  @param    username            Submitted username
  @param    password            Submitted password
  @param    permissionLevel     Destination for the authenticated role
  @return   String              Session token, empty when login fails
*/
/******************************************************************************/
String UserHandler::createSession(String username, String password, uint8_t* permissionLevel) {
    *permissionLevel = PERMISSION_LEVEL_ALL;
    for (uint8_t i = 0; i < _numberUsers; i++) {
        uint8_t userIndex = i * USER_INFO_LENGTH;
        if (_users[userIndex] != username ||
            _users[userIndex + 1] != password) {
            continue;
        }

        *permissionLevel = atoi(_users[userIndex + 2].c_str());
        _sessionTokens[i] = String(ESP.getChipId(), HEX) +
                            String(micros(), HEX) +
                            String(random(0x7fffffff), HEX);
        return _sessionTokens[i];
    }
    return "";
}

/******************************************************************************/
/*!
  @brief    Clears the session associated with the request cookie.
  @param    server              Webserver object
*/
/******************************************************************************/
void UserHandler::clearSession(ESP8266WebServer *server) {
    String sessionToken = _getSessionToken(server);
    for (uint8_t i = 0; i < _numberUsers; i++) {
        if (_sessionTokens[i] == sessionToken) {
            _sessionTokens[i] = "";
            return;
        }
    }
}

/******************************************************************************/
/*!
  @brief    Gets the permission level for a session or HTTP Basic request.
  @param    server              Webserver object
  @return   uint8_t             Effective permission level
*/
/******************************************************************************/
uint8_t UserHandler::getPermissionLevel(ESP8266WebServer *server) {
    String sessionToken = _getSessionToken(server);
    for (uint8_t i = 0; i < _numberUsers; i++) {
        uint8_t userIndex = i * USER_INFO_LENGTH;
        if (sessionToken != "" && _sessionTokens[i] == sessionToken) {
            return atoi(_users[userIndex + 2].c_str());
        }
        if (server->authenticate(_users[userIndex].c_str(),
                                 _users[userIndex + 1].c_str())) {
            return atoi(_users[userIndex + 2].c_str());
        }
    }
    return PERMISSION_LEVEL_ALL;
}

/******************************************************************************/
/*!
  @brief    Checks if user has permission.
  @param    permissionLevel     0 = not logged in, 1 = user, 2 = admin
  @param    server              Webserver object
  @return   bool                True if user has permission
*/
/******************************************************************************/
bool UserHandler::checkPermission(uint8_t permissionLevel, ESP8266WebServer *server) {
    if (permissionLevel == PERMISSION_LEVEL_ALL) {
        return true;
    }
    return getPermissionLevel(server) >= permissionLevel;
}

/******************************************************************************/
/*!
  @brief    Extracts the configured session token from the Cookie header.
  @param    server              Webserver object
  @return   String              Session token, empty when absent
*/
/******************************************************************************/
String UserHandler::_getSessionToken(ESP8266WebServer *server) {
    String cookieHeader = server->header("Cookie");
    String cookieName = String(SESSION_COOKIE_NAME) + "=";
    int16_t tokenStart = cookieHeader.indexOf(cookieName);

    if (tokenStart < 0) {
        return "";
    }

    tokenStart += cookieName.length();
    int16_t tokenEnd = cookieHeader.indexOf(';', tokenStart);
    if (tokenEnd < 0) {
        tokenEnd = cookieHeader.length();
    }

    return cookieHeader.substring(tokenStart, tokenEnd);
}

/******************************************************************************/
/*!
  @brief    Parses configuration file line to user information array.
  @param    line                The string to parse
  @return   String*             Array of user information
*/
/******************************************************************************/
String* UserHandler::_parseLine(String line) {
    static String userInfo[3];

    int16_t indexForUsername = line.indexOf(":");                               //gets loc of first ":"
    int16_t indexForPassword = line.indexOf(":", indexForUsername + 1);         //gets loc of second ":"

    if (indexForUsername <= 0 || indexForPassword <= indexForUsername + 1 ||
        indexForPassword >= (int16_t) line.length() - 1) {
        return NULL;
    }

    userInfo[0] = line.substring(0, indexForUsername);                          //Selects xxxx from xxxx:yyyy:zzzz, username
    userInfo[1] = line.substring(indexForUsername + 1, indexForPassword);       //Selects yyyy from xxxx:yyyy:zzzz, password
    userInfo[2] = line.substring(indexForPassword + 1);                         //Selects zzzz from xxxx:yyyy:zzzz, usertype
	
    return userInfo;
}
