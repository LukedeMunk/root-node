/******************************************************************************/
/*
 * File:    SerialCommandExecuter.cpp
 * Authors: Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Provides a non-blocking, simulated embedded Linux serial shell.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#include "SerialCommandExecuter.h"

#include "ESP8266WiFi.h"
#include "FS.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

const SerialCommandExecuter::CommandDefinition SerialCommandExecuter::COMMAND_REGISTRY[] = {
    {"help", &SerialCommandExecuter::_handleHelp, 0, 1, false, "help [command]"},
    {"debug", &SerialCommandExecuter::_handleDebug, 1, 1, false, "debug [--on|--off]"},
    {"su", &SerialCommandExecuter::_handleSu, 0, 0, false, "su"},
    {"reboot", &SerialCommandExecuter::_handleReboot, 0, 0, false, "reboot"},
    {"webusers", &SerialCommandExecuter::_handleWebUsers, 0, 0, true, "webusers"},
    {"hostname", &SerialCommandExecuter::_handleHostname, 0, 2, false, "hostname [--set name|--default]"},
    {"whoami", &SerialCommandExecuter::_handleWhoAmI, 0, 0, false, "whoami"},
    {"pwd", &SerialCommandExecuter::_handlePwd, 0, 0, false, "pwd"},
    {"cd", &SerialCommandExecuter::_handleCd, 0, 1, false, "cd [directory]"},
    {"ls", &SerialCommandExecuter::_handleLs, 0, 2, false, "ls [-l] [path]"},
    {"cat", &SerialCommandExecuter::_handleCat, 1, 1, false, "cat file"},
    {"head", &SerialCommandExecuter::_handleHead, 1, 3, false, "head [-n N] file"},
    {"tail", &SerialCommandExecuter::_handleTail, 1, 3, false, "tail [-n N] file"},
    {"echo", &SerialCommandExecuter::_handleEcho, 0, SHELL_MAX_ARGUMENTS, false, "echo [arguments...]"},
    {"clear", &SerialCommandExecuter::_handleClear, 0, 0, false, "clear"},
    {"uname", &SerialCommandExecuter::_handleUname, 0, 1, false, "uname [-a]"},
    {"uptime", &SerialCommandExecuter::_handleUptime, 0, 0, false, "uptime"},
    {"free", &SerialCommandExecuter::_handleFree, 0, 0, false, "free"},
    {"df", &SerialCommandExecuter::_handleDf, 0, 0, false, "df"},
    {"id", &SerialCommandExecuter::_handleId, 0, 0, false, "id"},
    {"date", &SerialCommandExecuter::_handleDate, 0, 0, false, "date"},
    {"env", &SerialCommandExecuter::_handleEnv, 0, 0, false, "env"},
    {"dmesg", &SerialCommandExecuter::_handleDmesg, 0, 0, false, "dmesg"},
    {"ip", &SerialCommandExecuter::_handleIp, 1, 1, false, "ip [addr|route]"},
    {"exit", &SerialCommandExecuter::_handleExit, 0, 0, false, "exit"},
    {"logout", &SerialCommandExecuter::_handleExit, 0, 0, false, "logout"},
    {"./", &SerialCommandExecuter::_handleRun, 0, 1, false, "./filename [argument]"},
    {"objdump", &SerialCommandExecuter::_handleObjdump, 2, 2, false, "objdump -d filename"},
    {"gpg", &SerialCommandExecuter::_handleGpg, 3, 3, false, "gpg [--encrypt|--decrypt] key text"}
};

const uint8_t SerialCommandExecuter::COMMAND_COUNT =
    sizeof(SerialCommandExecuter::COMMAND_REGISTRY) /
    sizeof(SerialCommandExecuter::COMMAND_REGISTRY[0]);

/******************************************************************************/
/*!
  @brief    Initializes bounded shell state without performing serial output.
*/
/******************************************************************************/
SerialCommandExecuter::SerialCommandExecuter() {
    _isLoggedIn = false;
    _isReadingPassword = false;
    _isConsoleEnabled = false;
    _isLineOverflow = false;
    _isPromptInitialized = false;
    _isPromptVisible = false;
    _lastCharacterWasCarriageReturn = false;
    _uid = SERIAL_COMMAND_USER_UID;
    _lineLength = 0;
    _numberUsers = 0;
    _passwordPurpose = passwordPurposeNone;
    _activeCommandName = NULL;
    _lineBuffer[0] = '\0';
    strncpy(_currentDirectory, "/home/user", sizeof(_currentDirectory));
    strncpy(_previousDirectory, "/home/user", sizeof(_previousDirectory));
    _hostname[0] = '\0';
}

/******************************************************************************/
/*!
  @brief    Sets the users shown by the webusers command.
  @param    users               Array of user information fields
  @param    numUsers            Number of users
*/
/******************************************************************************/
void SerialCommandExecuter::setUsers(String* users, uint8_t numUsers) {
    for (uint8_t i = 0; i < MAX_NUMBER_USERS * USER_INFO_LENGTH; i++) {
        _users[i] = "";
    }

    if (numUsers > MAX_NUMBER_USERS) {
        numUsers = MAX_NUMBER_USERS;
    }

    for (uint8_t i = 0; i < numUsers * USER_INFO_LENGTH; i++) {
        _users[i] = users[i];
    }
    
    _numberUsers = numUsers;
}

