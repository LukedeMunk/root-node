/******************************************************************************/
/*
 * File:    RootNode.ino
 * Authors: Luke de Munk
 * Version: 1.0.0
 * 
 * Brief:   The main firmware file of the vunerable-by-design ESP8266
 *          controller.
 * 
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#include "ESP8266WebServer.h"                                                   //For running the webserver
#include "FS.h"                                                                 //For SPIFFS
#include "stdint.h"                                                             //For defining bits per integer
#include "configuration.h"                                                      //For the configuration
#include "ExternalFileRegistry.h"                                               //For virtual-to-external file mappings
#include "UserHandler.h"                                                        //For handling the users from the configuration.conf
#include "SerialCommandExecuter.h"                                              //For handling serial commands
#include "Debugger.h"                                                           //For handling debug messages
#include "PersistentMemory.h"                                                      //For reading and writing the hostname
#include "StartupText.h"                                                        //For printing startup log files

#define EXTERNAL_UPLOAD_TEMP_PATH       EXTERNAL_FILE_FOLDER    "/.upload.tmp"
#define EXTERNAL_UPLOAD_BACKUP_PATH     EXTERNAL_FILE_FOLDER    "/.upload.bak"

struct NetworkConfiguration {
    String ssid = "";
    String password;
    String hostname;
    IPAddress ip;
};

enum FileUploadResult {
    fileUploadIdle,
    fileUploadInProgress,
    fileUploadSuccess,
    fileUploadUnauthorized,
    fileUploadInvalidName,
    fileUploadTooLarge,
    fileUploadStorageError
};

void handleFileRequest(String endpoint, String path, uint8_t permissionLevel = PERMISSION_LEVEL_ALL);
void setLedBrightness(uint16_t brightness, bool turnOff = false);

uint8_t ledState = OFF;                                                         //For led state variable
uint16_t ledBrightness = MAX_BRIGHTNESS;                                        //For led brightness

ESP8266WebServer server(80);                                                    //Object that listens for HTTP requests on port 80
NetworkConfiguration networkConfig;
UserHandler userHandler;                                                        //For handling the authentication
SerialCommandExecuter cliExecuter;                                              //For handling serial commands

File fsUploadFile;                                                              //A File object to temporarily store the received file
bool isUploadAuthorized = false;
char activeUploadStoragePath[EXTERNAL_FILE_MAX_STORAGE_PATH_LENGTH] = "";
char activeUploadVirtualPath[VIRTUAL_FILE_MAX_PATH_LENGTH] = "";
uint32_t activeUploadSize = 0;

FileUploadResult fileUploadResult = fileUploadIdle;
const char* REQUEST_HEADER_KEYS[] = {"Cookie"};
const size_t NUMBER_OF_REQUEST_HEADERS = 1;

/******************************************************************************/
/*!
  @brief    Setup the ESP8266.
*/
/******************************************************************************/
void setup() {
    Serial.begin(115200);                                                       //Serial port for debugging purposes
    analogWriteResolution(10);                                                  //Set PWM resolution to 10 bits
    delay(500);
    
    initializeSpiffs();

    bool isSerialConsoleEnabled = PersistentMemory::getDebugEnabled();

    /* Intentional challenge: the serial password is leaked in encoded boot bytes. */
    printStartupText(SERIAL_PASSWORD, isSerialConsoleEnabled);
    
    ledBrightness = PersistentMemory::getBrightness();
    ledState = PersistentMemory::getPowerState();
    pinMode(LED_BUILTIN, OUTPUT);
    if (ledState == ON) {
        setLedBrightness(ledBrightness);
    } else {
        setLedBrightness(0, true);
    }

    initializeHostname();
    initializeWifi();
    initializeServer();
    
    userHandler.updateUsers();
    cliExecuter.setUsers(userHandler.getUsers(), userHandler.getNumberOfUsers()); //Send users to the command executer for the 'webusers' command
    
    debugln("Serial commands available. Type 'help' for help.");
    cliExecuter.begin();
}

/******************************************************************************/
/*!
  @brief    Mainloop.
*/
/******************************************************************************/
void loop() {
    server.handleClient();
    cliExecuter.executeCommand();
}

