/******************************************************************************/
/*
 * File:    VirtualFileSystem.cpp
 * Author:  Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Provides a bounded, JSON-defined virtual Linux filesystem.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#include "VirtualFileSystem.h"

#include "ExternalFileRegistry.h"
#include "configuration.h"

#include "FS.h"

#include <pgmspace.h>
#include <string.h>

static const char VIRTUAL_FILE_SYSTEM_JSON[] PROGMEM = R"json(
[
 {"path":"/","type":"directory","owner":"root","mode":493,"content":""},
 {"path":"/bin","type":"directory","owner":"root","mode":493,"content":""},
 {"path":"/etc","type":"directory","owner":"root","mode":493,"content":""},
 {"path":"/etc/SmartLight","type":"directory","owner":"root","mode":493,"content":""},
 {"path":"/home","type":"directory","owner":"root","mode":493,"content":""},
 {"path":"/home/user","type":"directory","owner":"user","mode":493,"content":""},
 {"path":"/home/user/uploads","type":"directory","owner":"user","mode":493,"content":""},
 {"path":"/root","type":"directory","owner":"root","mode":448,"content":""},
 {"path":"/proc","type":"directory","owner":"root","mode":365,"content":""},
 {"path":"/tmp","type":"directory","owner":"root","mode":511,"content":""},
 {"path":"/var","type":"directory","owner":"root","mode":493,"content":""},
 {"path":"/var/log","type":"directory","owner":"root","mode":493,"content":""},
 {"path":"/etc/hostname","type":"file","owner":"root","mode":420,"content":"$HOSTNAME"},
 {"path":"/etc/passwd","type":"file","owner":"root","mode":420,"content":"root:x:0:0:root:/root:/bin/sh\nuser:x:1000:1000:Linux User:/home/user:/bin/sh"},
 {"path":"/etc/shadow","type":"file","owner":"root","mode":384,"content":"root:$1$iot$2zKqV4iP0x:19000:0:99999:7:::\nuser:*:19000:0:99999:7:::"},
 {"path":"/etc/SmartLight/webusers.db","type":"file","owner":"root","mode":614, "content":""},
 {"path":"/etc/SmartLight/aes.key","type":"file","owner":"root","mode":614,"content":"ThisIsNotTheKey!"},
 {"path":"/home/user/notes.txt","type":"file","owner":"user","mode":420,"content":""},
 {"path":"/home/user/testprogram.c","type":"file","owner":"root","mode":292, "content":""},
 {"path":"/home/user/testprogram","type":"file","owner":"root","mode":365,"content":"ELF32 ARM executable"},
 {"path":"/proc/uptime","type":"file","owner":"root","mode":292,"content":"$UPTIME"},
 {"path":"/proc/meminfo","type":"file","owner":"root","mode":292,"content":"$MEMINFO"},
 {"path":"/var/log/smartlight.log","type":"file","owner":"root","mode":416,"content":"smartlightd: service started\nhttpd: listening on port 80"}
]
)json";

/******************************************************************************/
/*!
  @brief    Creates an empty filesystem. Call initialize after Arduino startup.
*/
/******************************************************************************/
VirtualFileSystem::VirtualFileSystem() {
    _nodeCount = 0;
    _isInitialized = false;
}

/******************************************************************************/
/*!
  @brief    Loads the bounded virtual tree from its flash-resident JSON source.
  @returns  bool                True when every node was loaded
*/
/******************************************************************************/
bool VirtualFileSystem::initialize() {
    uint16_t jsonPosition = 0;

    if (_isInitialized) {
        return true;
    }

    _nodeCount = 0;
    while (_nodeCount < VIRTUAL_FILE_MAX_NODES &&
           _loadNextNode(jsonPosition, _nodes[_nodeCount])) {
        _nodeCount++;
    }

    while (_readJsonCharacter(jsonPosition) == ' ' ||
           _readJsonCharacter(jsonPosition) == '\r' ||
           _readJsonCharacter(jsonPosition) == '\n' ||
           _readJsonCharacter(jsonPosition) == '\t' ||
           _readJsonCharacter(jsonPosition) == ',') {
        jsonPosition++;
    }
    
    _isInitialized = _nodeCount > 0 && _readJsonCharacter(jsonPosition) == ']';
    return _isInitialized;
}