/******************************************************************************/
/*!
  @brief    Initializes the virtual shell and applies persisted console policy.
*/
/******************************************************************************/
void SerialCommandExecuter::begin() {
    if (_isPromptInitialized) {
        return;
    }

    String hostname = PersistentMemory::getHostname();
    const char* hostnameSource = hostname.length() == 0 ? DEFAULT_HOSTNAME : hostname.c_str();

    strncpy(_hostname, hostnameSource, sizeof(_hostname) - 1);
    _hostname[sizeof(_hostname) - 1] = '\0';
    if (!_fileSystem.initialize()) {
        Serial.println(F("bash: failed to initialize virtual filesystem"));
    }

    _isPromptInitialized = true;

    _isConsoleEnabled = PersistentMemory::getDebugEnabled();
    if (!_isConsoleEnabled) {
        Serial.println(F("Serial console locked. Type 'debug --on' to request access."));
    }

    _printActivePrompt();
}

/******************************************************************************/
/*!
  @brief    Consumes currently available serial bytes without waiting.
*/
/******************************************************************************/
void SerialCommandExecuter::executeCommand() {
    if (!_isPromptInitialized) {
        begin();
    }

    if (!_isPromptVisible && !_isReadingPassword && Serial.available() > 0) {
        _printActivePrompt();
    }

    while (Serial.available() > 0) {
        _processInputCharacter(static_cast<char>(Serial.read()));
    }
}

/******************************************************************************/
/*!
  @brief    Gets the API file list from the shell's initialized virtual tree.
  @param    output              Receives the JSON response
  @returns  bool                False when the response could not be built
*/
/******************************************************************************/
bool SerialCommandExecuter::getFileListJson(String& output) const {
    return _fileSystem.getFileListJson(output);
}

/******************************************************************************/
/*!
  @brief    Processes one terminal byte, including CRLF and backspace handling.
  @param    character           Received serial character
*/
/******************************************************************************/
void SerialCommandExecuter::_processInputCharacter(char character) {
    if (character == 3 && _isReadingPassword) {
        memset(_lineBuffer, 0, sizeof(_lineBuffer));
        _lineLength = 0;
        _isLineOverflow = false;
        _isReadingPassword = false;
        _passwordPurpose = passwordPurposeNone;
        Serial.println(F("^C"));
        _printActivePrompt();
        return;
    }

    if (character == '\n' && _lastCharacterWasCarriageReturn) {
        _lastCharacterWasCarriageReturn = false;
        return;
    }

    if (character == '\r' || character == '\n') {
        _lastCharacterWasCarriageReturn = character == '\r';
        Serial.println();
        _processCompletedLine();
        return;
    }
    _lastCharacterWasCarriageReturn = false;

    if (character == '\b' || character == 127) {
        if (_lineLength > 0 && !_isLineOverflow) {
            _lineLength--;
            _lineBuffer[_lineLength] = '\0';
            if (!_isReadingPassword) {
                Serial.print(F("\b \b"));
            }
        }
        return;
    }

    if (character < 32 || character > 126) {
        return;
    }

    if (_isLineOverflow) {
        return;
    }

    if (_lineLength >= SERIAL_COMMAND_MAX_LINE_LENGTH) {
        _isLineOverflow = true;
        return;
    }

    _lineBuffer[_lineLength++] = character;
    _lineBuffer[_lineLength] = '\0';
    if (!_isReadingPassword) {
        Serial.write(character);
    }
}

/******************************************************************************/
/*!
  @brief    Executes or rejects a completed bounded input line.
*/
/******************************************************************************/
void SerialCommandExecuter::_processCompletedLine() {
    ShellCommand command;
    ShellParseResult result;

    if (_isLineOverflow) {
        if (_isReadingPassword) {
            Serial.println(F("Authentication input too long"));
        } else {
            Serial.println(F("bash: input line too long"));
        }

        memset(_lineBuffer, 0, sizeof(_lineBuffer));
        _lineLength = 0;
        _isLineOverflow = false;
        _isReadingPassword = false;
        _passwordPurpose = passwordPurposeNone;
        _printActivePrompt();
        return;
    }

    if (_isReadingPassword) {
        _processPassword();
        return;
    }

    result = _parser.parse(_lineBuffer, command);
    if (!_isConsoleEnabled && result == shellParseOk) {
        _processLockedCommand(command);
    } else if (!_isConsoleEnabled && result != shellParseEmpty) {
        Serial.println(F("Serial console locked; use 'debug --on'."));
    } else if (result != shellParseEmpty && result != shellParseOk) {
        _printParseError(result);
    } else if (result == shellParseOk) {
        _executeParsedCommand(command);
    }

    _lineLength = 0;
    _lineBuffer[0] = '\0';
    if (!_isReadingPassword) {
        _printActivePrompt();
    }
}

/******************************************************************************/
/*!
  @brief    Allows only the serial unlock request while the shell is locked.
  @param    command             Parsed restricted command line
*/
/******************************************************************************/
void SerialCommandExecuter::_processLockedCommand(const ShellCommand& command) {
    if (command.itemCount == 2 && strcmp(command.items[0], "debug") == 0 &&
        strcmp(command.items[1], "--on") == 0) {
        _isReadingPassword = true;
        _passwordPurpose = passwordPurposeSerialConsole;
        Serial.print(F("Serial password: "));
        return;
    }

    Serial.println(F("Serial console locked; use 'debug --on'."));
}

