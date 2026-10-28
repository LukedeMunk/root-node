/******************************************************************************/
/*
 * File:    files.js
 * Author:  Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Asynchronous device file uploads and downloads.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
import {
    getAuthenticationRole,
    getText,
    initializePage,
    requestResponse
} from "../base.js";

const fileLoadingFieldElem = document.getElementById("fileLoadingField");
const fileExplorerCardElem = document.getElementById("fileExplorerCard");
const uploadCardElem = document.getElementById("uploadCard");
const fileEmptyFieldElem = document.getElementById("fileEmptyField");
const fileErrorFieldElem = document.getElementById("fileErrorField");
const fileListElem = document.getElementById("fileList");
const fileBreadcrumbsElem = document.getElementById("fileBreadcrumbs");
const fileMessageFieldElem = document.getElementById("fileMessageField");
const parentDirectoryBtnElem = document.getElementById("parentDirectoryBtn");
const uploadFormElem = document.getElementById("uploadForm");
const uploadFileInputElem = document.getElementById("uploadFileInput");
const selectedFileFieldElem = document.getElementById("selectedFileField");
const uploadBtnElem = document.getElementById("uploadBtn");

let isRequestActive = false;
let isFileListRequestActive = false;
let messageKey = "";
let messageIsError = false;
let availableFiles = [];
let currentDirectory = "/";
let fileListState = "loading";

/******************************************************************************/
/*!
  @brief    Disables file actions while one request is active.
  @param    isDisabled          Whether all file actions are disabled
*/
/******************************************************************************/
function setActionsDisabled(isDisabled) {
    isRequestActive = isDisabled;
    uploadBtnElem.disabled = isDisabled;
    uploadFileInputElem.disabled = isDisabled;
    for (const buttonElem of fileExplorerCardElem.querySelectorAll("button")) {
        buttonElem.disabled = isDisabled;
    }
}

/******************************************************************************/
/*!
  @brief    Shows a localized file operation message.
  @param    key                 Translation key
  @param    isError             Whether the message is an error
*/
/******************************************************************************/
function showFileMessage(key, isError = false) {
    messageKey = key;
    messageIsError = isError;
    fileMessageFieldElem.textContent = getText(key);
    fileMessageFieldElem.className = isError
        ? "message message-error"
        : "message message-success";
    fileMessageFieldElem.hidden = false;
}

/******************************************************************************/
/*!
  @brief    Updates the selected filename beside the file chooser.
*/
/******************************************************************************/
function updateSelectedFile() {
    selectedFileFieldElem.textContent = uploadFileInputElem.files.length > 0
        ? uploadFileInputElem.files[0].name
        : getText("noFileChosen");
}

/******************************************************************************/
/*!
  @brief    Starts a browser download for a received file blob.
  @param    blob                 Downloaded file data
  @param    filename             Local download filename
*/
/******************************************************************************/
function saveBlob(blob, filename) {
    const objectUrl = URL.createObjectURL(blob);
    const linkElem = document.createElement("a");
    linkElem.href = objectUrl;
    linkElem.download = filename;
    document.body.append(linkElem);
    linkElem.click();
    linkElem.remove();
    URL.revokeObjectURL(objectUrl);
}

/******************************************************************************/
/*!
  @brief    Downloads one file through the existing multipart route.
  @param    event               Native download button event
*/
/******************************************************************************/
async function downloadFile(event) {
    if (isRequestActive) {
        return;
    }
    const buttonElem = event.currentTarget;
    const filePath = buttonElem.dataset.filePath;
    const formData = new FormData();
    formData.append("filekey", filePath);
    formData.append("name", new Blob([]), "download.request");
    setActionsDisabled(true);
    fileMessageFieldElem.hidden = true;
    try {
        const response = await requestResponse("/download", {
            method: "POST",
            body: formData
        });
        const blob = await response.blob();
        saveBlob(blob, getPathName(filePath));
        showFileMessage("downloadStarted");
    } catch (error) {
        showFileMessage("downloadError", true);
    } finally {
        setActionsDisabled(false);
    }
}

/******************************************************************************/
/*!
  @brief    Gets the final name component from an absolute path.
  @param    path                Absolute file path
  @returns                      Filename, or a root marker
*/
/******************************************************************************/
function getPathName(path) {
    return path.split("/").filter(Boolean).pop() || "/";
}

