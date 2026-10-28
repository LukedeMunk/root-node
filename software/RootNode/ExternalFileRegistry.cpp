/******************************************************************************/
/*
 * File:    ExternalFileRegistry.cpp
 * Author:  Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Maps downloadable virtual files to bounded SPIFFS storage paths.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#include "ExternalFileRegistry.h"

#include "configuration.h"

#include <ctype.h>
#include <string.h>

static const ExternalFileMapping EXTERNAL_FILE_MAPPINGS[] = {
    {"/etc/SmartLight/webusers.db", EXT_FILE_WEBUSERS_DB},
    {"/home/user/testprogram.c", EXT_FILE_TEST_PROGRAM},
    {"/home/user/testprogram", EXT_FILE_TEST_PROGRAM_OBJ},
    {"/home/user/notes.txt", EXT_FILE_NOTES}
};

static const uint8_t EXTERNAL_FILE_MAPPING_COUNT =
    sizeof(EXTERNAL_FILE_MAPPINGS) / sizeof(EXTERNAL_FILE_MAPPINGS[0]);

/******************************************************************************/
/*!
  @brief    Finds external storage for one exact virtual Linux path.
  @param    virtualPath         Virtual path supplied by the caller
  @returns  ExternalFileMapping Mapping pointer, or NULL when unsupported
*/
/******************************************************************************/
const ExternalFileMapping* ExternalFileRegistry::findByVirtualPath(
    const char* virtualPath) {
    if (virtualPath == NULL) {
        return NULL;
    }

    for (uint8_t i = 0; i < EXTERNAL_FILE_MAPPING_COUNT; i++) {
        if (strcmp(EXTERNAL_FILE_MAPPINGS[i].virtualPath, virtualPath) == 0) {
            return &EXTERNAL_FILE_MAPPINGS[i];
        }
    }

    return NULL;
}

/******************************************************************************/
/*!
  @brief    Finds a registered mapping by its external basename.
  @param    fileName            External file basename
  @returns  ExternalFileMapping Mapping pointer, or NULL when unsupported
*/
/******************************************************************************/
const ExternalFileMapping* ExternalFileRegistry::findByExternalFileName(
    const char* fileName) {
    if (fileName == NULL) {
        return NULL;
    }

    for (uint8_t i = 0; i < EXTERNAL_FILE_MAPPING_COUNT; i++) {
        if (strcmp(fileName, getFileName(EXTERNAL_FILE_MAPPINGS[i])) == 0) {
            return &EXTERNAL_FILE_MAPPINGS[i];
        }
    }

    return NULL;
}

/******************************************************************************/
/*!
  @brief    Validates a bounded filename accepted by every external file flow.
  @param    fileName            Untrusted basename
  @returns  bool                True for a safe portable basename
*/
/******************************************************************************/
bool ExternalFileRegistry::isSafeFileName(const char* fileName) {
    size_t fileNameLength;

    if (fileName == NULL || !isalnum(static_cast<unsigned char>(fileName[0]))) {
        return false;
    }
    
    fileNameLength = strlen(fileName);
    if (fileNameLength == 0 || fileNameLength > EXTERNAL_FILE_MAX_NAME_LENGTH ||
        strcmp(fileName, ".") == 0 || strcmp(fileName, "..") == 0) {
        return false;
    }

    for (size_t i = 1; i < fileNameLength; i++) {
        unsigned char character = static_cast<unsigned char>(fileName[i]);
        if (!isalnum(character) && character != '.' && character != '-' &&
            character != '_') {
            return false;
        }
    }

    return true;
}

/******************************************************************************/
/*!
  @brief    Resolves one upload basename to its virtual and physical paths.
*/
/******************************************************************************/
bool ExternalFileRegistry::resolveUploadName(const char* uploadName,
                                             char* virtualPath,
                                             size_t virtualPathLength,
                                             char* storagePath,
                                             size_t storagePathLength) {
    if (!isSafeFileName(uploadName) ||
        !getVirtualPathForExternalFile(uploadName, virtualPath, virtualPathLength)) {
        return false;
    }

    return _buildStoragePath(uploadName, storagePath, storagePathLength);
}

