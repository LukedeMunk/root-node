/******************************************************************************/
/*
 * File:    ShellCommandParser.cpp
 * Author:  Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Tokenizes bounded serial shell command lines in place.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#include "ShellCommandParser.h"

/******************************************************************************/
/*!
  @brief    Tokenizes a command line without allocating additional strings.
  @param    commandLine         Writable, zero-terminated input buffer
  @param    command             Receives pointers into commandLine
  @returns  ShellParseResult    Parse status
*/
/******************************************************************************/
ShellParseResult ShellCommandParser::parse(char* commandLine, ShellCommand& command) const {
    char* readPosition = commandLine;
    char* writePosition = commandLine;
    char quote = '\0';
    bool isEscaped = false;
    bool isTokenActive = false;

    command.itemCount = 0;

    while (*readPosition != '\0') {
        char character = *readPosition++;

        if (isEscaped) {
            if (!isTokenActive) {
                if (command.itemCount >= SHELL_MAX_ITEMS) {
                    return shellParseTooManyArguments;
                }
                command.items[command.itemCount++] = writePosition;
                isTokenActive = true;
            }
            *writePosition++ = character;
            isEscaped = false;
            continue;
        }

        /* Preserve the challenge's hexadecimal byte notation (for example
         * \\x88) while treating other backslashes as shell escapes. */
        if (character == '\\' && readPosition[0] == 'x' &&
            readPosition[1] != '\0' && readPosition[2] != '\0' &&
            ((readPosition[1] >= '0' && readPosition[1] <= '9') ||
             (readPosition[1] >= 'a' && readPosition[1] <= 'f') ||
             (readPosition[1] >= 'A' && readPosition[1] <= 'F')) &&
            ((readPosition[2] >= '0' && readPosition[2] <= '9') ||
             (readPosition[2] >= 'a' && readPosition[2] <= 'f') ||
             (readPosition[2] >= 'A' && readPosition[2] <= 'F'))) {

            if (!isTokenActive) {
                if (command.itemCount >= SHELL_MAX_ITEMS) {
                    return shellParseTooManyArguments;
                }
                command.items[command.itemCount++] = writePosition;
                isTokenActive = true;
            }
            *writePosition++ = character;
            continue;
        }

        if (character == '\\') {
            isEscaped = true;
            continue;
        }

        if (quote != '\0') {
            if (character == quote) {
                quote = '\0';
            } else {
                *writePosition++ = character;
            }
            continue;
        }

        if (character == '\'' || character == '"') {
            if (!isTokenActive) {
                if (command.itemCount >= SHELL_MAX_ITEMS) {
                    return shellParseTooManyArguments;
                }

                command.items[command.itemCount++] = writePosition;
                isTokenActive = true;
            }

            quote = character;
            continue;
        }

        if (character == ' ' || character == '\t') {
            if (isTokenActive) {
                *writePosition++ = '\0';
                isTokenActive = false;
            }
            continue;
        }

        if (!isTokenActive) {
            if (command.itemCount >= SHELL_MAX_ITEMS) {
                return shellParseTooManyArguments;
            }

            command.items[command.itemCount++] = writePosition;
            isTokenActive = true;
        }
        *writePosition++ = character;
    }

    if (isEscaped) {
        return shellParseTrailingEscape;
    }
    
    if (quote != '\0') {
        return shellParseUnterminatedQuote;
    }

    if (isTokenActive) {
        *writePosition = '\0';
    }

    if (command.itemCount == 0) {
        return shellParseEmpty;
    }

    return shellParseOk;
}