/******************************************************************************/
/*!
  @brief    Validates and normalizes one file returned by the firmware.
  @param    file                File-list response item
  @returns                      Normalized file, or null when invalid
*/
/******************************************************************************/
function normalizeFile(file) {
    if (file === null || typeof file !== "object" || typeof file.path !== "string" ||
        typeof file.isDownloadable !== "boolean" ||
        !["file", "directory"].includes(file.type)) {
        return null;
    }
    const size = Number(file.size);
    const pathParts = file.path.split("/").filter(Boolean);
    const hasUnsafePart = pathParts.some((part) => part === "." || part === "..");
    if (!file.path.startsWith("/") || hasUnsafePart ||
        !Number.isFinite(size) || size < 0) {
        return null;
    }
    return Object.freeze({
        path: `/${pathParts.join("/")}`,
        size,
        isDownloadable: file.type === "file" && file.isDownloadable,
        type: file.type
    });
}

/******************************************************************************/
/*!
  @brief    Returns direct folders and files below the active directory.
  @returns                      Sorted explorer entries
*/
/******************************************************************************/
function getCurrentDirectoryEntries() {
    const directoryPrefix = currentDirectory === "/" ? "/" : `${currentDirectory}/`;
    const directoryPaths = new Map();
    const fileEntries = [];

    for (const file of availableFiles) {
        if (!file.path.startsWith(directoryPrefix)) {
            continue;
        }
        const relativePath = file.path.slice(directoryPrefix.length);
        const separatorIndex = relativePath.indexOf("/");
        if (separatorIndex >= 0) {
            const directoryName = relativePath.slice(0, separatorIndex);
            const directoryPath = currentDirectory === "/"
                ? `/${directoryName}`
                : `${currentDirectory}/${directoryName}`;
            directoryPaths.set(directoryName, directoryPath);
        } else if (relativePath !== "" && file.type === "directory") {
            directoryPaths.set(relativePath, file.path);
        } else if (relativePath !== "" && file.type === "file") {
            fileEntries.push({
                name: relativePath,
                path: file.path,
                size: file.size,
                isDownloadable: file.isDownloadable,
                type: "file"
            });
        }
    }

    const directoryEntries = Array.from(directoryPaths, ([name, path]) => ({
        name,
        path,
        type: "directory"
    }));
    directoryEntries.sort((first, second) => first.name.localeCompare(second.name));
    fileEntries.sort((first, second) => first.name.localeCompare(second.name));
    return [...directoryEntries, ...fileEntries];
}

/******************************************************************************/
/*!
  @brief    Navigates to the directory selected by a generated control.
  @param    event               Native button event
*/
/******************************************************************************/
function openDirectory(event) {
    currentDirectory = event.currentTarget.dataset.directoryPath;
    renderFiles();
}

/******************************************************************************/
/*!
  @brief    Navigates from the active directory to its parent.
*/
/******************************************************************************/
function openParentDirectory() {
    const directoryParts = currentDirectory.split("/").filter(Boolean);
    directoryParts.pop();
    currentDirectory = directoryParts.length === 0 ? "/" : `/${directoryParts.join("/")}`;
    renderFiles();
}

/******************************************************************************/
/*!
  @brief    Creates one accessible folder list item.
  @param    entry               Directory entry to render
  @returns                      Rendered directory list item
*/
/******************************************************************************/
function createDirectoryItem(entry) {
    const itemElem = document.createElement("li");
    itemElem.className = "file-item file-directory-item";
    const openBtnElem = document.createElement("button");
    openBtnElem.className = "file-entry-button";
    openBtnElem.type = "button";
    openBtnElem.dataset.directoryPath = entry.path;
    openBtnElem.setAttribute("aria-label", `${getText("openFolder")}: ${entry.name}`);
    const markerElem = document.createElement("span");
    markerElem.className = "file-entry-marker file-directory-marker";
    markerElem.setAttribute("aria-hidden", "true");
    const detailsElem = document.createElement("span");
    detailsElem.className = "file-details";
    const nameElem = document.createElement("span");
    nameElem.className = "file-name";
    nameElem.textContent = entry.name;
    const typeElem = document.createElement("span");
    typeElem.className = "file-type";
    typeElem.textContent = getText("folderType");
    detailsElem.append(nameElem, typeElem);
    openBtnElem.append(markerElem, detailsElem);
    openBtnElem.addEventListener("click", openDirectory);
    itemElem.append(openBtnElem);
    return itemElem;
}