/******************************************************************************/
/*!
  @brief    Normalizes an absolute or current-directory-relative path.
  @param    currentDirectory    Absolute current directory
  @param    requestedPath       Path supplied by the user
  @param    normalizedPath      Receives the canonical absolute path
  @param    normalizedLength    Size of normalizedPath
  @returns  bool                False when the path exceeds the fixed buffer
*/
/******************************************************************************/
bool VirtualFileSystem::normalizePath(const char* currentDirectory, const char* requestedPath,
                                      char* normalizedPath, size_t normalizedLength) const {
    char combinedPath[VIRTUAL_FILE_MAX_PATH_LENGTH];
    size_t combinedLength = 0;
    size_t readPosition = 0;
    size_t writePosition = 1;

    if (normalizedLength < 2 || requestedPath == NULL || requestedPath[0] == '\0') {
        return false;
    }

    combinedPath[0] = '\0';
    if (requestedPath[0] == '/') {
        if (strlen(requestedPath) >= sizeof(combinedPath)) {
            return false;
        }
        strncpy(combinedPath, requestedPath, sizeof(combinedPath) - 1);
    } else {
        combinedLength = strlen(currentDirectory);
        if (combinedLength + strlen(requestedPath) + 2 > sizeof(combinedPath)) {
            return false;
        }
        strncpy(combinedPath, currentDirectory, sizeof(combinedPath) - 1);
        combinedPath[sizeof(combinedPath) - 1] = '\0';
        if (combinedLength > 1) {
            combinedPath[combinedLength++] = '/';
            combinedPath[combinedLength] = '\0';
        }
        strncat(combinedPath, requestedPath, sizeof(combinedPath) - combinedLength - 1);
    }

    normalizedPath[0] = '/';
    normalizedPath[1] = '\0';

    while (combinedPath[readPosition] != '\0') {
        char segment[VIRTUAL_FILE_MAX_PATH_LENGTH];
        size_t segmentLength = 0;

        while (combinedPath[readPosition] == '/') {
            readPosition++;
        }
        while (combinedPath[readPosition] != '\0' && combinedPath[readPosition] != '/') {
            if (segmentLength + 1 >= sizeof(segment)) {
                return false;
            }
            segment[segmentLength++] = combinedPath[readPosition++];
        }
        segment[segmentLength] = '\0';

        if (segmentLength == 0 || strcmp(segment, ".") == 0) {
            continue;
        }
        if (strcmp(segment, "..") == 0) {
            if (writePosition > 1) {
                writePosition--;
                while (writePosition > 1 && normalizedPath[writePosition - 1] != '/') {
                    writePosition--;
                }
                if (writePosition > 1) {
                    writePosition--;
                }
                normalizedPath[writePosition] = '\0';
                if (writePosition == 0) {
                    writePosition = 1;
                    normalizedPath[0] = '/';
                    normalizedPath[1] = '\0';
                }
            }
            continue;
        }

        if (writePosition > 1) {
            if (writePosition + 1 >= normalizedLength) {
                return false;
            }
            normalizedPath[writePosition++] = '/';
        }
        if (writePosition + segmentLength >= normalizedLength) {
            return false;
        }
        memcpy(normalizedPath + writePosition, segment, segmentLength);
        writePosition += segmentLength;
        normalizedPath[writePosition] = '\0';
    }

    return true;
}

/******************************************************************************/
/*!
  @brief    Finds a node by canonical path.
  @param    path                Canonical absolute path
  @returns  VirtualNode        Node pointer, or NULL when not found
*/
/******************************************************************************/
const VirtualNode* VirtualFileSystem::find(const char* path) const {
    for (uint8_t i = 0; i < _nodeCount; i++) {
        if (strcmp(_nodes[i].path, path) == 0) {
            return &_nodes[i];
        }
    }
    return NULL;
}

/******************************************************************************/
/*!
  @brief    Gets a node by its bounded array index.
  @param    index               Node index
  @returns  VirtualNode        Node pointer, or NULL when out of bounds
*/
/******************************************************************************/
const VirtualNode* VirtualFileSystem::getNode(uint8_t index) const {
    if (index >= _nodeCount) {
        return NULL;
    }
    return &_nodes[index];
}

/******************************************************************************/
/*!
  @brief    Gets the number of loaded nodes.
  @returns  uint8_t             Number of nodes
*/
/******************************************************************************/
uint8_t VirtualFileSystem::getNodeCount() const {
    return _nodeCount;
}

