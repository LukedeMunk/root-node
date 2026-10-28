/******************************************************************************/
/*
 * File:    admin.js
 * Author:  Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Administrator page actions and restart confirmation.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
import {
    getText,
    initializePage,
    requestResponse,
    requestText
} from "../base.js";

const restartBtnElem = document.getElementById("restartBtn");
const restartDialogElem = document.getElementById("restartDialog");
const restartCancelBtnElem = document.getElementById("restartCancelBtn");
const restartConfirmBtnElem = document.getElementById("restartConfirmBtn");
const restartMessageFieldElem = document.getElementById("restartMessageField");
const resetNetworkBtnElem = document.getElementById("resetNetworkBtn");
const resetNetworkDialogElem = document.getElementById("resetNetworkDialog");
const resetNetworkCancelBtnElem = document.getElementById(
    "resetNetworkCancelBtn"
);
const resetNetworkConfirmBtnElem = document.getElementById(
    "resetNetworkConfirmBtn"
);
const resetNetworkMessageFieldElem = document.getElementById(
    "resetNetworkMessageField"
);
const chipModelFieldElem = document.getElementById("chipModelField");
const chipIdFieldElem = document.getElementById("chipIdField");
const flashSizeFieldElem = document.getElementById("flashSizeField");
const freeHeapFieldElem = document.getElementById("freeHeapField");
const firmwareVersionFieldElem = document.getElementById(
    "firmwareVersionField"
);
const buildVersionFieldElem = document.getElementById("buildVersionField");
const hostnameFieldElem = document.getElementById("hostnameField");
const ipAddressFieldElem = document.getElementById("ipAddressField");
const deviceInfoMessageFieldElem = document.getElementById(
    "deviceInfoMessageField"
);
let restartMessageKey = "";
let restartMessageIsError = false;
let resetNetworkMessageKey = "";
let resetNetworkMessageIsError = false;
let deviceInfoMessageKey = "deviceInfoLoading";
let deviceInformation = null;
let isDeviceInfoRequestActive = false;

/******************************************************************************/
/*!
  @brief    Formats a byte count with the active interface language.
  @param    value               Byte count from the firmware
  @returns                      Localized byte value
*/
/******************************************************************************/
function formatBytes(value) {
    if (!Number.isFinite(Number(value))) {
        return getText("informationUnavailable");
    }
    return `${Number(value).toLocaleString()} ${getText("bytesUnit")}`;
}

/******************************************************************************/
/*!
  @brief    Formats an optional device information text value.
  @param    value               Value supplied by the firmware
  @returns                      Value or localized unavailable text
*/
/******************************************************************************/
function formatDeviceText(value) {
    return typeof value === "string" && value !== ""
        ? value
        : getText("informationUnavailable");
}

/******************************************************************************/
/*!
  @brief    Shows a localized device information state.
  @param    key                 Translation key
  @param    isError             Whether the state is an error
*/
/******************************************************************************/
function showDeviceInfoMessage(key, isError = false) {
    deviceInfoMessageKey = key;
    deviceInfoMessageFieldElem.textContent = getText(key);
    deviceInfoMessageFieldElem.className = isError
        ? "message message-error"
        : "loading-state";
    deviceInfoMessageFieldElem.hidden = false;
}

/******************************************************************************/
/*!
  @brief    Renders current chip and firmware values.
*/
/******************************************************************************/
function renderDeviceInformation() {
    if (deviceInformation === null) {
        return;
    }
    chipModelFieldElem.textContent = formatDeviceText(
        deviceInformation.chipModel
    );
    chipIdFieldElem.textContent = formatDeviceText(deviceInformation.chipId);
    flashSizeFieldElem.textContent = formatBytes(deviceInformation.flashSize);
    freeHeapFieldElem.textContent = formatBytes(deviceInformation.freeHeap);
    firmwareVersionFieldElem.textContent = formatDeviceText(
        deviceInformation.firmwareVersion
    );
    buildVersionFieldElem.textContent = formatDeviceText(
        deviceInformation.buildVersion
    );
    hostnameFieldElem.textContent = formatDeviceText(deviceInformation.hostname);
    ipAddressFieldElem.textContent = formatDeviceText(
        deviceInformation.ipAddress
    );
}

