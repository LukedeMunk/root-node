/******************************************************************************/
/*
 * File:    ShellCommandParser.h
 * Author:  Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Tokenizes bounded serial shell command lines in place.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#ifndef SHELL_COMMAND_PARSER_H_
#define SHELL_COMMAND_PARSER_H_

#include <stdint.h>

#define SHELL_MAX_ARGUMENTS     8
#define SHELL_MAX_ITEMS         (1 + SHELL_MAX_ARGUMENTS)

enum ShellParseResult {
    shellParseOk,
    shellParseEmpty,
    shellParseTooManyArguments,
    shellParseUnterminatedQuote,
    shellParseTrailingEscape
};

struct ShellCommand {
    char* items[SHELL_MAX_ITEMS];
    uint8_t itemCount;
};

class ShellCommandParser {
    public:
        ShellParseResult parse(char* commandLine, ShellCommand& command) const;
};

#endif  // SHELL_COMMAND_PARSER_H_