/******************************************************************************/
/*!
  @brief    Checks whether childPath is immediately below parentPath.
  @param    parentPath          Canonical parent path
  @param    childPath           Canonical possible child path
  @returns  bool                True for a direct child
*/
/******************************************************************************/
bool VirtualFileSystem::isDirectChild(const char* parentPath, const char* childPath) const {
    size_t parentLength = strlen(parentPath);
    const char* relativePath;

    if (strcmp(parentPath, childPath) == 0) {
        return false;
    }
    if (strcmp(parentPath, "/") == 0) {
        relativePath = childPath + 1;
    } else {
        if (strncmp(parentPath, childPath, parentLength) != 0 ||
            childPath[parentLength] != '/') {
            return false;
        }
        relativePath = childPath + parentLength + 1;
    }

    return relativePath[0] != '\0' && strchr(relativePath, '/') == NULL;
}

/******************************************************************************/
/*!
  @brief    Checks simulated read permission for a uid.
*/
/******************************************************************************/
bool VirtualFileSystem::canRead(const VirtualNode& node, uint16_t uid) const {
    if (uid == 0) {
        return true;
    }
    if (strcmp(node.owner, "user") == 0) {
        return (node.mode & 0400) != 0;
    }
    return (node.mode & 0004) != 0;
}

/******************************************************************************/
/*!
  @brief    Checks simulated execute permission for a uid.
*/
/******************************************************************************/
bool VirtualFileSystem::canExecute(const VirtualNode& node, uint16_t uid) const {
    if (uid == 0) {
        return true;
    }
    if (strcmp(node.owner, "user") == 0) {
        return (node.mode & 0100) != 0;
    }
    return (node.mode & 0001) != 0;
}

/******************************************************************************/
/*!
  @brief    Compares flash-resident content with a short unescaped marker.
*/
/******************************************************************************/
bool VirtualFileSystem::contentEquals(const VirtualNode& node, const char* content) const {
    size_t contentLength = strlen(content);

    if (contentLength != node.contentLength) {
        return false;
    }
    for (uint16_t i = 0; i < node.contentLength; i++) {
        if (_readJsonCharacter(node.contentPosition + i) != content[i]) {
            return false;
        }
    }
    return true;
}

/******************************************************************************/
/*!
  @brief    Builds the web API list from all file nodes in the virtual tree.
  @param    output              Receives path, size and download availability
  @returns  bool                False when the tree or output allocation failed
*/
/******************************************************************************/
bool VirtualFileSystem::getFileListJson(String& output) const {
    bool isFirstItem = true;

    if (!_isInitialized) {
        return false;
    }

    output = "";
    if (!output.reserve(1024)) {
        return false;
    }

    output += '[';
    for (uint8_t i = 0; i < _nodeCount; i++) {
        char storagePath[EXTERNAL_FILE_MAX_STORAGE_PATH_LENGTH];
        const ExternalFileMapping* mapping;
        uint32_t fileSize;
        bool isDownloadable = false;

        fileSize = _nodes[i].type == virtualNodeFile
                       ? _getDecodedContentLength(_nodes[i])
                       : 0;
        if (_nodes[i].type == virtualNodeFile) {
            mapping = ExternalFileRegistry::findByVirtualPath(_nodes[i].path);
            if (mapping != NULL &&
                ExternalFileRegistry::getStoragePath(*mapping, storagePath,
                                                     sizeof(storagePath))) {
                File externalFile = SPIFFS.open(storagePath, "r");
                if (externalFile) {
                    fileSize = externalFile.size();
                    isDownloadable = true;
                    externalFile.close();
                }
            }
        }
        if (!isFirstItem) {
            output += ',';
        }
        output += F("{\"path\":\"");
        output += _nodes[i].path;
        output += F("\",\"type\":\"");
        output += _nodes[i].type == virtualNodeDirectory ? F("directory") : F("file");
        output += F("\",\"size\":");
        output += fileSize;
        output += F(",\"isDownloadable\":");
        output += isDownloadable ? F("true") : F("false");
        output += '}';
        isFirstItem = false;
    }

    Dir directory = SPIFFS.openDir(EXTERNAL_FILE_FOLDER);
    while (directory.next()) {
        char virtualPath[VIRTUAL_FILE_MAX_PATH_LENGTH];
        String currentStoragePath = directory.fileName();
        const char* storagePath = currentStoragePath.c_str();
        size_t folderLength = strlen(EXTERNAL_FILE_FOLDER);
        if (strncmp(storagePath, EXTERNAL_FILE_FOLDER, folderLength) != 0 ||
            storagePath[folderLength] != '/' ||
            strchr(storagePath + folderLength + 1, '/') != NULL) {
            continue;
        }
        const char* fileName = strrchr(storagePath, '/');
        fileName = fileName == NULL ? storagePath : fileName + 1;
        if (!ExternalFileRegistry::getVirtualPathForExternalFile(
                fileName, virtualPath, sizeof(virtualPath)) ||
            ExternalFileRegistry::findByExternalFileName(fileName) != NULL) {
            continue;
        }
        File externalFile = directory.openFile("r");
        bool isDownloadable = static_cast<bool>(externalFile);
        uint32_t fileSize = externalFile ? externalFile.size() : 0;
        if (externalFile) {
            externalFile.close();
        }
        if (!isFirstItem) {
            output += ',';
        }
        output += F("{\"path\":\"");
        output += virtualPath;
        output += F("\",\"type\":\"file\",\"size\":");
        output += fileSize;
        output += F(",\"isDownloadable\":");
        output += isDownloadable ? F("true") : F("false");
        output += '}';
        isFirstItem = false;
    }
    output += ']';
    return true;
}