#pragma region Request handlers
/******************************************************************************/
/*!
  @brief    Returns device and firmware information to authenticated users.
*/
/******************************************************************************/
void getDeviceInfoRequest() {
    if (!requireApiPermission(PERMISSION_LEVEL_USER)) {
        return;
    }

    String response = "{";
    response += "\"chipModel\":\"" DEVICE_CHIP_MODEL "\",";
    response += "\"chipId\":\"" + String(ESP.getChipId(), HEX) + "\",";
    response += "\"flashSize\":" + String(ESP.getFlashChipRealSize()) + ",";
    response += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    response += "\"firmwareVersion\":\"" FIRMWARE_VERSION "\",";
    response += "\"buildVersion\":\"" + String(__DATE__) + " " + String(__TIME__) + "\",";
    response += "\"hostname\":\"" + WiFi.hostname() + "\",";
    response += "\"ipAddress\":\"" + WiFi.localIP().toString() + "\"}";

    server.sendHeader("Cache-Control", "no-store");
    server.send(HTTP_CODE_OK, "application/json", response);
}

/******************************************************************************/
/*!
  @brief    Returns the simulated virtual filesystem as JSON.
*/
/******************************************************************************/
void getFileListRequest() {
    String response;

    if (!requireApiPermission(PERMISSION_LEVEL_USER)) {
        return;
    }
    if (!cliExecuter.getFileListJson(response)) {
        server.send(HTTP_CODE_INTERNAL_ERROR, "application/json", "{\"error\":\"virtual_file_list_failed\"}");
        return;
    }

    server.sendHeader("Cache-Control", "no-store");
    server.send(HTTP_CODE_OK, "application/json", response);
}

/******************************************************************************/
/*!
  @brief    Updates the LED power state from the request parameters.
*/
/******************************************************************************/
void setLedPowerRequest() {
    if (!server.arg("state")) {
        server.send(HTTP_CODE_BAD_REQUEST);
        return;
    }

    uint8_t requestedState = atoi(server.arg("state").c_str()) == ON ? ON : OFF;
    if (ledState != requestedState) {
        ledState = requestedState;
        PersistentMemory::writePowerState(ledState);
    }
    if (ledState == ON) {
        setLedBrightness(ledBrightness);
    } else {
        setLedBrightness(0, true);
    }

    server.send(HTTP_CODE_OK);
}

/******************************************************************************/
/*!
  @brief    Updates the LED brightness from the request parameters.
*/
/******************************************************************************/
void setLedBrightnessRequest() {
    if (!server.arg("brightness")) {
        server.send(HTTP_CODE_BAD_REQUEST);
        return;
    }

    uint16_t requestedBrightness = (uint16_t) atoi(server.arg("brightness").c_str());
    if (requestedBrightness > MAX_BRIGHTNESS) {
        requestedBrightness = MAX_BRIGHTNESS;
    }
    if (requestedBrightness < MIN_BRIGHTNESS) {
        requestedBrightness = MIN_BRIGHTNESS;
    }
    if (ledBrightness != requestedBrightness) {
        ledBrightness = requestedBrightness;
        PersistentMemory::writeBrightness(ledBrightness);
    }
    if (ledState == ON) {
        setLedBrightness(ledBrightness);
    }
    
    server.send(HTTP_CODE_OK);
}

/******************************************************************************/
/*!
  @brief    Restarts the controller for an authenticated administrator.
*/
/******************************************************************************/
void restartRequest() {
    if (!requireApiPermission(PERMISSION_LEVEL_ADMIN)) {
        return;
    }

    server.send(HTTP_CODE_OK);
    delay(100);
    ESP.restart();
}

/******************************************************************************/
/*!
  @brief    Clears stored Wi-Fi configuration and restarts the controller.
*/
/******************************************************************************/
void resetNetworkRequest() {
    if (!requireApiPermission(PERMISSION_LEVEL_ADMIN)) {
        return;
    }

    server.send(HTTP_CODE_OK);
    delay(100);
    WiFi.disconnect(true);
    delay(100);
    ESP.restart();
}

/******************************************************************************/
/*!
  @brief    Authenticates web credentials and creates a session.
*/
/******************************************************************************/
void loginRequest() {
    if (!server.hasArg("username") || !server.hasArg("password")) {
        server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"authenticated\":false,\"role\":\"guest\"}");
        return;
    }

    uint8_t permissionLevel;
    String sessionToken = userHandler.createSession(
        server.arg("username"),
        server.arg("password"),
        &permissionLevel
    );

    if (sessionToken == "") {
        server.send(HTTP_CODE_UNAUTHORIZED, "application/json", "{\"authenticated\":false,\"role\":\"guest\"}");
        return;
    }

    String cookie = String(SESSION_COOKIE_NAME) + "=" + sessionToken + "; Path=/; HttpOnly; SameSite=Strict";
    server.sendHeader("Set-Cookie", cookie);
    server.send(HTTP_CODE_OK, "application/json", getAuthenticationJson(permissionLevel));
}

