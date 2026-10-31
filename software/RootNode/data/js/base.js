/******************************************************************************/
/*
 * File:    base.js
 * Author:  Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Shared localization, theme, navigation and request utilities.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
import { ENGLISH_TRANSLATIONS } from "./language_pack_english.js";
import { DUTCH_TRANSLATIONS } from "./language_pack_dutch.js";

const THEME_STORAGE_KEY = "RootNodeTheme";
const LANGUAGE_STORAGE_KEY = "RootNodeLanguage";
const AUTHENTICATION_STORAGE_KEY = "RootNode.authenticationStatus";
const LEGACY_USER_ROLE_STORAGE_KEY = "userRole";
const DEFAULT_THEME = "dark-blue";
const DEFAULT_LANGUAGE = "en";
const THEMES = Object.freeze([
    "dark-blue",
    "dark-green",
    "light-blue",
    "light-green"
]);
const LANGUAGES = Object.freeze(["en", "nl"]);
const AUTHENTICATION_ROLES = Object.freeze(["guest", "user", "admin"]);
const LANGUAGE_PACKS = Object.freeze({
    en: ENGLISH_TRANSLATIONS,
    nl: DUTCH_TRANSLATIONS
});
let activeLanguage = DEFAULT_LANGUAGE;
let activeTitleKey = "appName";
let authenticationStatus = Object.freeze({
    authenticated: false,
    role: "guest"
});

/******************************************************************************/
/*!
  @brief    Reads a supported value from local storage.
  @param    storageKey          Local storage key
  @param    supportedValues     Supported stored values
  @param    defaultValue        Fallback value
  @returns                      Stored or fallback value
*/
/******************************************************************************/
function getStoredValue(storageKey, supportedValues, defaultValue) {
    try {
        const storedValue = window.localStorage.getItem(storageKey);
        return supportedValues.includes(storedValue)
            ? storedValue
            : defaultValue;
    } catch (error) {
        return defaultValue;
    }
}

/******************************************************************************/
/*!
  @brief    Stores a preference when local storage is available.
  @param    storageKey          Local storage key
  @param    value               Value to store
*/
/******************************************************************************/
function storeValue(storageKey, value) {
    try {
        window.localStorage.setItem(storageKey, value);
    } catch (error) {
        return;
    }
}

/******************************************************************************/
/*!
  @brief    Returns text from the active language pack.
  @param    key                 Translation key
  @returns                      Localized text or an empty string
*/
/******************************************************************************/
export function getText(key) {
    return LANGUAGE_PACKS[activeLanguage][key] ?? "";
}

/******************************************************************************/
/*!
  @brief    Returns the role reported by the authentication status route.
  @returns                      Guest, user or administrator role
*/
/******************************************************************************/
export function getAuthenticationRole() {
    return authenticationStatus.role;
}

/******************************************************************************/
/*!
  @brief    Validates an authentication status object from the server.
  @param    status              Authentication status candidate
  @returns                      True when the status has the expected shape
*/
/******************************************************************************/
function isAuthenticationStatusValid(status) {
    return status !== null &&
        typeof status === "object" &&
        typeof status.authenticated === "boolean" &&
        AUTHENTICATION_ROLES.includes(status.role) &&
        status.authenticated === (status.role !== "guest");
}

/******************************************************************************/
/*!
  @brief    Reads the cached UI authentication status for this tab.
  @returns                      Valid cached status or null
*/
/******************************************************************************/
function getStoredAuthenticationStatus() {
    try {
        const storedStatus = window.sessionStorage.getItem(
            AUTHENTICATION_STORAGE_KEY
        );
        if (storedStatus === null) {
            return null;
        }
        const parsedStatus = JSON.parse(storedStatus);
        return isAuthenticationStatusValid(parsedStatus)
            ? parsedStatus
            : null;
    } catch (error) {
        return null;
    }
}

/******************************************************************************/
/*!
  @brief    Removes cached authentication UI state.
*/
/******************************************************************************/
export function clearAuthenticationStatus() {
    authenticationStatus = Object.freeze({
        authenticated: false,
        role: "guest"
    });
    try {
        window.sessionStorage.removeItem(AUTHENTICATION_STORAGE_KEY);
        window.sessionStorage.removeItem(LEGACY_USER_ROLE_STORAGE_KEY);
        window.localStorage.removeItem(LEGACY_USER_ROLE_STORAGE_KEY);
    } catch (error) {
        return;
    }
}