/******************************************************************************/
/*!
  @brief    Checks whether one safe virtual external file currently exists.
*/
/******************************************************************************/
bool VirtualFileSystem::externalFileExists(const char* virtualPath) const {
    char storagePath[EXTERNAL_FILE_MAX_STORAGE_PATH_LENGTH];
    char fileName[EXTERNAL_FILE_MAX_NAME_LENGTH + 1];

    return ExternalFileRegistry::resolveVirtualPath(
               virtualPath, storagePath, sizeof(storagePath), fileName,
               sizeof(fileName)) && SPIFFS.exists(storagePath);
}

/******************************************************************************/
/*!
  @brief    Prints safe, non-reserved external files in the upload directory.
*/
/******************************************************************************/
void VirtualFileSystem::printExternalDirectory(Print& output,
                                               bool isLongFormat) const {
    Dir directory = SPIFFS.openDir(EXTERNAL_FILE_FOLDER);

    while (directory.next()) {
        String currentStoragePath = directory.fileName();
        const char* storagePath = currentStoragePath.c_str();
        size_t folderLength = strlen(EXTERNAL_FILE_FOLDER);
        if (strncmp(storagePath, EXTERNAL_FILE_FOLDER, folderLength) != 0 ||
            storagePath[folderLength] != '/' ||
            strchr(storagePath + folderLength + 1, '/') != NULL) {
            continue;
        }
        const char* fileName = strrchr(storagePath, '/');
        fileName = fileName == NULL ? storagePath : fileName + 1;
        if (!ExternalFileRegistry::isSafeFileName(fileName) ||
            ExternalFileRegistry::findByExternalFileName(fileName) != NULL) {
            continue;
        }
        if (isLongFormat) {
            output.printf("-rw-r--r-- %-5s %s\r\n", "user", fileName);
        } else {
            output.println(fileName);
        }
    }
}

/******************************************************************************/
/*!
  @brief    Streams one externally backed virtual file with terminal newlines.
*/
/******************************************************************************/
bool VirtualFileSystem::printExternalFile(const char* virtualPath,
                                          Print& output) const {
    char storagePath[EXTERNAL_FILE_MAX_STORAGE_PATH_LENGTH];
    char fileName[EXTERNAL_FILE_MAX_NAME_LENGTH + 1];
    bool lastCharacterWasCarriageReturn = false;
    bool endedWithNewline = false;
    bool wroteContent = false;

    if (!ExternalFileRegistry::resolveVirtualPath(
            virtualPath, storagePath, sizeof(storagePath), fileName,
            sizeof(fileName))) {
        return false;
    }
    File externalFile = SPIFFS.open(storagePath, "r");
    if (!externalFile) {
        return false;
    }
    while (externalFile.available()) {
        char character = static_cast<char>(externalFile.read());
        wroteContent = true;
        if (character == '\n' && lastCharacterWasCarriageReturn) {
            lastCharacterWasCarriageReturn = false;
            endedWithNewline = true;
            continue;
        }
        if (character == '\r' || character == '\n') {
            output.print(F("\r\n"));
            lastCharacterWasCarriageReturn = character == '\r';
            endedWithNewline = true;
            continue;
        }
        output.write(character);
        lastCharacterWasCarriageReturn = false;
        endedWithNewline = false;
    }
    externalFile.close();
    if (wroteContent && !endedWithNewline) {
        output.print(F("\r\n"));
    }
    return true;
}