/******************************************************************************/
/*!
  @brief    Verifies a hidden password and immediately clears its buffer.
*/
/******************************************************************************/
void SerialCommandExecuter::_processPassword() {
    PasswordPurpose completedPurpose = _passwordPurpose;
    const char* expectedPassword = completedPurpose == passwordPurposeRoot
                                       ? SERIAL_ROOT_PASSWORD
                                       : SERIAL_PASSWORD;
    bool isPasswordCorrect = strcmp(_lineBuffer, expectedPassword) == 0;

    memset(_lineBuffer, 0, sizeof(_lineBuffer));
    _lineLength = 0;
    _isReadingPassword = false;
    _passwordPurpose = passwordPurposeNone;

    if (completedPurpose == passwordPurposeSerialConsole && isPasswordCorrect) {
        PersistentMemory::setDebugEnabled(true);
        _isConsoleEnabled = PersistentMemory::getDebugEnabled();
        if (_isConsoleEnabled) {
            Serial.println(F("debug: enabled"));
            Serial.println(F("Serial commands available. Type 'help' for help."));
        } else {
            Serial.println(F("debug: could not persist serial console state"));
        }
    } else if (completedPurpose == passwordPurposeSerialConsole) {
        Serial.println(F("debug: Authentication failure"));
    } else if (isPasswordCorrect) {
        _uid = SERIAL_COMMAND_ROOT_UID;
        _isLoggedIn = true;
    } else {
        Serial.println(F("su: Authentication failure"));
    }

    _printActivePrompt();
}

/******************************************************************************/
/*!
  @brief    Validates and dispatches a parsed command through the registry.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_executeParsedCommand(ShellCommand& command) {
    const CommandDefinition* definition = _findCommand(command.items[0]);
    uint8_t argumentCount = command.itemCount - 1;
    char** arguments = command.items + 1;

    if (definition == NULL) {
        _printCommandError(command.items[0], "command not found");
        return 127;
    }

    if (argumentCount > 0 &&
        (strcmp(arguments[0], "-h") == 0 || strcmp(arguments[0], "--help") == 0)) {
        Serial.print(F("Usage: "));
        Serial.println(definition->usage);
        return 0;
    }

    if (argumentCount < definition->minimumArguments) {
        _printCommandError(command.items[0], "too few arguments");
        Serial.print(F("Usage: "));
        Serial.println(definition->usage);
        return 2;
    }

    if (argumentCount > definition->maximumArguments) {
        _printCommandError(command.items[0], "too many arguments");
        Serial.print(F("Usage: "));
        Serial.println(definition->usage);
        return 2;
    }

    if (definition->isRootRequired && _uid != SERIAL_COMMAND_ROOT_UID) {
        _printCommandError(command.items[0], "Permission denied");
        return 126;
    }

    _activeCommandName = command.items[0];
    return (this->*(definition->handler))(argumentCount, arguments);
}

/******************************************************************************/
/*!
  @brief    Finds an exact command or the special ./ executable dispatcher.
*/
/******************************************************************************/
const SerialCommandExecuter::CommandDefinition* SerialCommandExecuter::_findCommand(const char* name) const {
    for (uint8_t i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(COMMAND_REGISTRY[i].name, name) == 0 ||
            (strcmp(COMMAND_REGISTRY[i].name, "./") == 0 && strncmp(name, "./", 2) == 0)) {
            return &COMMAND_REGISTRY[i];
        }
    }

    return NULL;
}

/******************************************************************************/
/*!
  @brief    Prints a Linux-style prompt using current identity, hostname and cwd.
*/
/******************************************************************************/
void SerialCommandExecuter::_printPrompt() {
    const char* shownDirectory = _currentDirectory;

    if (strcmp(_currentDirectory, _getHomeDirectory()) == 0) {
        shownDirectory = "~";
    }

    Serial.print(_getUsername());
    Serial.print('@');
    Serial.print(_hostname);
    Serial.print(':');
    Serial.print(shownDirectory);
    Serial.print(_uid == SERIAL_COMMAND_ROOT_UID ? F("# ") : F("$ "));
    _isPromptVisible = true;
}

/******************************************************************************/
/*!
  @brief    Prints the prompt appropriate for the active console policy.
*/
/******************************************************************************/
void SerialCommandExecuter::_printActivePrompt() {
    if (_isConsoleEnabled) {
        _printPrompt();
    } else {
        _printLockedPrompt();
    }
}

/******************************************************************************/
/*!
  @brief    Prints the restricted pre-authentication prompt.
*/
/******************************************************************************/
void SerialCommandExecuter::_printLockedPrompt() {
    Serial.print(F("serial-console> "));
    _isPromptVisible = true;
}

/******************************************************************************/
/*!
  @brief    Prints a consistent parser diagnostic.
*/
/******************************************************************************/
void SerialCommandExecuter::_printParseError(ShellParseResult result) const {
    if (result == shellParseTooManyArguments) {
        Serial.println(F("bash: too many arguments"));
    } else if (result == shellParseUnterminatedQuote) {
        Serial.println(F("bash: unexpected EOF while looking for matching quote"));
    } else if (result == shellParseTrailingEscape) {
        Serial.println(F("bash: unexpected EOF after escape character"));
    }
}

/******************************************************************************/
/*!
  @brief    Prints a command-scoped Linux-style error.
*/
/******************************************************************************/
void SerialCommandExecuter::_printCommandError(const char* command, const char* message) const {
    Serial.print(F("bash: "));
    Serial.print(command);
    Serial.print(F(": "));
    Serial.println(message);
}