/******************************************************************************/
/*!
  @brief    Stores and applies server-validated authentication UI state.
  @param    status              Valid server authentication response
*/
/******************************************************************************/
export function setAuthenticationStatus(status) {
    if (!isAuthenticationStatusValid(status)) {
        clearAuthenticationStatus();
        return;
    }
    authenticationStatus = Object.freeze({
        authenticated: status.authenticated,
        role: status.role
    });
    try {
        window.sessionStorage.setItem(
            AUTHENTICATION_STORAGE_KEY,
            JSON.stringify(authenticationStatus)
        );
    } catch (error) {
        return;
    }
}

/******************************************************************************/
/*!
  @brief    Applies a theme and synchronizes the theme menu state.
  @param    theme               Theme identifier to apply
  @param    persist             Whether to store the selection locally
*/
/******************************************************************************/
function applyTheme(theme, persist = true) {
    const selectedTheme = THEMES.includes(theme) ? theme : DEFAULT_THEME;
    document.documentElement.dataset.theme = selectedTheme;

    for (const optionElem of document.querySelectorAll("[data-theme-option]")) {
        const isSelected = optionElem.dataset.themeOption === selectedTheme;
        optionElem.setAttribute("aria-checked", String(isSelected));
    }

    if (persist) {
        storeValue(THEME_STORAGE_KEY, selectedTheme);
    }
}

/******************************************************************************/
/*!
  @brief    Applies active language text to marked document elements.
*/
/******************************************************************************/
function loadLanguage() {
    document.documentElement.lang = activeLanguage;
    for (const element of document.querySelectorAll("[data-i18n]")) {
        const value = getText(element.dataset.i18n);
        if (value !== "") {
            element.textContent = value;
        }
    }

    const placeholderElems = document.querySelectorAll("[data-i18n-placeholder]");
    for (const element of placeholderElems) {
        const value = getText(element.dataset.i18nPlaceholder);
        if (value !== "") {
            element.setAttribute("placeholder", value);
        }
    }

    const ariaLabelElems = document.querySelectorAll("[data-i18n-aria-label]");
    for (const element of ariaLabelElems) {
        const value = getText(element.dataset.i18nAriaLabel);
        if (value !== "") {
            element.setAttribute("aria-label", value);
        }
    }

    const pageTitle = getText(activeTitleKey) || getText("appName");
    document.title = `${pageTitle} | ${getText("appName")}`;
}

/******************************************************************************/
/*!
  @brief    Closes a shared navigation dropdown.
  @param    buttonId            Dropdown button identifier
  @param    optionsId           Dropdown list identifier
*/
/******************************************************************************/
function closeMenu(buttonId, optionsId) {
    const buttonElem = document.getElementById(buttonId);
    const optionsElem = document.getElementById(optionsId);

    if (!buttonElem || !optionsElem) {
        return;
    }

    buttonElem.setAttribute("aria-expanded", "false");
    optionsElem.hidden = true;
}

/******************************************************************************/
/*!
  @brief    Toggles a shared navigation dropdown.
  @param    buttonId            Dropdown button identifier
  @param    optionsId           Dropdown list identifier
*/
/******************************************************************************/
function toggleMenu(buttonId, optionsId) {
    const buttonElem = document.getElementById(buttonId);
    const optionsElem = document.getElementById(optionsId);

    if (!buttonElem || !optionsElem) {
        return;
    }

    const shouldOpen = optionsElem.hidden;
    buttonElem.setAttribute("aria-expanded", String(shouldOpen));
    optionsElem.hidden = !shouldOpen;

    if (shouldOpen) {
        optionsElem.querySelector("[aria-checked=\"true\"]")?.focus();
    }
}

/******************************************************************************/
/*!
  @brief    Applies a theme selected in the theme menu.
  @param    event               Native button click event
*/
/******************************************************************************/
function selectTheme(event) {
    applyTheme(event.currentTarget.dataset.themeOption);
    closeMenu("themeBtn", "themeOptions");
    document.getElementById("themeBtn")?.focus();
}

/******************************************************************************/
/*!
  @brief    Applies a language selected in the language menu.
  @param    event               Native button click event
*/
/******************************************************************************/
function selectLanguage(event) {
    const language = event.currentTarget.dataset.languageOption;
    activeLanguage = LANGUAGES.includes(language) ? language : DEFAULT_LANGUAGE;

    storeValue(LANGUAGE_STORAGE_KEY, activeLanguage);
    synchronizeLanguageOptions();
    loadLanguage();
    closeMenu("languageBtn", "languageOptions");

    document.getElementById("languageBtn")?.focus();
    document.dispatchEvent(new CustomEvent("languagechange"));
}

/******************************************************************************/
/*!
  @brief    Synchronizes language options with the active language.
*/
/******************************************************************************/
function synchronizeLanguageOptions() {
    const optionElems = document.querySelectorAll("[data-language-option]");

    for (const optionElem of optionElems) {
        const isSelected = optionElem.dataset.languageOption === activeLanguage;
        optionElem.setAttribute("aria-checked", String(isSelected));
    }
}