/******************************************************************************/
/*!
  @brief    Streams JSON content from flash with terminal-safe CRLF newlines.
*/
/******************************************************************************/
void VirtualFileSystem::printContent(const VirtualNode& node, Print& output) const {
    bool lastCharacterWasCarriageReturn = false;
    bool endedWithNewline = false;
    uint16_t contentEnd = node.contentPosition + node.contentLength;

    if (printExternalFile(node.path, output)) {
        return;
    }

    for (uint16_t position = node.contentPosition; position < contentEnd; position++) {
        char character = _readJsonCharacter(position);
        if (character == '\\' && position + 1 < contentEnd) {
            character = _readJsonCharacter(++position);
            if (character == 'n') {
                character = '\n';
            } else if (character == 'r') {
                character = '\r';
            } else if (character == 't') {
                character = '\t';
            }
        }

        if (character == '\n' && lastCharacterWasCarriageReturn) {
            lastCharacterWasCarriageReturn = false;
            endedWithNewline = true;
            continue;
        }
        if (character == '\r' || character == '\n') {
            output.print(F("\r\n"));
            lastCharacterWasCarriageReturn = character == '\r';
            endedWithNewline = true;
            continue;
        }

        output.write(character);
        lastCharacterWasCarriageReturn = false;
        endedWithNewline = false;
    }

    if (!endedWithNewline) {
        output.print(F("\r\n"));
    }
}

/******************************************************************************/
/*!
  @brief    Streams the first or last lines of virtual file content.
  @param    node                Virtual file to read
  @param    output              Destination stream
  @param    lineCount           Maximum number of lines to print
  @param    isTail              True to select lines from the end
*/
/******************************************************************************/
void VirtualFileSystem::printContentLines(const VirtualNode& node, Print& output,
                                          uint16_t lineCount, bool isTail) const {
    if (lineCount == 0 ||
        printExternalFileLines(node.path, output, lineCount, isTail)) {
        return;
    }
    _printJsonContentLines(node, output, lineCount, isTail);
}

/******************************************************************************/
/*!
  @brief    Reads one decoded byte from a flash-resident JSON content string.
  @param    node                Virtual file being read
  @param    position            Current source position, advanced on success
  @param    character           Receives the decoded byte
  @returns  bool                False at the end of the content
*/
/******************************************************************************/
bool VirtualFileSystem::_readDecodedContentCharacter(const VirtualNode& node,
                                                     uint16_t& position,
                                                     char& character) const {
    uint16_t contentEnd = node.contentPosition + node.contentLength;

    if (position >= contentEnd) {
        return false;
    }
    character = _readJsonCharacter(position++);
    if (character == '\\' && position < contentEnd) {
        character = _readJsonCharacter(position++);
        if (character == 'n') {
            character = '\n';
        } else if (character == 'r') {
            character = '\r';
        } else if (character == 't') {
            character = '\t';
        }
    }
    return true;
}

/******************************************************************************/
/*!
  @brief    Counts logical lines in flash-resident virtual file content.
*/
/******************************************************************************/
uint16_t VirtualFileSystem::_countContentLines(const VirtualNode& node) const {
    uint16_t position = node.contentPosition;
    uint16_t lineCount = 0;
    bool hasContent = false;
    bool endedWithNewline = false;
    bool lastCharacterWasCarriageReturn = false;
    char character;

    while (_readDecodedContentCharacter(node, position, character)) {
        hasContent = true;
        if (character == '\n' && lastCharacterWasCarriageReturn) {
            lastCharacterWasCarriageReturn = false;
            endedWithNewline = true;
            continue;
        }
        if (character == '\r' || character == '\n') {
            if (lineCount < UINT16_MAX) {
                lineCount++;
            }
            lastCharacterWasCarriageReturn = character == '\r';
            endedWithNewline = true;
        } else {
            lastCharacterWasCarriageReturn = false;
            endedWithNewline = false;
        }
    }
    if (hasContent && !endedWithNewline && lineCount < UINT16_MAX) {
        lineCount++;
    }
    return lineCount;
}