/******************************************************************************/
/*!
  @brief    Creates one accessible file list item.
  @param    file                Available file description
  @returns                      Rendered file list item
*/
/******************************************************************************/
function createFileItem(file) {
    const itemElem = document.createElement("li");
    itemElem.className = "file-item";
    const markerElem = document.createElement("span");
    markerElem.className = "file-entry-marker file-marker";
    markerElem.setAttribute("aria-hidden", "true");
    const detailsElem = document.createElement("div");
    detailsElem.className = "file-details";
    const nameElem = document.createElement("span");
    nameElem.className = "file-name";
    nameElem.textContent = file.name;
    const sizeElem = document.createElement("span");
    sizeElem.className = "file-type";
    sizeElem.textContent = `${getText("fileType")} · ${file.size.toLocaleString()} ` +
        getText("bytesUnit");
    detailsElem.append(nameElem, sizeElem);
    itemElem.append(markerElem, detailsElem);

    if (file.isDownloadable) {
        const downloadBtnElem = document.createElement("button");
        downloadBtnElem.className = "button";
        downloadBtnElem.type = "button";
        downloadBtnElem.dataset.filePath = file.path;
        downloadBtnElem.textContent = getText("downloadFile");
        downloadBtnElem.addEventListener("click", downloadFile);
        itemElem.append(downloadBtnElem);
    } else {
        const unavailableElem = document.createElement("span");
        unavailableElem.className = "file-unavailable";
        unavailableElem.textContent = getText("fileNotDownloadable");
        itemElem.append(unavailableElem);
    }

    return itemElem;
}

/******************************************************************************/
/*!
  @brief    Renders root and directory breadcrumb controls.
*/
/******************************************************************************/
function renderBreadcrumbs() {
    const directoryParts = currentDirectory.split("/").filter(Boolean);
    const breadcrumbParts = [{name: getText("rootDirectory"), path: "/"}];
    let breadcrumbPath = "";

    for (const directoryPart of directoryParts) {
        breadcrumbPath += `/${directoryPart}`;
        breadcrumbParts.push({name: directoryPart, path: breadcrumbPath});
    }

    fileBreadcrumbsElem.replaceChildren();
    for (const [index, part] of breadcrumbParts.entries()) {
        const itemElem = document.createElement("li");
        if (index === breadcrumbParts.length - 1) {
            const currentElem = document.createElement("span");
            currentElem.className = "file-breadcrumb-current";
            currentElem.setAttribute("aria-current", "page");
            currentElem.textContent = part.name;
            itemElem.append(currentElem);
        } else {
            const buttonElem = document.createElement("button");
            buttonElem.className = "file-breadcrumb-button";
            buttonElem.type = "button";
            buttonElem.dataset.directoryPath = part.path;
            buttonElem.textContent = part.name;
            buttonElem.addEventListener("click", openDirectory);
            itemElem.append(buttonElem);
        }
        fileBreadcrumbsElem.append(itemElem);
    }
    parentDirectoryBtnElem.hidden = currentDirectory === "/";
}

/******************************************************************************/
/*!
  @brief    Renders loading, empty, error and success file list states.
*/
/******************************************************************************/
function renderFiles() {
    const entries = getCurrentDirectoryEntries();

    fileListElem.replaceChildren();
    fileLoadingFieldElem.hidden = true;
    fileEmptyFieldElem.hidden = true;
    fileErrorFieldElem.hidden = true;
    renderBreadcrumbs();
    try {
        if (availableFiles.length === 0) {
            fileEmptyFieldElem.textContent = getText("filesEmpty");
            fileEmptyFieldElem.hidden = false;
            fileListElem.hidden = true;
            return;
        }
        if (entries.length === 0) {
            fileEmptyFieldElem.textContent = getText("folderEmpty");
            fileEmptyFieldElem.hidden = false;
            fileListElem.hidden = true;
            return;
        }
        for (const entry of entries) {
            fileListElem.append(entry.type === "directory"
                ? createDirectoryItem(entry)
                : createFileItem(entry));
        }
        fileListElem.hidden = false;
    } catch (error) {
        console.error("Error creating file list:", error);
        fileErrorFieldElem.textContent = getText("filesError");
        fileErrorFieldElem.hidden = false;
    }
}