/******************************************************************************/
/*!
  @brief    Clears the active web session and authentication cookie.
*/
/******************************************************************************/
void logoutRequest() {
    userHandler.clearSession(&server);
    String cookie = String(SESSION_COOKIE_NAME) + "=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict";
    server.sendHeader("Set-Cookie", cookie);
    server.send(HTTP_CODE_OK, "application/json", getAuthenticationJson(PERMISSION_LEVEL_ALL));
}

/******************************************************************************/
/*!
  @brief    Handles HTTP request. Sets the network configuration with the
            specified configuration and restarts the controller.
*/
/******************************************************************************/
void configureNetworkRequest() {
    const char* parameters[] = {"ssid", "password"};

    if (!server.arg("ssid") || !server.arg("password")) {
        server.send(HTTP_CODE_BAD_REQUEST);
        return;
    }

    server.send(HTTP_CODE_OK);
    
    networkConfig.ssid = server.arg("ssid");
    networkConfig.password = server.arg("password");

    /* Save in non-volatile memory */
    WiFi.persistent(true);
    WiFi.begin(networkConfig.ssid.c_str(), networkConfig.password.c_str());

    debugln("Controller needs to be rebooted in order to finish configuration", true);

    ESP.restart();
}

/******************************************************************************/
/*!
  @brief    Sends the requested file if the user has permission.
  @param    endpoint            Public endpoint for the requested file
  @param    path                Path to the file
  @param    permissionLevel     0 = not logged in, 1 = user, 2 = admin
*/
/******************************************************************************/
void handleFileRequest(String endpoint, String path, uint8_t permissionLevel) {
    if (!userHandler.checkPermission(permissionLevel, &server)) {
        String loginLocation = "/login?redirect=" + endpoint;
        server.sendHeader("Location", loginLocation);
        server.send(HTTP_CODE_FOUND);
        return;
    }

    String contentType = getContentType(path);
    String filePath = path;
    String compressedPath = path + ".gz";
    bool isCompressed = SPIFFS.exists(compressedPath);

    if (isCompressed) {
        filePath = compressedPath;
    }

    if (!SPIFFS.exists(filePath)) {
        debugln(String("File not found for endpoint '") + endpoint + "': " + filePath);
        server.send(HTTP_CODE_NOT_FOUND, "text/plain", "Not Found");
        return;
    }

    File file = SPIFFS.open(filePath, "r");
    if (!file) {
        debugln(String("Could not open file for endpoint '") + endpoint + "': " + filePath);
        server.send(HTTP_CODE_INTERNAL_ERROR, "text/plain", "Could not open file");
        return;
    }

    if (isCompressed) {
        server.sendHeader("Content-Encoding", "gzip");
    }

    server.streamFile(file, contentType);
    file.close();
}

