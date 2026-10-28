/******************************************************************************/
/*
 * File:    VirtualFileSystem.h
 * Author:  Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Provides a bounded, JSON-defined virtual Linux filesystem.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#ifndef VIRTUAL_FILESYSTEM_H_
#define VIRTUAL_FILESYSTEM_H_

#include "Arduino.h"

#define VIRTUAL_FILE_MAX_NODES           24
#define VIRTUAL_FILE_MAX_PATH_LENGTH     64
#define VIRTUAL_FILE_MAX_OWNER_LENGTH    8

enum VirtualNodeType {
    virtualNodeFile,
    virtualNodeDirectory
};

struct VirtualNode {
    char path[VIRTUAL_FILE_MAX_PATH_LENGTH];
    char owner[VIRTUAL_FILE_MAX_OWNER_LENGTH];
    uint16_t contentPosition;
    uint16_t contentLength;
    uint16_t mode;
    VirtualNodeType type;
};

class VirtualFileSystem {
    public:
        VirtualFileSystem();
        bool initialize();
        bool normalizePath(const char* currentDirectory, const char* requestedPath, char* normalizedPath, size_t normalizedLength) const;
        const VirtualNode* find(const char* path) const;
        const VirtualNode* getNode(uint8_t index) const;
        uint8_t getNodeCount() const;
        bool isDirectChild(const char* parentPath, const char* childPath) const;
        bool canRead(const VirtualNode& node, uint16_t uid) const;
        bool canExecute(const VirtualNode& node, uint16_t uid) const;
        bool contentEquals(const VirtualNode& node, const char* content) const;
        bool getFileListJson(String& output) const;
        bool externalFileExists(const char* virtualPath) const;
        bool printExternalFile(const char* virtualPath, Print& output) const;
        bool printExternalFileLines(const char* virtualPath, Print& output, uint16_t lineCount, bool isTail) const;
        void printExternalDirectory(Print& output, bool isLongFormat) const;
        void printContent(const VirtualNode& node, Print& output) const;
        void printContentLines(const VirtualNode& node, Print& output, uint16_t lineCount, bool isTail) const;
        void formatPermissions(const VirtualNode& node, char* output, size_t outputLength) const;

    private:
        bool _loadNextNode(uint16_t& jsonPosition, VirtualNode& node) const;
        bool _readJsonString(uint16_t& jsonPosition, const char* key, char* output, size_t outputLength) const;
        bool _readJsonStringReference(uint16_t& jsonPosition, const char* key, uint16_t& valuePosition, uint16_t& valueLength) const;
        bool _readJsonNumber(uint16_t& jsonPosition, const char* key, uint16_t& output) const;
        bool _readDecodedContentCharacter(const VirtualNode& node, uint16_t& position, char& character) const;
        uint16_t _countContentLines(const VirtualNode& node) const;
        void _printJsonContentLines(const VirtualNode& node, Print& output, uint16_t lineCount, bool isTail) const;
        uint16_t _getDecodedContentLength(const VirtualNode& node) const;
        char _readJsonCharacter(uint16_t position) const;

        VirtualNode _nodes[VIRTUAL_FILE_MAX_NODES];
        uint8_t _nodeCount;
        bool _isInitialized;
};

#endif  // VIRTUAL_FILESYSTEM_H_