/******************************************************************************/
/*!
  @brief    Loads current chip and firmware values from the protected API.
*/
/******************************************************************************/
async function loadDeviceInformation() {
    if (isDeviceInfoRequestActive) {
        return;
    }
    isDeviceInfoRequestActive = true;
    showDeviceInfoMessage("deviceInfoLoading");
    try {
        const response = await requestResponse("/get_device_info");
        deviceInformation = await response.json();
        renderDeviceInformation();
        deviceInfoMessageFieldElem.hidden = true;
    } catch (error) {
        showDeviceInfoMessage("deviceInfoError", true);
    } finally {
        isDeviceInfoRequestActive = false;
    }
}

/******************************************************************************/
/*!
  @brief    Shows a status message below the restart action.
  @param    key                 Message translation key
  @param    isError             Whether the message represents a failure
*/
/******************************************************************************/
function showRestartMessage(key, isError = false) {
    restartMessageKey = key;
    restartMessageIsError = isError;
    restartMessageFieldElem.textContent = getText(key);
    restartMessageFieldElem.className = isError
        ? "message message-error"
        : "message message-success";
    restartMessageFieldElem.hidden = false;
}

/******************************************************************************/
/*!
  @brief    Sends the restart request after explicit confirmation.
*/
/******************************************************************************/
async function restartDevice() {
    restartDialogElem.close();
    restartBtnElem.disabled = true;
    showRestartMessage("restarting");
    try {
        await requestText("/restart");
    } catch (error) {
        showRestartMessage("restartError", true);
        restartBtnElem.disabled = false;
    }
}

/******************************************************************************/
/*!
  @brief    Opens the restart confirmation dialog.
*/
/******************************************************************************/
function showRestartDialog() {
    if (restartDialogElem.hidden) {
        return;
    }
    restartDialogElem.showModal();
}

/******************************************************************************/
/*!
  @brief    Closes the restart confirmation dialog.
*/
/******************************************************************************/
function closeRestartDialog() {
    restartDialogElem.close();
}

/******************************************************************************/
/*!
  @brief    Shows a status message below the network reset action.
  @param    key                 Message translation key
  @param    isError             Whether the message represents a failure
*/
/******************************************************************************/
function showResetNetworkMessage(key, isError = false) {
    resetNetworkMessageKey = key;
    resetNetworkMessageIsError = isError;
    resetNetworkMessageFieldElem.textContent = getText(key);
    resetNetworkMessageFieldElem.className = isError
        ? "message message-error"
        : "message message-success";
    resetNetworkMessageFieldElem.hidden = false;
}

/******************************************************************************/
/*!
  @brief    Sends the network reset request after explicit confirmation.
*/
/******************************************************************************/
async function resetNetwork() {
    resetNetworkDialogElem.close();
    resetNetworkBtnElem.disabled = true;
    showResetNetworkMessage("resettingNetwork");
    try {
        await requestText("/reset_network", { method: "POST" });
    } catch (error) {
        showResetNetworkMessage("resetNetworkError", true);
        resetNetworkBtnElem.disabled = false;
    }
}

/******************************************************************************/
/*!
  @brief    Opens the network reset confirmation dialog when available.
*/
/******************************************************************************/
function showResetNetworkDialog() {
    if (resetNetworkDialogElem.hidden) {
        return;
    }
    resetNetworkDialogElem.showModal();
}

/******************************************************************************/
/*!
  @brief    Closes the network reset confirmation dialog.
*/
/******************************************************************************/
function closeResetNetworkDialog() {
    resetNetworkDialogElem.close();
}

/******************************************************************************/
/*!
  @brief    Refreshes the restart status after a language change.
*/
/******************************************************************************/
function updateDynamicLanguage() {
    renderDeviceInformation();
    if (!deviceInfoMessageFieldElem.hidden) {
        const isError = deviceInfoMessageFieldElem.classList.contains(
            "message-error"
        );
        showDeviceInfoMessage(deviceInfoMessageKey, isError);
    }
    if (restartMessageKey !== "") {
        showRestartMessage(restartMessageKey, restartMessageIsError);
    }
    if (resetNetworkMessageKey !== "") {
        showResetNetworkMessage(
            resetNetworkMessageKey,
            resetNetworkMessageIsError
        );
    }
}

initializePage("adminTitle", "admin");
restartBtnElem.addEventListener("click", showRestartDialog);
restartCancelBtnElem.addEventListener("click", closeRestartDialog);
restartConfirmBtnElem.addEventListener("click", restartDevice);
resetNetworkBtnElem.addEventListener("click", showResetNetworkDialog);
resetNetworkCancelBtnElem.addEventListener("click", closeResetNetworkDialog);
resetNetworkConfirmBtnElem.addEventListener("click", resetNetwork);
document.addEventListener("languagechange", updateDynamicLanguage);
loadDeviceInformation();