/******************************************************************************/
/*!
  @brief    Handles the file upload to the SPIFFS.
*/
/******************************************************************************/
void handleFileUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        fileUploadResult = fileUploadIdle;
        activeUploadStoragePath[0] = '\0';
        activeUploadVirtualPath[0] = '\0';
        activeUploadSize = 0;
        isUploadAuthorized = userHandler.checkPermission(PERMISSION_LEVEL_ADMIN, &server);
        if (!isUploadAuthorized) {
            fileUploadResult = fileUploadUnauthorized;
            return;
        }

        if (!ExternalFileRegistry::resolveUploadName(
                upload.filename.c_str(), activeUploadVirtualPath,
                sizeof(activeUploadVirtualPath), activeUploadStoragePath,
                sizeof(activeUploadStoragePath))) {
            fileUploadResult = fileUploadInvalidName;
            return;
        }

        debugln(String("Upload external file: ") + activeUploadVirtualPath);

        if (SPIFFS.exists(EXTERNAL_UPLOAD_BACKUP_PATH)) {
            if (!SPIFFS.exists(activeUploadStoragePath)) {
                if (!SPIFFS.rename(EXTERNAL_UPLOAD_BACKUP_PATH, activeUploadStoragePath)) {
                    fileUploadResult = fileUploadStorageError;
                    return;
                }
            } else {
                SPIFFS.remove(EXTERNAL_UPLOAD_BACKUP_PATH);
            }
        }

        SPIFFS.remove(EXTERNAL_UPLOAD_TEMP_PATH);
        fsUploadFile = SPIFFS.open(EXTERNAL_UPLOAD_TEMP_PATH, "w");
        fileUploadResult = fsUploadFile ? fileUploadInProgress : fileUploadStorageError;
        return;
    }

    if (!isUploadAuthorized || fileUploadResult != fileUploadInProgress) {
        return;
    }

    if (upload.status == UPLOAD_FILE_WRITE && fsUploadFile) {
        if (upload.currentSize > EXTERNAL_FILE_MAX_UPLOAD_SIZE - activeUploadSize) {
            fsUploadFile.close();
            SPIFFS.remove(EXTERNAL_UPLOAD_TEMP_PATH);
            fileUploadResult = fileUploadTooLarge;
            return;
        }

        if (fsUploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
            fsUploadFile.close();
            SPIFFS.remove(EXTERNAL_UPLOAD_TEMP_PATH);
            fileUploadResult = fileUploadStorageError;
            return;
        }

        activeUploadSize += upload.currentSize;
        return;
    }

    if (upload.status == UPLOAD_FILE_END) {
        if (!fsUploadFile) {
            fileUploadResult = fileUploadStorageError;
            return;
        }

        fsUploadFile.close();
        bool hadExistingFile = SPIFFS.exists(activeUploadStoragePath);
        SPIFFS.remove(EXTERNAL_UPLOAD_BACKUP_PATH);

        if (hadExistingFile &&
            !SPIFFS.rename(activeUploadStoragePath, EXTERNAL_UPLOAD_BACKUP_PATH)) {
            SPIFFS.remove(EXTERNAL_UPLOAD_TEMP_PATH);
            fileUploadResult = fileUploadStorageError;
            return;
        }

        if (!SPIFFS.rename(EXTERNAL_UPLOAD_TEMP_PATH, activeUploadStoragePath)) {
            SPIFFS.remove(EXTERNAL_UPLOAD_TEMP_PATH);

            if (hadExistingFile) {
                SPIFFS.rename(EXTERNAL_UPLOAD_BACKUP_PATH, activeUploadStoragePath);
            }

            fileUploadResult = fileUploadStorageError;
            return;
        }

        SPIFFS.remove(EXTERNAL_UPLOAD_BACKUP_PATH);
        debugln("File uploaded. Size: " + String(upload.totalSize) + " bytes");
        const ExternalFileMapping* mapping = ExternalFileRegistry::findByVirtualPath(activeUploadVirtualPath);

        if (mapping != NULL && strcmp(mapping->externalPath, EXT_FILE_WEBUSERS_DB) == 0) {
            userHandler.updateUsers();
            cliExecuter.setUsers(userHandler.getUsers(), userHandler.getNumberOfUsers());
        }

        fileUploadResult = fileUploadSuccess;
        return;
    }

    if (upload.status == UPLOAD_FILE_ABORTED) {
        if (fsUploadFile) {
            fsUploadFile.close();
        }

        SPIFFS.remove(EXTERNAL_UPLOAD_TEMP_PATH);
        fileUploadResult = fileUploadStorageError;
    }
}

/******************************************************************************/
/*!
  @brief    Streams a registered external file by its virtual Linux path.
*/
/******************************************************************************/
void handleFileDownload() {
    if (!requireApiPermission(PERMISSION_LEVEL_USER)) {
        return;
    }

    debugln("File download request");

    const String virtualPath = server.arg("filekey");
    char storagePath[EXTERNAL_FILE_MAX_STORAGE_PATH_LENGTH];
    char fileName[EXTERNAL_FILE_MAX_NAME_LENGTH + 1];
    if (!ExternalFileRegistry::resolveVirtualPath(virtualPath.c_str(), storagePath, sizeof(storagePath), fileName, sizeof(fileName))) {
        server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"unsupported_file\"}");
        return;
    }

    File download = SPIFFS.open(storagePath, "r");
    if (!download) {
        server.send(HTTP_CODE_NOT_FOUND, "application/json", "{\"error\":\"file_not_available\"}");
        return;
    }

    server.sendHeader("Content-Disposition", "attachment; filename=\"" + String(fileName) + "\"");
    server.sendHeader("Connection", "close");
    server.streamFile(download, "application/octet-stream");
    download.close();
}
#pragma endregion