/******************************************************************************/
/*!
  @brief    Prints a selected range of flash-resident content lines.
*/
/******************************************************************************/
void VirtualFileSystem::_printJsonContentLines(const VirtualNode& node, Print& output,
                                               uint16_t lineCount, bool isTail) const {
    uint16_t totalLines = isTail ? _countContentLines(node) : lineCount;
    uint16_t startLine = isTail && totalLines > lineCount ? totalLines - lineCount : 0;
    uint32_t endLine = static_cast<uint32_t>(startLine) + lineCount;
    uint16_t position = node.contentPosition;
    uint32_t currentLine = 0;
    bool lastCharacterWasCarriageReturn = false;
    bool wroteContent = false;
    bool endedWithNewline = true;
    char character;

    while (currentLine < endLine &&
           _readDecodedContentCharacter(node, position, character)) {
        if (character == '\n' && lastCharacterWasCarriageReturn) {
            lastCharacterWasCarriageReturn = false;
            continue;
        }
        if (character == '\r' || character == '\n') {
            if (currentLine >= startLine) {
                output.print(F("\r\n"));
                wroteContent = true;
                endedWithNewline = true;
            }
            currentLine++;
            lastCharacterWasCarriageReturn = character == '\r';
            continue;
        }
        if (currentLine >= startLine) {
            output.write(character);
            wroteContent = true;
            endedWithNewline = false;
        }
        lastCharacterWasCarriageReturn = false;
    }
    if (wroteContent && !endedWithNewline) {
        output.print(F("\r\n"));
    }
}

/******************************************************************************/
/*!
  @brief    Formats a Linux-style type and permission string.
*/
/******************************************************************************/
void VirtualFileSystem::formatPermissions(const VirtualNode& node, char* output,
                                          size_t outputLength) const {
    static const uint16_t masks[9] = {0400, 0200, 0100, 0040, 0020, 0010, 0004, 0002, 0001};
    static const char permissions[3] = {'r', 'w', 'x'};

    if (outputLength < 11) {
        if (outputLength > 0) {
            output[0] = '\0';
        }
        return;
    }

    output[0] = node.type == virtualNodeDirectory ? 'd' : '-';
    for (uint8_t i = 0; i < 9; i++) {
        output[i + 1] = (node.mode & masks[i]) != 0 ? permissions[i % 3] : '-';
    }
    output[10] = '\0';
}

/******************************************************************************/
/*!
  @brief    Loads the next object from the trusted JSON definition.
*/
/******************************************************************************/
bool VirtualFileSystem::_loadNextNode(uint16_t& jsonPosition, VirtualNode& node) const {
    char type[12];

    while (_readJsonCharacter(jsonPosition) != '\0' &&
           _readJsonCharacter(jsonPosition) != '{' &&
           _readJsonCharacter(jsonPosition) != ']') {
        jsonPosition++;
    }
    if (_readJsonCharacter(jsonPosition) != '{') {
        return false;
    }

    memset(&node, 0, sizeof(node));
    if (!_readJsonString(jsonPosition, "path", node.path, sizeof(node.path)) ||
        !_readJsonString(jsonPosition, "type", type, sizeof(type)) ||
        !_readJsonString(jsonPosition, "owner", node.owner, sizeof(node.owner)) ||
        !_readJsonNumber(jsonPosition, "mode", node.mode) ||
        !_readJsonStringReference(jsonPosition, "content", node.contentPosition,
                                  node.contentLength)) {
        return false;
    }
    node.type = strcmp(type, "directory") == 0 ? virtualNodeDirectory : virtualNodeFile;

    while (_readJsonCharacter(jsonPosition) != '\0' &&
           _readJsonCharacter(jsonPosition) != '}') {
        jsonPosition++;
    }
    if (_readJsonCharacter(jsonPosition) == '}') {
        jsonPosition++;
    }
    return true;
}

