#pragma once

#include <tokenfactory.h>

enum {
    SN_ADD,
    SN_APPEND,
    SN_ASSIGN,
    SN_BIN_AND,
    SN_BIN_OR,
    SN_BLOCK,
    SN_BREAK,
    SN_CALL,
    SN_DIVIDE,
    SN_EQUALS,
    SN_FALSE,
    SN_FUNCTION,
    SN_IDENT,
    SN_IF,
    SN_INDEX,
    SN_INT,
    SN_ITERATE,
    SN_LIST,
    SN_MEMBER,
    SN_MULTIPLY,
    SN_NOT,
    SN_NOT_EQUALS,
    SN_NULL,
    SN_OBJECT,
    SN_REAL,
    SN_RETURN,
    SN_SCRIPT,
    SN_STRING,
    SN_SUBTRACT,
    SN_TRUE,
    SN_WHILE
};

AstNode_t* parse(const char* filename, const char* script);

typedef struct
{
    char** globals;
    size_t num_globals;
}
ast_properties_t;