/******************************************************************************/
/*!
  @brief    Prints generated help for one command or the complete registry.
*/
/******************************************************************************/
void SerialCommandExecuter::_printHelp(const char* command) const {
    if (command != NULL && command[0] != '\0') {
        const CommandDefinition* definition = _findCommand(command);
        if (definition == NULL) {
            _printCommandError(command, "command not found");
            return;
        }
        Serial.print(F("Usage: "));
        Serial.println(definition->usage);
        return;
    }

    Serial.println(F("Available commands:"));
    for (uint8_t i = 0; i < COMMAND_COUNT; i++) {
        Serial.print(F("  "));
        Serial.println(COMMAND_REGISTRY[i].usage);
    }
}

/******************************************************************************/
/*!
  @brief    Returns the active simulated account name.
*/
/******************************************************************************/
const char* SerialCommandExecuter::_getUsername() const {
    return _uid == SERIAL_COMMAND_ROOT_UID ? "root" : "user";
}

/******************************************************************************/
/*!
  @brief    Returns the active simulated account home directory.
*/
/******************************************************************************/
const char* SerialCommandExecuter::_getHomeDirectory() const {
    return _uid == SERIAL_COMMAND_ROOT_UID ? "/root" : "/home/user";
}

/******************************************************************************/
/*!
  @brief    Resolves a user path against the shell current directory.
*/
/******************************************************************************/
bool SerialCommandExecuter::_resolvePath(const char* path, char* resolvedPath) const {
    return _fileSystem.normalizePath(_currentDirectory, path, resolvedPath, VIRTUAL_FILE_MAX_PATH_LENGTH);
}

/******************************************************************************/
/*!
  @brief    Parses the supported head and tail argument forms.
  @param    argumentCount       Number of supplied arguments
  @param    arguments           Supplied argument values
  @param    filePath            Receives the requested file path
  @param    lineCount           Receives the requested number of lines
  @returns  bool                True when the arguments are valid
*/
/******************************************************************************/
bool SerialCommandExecuter::_parseLineArguments(uint8_t argumentCount, char* arguments[],
                                                const char*& filePath,
                                                uint16_t& lineCount) const {
    lineCount = 10;
    if (argumentCount == 1) {
        filePath = arguments[0];
        return true;
    }

    if (argumentCount != 3 || strcmp(arguments[0], "-n") != 0) {
        return false;
    }

    char* numberEnd = NULL;
    unsigned long requestedLines = strtoul(arguments[1], &numberEnd, 10);
    if (arguments[1][0] == '\0' || numberEnd == NULL || numberEnd[0] != '\0' ||
        requestedLines > UINT16_MAX) {
        return false;
    }

    lineCount = static_cast<uint16_t>(requestedLines);
    filePath = arguments[2];
    return true;
}

/******************************************************************************/
/*!
  @brief    Prints the selected first or last lines of one readable file.
  @param    argumentCount       Number of supplied arguments
  @param    arguments           Supplied argument values
  @param    isTail              True to print the last lines
  @returns  uint8_t             Shell-style status code
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_printFileLines(uint8_t argumentCount, char* arguments[], bool isTail) {
    const char* filePath = NULL;
    uint16_t lineCount = 0;
    char resolvedPath[VIRTUAL_FILE_MAX_PATH_LENGTH];

    if (!_parseLineArguments(argumentCount, arguments, filePath, lineCount)) {
        _printCommandError(_activeCommandName, "invalid argument");
        Serial.print(F("Usage: "));
        Serial.print(_activeCommandName);
        Serial.println(F(" [-n N] file"));
        return 2;
    }

    if (!_resolvePath(filePath, resolvedPath)) {
        _printCommandError(_activeCommandName, "path too long");
        return 1;
    }

    const VirtualNode* node = _fileSystem.find(resolvedPath);
    if (node == NULL) {
        if (_fileSystem.externalFileExists(resolvedPath)) {
            if (lineCount > 0) {
                _fileSystem.printExternalFileLines(resolvedPath, Serial,
                                                   lineCount, isTail);
            }
            return 0;
        }
        _printCommandError(_activeCommandName, "No such file or directory");
        return 1;
    }

    if (node->type == virtualNodeDirectory) {
        _printCommandError(_activeCommandName, "Is a directory");
        return 1;
    }

    if (!_fileSystem.canRead(*node, _uid)) {
        _printCommandError(_activeCommandName, "Permission denied");
        return 1;
    }

    if (lineCount == 0) {
        return 0;
    }

    if (_fileSystem.contentEquals(*node, "$HOSTNAME")) {
        Serial.println(_hostname);
    } else if (_fileSystem.contentEquals(*node, "$UPTIME")) {
        uint32_t uptime = millis() / 1000;
        Serial.printf_P(PSTR("%lu.00 %lu.00\r\n"), static_cast<unsigned long>(uptime),
                        static_cast<unsigned long>(uptime));
    } else if (_fileSystem.contentEquals(*node, "$MEMINFO")) {
        Serial.printf_P(PSTR("MemFree:        %lu kB\r\n"),
                        static_cast<unsigned long>(ESP.getFreeHeap() / 1024));
    } else {
        _fileSystem.printContentLines(*node, Serial, lineCount, isTail);
    }

    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints a compact Linux-style uptime line based on millis.
*/
/******************************************************************************/
void SerialCommandExecuter::_printUptime() const {
    uint32_t totalMinutes = millis() / 60000UL;
    uint16_t days = static_cast<uint16_t>(totalMinutes / 1440UL);
    uint8_t hours = static_cast<uint8_t>((totalMinutes / 60UL) % 24UL);
    uint8_t minutes = static_cast<uint8_t>(totalMinutes % 60UL);

    Serial.printf_P(PSTR("up %u day%s, %02u:%02u\r\n"), days,
                    days == 1 ? "" : "s", hours, minutes);
}