/******************************************************************************/
/*!
  @brief    Closes navigation menus after a click outside them.
  @param    event               Native document click event
*/
/******************************************************************************/
function closeMenusOnOutsideClick(event) {
    if (!event.target.closest(".theme-menu")) {
        closeMenu("themeBtn", "themeOptions");
    }

    if (!event.target.closest(".language-menu")) {
        closeMenu("languageBtn", "languageOptions");
    }
}

/******************************************************************************/
/*!
  @brief    Closes navigation menus when Escape is pressed.
  @param    event               Native keyboard event
*/
/******************************************************************************/
function closeMenusOnEscape(event) {
    if (event.key !== "Escape") {
        return;
    }

    closeMenu("themeBtn", "themeOptions");
    closeMenu("languageBtn", "languageOptions");
}

/******************************************************************************/
/*!
  @brief    Toggles the theme menu from its button.
*/
/******************************************************************************/
function toggleThemeMenu() {
    toggleMenu("themeBtn", "themeOptions");
}

/******************************************************************************/
/*!
  @brief    Toggles the language menu from its button.
*/
/******************************************************************************/
function toggleLanguageMenu() {
    toggleMenu("languageBtn", "languageOptions");
}

/******************************************************************************/
/*!
  @brief    Adds controls to the shared theme and language menus.
*/
/******************************************************************************/
function initializeMenus() {
    document.getElementById("themeBtn")?.addEventListener(
        "click", toggleThemeMenu
    );

    document.getElementById("languageBtn")?.addEventListener(
        "click", toggleLanguageMenu
    );

    for (const optionElem of document.querySelectorAll("[data-theme-option]")) {
        optionElem.addEventListener("click", selectTheme);
    }

    const languageOptionElems = document.querySelectorAll(
        "[data-language-option]"
    );

    for (const optionElem of languageOptionElems) {
        optionElem.addEventListener("click", selectLanguage);
    }

    document.addEventListener("click", closeMenusOnOutsideClick);
    document.addEventListener("keydown", closeMenusOnEscape);
}

/******************************************************************************/
/*!
  @brief    Marks the navigation link matching the active page.
  @param    activePage          Active navigation page identifier
*/
/******************************************************************************/
function setActiveNavigation(activePage) {
    const linkElems = document.querySelectorAll("[data-navigation-page]");

    for (const linkElem of linkElems) {
        if (linkElem.dataset.navigationPage === activePage) {
            linkElem.setAttribute("aria-current", "page");
        } else {
            linkElem.removeAttribute("aria-current");
        }
    }
}

/******************************************************************************/
/*!
  @brief    Returns the translation key for an authentication role.
  @param    role                Guest, user or administrator role
  @returns                      Role translation key
*/
/******************************************************************************/
function getRoleTextKey(role) {
    if (role === "admin") {
        return "roleAdmin";
    }

    if (role === "user") {
        return "roleUser";
    }

    return "roleGuest";
}

/******************************************************************************/
/*!
  @brief    Creates the navigation role indicator when it is absent.
  @param    authLinkElem        Shared authentication navigation link
  @returns                      Role output element
*/
/******************************************************************************/
function getAuthenticationRoleField(authLinkElem) {
    const existingFieldElem = document.getElementById("authRoleField");
    if (existingFieldElem) {
        return existingFieldElem;
    }
    
    const itemElem = document.createElement("li");
    const roleFieldElem = document.createElement("span");
    roleFieldElem.id = "authRoleField";
    roleFieldElem.className = "authentication-status";
    roleFieldElem.setAttribute("data-i18n-aria-label", "authRoleLabel");
    itemElem.append(roleFieldElem);
    authLinkElem.closest("li").before(itemElem);

    return roleFieldElem;
}

/******************************************************************************/
/*!
  @brief    Shows elements that are available to the current role.
*/
/******************************************************************************/
function updateRoleVisibility() {
    const roleElements = document.querySelectorAll("[data-role-required]");

    for (const roleElem of roleElements) {
        roleElem.hidden = roleElem.dataset.roleRequired !==
            authenticationStatus.role;
    }
}