#pragma region Initializers
/******************************************************************************/
/*!
  @brief    Initializes hostname.
*/
/******************************************************************************/
void initializeHostname() {
    String hostname = PersistentMemory::getHostname();
    
    /* Check if custom hostname is set, otherwise use default */
    if (hostname == "") {
        hostname = DEFAULT_HOSTNAME;
    }
    
    if (WiFi.hostname(hostname)) {
        debugln("Hostname setted to '" + hostname + "'");
    } else {
        debugln("Could not set '" + hostname + "' as hostname");
    }
}

/******************************************************************************/
/*!
  @brief    Initializes WiFi configuration
*/
/******************************************************************************/
void initializeWifi() {
    debugln("Initializing WiFi", true);

    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
    networkConfig.ssid = WiFi.SSID();
    networkConfig.password = WiFi.psk();

    uint8_t baseMac[6];
    WiFi.macAddress(baseMac);
    String uniqueApId = String(baseMac[0], HEX) + String(baseMac[2], HEX) + String(baseMac[4], HEX);
    String configNetworkApSsid = NETWORK_CONFIG_AP_SSID + uniqueApId;

    /* If network is not initialized, start as access point */
    if (networkConfig.ssid == "") {
        debugln("No configured network found. Starting as AP and waiting for network configuration", true);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(configNetworkApSsid);
        return;
    }
    
    connectToWiFi();

    if (WiFi.status() != WL_CONNECTED) {
        debugln("Failed to connect. Starting network configuration AP", true);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(configNetworkApSsid);
    }
}

/******************************************************************************/
/*!
  @brief    Tries to connect to the configured Wi-Fi network.
*/
/******************************************************************************/
void connectToWiFi() {
    debugln("Connecting to WiFi", true);

    WiFi.begin(networkConfig.ssid.c_str(), networkConfig.password.c_str());

    uint32_t connectionStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - connectionStart < 60000) {
        delay(500);
        yield();
    }

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    networkConfig.ip = WiFi.localIP();

    debugln("WiFi strength [" + String(WiFi.RSSI()) + String("dBm]"));
    debugln("Copy and paste the following URL: http://" + WiFi.hostname(), true);
}

/******************************************************************************/
/*!
  @brief    Initializes the internal SPI Flash File System.
*/
/******************************************************************************/
void initializeSpiffs() {
    debugln("Mounting SPI Flash File System");

    if (!SPIFFS.begin()) {
        debugln("Failed to mount SPIFFS, please check it or reflash the filesystem", true);
        return;
    }
}

/******************************************************************************/
/*!
  @brief    Takes care of the webservices like page loading.
*/
/******************************************************************************/
void initializeServer() {
    server.collectHeaders(REQUEST_HEADER_KEYS, NUMBER_OF_REQUEST_HEADERS);
    server.onNotFound([]() {
        debugln("Route not found: " + server.uri());
        server.send(HTTP_CODE_NOT_FOUND, "text/plain", "Not Found");
    });

    /* If in network configuration mode, load only configuration command and page */
    if (WiFi.status() != WL_CONNECTED) {
        server.serveStatic("/", SPIFFS, "/templates/network_config.html");
        
        /* CSS files */
        server.serveStatic("/variables.css", SPIFFS, "/css/variables.css");
        server.serveStatic("/styles.css", SPIFFS, "/css/styles.css");

        /* JavaScript files */
        server.serveStatic("/base.js", SPIFFS, "/js/base.js");
        server.serveStatic("/network_config.js", SPIFFS, "/js/pages/network_config.js");
        server.serveStatic("/language_pack_dutch.js", SPIFFS, "/js/language_pack_dutch.js");
        server.serveStatic("/language_pack_english.js", SPIFFS, "/js/language_pack_english.js");
        
        /* Images */
        server.serveStatic("/favicon.ico", SPIFFS, "/images/favicon.ico");

        server.on("/configure_network", HTTP_POST, configureNetworkRequest);
        server.begin();
        return;
    }

    initializeFileEndpoints();
    initializeCommandEndpoints();
    
    server.begin();                                                             //Start server
}

