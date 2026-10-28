/******************************************************************************/
/*
 * File:    ExternalFileRegistry.h
 * Author:  Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Maps downloadable virtual files to bounded SPIFFS storage paths.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#ifndef EXTERNAL_FILE_REGISTRY_H_
#define EXTERNAL_FILE_REGISTRY_H_

#include "Arduino.h"

#define EXTERNAL_FILE_MAX_STORAGE_PATH_LENGTH  64

struct ExternalFileMapping {
    const char* virtualPath;
    const char* externalPath;
};

class ExternalFileRegistry {
    public:
        static const ExternalFileMapping* findByVirtualPath(const char* virtualPath);
        static const ExternalFileMapping* findByExternalFileName(const char* fileName);
        static bool isSafeFileName(const char* fileName);
        static bool resolveUploadName(const char* uploadName, char* virtualPath,
                                      size_t virtualPathLength, char* storagePath,
                                      size_t storagePathLength);
        static bool resolveVirtualPath(const char* virtualPath, char* storagePath,
                                       size_t storagePathLength, char* fileName,
                                       size_t fileNameLength);
        static bool getVirtualPathForExternalFile(const char* fileName, char* virtualPath, size_t virtualPathLength);
        static bool getStoragePath(const ExternalFileMapping& mapping, char* output, size_t outputLength);
        static const char* getFileName(const ExternalFileMapping& mapping);

    private:
        static bool _buildStoragePath(const char* fileName, char* output, size_t outputLength);
};

#endif  // EXTERNAL_FILE_REGISTRY_H_
