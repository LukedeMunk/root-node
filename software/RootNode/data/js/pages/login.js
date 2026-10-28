/******************************************************************************/
/*
 * File:    login.js
 * Author:  Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Custom device login and safe post-login redirection.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
import {
    getText,
    initializePage,
    refreshAuthenticationStatus
} from "../base.js";

const loginFormElem = document.getElementById("loginForm");
const usernameTxtElem = document.getElementById("usernameTxt");
const loginPasswordTxtElem = document.getElementById("loginPasswordTxt");
const loginBtnElem = document.getElementById("loginBtn");
const loginMessageFieldElem = document.getElementById("loginMessageField");
let loginMessageKey = "";
let loginMessageIsError = false;
let redirectPath = "/";

/******************************************************************************/
/*!
  @brief    Returns a safe local redirect from the current query string.
  @returns                      Valid local path or dashboard path
*/
/******************************************************************************/
function getRedirectPath() {
    const candidate = new URLSearchParams(window.location.search).get(
        "redirect"
    );
    if (candidate === null || !candidate.startsWith("/") ||
        candidate.startsWith("//") || candidate.includes("\\")) {
        return "/";
    }
    const target = new URL(candidate, window.location.origin);
    if (target.origin !== window.location.origin ||
        target.pathname === "/login") {
        return "/";
    }
    return target.pathname + target.search + target.hash;
}

/******************************************************************************/
/*!
  @brief    Shows a localized login status message.
  @param    key                 Translation key
  @param    isError             Whether the status is an error
*/
/******************************************************************************/
function showLoginMessage(key, isError = false) {
    loginMessageKey = key;
    loginMessageIsError = isError;
    loginMessageFieldElem.textContent = getText(key);
    loginMessageFieldElem.className = isError
        ? "message message-error"
        : "message message-success";
    loginMessageFieldElem.hidden = false;
}

/******************************************************************************/
/*!
  @brief    Redirects to the validated post-login destination.
*/
/******************************************************************************/
function redirectAfterLogin() {
    window.location.assign(redirectPath);
}

/******************************************************************************/
/*!
  @brief    Authenticates existing credentials without browser prompts.
  @param    event               Native login form event
*/
/******************************************************************************/
async function submitLogin(event) {
    event.preventDefault();
    loginBtnElem.disabled = true;
    usernameTxtElem.disabled = true;
    loginPasswordTxtElem.disabled = true;
    showLoginMessage("loginLoading");

    const data = new URLSearchParams();
    data.set("username", usernameTxtElem.value);
    data.set("password", loginPasswordTxtElem.value);
    
    try {
        const response = await window.fetch("/login", {
            method: "POST",
            headers: {
                "Content-Type":
                    "application/x-www-form-urlencoded;charset=UTF-8"
            },
            body: data.toString()
        });
        await refreshAuthenticationStatus();
        if (!response.ok) {
            showLoginMessage(
                response.status === 401 ? "loginError" : "loginRequestError",
                true
            );
            return;
        }
        showLoginMessage("loginSuccess");
        window.setTimeout(redirectAfterLogin, 250);
    } catch (error) {
        showLoginMessage("loginRequestError", true);
    } finally {
        loginBtnElem.disabled = false;
        usernameTxtElem.disabled = false;
        loginPasswordTxtElem.disabled = false;
    }
}

/******************************************************************************/
/*!
  @brief    Refreshes the active login status message after language changes.
*/
/******************************************************************************/
function updateDynamicLanguage() {
    if (loginMessageKey !== "") {
        showLoginMessage(loginMessageKey, loginMessageIsError);
    }
}

redirectPath = getRedirectPath();
initializePage("loginTitle");
loginFormElem.addEventListener("submit", submitLogin);
document.addEventListener("languagechange", updateDynamicLanguage);