/******************************************************************************/
/*!
  @brief    XX
*/
/******************************************************************************/
void initializeFileEndpoints() {
    /* Route for home page */
    server.on("/", HTTP_GET, []() {
        handleFileRequest("/", "/templates/index.html");
    });
    
    /* Route for shared user and administrator information */
    server.on("/device", HTTP_GET, []() {
        handleFileRequest("/device", "/templates/device.html", PERMISSION_LEVEL_USER);
    });

    /* Route for the login page */
    server.on("/login", HTTP_GET, []() {
        handleFileRequest("/login", "/templates/login.html");
    });

    /* Route for unified file management */
    server.on("/files", HTTP_GET, []() {
        handleFileRequest("/files", "/templates/files.html", PERMISSION_LEVEL_USER);
    });
    
    /* Load variables.css file, CSS globals */
    server.on("/variables.css", HTTP_GET, []() {
        handleFileRequest("/variables.css", "/css/variables.css");
    });
    
    /* Load styles.css file, styling for desktop version */
    server.on("/styles.css", HTTP_GET, []() {
        handleFileRequest("/styles.css", "/css/styles.css");
    });

    /* Load base.js file, JavaScript for site */
    server.on("/base.js", HTTP_GET, []() {
        handleFileRequest("/base.js", "/js/base.js");
    });

    /* Load language_pack_dutch.js file, translations */
    server.on("/language_pack_dutch.js", HTTP_GET, []() {
        handleFileRequest("/language_pack_dutch.js", "/js/language_pack_dutch.js");
    });

    /* Load language_pack_english.js file, translations */
    server.on("/language_pack_english.js", HTTP_GET, []() {
        handleFileRequest("/language_pack_english.js", "/js/language_pack_english.js");
    });

    /* Load admin.js file, JavaScript for the shared device page */
    server.on("/device.js", HTTP_GET, []() {
        handleFileRequest("/device.js", "/js/pages/device.js", PERMISSION_LEVEL_USER);
    });

    /* Load files.js file, JavaScript for files page */
    server.on("/files.js", HTTP_GET, []() {
        handleFileRequest("/files.js", "/js/pages/files.js", PERMISSION_LEVEL_USER);
    });

    /* Load index.js file, JavaScript for index page */
    server.on("/index.js", HTTP_GET, []() {
        handleFileRequest("/index.js", "/js/pages/index.js");
    });

    /* Load login.js file, JavaScript for login page */
    server.on("/login.js", HTTP_GET, []() {
        handleFileRequest("/login.js", "/js/pages/login.js");
    });

    /* Favicon */
    server.on("/favicon.ico", HTTP_GET, []() {
        handleFileRequest("/favicon.ico", "/images/favicon.ico");
    });
}

/******************************************************************************/
/*!
  @brief    XX
*/
/******************************************************************************/
void initializeCommandEndpoints() {
    /*
    * Routes for JavaScript data requesting and receiving
    */
    /* Reports the current Basic or session authentication state */
    server.on("/get_authentication_status", HTTP_GET, []() {
        uint8_t permissionLevel = userHandler.getPermissionLevel(&server);
        server.sendHeader("Cache-Control", "no-store");
        server.send(HTTP_CODE_OK, "application/json", getAuthenticationJson(permissionLevel));
    });

    /* Returns LED state */
    server.on("/get_state", HTTP_GET, []() {
        server.send(HTTP_CODE_OK, "text/plain", String(ledState));
    });

    /* Returns LED brightness */
    server.on("/get_brightness", HTTP_GET, []() {
        server.send(HTTP_CODE_OK, "text/plain", String(ledBrightness));
    });

    /* Returns current chip and firmware information to authenticated users */
    server.on("/get_device_info", HTTP_GET, []() {
        getDeviceInfoRequest();
    });

    /* Returns files in the simulated Linux filesystem */
    server.on("/get_files", HTTP_GET, []() {
        getFileListRequest();
    });

    /* Route for setting power */
    server.on("/set_power", HTTP_GET, []() {
        setLedPowerRequest();
    });
    
    /* Route for setting brightness */
    server.on("/set_brightness", HTTP_GET, []() {
        setLedBrightnessRequest();
    });

    /* Route for restarting the server */
    server.on("/restart", HTTP_GET, []() {
        restartRequest();
    });

    /* Route for clearing the configured network */
    server.on("/reset_network", HTTP_POST, []() {
        resetNetworkRequest();
    });

    /* Creates an HttpOnly session from the existing user credentials */
    server.on("/login", HTTP_POST, []() {
        loginRequest();
    });

    /* Ends the current web session */
    server.on("/logout", HTTP_POST, []() {
        logoutRequest();
    });
    /*
    * End of JavaScript data receiving
    */
    
    /*
    * Routes for file management
    */
    /* Route for file upload request */
    server.on("/upload", HTTP_POST, []() {
        if (fileUploadResult == fileUploadUnauthorized || !isUploadAuthorized) {
            requireApiPermission(PERMISSION_LEVEL_ADMIN);
            fileUploadResult = fileUploadIdle;
            return;
        }

        isUploadAuthorized = false;
        if (fileUploadResult == fileUploadInvalidName) {
            fileUploadResult = fileUploadIdle;
            server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"invalid_filename\"}");
            return;
        }

        if (fileUploadResult == fileUploadTooLarge) {
            fileUploadResult = fileUploadIdle;
            server.send(HTTP_CODE_PAYLOAD_TOO_LARGE, "application/json", "{\"error\":\"file_too_large\"}");
            return;
        }

        if (fileUploadResult != fileUploadSuccess) {
            fileUploadResult = fileUploadIdle;
            server.send(HTTP_CODE_INTERNAL_ERROR, "application/json", "{\"error\":\"upload_failed\"}");
            return;
        }

        fileUploadResult = fileUploadIdle;
        String response = String("{\"result\":\"uploaded\",\"path\":\"") + activeUploadVirtualPath + "\"}";
        server.send(HTTP_CODE_OK, "application/json", response);

        debugln("File upload request");
    }, handleFileUpload);
    
    /* Route for file download request */
    server.on("/download", HTTP_POST, []() {
        handleFileDownload();
    });
    /*
    * End of routes for file management
    */
}
#pragma endregion