/******************************************************************************/
/*!
  @brief    Resolves a supported virtual path to external storage.
*/
/******************************************************************************/
bool ExternalFileRegistry::resolveVirtualPath(const char* virtualPath,
                                              char* storagePath,
                                              size_t storagePathLength,
                                              char* fileName,
                                              size_t fileNameLength) {
    const ExternalFileMapping* mapping = findByVirtualPath(virtualPath);
    const char* resolvedFileName;
    size_t uploadDirectoryLength = strlen(VIRTUAL_UPLOAD_DIRECTORY);

    if (mapping != NULL) {
        resolvedFileName = getFileName(*mapping);
    } else {
        if (virtualPath == NULL ||
            strncmp(virtualPath, VIRTUAL_UPLOAD_DIRECTORY, uploadDirectoryLength) != 0 ||
            virtualPath[uploadDirectoryLength] != '/') {
            return false;
        }

        resolvedFileName = virtualPath + uploadDirectoryLength + 1;
        if (!isSafeFileName(resolvedFileName)) {
            return false;
        }
    }

    size_t requiredFileNameLength = strlen(resolvedFileName) + 1;
    if (fileName == NULL || fileNameLength < requiredFileNameLength ||
        !_buildStoragePath(resolvedFileName, storagePath, storagePathLength)) {
        return false;
    }

    strncpy(fileName, resolvedFileName, fileNameLength - 1);
    fileName[fileNameLength - 1] = '\0';

    return true;
}

/******************************************************************************/
/*!
  @brief    Maps a stored basename to its registered or dynamic virtual path.
*/
/******************************************************************************/
bool ExternalFileRegistry::getVirtualPathForExternalFile(
    const char* fileName, char* virtualPath, size_t virtualPathLength) {
    if (!isSafeFileName(fileName) || virtualPath == NULL) {
        return false;
    }

    const ExternalFileMapping* mapping = findByExternalFileName(fileName);
    const char* registeredPath = mapping == NULL ? NULL : mapping->virtualPath;
    size_t requiredLength = registeredPath == NULL
                                ? strlen(VIRTUAL_UPLOAD_DIRECTORY) + strlen(fileName) + 2
                                : strlen(registeredPath) + 1;

    if (virtualPathLength < requiredLength) {
        return false;
    }

    if (registeredPath != NULL) {
        strncpy(virtualPath, registeredPath, virtualPathLength - 1);
        virtualPath[virtualPathLength - 1] = '\0';
        return true;
    }

    strncpy(virtualPath, VIRTUAL_UPLOAD_DIRECTORY, virtualPathLength - 1);
    virtualPath[virtualPathLength - 1] = '\0';
    strncat(virtualPath, "/", virtualPathLength - strlen(virtualPath) - 1);
    strncat(virtualPath, fileName, virtualPathLength - strlen(virtualPath) - 1);
    return true;
}

/******************************************************************************/
/*!
  @brief    Builds one bounded SPIFFS path below the external file directory.
  @param    mapping             Registered external file mapping
  @param    output              Receives the complete SPIFFS path
  @param    outputLength        Size of output
  @returns  bool                False when the destination is too small
*/
/******************************************************************************/
bool ExternalFileRegistry::getStoragePath(const ExternalFileMapping& mapping, char* output, size_t outputLength) {
    return _buildStoragePath(getFileName(mapping), output, outputLength);
}

/******************************************************************************/
/*!
  @brief    Gets the basename of a registered slash-prefixed external path.
*/
/******************************************************************************/
const char* ExternalFileRegistry::getFileName(const ExternalFileMapping& mapping) {
    return mapping.externalPath[0] == '/' ? mapping.externalPath + 1 : mapping.externalPath;
}

/******************************************************************************/
/*!
  @brief    Builds a bounded physical path below the external file directory.
*/
/******************************************************************************/
bool ExternalFileRegistry::_buildStoragePath(const char* fileName, char* output, size_t outputLength) {
    if (!isSafeFileName(fileName) || output == NULL) {
        return false;
    }

    size_t requiredLength = strlen(EXTERNAL_FILE_FOLDER) + strlen(fileName) + 2;

    if (outputLength < requiredLength) {
        return false;
    }
    strncpy(output, EXTERNAL_FILE_FOLDER, outputLength - 1);
    output[outputLength - 1] = '\0';
    strncat(output, "/", outputLength - strlen(output) - 1);
    strncat(output, fileName, outputLength - strlen(output) - 1);
    return true;
}
