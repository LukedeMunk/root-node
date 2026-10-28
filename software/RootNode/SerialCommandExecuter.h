/******************************************************************************/
/*
 * File:    SerialCommandExecuter.h
 * Authors: Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Provides a non-blocking, simulated embedded Linux serial shell.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#ifndef SERIAL_COMMAND_EXECUTER_H_
#define SERIAL_COMMAND_EXECUTER_H_

#include "Arduino.h"
#include "BufferOverflow.h"
#include "CbcEncryptor.h"
#include "Debugger.h"
#include "PersistentMemory.h"
#include "ShellCommandParser.h"
#include "VirtualFileSystem.h"
#include "configuration.h"

#define SERIAL_COMMAND_MAX_LINE_LENGTH  192
#define SERIAL_COMMAND_USER_UID         1000
#define SERIAL_COMMAND_ROOT_UID         0
#define SERIAL_SIMULATED_RAM_BYTES      81920UL

class SerialCommandExecuter {
    public:
        SerialCommandExecuter();
        void begin();
        void executeCommand();
        bool getFileListJson(String& output) const;
        void setUsers(String* users, uint8_t numUsers);

    private:
        typedef uint8_t (SerialCommandExecuter::*CommandHandler)(uint8_t argumentCount, char* arguments[]);

        struct CommandDefinition {
            const char* name;
            CommandHandler handler;
            uint8_t minimumArguments;
            uint8_t maximumArguments;
            bool isRootRequired;
            const char* usage;
        };

        enum PasswordPurpose {
            passwordPurposeNone,
            passwordPurposeRoot,
            passwordPurposeSerialConsole
        };

        static const CommandDefinition COMMAND_REGISTRY[];
        static const uint8_t COMMAND_COUNT;

        void _processInputCharacter(char character);
        void _processCompletedLine();
        void _processLockedCommand(const ShellCommand& command);
        void _processPassword();
        uint8_t _executeParsedCommand(ShellCommand& command);
        const CommandDefinition* _findCommand(const char* name) const;
        void _printPrompt();
        void _printActivePrompt();
        void _printLockedPrompt();
        void _printParseError(ShellParseResult result) const;
        void _printCommandError(const char* command, const char* message) const;
        void _printHelp(const char* command) const;
        const char* _getUsername() const;
        const char* _getHomeDirectory() const;
        bool _resolvePath(const char* path, char* resolvedPath) const;
        bool _parseLineArguments(uint8_t argumentCount, char* arguments[], const char*& filePath, uint16_t& lineCount) const;
        uint8_t _printFileLines(uint8_t argumentCount, char* arguments[], bool isTail);
        void _printUptime() const;
        void _printFreeMemory() const;

        uint8_t _handleHelp(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleDebug(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleSu(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleReboot(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleWebUsers(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleHostname(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleWhoAmI(uint8_t argumentCount, char* arguments[]);
        uint8_t _handlePwd(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleCd(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleLs(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleCat(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleHead(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleTail(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleEcho(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleClear(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleUname(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleUptime(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleFree(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleDf(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleId(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleDate(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleEnv(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleDmesg(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleIp(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleExit(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleRun(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleObjdump(uint8_t argumentCount, char* arguments[]);
        uint8_t _handleGpg(uint8_t argumentCount, char* arguments[]);

        bool _isLoggedIn;
        bool _isReadingPassword;
        bool _isConsoleEnabled;
        bool _isLineOverflow;
        bool _isPromptInitialized;
        bool _isPromptVisible;
        bool _lastCharacterWasCarriageReturn;
        uint16_t _uid;
        uint16_t _lineLength;
        uint8_t _numberUsers;
        PasswordPurpose _passwordPurpose;
        char _lineBuffer[SERIAL_COMMAND_MAX_LINE_LENGTH + 1];
        char _currentDirectory[VIRTUAL_FILE_MAX_PATH_LENGTH];
        char _previousDirectory[VIRTUAL_FILE_MAX_PATH_LENGTH];
        char _hostname[MAX_HOSTNAME_LENGTH + 1];
        const char* _activeCommandName;
        String _users[MAX_NUMBER_USERS * USER_INFO_LENGTH];
        ShellCommandParser _parser;
        VirtualFileSystem _fileSystem;
        BufferOverflow _bufferOverflow;
        CbcEncryptor _cryptor;
};

#endif  // SERIAL_COMMAND_EXECUTER_H_