/******************************************************************************/
/*!
  @brief    Loads the available files from the protected firmware API.
*/
/******************************************************************************/
async function loadFiles() {
    if (isFileListRequestActive) {
        return;
    }
    isFileListRequestActive = true;
    fileListState = "loading";
    fileLoadingFieldElem.hidden = false;
    fileEmptyFieldElem.hidden = true;
    fileErrorFieldElem.hidden = true;
    fileListElem.hidden = true;
    try {
        const response = await requestResponse("/get_files");
        const files = await response.json();
        if (!Array.isArray(files)) {
            throw new Error("Invalid file list response");
        }
        const normalizedFiles = files.map(normalizeFile).filter(Boolean);
        if (files.length > 0 && normalizedFiles.length === 0) {
            throw new Error("File list contains no valid files");
        }
        availableFiles = Array.from(
            new Map(normalizedFiles.map((file) => [file.path, file])).values()
        );
        fileListState = "loaded";
        renderFiles();
    } catch (error) {
        fileListState = "error";
        fileLoadingFieldElem.hidden = true;
        fileErrorFieldElem.textContent = getText("filesError");
        fileErrorFieldElem.hidden = false;
    } finally {
        isFileListRequestActive = false;
    }
}

/******************************************************************************/
/*!
  @brief    Uploads the selected file through the existing multipart route.
  @param    event               Native form submission event
*/
/******************************************************************************/
async function uploadFile(event) {
    event.preventDefault();
    if (isRequestActive) {
        return;
    }
    if (uploadFileInputElem.files.length === 0) {
        showFileMessage("noFileSelected", true);
        uploadFileInputElem.focus();
        return;
    }
    const formData = new FormData();
    formData.append("name", uploadFileInputElem.files[0]);
    setActionsDisabled(true);
    showFileMessage("uploadingFile");
    try {
        const response = await requestResponse("/upload", {
            method: "POST",
            body: formData,
            redirect: "follow"
        });
        const result = await response.json();
        uploadFormElem.reset();
        updateSelectedFile();
        showFileMessage("uploadSuccess");
        if (typeof result.path === "string" && result.path.startsWith("/")) {
            const pathParts = result.path.split("/").filter(Boolean);
            pathParts.pop();
            currentDirectory = pathParts.length === 0
                ? "/"
                : `/${pathParts.join("/")}`;
        }
        await loadFiles();
    } catch (error) {
        const errorKeys = {
            invalid_filename: "uploadInvalidFilename",
            file_too_large: "uploadTooLarge",
            upload_failed: "uploadStorageError"
        };
        showFileMessage(errorKeys[error.code] || "uploadError", true);
    } finally {
        setActionsDisabled(false);
    }
}

/******************************************************************************/
/*!
  @brief    Refreshes dynamic file text after a language change.
*/
/******************************************************************************/
function updateDynamicLanguage() {
    updateSelectedFile();
    if (fileListState === "loaded") {
        renderFiles();
    } else if (fileListState === "error") {
        fileErrorFieldElem.textContent = getText("filesError");
    }
    if (messageKey !== "") {
        showFileMessage(messageKey, messageIsError);
    }
}

/******************************************************************************/
/*!
  @brief    Shows uploading only to administrators.
*/
/******************************************************************************/
function updateUploadPermission() {
    const canUpload = getAuthenticationRole() === "admin";
    uploadCardElem.hidden = !canUpload;
    uploadCardElem.parentElement.classList.toggle("single-column", !canUpload);
}

initializePage("downloadTitle", "admin");
uploadFileInputElem.addEventListener("change", updateSelectedFile);
uploadFormElem.addEventListener("submit", uploadFile);
parentDirectoryBtnElem.addEventListener("click", openParentDirectory);
document.addEventListener("languagechange", updateDynamicLanguage);
document.addEventListener("authenticationchange", updateUploadPermission);
updateUploadPermission();
loadFiles();
