/******************************************************************************/
/*
 * File:    index.js
 * Author:  Luke de Munk
 *
 * Brief:   Dashboard state loading and device controls.
 */
/******************************************************************************/
import { getText, initializePage, requestText } from "../base.js";

const powerToggleElem = document.getElementById("powerToggle");
const brightnessRangeElem = document.getElementById("brightnessRange");
const brightnessOutputElem = document.getElementById("brightnessOutput");
const deviceStatusFieldElem = document.getElementById("deviceStatusField");
let deviceStatusKey = "statusLoading";
let deviceStatusIsError = false;
let deviceTitleKey = "";

/******************************************************************************/
/*!
  @brief    Formats the firmware brightness range as a percentage.
  @param    value               Brightness value between 0 and 1023
  @returns                      Rounded percentage for the interface
*/
/******************************************************************************/
function formatBrightness(value) {
    return `${Math.round((value / 1023) * 100)}%`;
}

/******************************************************************************/
/*!
  @brief    Updates the visible connection state of the device.
  @param    key                 Status translation key
  @param    isError             Whether the status is a request failure
*/
/******************************************************************************/
function setDeviceStatus(key, isError = false) {
    deviceStatusKey = key;
    deviceStatusIsError = isError;
    deviceStatusFieldElem.textContent = getText(key);
    deviceStatusFieldElem.classList.toggle("status-success", !isError);
    deviceStatusFieldElem.classList.toggle("status-error", isError);
}

/******************************************************************************/
/*!
  @brief    Enables controls after their current values have loaded.
*/
/******************************************************************************/
function enableControls() {
    powerToggleElem.disabled = false;
    brightnessRangeElem.disabled = false;
}

/******************************************************************************/
/*!
  @brief    Loads power and brightness from the firmware endpoints.
*/
/******************************************************************************/
async function loadDeviceState() {
    try {
        const [stateText, brightnessText] = await Promise.all([
            requestText("/get_state"),
            requestText("/get_brightness")
        ]);
        const brightness = Number.parseInt(brightnessText, 10);
        if (!Number.isFinite(brightness)) {
            throw new Error(getText("loadDeviceError"));
        }

        powerToggleElem.checked = stateText.trim() === "1";
        brightnessRangeElem.value = String(
            Math.min(1023, Math.max(0, brightness))
        );
        updateBrightnessOutput();
        setDeviceStatus("statusOnline");
        enableControls();
    } catch (error) {
        setDeviceStatus("statusError", true);
        deviceTitleKey = "loadDeviceError";
        deviceStatusFieldElem.title = getText(deviceTitleKey);
    }
}

/******************************************************************************/
/*!
  @brief    Sends the selected power state to the firmware endpoint.
*/
/******************************************************************************/
async function updatePower() {
    const previousState = !powerToggleElem.checked;
    powerToggleElem.disabled = true;
    try {
        const state = powerToggleElem.checked ? 1 : 0;
        await requestText(`/set_power?state=${state}`);
    } catch (error) {
        powerToggleElem.checked = previousState;
        setDeviceStatus("statusError", true);
        deviceTitleKey = "updateDeviceError";
        deviceStatusFieldElem.title = getText(deviceTitleKey);
    } finally {
        powerToggleElem.disabled = false;
    }
}

/******************************************************************************/
/*!
  @brief    Sends the selected brightness to the firmware endpoint.
*/
/******************************************************************************/
async function updateBrightness() {
    brightnessRangeElem.disabled = true;
    try {
        const brightness = encodeURIComponent(brightnessRangeElem.value);
        await requestText(`/set_brightness?brightness=${brightness}`);
    } catch (error) {
        setDeviceStatus("statusError", true);
        deviceTitleKey = "updateDeviceError";
        deviceStatusFieldElem.title = getText(deviceTitleKey);
    } finally {
        brightnessRangeElem.disabled = false;
    }
}

/******************************************************************************/
/*!
  @brief    Updates the displayed brightness percentage.
*/
/******************************************************************************/
function updateBrightnessOutput() {
    brightnessOutputElem.value = formatBrightness(
        Number(brightnessRangeElem.value)
    );
}

/******************************************************************************/
/*!
  @brief    Refreshes dynamic dashboard text after a language change.
*/
/******************************************************************************/
function updateDynamicLanguage() {
    setDeviceStatus(deviceStatusKey, deviceStatusIsError);
    if (deviceTitleKey !== "") {
        deviceStatusFieldElem.title = getText(deviceTitleKey);
    }
}

initializePage("indexTitle", "dashboard");
updateBrightnessOutput();
powerToggleElem.addEventListener("change", updatePower);
brightnessRangeElem.addEventListener("input", updateBrightnessOutput);
brightnessRangeElem.addEventListener("change", updateBrightness);
document.addEventListener("languagechange", updateDynamicLanguage);
loadDeviceState();
