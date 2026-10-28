/******************************************************************************/
/*
 * File:    network_config.js
 * Author:  Luke de Munk
 *
 * Brief:   Validates and submits wireless network configuration.
 */
/******************************************************************************/
import { getText, initializePage, requestText } from "../base.js";

const INVALID_SSID_RE = /[$*|~=\[\]:;#"'<>?,./]+/;
const networkFormElem = document.getElementById("networkForm");
const networkSsidInputElem = document.getElementById("networkSsidInput");
const networkPasswordInputElem = document.getElementById(
    "networkPasswordInput"
);
const networkMessageFieldElem = document.getElementById("networkMessageField");
const networkDialogElem = document.getElementById("networkDialog");
const networkCancelBtnElem = document.getElementById("networkCancelBtn");
const networkConfirmBtnElem = document.getElementById("networkConfirmBtn");
let networkMessageKey = "";
let networkMessageIsError = true;

/******************************************************************************/
/*!
  @brief    Shows a localized validation or submission message.
  @param    key                 Message translation key
  @param    isError             Whether the message represents an error
*/
/******************************************************************************/
function showNetworkMessage(key, isError = true) {
    networkMessageKey = key;
    networkMessageIsError = isError;
    networkMessageFieldElem.textContent = getText(key);
    networkMessageFieldElem.className = isError
        ? "message message-error"
        : "message message-success";
    networkMessageFieldElem.hidden = false;
}

/******************************************************************************/
/*!
  @brief    Marks a field invalid and moves keyboard focus to it.
  @param    inputElem           Invalid input field
  @param    key                 Validation message translation key
  @returns                      Always false for validation returns
*/
/******************************************************************************/
function rejectInput(inputElem, key) {
    inputElem.setAttribute("aria-invalid", "true");
    showNetworkMessage(key);
    inputElem.focus();
    return false;
}

/******************************************************************************/
/*!
  @brief    Validates both network fields without changing parameters.
  @returns                      True when the form can be submitted
*/
/******************************************************************************/
function validateNetworkForm() {
    const ssid = networkSsidInputElem.value.trim();
    const password = networkPasswordInputElem.value;
    networkSsidInputElem.removeAttribute("aria-invalid");
    networkPasswordInputElem.removeAttribute("aria-invalid");
    networkMessageFieldElem.hidden = true;

    if (ssid === "") {
        return rejectInput(networkSsidInputElem, "required");
    }
    if (INVALID_SSID_RE.test(ssid)) {
        return rejectInput(networkSsidInputElem, "ssidInvalid");
    }
    if (ssid.length > 50) {
        return rejectInput(networkSsidInputElem, "ssidTooLong");
    }
    if (password === "") {
        return rejectInput(networkPasswordInputElem, "required");
    }
    if (password.length < 8) {
        return rejectInput(networkPasswordInputElem, "passwordTooShort");
    }
    if (password.length > 64) {
        return rejectInput(networkPasswordInputElem, "passwordTooLong");
    }
    return true;
}

/******************************************************************************/
/*!
  @brief    Sends URL-encoded credentials to the configuration endpoint.
*/
/******************************************************************************/
async function saveNetworkConfiguration() {
    networkDialogElem.close();
    networkConfirmBtnElem.disabled = true;
    const formData = new URLSearchParams(new FormData(networkFormElem));
    try {
        await requestText("/configure_network", {
            method: "POST",
            headers: {
                "Content-Type":
                    "application/x-www-form-urlencoded;charset=UTF-8"
            },
            body: formData.toString()
        });
        showNetworkMessage("networkSaved", false);
        const submitBtnElem = networkFormElem.querySelector(
            "button[type=\"submit\"]"
        );
        submitBtnElem.disabled = true;
    } catch (error) {
        showNetworkMessage("networkError");
        networkConfirmBtnElem.disabled = false;
    }
}

/******************************************************************************/
/*!
  @brief    Validates submission before showing the confirmation dialog.
  @param    event               Native form submission event
*/
/******************************************************************************/
function confirmNetworkConfiguration(event) {
    event.preventDefault();
    if (validateNetworkForm()) {
        networkDialogElem.showModal();
    }
}

/******************************************************************************/
/*!
  @brief    Closes the network confirmation dialog.
*/
/******************************************************************************/
function closeNetworkDialog() {
    networkDialogElem.close();
}

/******************************************************************************/
/*!
  @brief    Moves focus to the password field when Enter is pressed.
  @param    event               Native keyboard event
*/
/******************************************************************************/
function focusPasswordOnEnter(event) {
    if (event.key === "Enter") {
        event.preventDefault();
        networkPasswordInputElem.focus();
    }
}

/******************************************************************************/
/*!
  @brief    Refreshes the network message after a language change.
*/
/******************************************************************************/
function updateDynamicLanguage() {
    if (networkMessageKey !== "") {
        showNetworkMessage(networkMessageKey, networkMessageIsError);
    }
}

initializePage("networkTitle");
networkFormElem.addEventListener("submit", confirmNetworkConfiguration);
networkCancelBtnElem.addEventListener("click", closeNetworkDialog);
networkConfirmBtnElem.addEventListener("click", saveNetworkConfiguration);
networkSsidInputElem.addEventListener("keydown", focusPasswordOnEnter);
document.addEventListener("languagechange", updateDynamicLanguage);