/******************************************************************************/
/*!
  @brief    Prints simulated memory totals with the real current free heap.
*/
/******************************************************************************/
void SerialCommandExecuter::_printFreeMemory() const {
    uint32_t freeBytes = ESP.getFreeHeap();
    uint32_t usedBytes = freeBytes < SERIAL_SIMULATED_RAM_BYTES
                             ? SERIAL_SIMULATED_RAM_BYTES - freeBytes
                             : 0;

    Serial.println(F("              total        used        free"));
    Serial.printf_P(PSTR("Mem:       %10lu  %10lu  %10lu\r\n"),
                    static_cast<unsigned long>(SERIAL_SIMULATED_RAM_BYTES / 1024UL),
                    static_cast<unsigned long>(usedBytes / 1024UL),
                    static_cast<unsigned long>(freeBytes / 1024UL));
    Serial.println(F("Swap:               0           0           0"));
}

/******************************************************************************/
/*!
  @brief    Handles generated command help.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleHelp(uint8_t argumentCount, char* arguments[]) {
    _printHelp(argumentCount == 1 ? arguments[0] : NULL);
    return 0;
}

/******************************************************************************/
/*!
  @brief    Enables or disables existing firmware debug output.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleDebug(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    if (strcmp(arguments[0], "--on") == 0) {
        Serial.println(F("debug: enabled"));
        return 0;
    }

    if (strcmp(arguments[0], "--off") == 0) {
        PersistentMemory::setDebugEnabled(false);
        _isConsoleEnabled = PersistentMemory::getDebugEnabled();
        if (!_isConsoleEnabled) {
            _uid = SERIAL_COMMAND_USER_UID;
            _isLoggedIn = false;
            _isReadingPassword = false;
            _passwordPurpose = passwordPurposeNone;
            Serial.println(F("debug: disabled"));
            return 0;
        }
        Serial.println(F("debug: could not persist serial console state"));
        return 1;
    }

    _printCommandError("debug", "invalid argument");
    return 2;
}

/******************************************************************************/
/*!
  @brief    Starts hidden password input for the simulated root account.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleSu(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    (void) arguments;
    if (_uid == SERIAL_COMMAND_ROOT_UID) {
        return 0;
    }

    _isReadingPassword = true;
    _passwordPurpose = passwordPurposeRoot;
    Serial.print(F("Password: "));
    return 0;
}

/******************************************************************************/
/*!
  @brief    Reboots the device using the existing challenge behavior.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleReboot(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    (void) arguments;
    Serial.print(F("Restarting in "));
    for (uint8_t second = 3; second > 0; second--) {
        Serial.print(second);
        Serial.print(' ');
        delay(1000);
    }

    ESP.restart();
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints the existing web-user table.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleWebUsers(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    (void) arguments;
    if (_numberUsers == 0) {
        _printCommandError("webusers", "no valid users loaded from /etc/SmartLight/webusers.db");
        return 1;
    }

    Serial.println(F("|-USERNAME-------|-PASSWORD-------|-ROLE---|"));
    for (uint8_t i = 0; i < _numberUsers; i++) {
        uint8_t userIndex = i * USER_INFO_LENGTH;
        const char* role = "";
        const char* password = "";
        uint8_t permission = static_cast<uint8_t>(atoi(_users[userIndex + 2].c_str()));

        if (permission == PERMISSION_LEVEL_USER) {
            password = _users[userIndex + 1].c_str();
            role = "User";
        } else if (permission == PERMISSION_LEVEL_ADMIN) {
            password = "******";
            role = "Admin";
        }
        Serial.printf("| %-15s| %-15s| %-7s|\r\n", _users[userIndex].c_str(), password, role);
    }
    return 0;
}

/******************************************************************************/
/*!
  @brief    Gets or changes the existing persistent hostname.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleHostname(uint8_t argumentCount, char* arguments[]) {
    if (argumentCount == 0) {
        Serial.println(_hostname);
        return 0;
    }

    if (argumentCount == 2 && strcmp(arguments[0], "--set") == 0) {
        if (strlen(arguments[1]) >= MAX_HOSTNAME_LENGTH) {
            _printCommandError("hostname", "name too long");
            return 2;
        }
        PersistentMemory::writeHostname(arguments[1]);
        strncpy(_hostname, arguments[1], sizeof(_hostname) - 1);
        _hostname[sizeof(_hostname) - 1] = '\0';
        Serial.print(F("Hostname saved. Restarting"));
        ESP.restart();
        return 0;
    }

    if (argumentCount == 1 && strcmp(arguments[0], "--default") == 0) {
        PersistentMemory::writeHostname(DEFAULT_HOSTNAME);
        strncpy(_hostname, DEFAULT_HOSTNAME, sizeof(_hostname) - 1);
        _hostname[sizeof(_hostname) - 1] = '\0';
        return 0;
    }

    _printCommandError("hostname", "invalid argument");
    return 2;
}

/******************************************************************************/
/*!
  @brief    Prints the active account name.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleWhoAmI(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    (void) arguments;
    Serial.println(_getUsername());
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints the current working directory.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handlePwd(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    (void) arguments;
    Serial.println(_currentDirectory);
    return 0;
}

/******************************************************************************/
/*!
  @brief    Changes current directory after type and execute checks.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleCd(uint8_t argumentCount, char* arguments[]) {
    char resolvedPath[VIRTUAL_FILE_MAX_PATH_LENGTH];
    const char* requestedPath = argumentCount == 0 ? _getHomeDirectory() : arguments[0];

    if (strcmp(requestedPath, "-") == 0) {
        strncpy(resolvedPath, _previousDirectory, sizeof(resolvedPath));
        resolvedPath[sizeof(resolvedPath) - 1] = '\0';
    } else if (!_resolvePath(requestedPath, resolvedPath)) {
        _printCommandError("cd", "path too long");
        return 1;
    }

    const VirtualNode* node = _fileSystem.find(resolvedPath);
    if (node == NULL) {
        _printCommandError("cd", "No such file or directory");
        return 1;
    }

    if (node->type != virtualNodeDirectory) {
        _printCommandError("cd", "Not a directory");
        return 1;
    }

    if (!_fileSystem.canExecute(*node, _uid)) {
        _printCommandError("cd", "Permission denied");
        return 1;
    }

    strncpy(_previousDirectory, _currentDirectory, sizeof(_previousDirectory));
    _previousDirectory[sizeof(_previousDirectory) - 1] = '\0';
    strncpy(_currentDirectory, resolvedPath, sizeof(_currentDirectory));
    _currentDirectory[sizeof(_currentDirectory) - 1] = '\0';
    if (strcmp(requestedPath, "-") == 0) {
        Serial.println(_currentDirectory);
    }

    return 0;
}

/******************************************************************************/
/*!
  @brief    Lists one virtual directory with optional long formatting.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleLs(uint8_t argumentCount, char* arguments[]) {
    bool isLongFormat = false;
    const char* requestedPath = ".";
    char resolvedPath[VIRTUAL_FILE_MAX_PATH_LENGTH];

    if (argumentCount > 0 && strcmp(arguments[0], "-l") == 0) {
        isLongFormat = true;
        if (argumentCount == 2) {
            requestedPath = arguments[1];
        }
    } else if (argumentCount == 1) {
        requestedPath = arguments[0];
    } else if (argumentCount == 2) {
        _printCommandError("ls", "invalid argument");
        return 2;
    }

    if (!_resolvePath(requestedPath, resolvedPath)) {
        _printCommandError("ls", "path too long");
        return 1;
    }

    const VirtualNode* directory = _fileSystem.find(resolvedPath);
    if (directory == NULL) {
        if (_fileSystem.externalFileExists(resolvedPath)) {
            Serial.println(strrchr(resolvedPath, '/') + 1);
            return 0;
        }
        _printCommandError("ls", "No such file or directory");
        return 1;
    }

    if (directory->type == virtualNodeFile) {
        Serial.println(strrchr(directory->path, '/') + 1);
        return 0;
    }

    if (!_fileSystem.canRead(*directory, _uid)) {
        _printCommandError("ls", "Permission denied");
        return 1;
    }

    for (uint8_t i = 0; i < _fileSystem.getNodeCount(); i++) {
        const VirtualNode* node = _fileSystem.getNode(i);
        if (node == NULL || !_fileSystem.isDirectChild(resolvedPath, node->path)) {
            continue;
        }

        const char* name = strrchr(node->path, '/') + 1;
        if (isLongFormat) {
            char permissions[11];
            _fileSystem.formatPermissions(*node, permissions, sizeof(permissions));
            Serial.printf("%s %-5s %s\r\n", permissions, node->owner, name);
        } else {
            Serial.println(name);
        }
    }

    if (strcmp(resolvedPath, VIRTUAL_UPLOAD_DIRECTORY) == 0) {
        _fileSystem.printExternalDirectory(Serial, isLongFormat);
    }
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints readable virtual file content, including dynamic proc files.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleCat(uint8_t argumentCount, char* arguments[]) {
    char resolvedPath[VIRTUAL_FILE_MAX_PATH_LENGTH];
    (void) argumentCount;

    if (!_resolvePath(arguments[0], resolvedPath)) {
        _printCommandError("cat", "path too long");
        return 1;
    }

    const VirtualNode* node = _fileSystem.find(resolvedPath);
    if (node == NULL) {
        if (_fileSystem.printExternalFile(resolvedPath, Serial)) {
            return 0;
        }
        _printCommandError("cat", "No such file or directory");
        return 1;
    }

    if (node->type == virtualNodeDirectory) {
        _printCommandError("cat", "Is a directory");
        return 1;
    }

    if (!_fileSystem.canRead(*node, _uid)) {
        _printCommandError("cat", "Permission denied");
        return 1;
    }

    if (_fileSystem.contentEquals(*node, "$HOSTNAME")) {
        Serial.println(_hostname);
    } else if (_fileSystem.contentEquals(*node, "$UPTIME")) {
        uint32_t uptime = millis() / 1000;
        Serial.printf("%lu.00 %lu.00\r\n", static_cast<unsigned long>(uptime),
                      static_cast<unsigned long>(uptime));
    } else if (_fileSystem.contentEquals(*node, "$MEMINFO")) {
        Serial.printf("MemFree:        %lu kB\r\n",
                      static_cast<unsigned long>(ESP.getFreeHeap() / 1024));
    } else {
        _fileSystem.printContent(*node, Serial);
    }
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints the first lines of a readable virtual file.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleHead(uint8_t argumentCount, char* arguments[]) {
    return _printFileLines(argumentCount, arguments, false);
}

/******************************************************************************/
/*!
  @brief    Prints the last lines of a readable virtual file.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleTail(uint8_t argumentCount, char* arguments[]) {
    return _printFileLines(argumentCount, arguments, true);
}

/******************************************************************************/
/*!
  @brief    Prints all supplied arguments separated by one space.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleEcho(uint8_t argumentCount, char* arguments[]) {
    for (uint8_t i = 0; i < argumentCount; i++) {
        if (i > 0) {
            Serial.print(' ');
        }
        Serial.print(arguments[i]);
    }

    Serial.println();
    return 0;
}

/******************************************************************************/
/*!
  @brief    Clears an ANSI-compatible terminal and moves the cursor home.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleClear(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    (void) arguments;
    Serial.print(F("\033[2J\033[H"));
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints simulated kernel and platform information.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleUname(uint8_t argumentCount, char* arguments[]) {
    if (argumentCount == 0) {
        Serial.println(F("Linux"));
        return 0;
    }

    if (strcmp(arguments[0], "-a") != 0) {
        _printCommandError("uname", "invalid argument");
        return 2;
    }

    Serial.printf_P(PSTR("Linux %s 4.14.87 #1 PREEMPT Sun Oct 12 19:42:17 UTC 2025 "
                         "mips GNU/Linux\r\n"), _hostname);
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints elapsed device runtime.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleUptime(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    (void) arguments;
    _printUptime();
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints simulated memory totals and current free heap.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleFree(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    (void) arguments;
    _printFreeMemory();
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints current SPIFFS capacity and usage.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleDf(uint8_t argumentCount, char* arguments[]) {
    FSInfo fileSystemInfo;
    (void) argumentCount;
    (void) arguments;

    if (!SPIFFS.info(fileSystemInfo)) {
        _printCommandError("df", "cannot read filesystem information");
        return 1;
    }

    uint32_t availableBytes = fileSystemInfo.totalBytes > fileSystemInfo.usedBytes
                                  ? fileSystemInfo.totalBytes - fileSystemInfo.usedBytes
                                  : 0;
    uint8_t usedPercentage = fileSystemInfo.totalBytes == 0
                                 ? 0
                                 : static_cast<uint8_t>((fileSystemInfo.usedBytes * 100UL) /
                                                        fileSystemInfo.totalBytes);
    Serial.println(F("Filesystem       1K-blocks      Used Available Use% Mounted on"));
    Serial.printf_P(PSTR("/dev/mtdblock3 %12lu %9lu %9lu %3u%% /\r\n"),
                    static_cast<unsigned long>(fileSystemInfo.totalBytes / 1024UL),
                    static_cast<unsigned long>(fileSystemInfo.usedBytes / 1024UL),
                    static_cast<unsigned long>(availableBytes / 1024UL), usedPercentage);
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints the active uid and account identity.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleId(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    (void) arguments;
    Serial.printf("uid=%u(%s) gid=%u(%s) groups=%u(%s)\r\n", _uid, _getUsername(), _uid,
                  _getUsername(), _uid, _getUsername());
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints UTC time when the system clock has been synchronized.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleDate(uint8_t argumentCount, char* arguments[]) {
    char formattedTime[40];
    time_t currentTime = time(NULL);
    (void) argumentCount;
    (void) arguments;

    if (currentTime < 1609459200) {
        _printCommandError("date", "time is not synchronized");
        return 1;
    }

    struct tm* timeInformation = gmtime(&currentTime);
    if (timeInformation == NULL ||
        strftime(formattedTime, sizeof(formattedTime), "%a %b %d %H:%M:%S UTC %Y",
                 timeInformation) == 0) {
        _printCommandError("date", "cannot format system time");
        return 1;
    }
    Serial.println(formattedTime);
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints a small simulated process environment.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleEnv(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    (void) arguments;
    Serial.print(F("USER="));
    Serial.println(_getUsername());
    Serial.print(F("LOGNAME="));
    Serial.println(_getUsername());
    Serial.print(F("HOME="));
    Serial.println(_getHomeDirectory());
    Serial.print(F("HOSTNAME="));
    Serial.println(_hostname);
    Serial.print(F("PWD="));
    Serial.println(_currentDirectory);
    Serial.println(F("SHELL=/bin/sh"));
    Serial.println(F("PATH=/bin:/usr/bin:/sbin:/usr/sbin"));
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints a concise fictional kernel boot log.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleDmesg(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    (void) arguments;
    Serial.println(F("[    0.000000] Linux version 4.14.87 (builder@smartlight-ci)"));
    Serial.println(F("[    0.012000] CPU0 revision is: 0001964c (MIPS 24KEc)"));
    Serial.println(F("[    0.074000] 8250/16550 driver, 1 ports, IRQ sharing disabled"));
    Serial.println(F("[    0.291000] spi-nor spi0.0: 4096 KiB flash detected"));
    Serial.println(F("[    0.424000] VFS: Mounted root (squashfs filesystem) readonly"));
    Serial.print(F("[    0.647000] smartlight-init[418]: serial console "));
    Serial.println(_isConsoleEnabled ? F("enabled") : F("disabled"));
    Serial.println(F("[    0.913000] smartlightd[521]: HTTP service listening on port 80"));
    Serial.println(WiFi.status() == WL_CONNECTED
                       ? F("[    1.104000] wlan0: link becomes ready")
                       : F("[    1.104000] wlan0: link is not ready"));
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints real Wi-Fi addressing through simulated Linux ip syntax.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleIp(uint8_t argumentCount, char* arguments[]) {
    bool isStationConnected = WiFi.status() == WL_CONNECTED;
    IPAddress address = isStationConnected ? WiFi.localIP() : WiFi.softAPIP();
    IPAddress subnet = isStationConnected ? WiFi.subnetMask() : IPAddress(255, 255, 255, 0);
    IPAddress gateway = isStationConnected ? WiFi.gatewayIP() : address;
    String macAddress = isStationConnected ? WiFi.macAddress() : WiFi.softAPmacAddress();
    uint8_t prefixLength = 0;
    (void) argumentCount;

    for (uint8_t byteIndex = 0; byteIndex < 4; byteIndex++) {
        uint8_t maskByte = subnet[byteIndex];
        for (uint8_t bit = 0; bit < 8; bit++) {
            prefixLength += (maskByte & (0x80 >> bit)) != 0 ? 1 : 0;
        }
    }

    IPAddress network(address[0] & subnet[0], address[1] & subnet[1],
                      address[2] & subnet[2], address[3] & subnet[3]);
    IPAddress broadcast(address[0] | static_cast<uint8_t>(~subnet[0]),
                        address[1] | static_cast<uint8_t>(~subnet[1]),
                        address[2] | static_cast<uint8_t>(~subnet[2]),
                        address[3] | static_cast<uint8_t>(~subnet[3]));

    if (strcmp(arguments[0], "addr") == 0) {
        Serial.println(F("1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 state UNKNOWN"));
        Serial.println(F("    link/loopback 00:00:00:00:00:00"));
        Serial.println(F("    inet 127.0.0.1/8 scope host lo"));
        Serial.println(F("2: wlan0: <BROADCAST,MULTICAST,UP> mtu 1500 state UP"));
        Serial.print(F("    link/ether "));
        Serial.println(macAddress);
        Serial.printf_P(PSTR("    inet %s/%u brd %s scope global wlan0\r\n"),
                        address.toString().c_str(), prefixLength,
                        broadcast.toString().c_str());
        return 0;
    }

    if (strcmp(arguments[0], "route") == 0) {
        if (isStationConnected) {
            Serial.printf_P(PSTR("default via %s dev wlan0\r\n"),
                            gateway.toString().c_str());
        }
        Serial.printf_P(PSTR("%s/%u dev wlan0 scope link src %s\r\n"),
                        network.toString().c_str(), prefixLength,
                        address.toString().c_str());
        return 0;
    }

    _printCommandError("ip", "object not found");
    return 1;
}

/******************************************************************************/
/*!
  @brief    Leaves a root shell while retaining the serial user session.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleExit(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    (void) arguments;

    if (_uid == SERIAL_COMMAND_ROOT_UID) {
        _uid = SERIAL_COMMAND_USER_UID;
        _isLoggedIn = false;
        Serial.println(F("logout"));
    } else {
        Serial.println(F("logout: not a login shell"));
    }
    return 0;
}

/******************************************************************************/
/*!
  @brief    Runs the existing vulnerable program challenge.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleRun(uint8_t argumentCount, char* arguments[]) {
    char resolvedPath[VIRTUAL_FILE_MAX_PATH_LENGTH];

    if (_activeCommandName == NULL || !_resolvePath(_activeCommandName, resolvedPath)) {
        return 1;
    }

    if (strcmp(resolvedPath, "/home/user/testprogram.c") == 0) {
        _printCommandError(_activeCommandName, "Permission denied");
        return 126;
    }

    if (strcmp(resolvedPath, "/home/user/testprogram") != 0) {
        _printCommandError(_activeCommandName, "No such file or directory");
        return 127;
    }

    if (_bufferOverflow.runCProgram(argumentCount == 1 ? String(arguments[0]) : String(""))) {
        _uid = SERIAL_COMMAND_ROOT_UID;
        _isLoggedIn = true;
        Serial.println(F("You are now super user."));
    }
    return 0;
}

/******************************************************************************/
/*!
  @brief    Prints the existing disassembly challenge output.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleObjdump(uint8_t argumentCount, char* arguments[]) {
    char resolvedPath[VIRTUAL_FILE_MAX_PATH_LENGTH];
    (void) argumentCount;

    if (strcmp(arguments[0], "-d") != 0) {
        _printCommandError("objdump", "invalid argument");
        return 2;
    }

    if (!_resolvePath(arguments[1], resolvedPath) ||
        strcmp(resolvedPath, "/home/user/testprogram") != 0) {
        _printCommandError("objdump", "No such file");
        return 1;
    }
    _bufferOverflow.objectDump();
    return 0;
}

/******************************************************************************/
/*!
  @brief    Encrypts or decrypts one quoted text argument with the existing AES helper.
*/
/******************************************************************************/
uint8_t SerialCommandExecuter::_handleGpg(uint8_t argumentCount, char* arguments[]) {
    (void) argumentCount;
    if (!_cryptor.setKey(String(arguments[1]))) {
        _printCommandError("gpg", "key must be 16 bytes long");
        return 2;
    }

    if (strcmp(arguments[0], "--encrypt") == 0) {
        Serial.print(F("Encrypted output: "));
        Serial.println(_cryptor.encryptLine(String(arguments[2])));
        return 0;
    }

    if (strcmp(arguments[0], "--decrypt") == 0) {
        Serial.print(F("Decrypted output: "));
        Serial.println(_cryptor.decryptLine(String(arguments[2])));
        return 0;
    }
    
    _printCommandError("gpg", "invalid argument");
    return 2;
}