/******************************************************************************/
/*!
  @brief    Applies authentication state to shared navigation controls.
*/
/******************************************************************************/
function applyAuthenticationStatus() {
    updateRoleVisibility();
    const authLinkElem = document.getElementById("authLink");
    if (!authLinkElem) {
        return;
    }

    const roleFieldElem = getAuthenticationRoleField(authLinkElem);
    const roleTextKey = getRoleTextKey(authenticationStatus.role);
    roleFieldElem.dataset.i18n = roleTextKey;
    roleFieldElem.textContent = getText(roleTextKey);
    roleFieldElem.setAttribute("aria-label", getText("authRoleLabel"));
    authLinkElem.dataset.i18n = authenticationStatus.authenticated
        ? "logout"
        : "login";
    authLinkElem.dataset.authenticationAction =
        authenticationStatus.authenticated ? "logout" : "login";
    const currentPath = window.location.pathname + window.location.search;
    authLinkElem.href = authenticationStatus.authenticated
        ? "/logout"
        : `/login?redirect=${encodeURIComponent(currentPath)}`;
    authLinkElem.textContent = getText(authLinkElem.dataset.i18n);

    document.dispatchEvent(new CustomEvent("authenticationchange", {
        detail: authenticationStatus
    }));
}

/******************************************************************************/
/*!
  @brief    Loads authentication status from the firmware.
  @returns                      Server-validated authentication status
*/
/******************************************************************************/
export async function refreshAuthenticationStatus() {
    const response = await window.fetch("/get_authentication_status", {
        cache: "no-store"
    });

    if (!response.ok) {
        throw new Error(`Request failed with status ${response.status}`);
    }

    const status = await response.json();
    setAuthenticationStatus(status);
    applyAuthenticationStatus();

    return authenticationStatus;
}

/******************************************************************************/
/*!
  @brief    Ends the current session from the navigation action.
  @param    event               Native authentication link event
*/
/******************************************************************************/
async function handleAuthenticationAction(event) {
    const authLinkElem = event.currentTarget;
    if (authLinkElem.dataset.authenticationAction !== "logout") {
        return;
    }

    event.preventDefault();
    try {
        const response = await window.fetch("/logout", {
            method: "POST"
        });

        if (!response.ok) {
            throw new Error(`Request failed with status ${response.status}`);
        }

        await refreshAuthenticationStatus();
    } catch (error) {
        clearAuthenticationStatus();
    } finally {
        window.location.assign("/");
    }
}

/******************************************************************************/
/*!
  @brief    Initializes authentication from session storage or the server.
*/
/******************************************************************************/
async function initializeAuthentication() {
    const authLinkElem = document.getElementById("authLink");
    if (!authLinkElem) {
        return;
    }

    authLinkElem.addEventListener("click", handleAuthenticationAction);
    if (window.location.pathname === "/login") {
        clearAuthenticationStatus();
        applyAuthenticationStatus();
        return;
    }
    const storedStatus = getStoredAuthenticationStatus();
    if (storedStatus !== null) {
        authenticationStatus = Object.freeze(storedStatus);
        applyAuthenticationStatus();
        return;
    }

    try {
        await refreshAuthenticationStatus();
    } catch (error) {
        setAuthenticationStatus({
            authenticated: false,
            role: "guest"
        });
        applyAuthenticationStatus();
    }
}

/******************************************************************************/
/*!
  @brief    Initializes text, title, navigation and shared controls.
  @param    titleKey            Translation key for the browser title
  @param    activePage          Active navigation page identifier
*/
/******************************************************************************/
export function initializePage(titleKey, activePage = "") {
    activeTitleKey = titleKey;
    activeLanguage = getStoredValue(
        LANGUAGE_STORAGE_KEY,
        LANGUAGES,
        DEFAULT_LANGUAGE
    );

    loadLanguage();
    synchronizeLanguageOptions();
    setActiveNavigation(activePage);
    initializeMenus();
    applyTheme(getStoredValue(THEME_STORAGE_KEY, THEMES, DEFAULT_THEME), false);
    initializeAuthentication();
}

/******************************************************************************/
/*!
  @brief    Requests a response and normalizes firmware HTTP failures.
  @param    url                 Endpoint to request
  @param    options             Fetch options
  @returns                      Successful response object
*/
/******************************************************************************/
export async function requestResponse(url, options = {}) {
    const response = await window.fetch(url, options);
    if (response.status === 401 || response.status === 403) {
        clearAuthenticationStatus();
        try {
            await refreshAuthenticationStatus();
        } catch (error) {
            applyAuthenticationStatus();
        }
    }

    if (!response.ok) {
        const requestError = new Error(`Request failed with status ${response.status}`);
        requestError.status = response.status;
        try {
            const errorResponse = await response.clone().json();
            requestError.code = errorResponse.error || "";
        } catch (error) {
            requestError.code = "";
        }
        throw requestError;
    }

    return response;
}

/******************************************************************************/
/*!
  @brief    Requests text and normalizes firmware HTTP failures.
  @param    url                 Endpoint to request
  @param    options             Fetch options
  @returns                      Response body as text
*/
/******************************************************************************/
export async function requestText(url, options = {}) {
    const response = await requestResponse(url, options);
    return response.text();
}

applyTheme(getStoredValue(THEME_STORAGE_KEY, THEMES, DEFAULT_THEME), false);