/******************************************************************************/
/*!
  @brief    Records a flash string's bounds without copying its content to RAM.
*/
/******************************************************************************/
bool VirtualFileSystem::_readJsonStringReference(uint16_t& jsonPosition, const char* key,
                                                 uint16_t& valuePosition,
                                                 uint16_t& valueLength) const {
    size_t keyPosition = 0;

    while (_readJsonCharacter(jsonPosition) != '\0') {
        if (_readJsonCharacter(jsonPosition) == '"') {
            uint16_t candidatePosition = jsonPosition + 1;
            keyPosition = 0;
            while (key[keyPosition] != '\0' &&
                   _readJsonCharacter(candidatePosition) == key[keyPosition]) {
                candidatePosition++;
                keyPosition++;
            }
            if (key[keyPosition] == '\0' && _readJsonCharacter(candidatePosition) == '"') {
                jsonPosition = candidatePosition + 1;
                break;
            }
        }
        jsonPosition++;
    }

    while (_readJsonCharacter(jsonPosition) != '\0' &&
           _readJsonCharacter(jsonPosition) != ':') {
        jsonPosition++;
    }
    if (_readJsonCharacter(jsonPosition++) != ':') {
        return false;
    }
    while (_readJsonCharacter(jsonPosition) == ' ') {
        jsonPosition++;
    }
    if (_readJsonCharacter(jsonPosition++) != '"') {
        return false;
    }

    valuePosition = jsonPosition;
    while (_readJsonCharacter(jsonPosition) != '\0') {
        char character = _readJsonCharacter(jsonPosition++);
        if (character == '\\') {
            if (_readJsonCharacter(jsonPosition) == '\0') {
                return false;
            }
            jsonPosition++;
        } else if (character == '"') {
            valueLength = jsonPosition - valuePosition - 1;
            return true;
        }
    }
    return false;
}

/******************************************************************************/
/*!
  @brief    Reads a string value for the next expected key.
*/
/******************************************************************************/
bool VirtualFileSystem::_readJsonString(uint16_t& jsonPosition, const char* key,
                                        char* output, size_t outputLength) const {
    size_t keyPosition = 0;
    size_t outputPosition = 0;

    while (_readJsonCharacter(jsonPosition) != '\0') {
        if (_readJsonCharacter(jsonPosition) == '"') {
            uint16_t candidatePosition = jsonPosition + 1;
            keyPosition = 0;
            while (key[keyPosition] != '\0' &&
                   _readJsonCharacter(candidatePosition) == key[keyPosition]) {
                candidatePosition++;
                keyPosition++;
            }
            if (key[keyPosition] == '\0' && _readJsonCharacter(candidatePosition) == '"') {
                jsonPosition = candidatePosition + 1;
                break;
            }
        }
        jsonPosition++;
    }

    while (_readJsonCharacter(jsonPosition) != '\0' &&
           _readJsonCharacter(jsonPosition) != ':') {
        jsonPosition++;
    }
    if (_readJsonCharacter(jsonPosition++) != ':') {
        return false;
    }
    while (_readJsonCharacter(jsonPosition) == ' ') {
        jsonPosition++;
    }
    if (_readJsonCharacter(jsonPosition++) != '"') {
        return false;
    }

    while (_readJsonCharacter(jsonPosition) != '\0') {
        char character = _readJsonCharacter(jsonPosition++);
        if (character == '"') {
            output[outputPosition] = '\0';
            return true;
        }
        if (character == '\\') {
            character = _readJsonCharacter(jsonPosition++);
            if (character == 'n') {
                character = '\n';
            } else if (character == 't') {
                character = '\t';
            }
        }
        if (outputPosition + 1 >= outputLength) {
            return false;
        }
        output[outputPosition++] = character;
    }
    return false;
}

/******************************************************************************/
/*!
  @brief    Reads an unsigned numeric value for the next expected key.
*/
/******************************************************************************/
bool VirtualFileSystem::_readJsonNumber(uint16_t& jsonPosition, const char* key,
                                        uint16_t& output) const {
    char number[7];
    size_t keyPosition = 0;
    size_t numberPosition = 0;

    while (_readJsonCharacter(jsonPosition) != '\0') {
        if (_readJsonCharacter(jsonPosition) == '"') {
            uint16_t candidatePosition = jsonPosition + 1;
            keyPosition = 0;
            while (key[keyPosition] != '\0' &&
                   _readJsonCharacter(candidatePosition) == key[keyPosition]) {
                candidatePosition++;
                keyPosition++;
            }
            if (key[keyPosition] == '\0' && _readJsonCharacter(candidatePosition) == '"') {
                jsonPosition = candidatePosition + 1;
                break;
            }
        }
        jsonPosition++;
    }

    while (_readJsonCharacter(jsonPosition) != '\0' &&
           _readJsonCharacter(jsonPosition) != ':') {
        jsonPosition++;
    }
    if (_readJsonCharacter(jsonPosition++) != ':') {
        return false;
    }
    while (_readJsonCharacter(jsonPosition) == ' ') {
        jsonPosition++;
    }
    while (_readJsonCharacter(jsonPosition) >= '0' &&
           _readJsonCharacter(jsonPosition) <= '9') {
        if (numberPosition + 1 >= sizeof(number)) {
            return false;
        }
        number[numberPosition++] = _readJsonCharacter(jsonPosition++);
    }
    if (numberPosition == 0) {
        return false;
    }
    number[numberPosition] = '\0';
    output = static_cast<uint16_t>(atoi(number));
    return true;
}