#pragma region General Utilities
/******************************************************************************/
/*!
  @brief    XXX
*/
/******************************************************************************/
void setLedBrightness(uint16_t brightness, bool turnOff) {
    if (brightness > MAX_BRIGHTNESS) {
        brightness = MAX_BRIGHTNESS;
    }

    if (!turnOff && brightness < MIN_BRIGHTNESS) {
        brightness = MIN_BRIGHTNESS;
    }

    analogWrite(LED_BUILTIN, MAX_BRIGHTNESS-brightness);                        //analogWrite() on ESP8266 D1 Mini board is inverted
}

/******************************************************************************/
/*!
  @brief    Converts a permission level to its API role name.
  @param    permissionLevel     Effective permission level
  @returns  String              Guest, user or administrator role
*/
/******************************************************************************/
String getRoleName(uint8_t permissionLevel) {
    if (permissionLevel >= PERMISSION_LEVEL_ADMIN) {
        return "admin";
    }

    if (permissionLevel >= PERMISSION_LEVEL_USER) {
        return "user";
    }

    return "guest";
}

/******************************************************************************/
/*!
  @brief    Builds the shared authentication status response.
  @param    permissionLevel     Effective permission level
  @returns  String              Authentication status JSON
*/
/******************************************************************************/
String getAuthenticationJson(uint8_t permissionLevel) {
    bool isAuthenticated = permissionLevel >= PERMISSION_LEVEL_USER;

    String response = "{\"authenticated\":";
    response += isAuthenticated ? "true" : "false";
    response += ",\"role\":\"" + getRoleName(permissionLevel) + "\"}";

    return response;
}

/******************************************************************************/
/*!
  @brief    Rejects an API request without opening a browser login popup.
  @param    permissionLevel     Required permission level
  @returns  bool                True when access is allowed
*/
/******************************************************************************/
bool requireApiPermission(uint8_t permissionLevel) {
    uint8_t currentPermission = userHandler.getPermissionLevel(&server);
    if (currentPermission >= permissionLevel) {
        return true;
    }

    uint16_t responseCode = currentPermission >= PERMISSION_LEVEL_USER
        ? HTTP_CODE_FORBIDDEN
        : HTTP_CODE_UNAUTHORIZED;

    server.send(responseCode, "application/json", "{\"error\":\"access_denied\"}");
    return false;
}

/******************************************************************************/
/*!
  @brief    Converts the file extension to the MIME type.
  @param    filename            Name of the file
  @returns  String              MIME type of the file
*/
/******************************************************************************/
String getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".gz")) return "application/x-gzip";
    else if (filename.endsWith(".txt")) return "text/plain";
    return "text/plain";
}
#pragma endregion