/******************************************************************************/
/*!
  @brief    Counts decoded bytes in one flash-resident JSON content string.
*/
/******************************************************************************/
uint16_t VirtualFileSystem::_getDecodedContentLength(const VirtualNode& node) const {
    uint16_t decodedLength = 0;
    uint16_t contentEnd = node.contentPosition + node.contentLength;

    for (uint16_t position = node.contentPosition; position < contentEnd; position++) {
        if (_readJsonCharacter(position) == '\\' && position + 1 < contentEnd) {
            position++;
        }
        decodedLength++;
    }
    return decodedLength;
}

/******************************************************************************/
/*!
  @brief    Streams selected lines from an externally backed virtual file.
  @param    virtualPath         Virtual file path to resolve
  @param    output              Destination stream
  @param    lineCount           Maximum number of lines to print
  @param    isTail              True to select lines from the end
  @returns  bool                True when an external file was handled
*/
/******************************************************************************/
bool VirtualFileSystem::printExternalFileLines(const char* virtualPath,
                                               Print& output,
                                               uint16_t lineCount,
                                               bool isTail) const {
    char storagePath[EXTERNAL_FILE_MAX_STORAGE_PATH_LENGTH];
    char fileName[EXTERNAL_FILE_MAX_NAME_LENGTH + 1];

    if (!ExternalFileRegistry::resolveVirtualPath(
            virtualPath, storagePath, sizeof(storagePath), fileName,
            sizeof(fileName))) {
        return false;
    }
    File externalFile = SPIFFS.open(storagePath, "r");
    if (!externalFile) {
        return false;
    }

    uint32_t totalLines = 0;
    uint32_t startLine = 0;
    bool hasContent = false;
    bool endedWithNewline = false;
    bool lastCharacterWasCarriageReturn = false;

    if (isTail) {
        while (externalFile.available()) {
            char character = static_cast<char>(externalFile.read());
            hasContent = true;
            if (character == '\n' && lastCharacterWasCarriageReturn) {
                lastCharacterWasCarriageReturn = false;
                endedWithNewline = true;
                continue;
            }
            if (character == '\r' || character == '\n') {
                totalLines++;
                lastCharacterWasCarriageReturn = character == '\r';
                endedWithNewline = true;
            } else {
                lastCharacterWasCarriageReturn = false;
                endedWithNewline = false;
            }
        }
        if (hasContent && !endedWithNewline) {
            totalLines++;
        }
        startLine = totalLines > lineCount ? totalLines - lineCount : 0;
        externalFile.close();
        externalFile = SPIFFS.open(storagePath, "r");
        if (!externalFile) {
            return true;
        }
    }

    uint32_t currentLine = 0;
    uint32_t endLine = startLine + lineCount;
    bool wroteContent = false;
    endedWithNewline = true;
    lastCharacterWasCarriageReturn = false;

    while (currentLine < endLine && externalFile.available()) {
        char character = static_cast<char>(externalFile.read());
        if (character == '\n' && lastCharacterWasCarriageReturn) {
            lastCharacterWasCarriageReturn = false;
            continue;
        }
        if (character == '\r' || character == '\n') {
            if (currentLine >= startLine) {
                output.print(F("\r\n"));
                wroteContent = true;
                endedWithNewline = true;
            }
            currentLine++;
            lastCharacterWasCarriageReturn = character == '\r';
            continue;
        }
        if (currentLine >= startLine) {
            output.write(character);
            wroteContent = true;
            endedWithNewline = false;
        }
        lastCharacterWasCarriageReturn = false;
    }
    externalFile.close();
    if (wroteContent && !endedWithNewline) {
        output.print(F("\r\n"));
    }
    return true;
}

/******************************************************************************/
/*!
  @brief    Reads one byte from the flash-resident JSON definition.
*/
/******************************************************************************/
char VirtualFileSystem::_readJsonCharacter(uint16_t position) const {
    return static_cast<char>(pgm_read_byte(VIRTUAL_FILE_SYSTEM_JSON + position));
}
