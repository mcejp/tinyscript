#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TFFUNC
#define TFFUNC
#endif

#define TFMAX(a_, b_) ((a_)>(b_)?(a_):(b_))
#define TFMIN(a_, b_) ((a_)<(b_)?(a_):(b_))

#define TF_FMTCLASS_C (-2)

enum {
    TOKENCLASS_CHAR, TOKENCLASS_NUMBER, TOKENCLASS_SEQUENCE, TOKENCLASS_STRING, TOKENCLASS_WORD
};

#define TOKEN_CLASS_CHAR(purpose_, name_, c_)\
    {TOKENCLASS_CHAR, (purpose_), (name_), 0,\
        (c_),\
        NULL,\
        -1, -1, -1,\
        NULL, 0,\
        -1, -1, -1, -1\
    }

#define TOKEN_CLASS_NUMBER(purpose_, name_, decimal_c_, decimal_name_, opening_c_, base_)\
    {TOKENCLASS_NUMBER, (purpose_), (name_), 0,\
        (opening_c_),\
        NULL,\
        (base_), (decimal_c_), (decimal_name_),\
        NULL, 0,\
        -1, -1, -1, -1\
    }

#define TOKEN_CLASS_SEQUENCE(purpose_, name_, ranges_, num_ranges_)\
    {TOKENCLASS_SEQUENCE, (purpose_), (name_), 0,\
        -1,\
        NULL,\
        -1, -1, -1,\
        (ranges_), (num_ranges_),\
        -1, -1, -1, -1\
    }

#define TOKEN_CLASS_STRING(purpose_, name_, oq_, cq_, esc_, fmtclass_)\
    {TOKENCLASS_STRING, (purpose_), (name_), 0,\
        -1,\
        NULL,\
        -1, -1, -1,\
        NULL, 0,\
        (oq_), (cq_), (esc_), (fmtclass_)\
    }

#define TOKEN_CLASS_WORD(purpose_, name_, word_)\
    {TOKENCLASS_WORD, (purpose_), (name_), 0,\
        -1,\
        (word_),\
        -1, -1, -1,\
        NULL, 0,\
        -1, -1, -1, -1\
    }

enum {
    CLASS_COMMENT,
    CLASS_TOKEN
};

typedef struct AstNode AstNode_t;
typedef struct Token Token_t;
typedef struct Tokenbuffer Tokenbuffer_t;
typedef struct Tokenclass Tokenclass_t;
typedef struct Tokenfactory Tokenfactory_t, Tf_t;

typedef void (*TF_ERROR_CB)(Tf_t* tf, int err);
typedef void (*TF_TOKEN_CB)(Tf_t* tf);

struct Tokenclass
{
    int type, purpose, name, isdynalloc;

    /* char (utf8) */
    int c;

    /* keyword */
    char* word;

    /* number */
    int base, decimal_c, decimal_name;

    /* SEQUENCE */
    uint32_t* ranges;
    size_t num_ranges;

    /* string */
    int oq, cq, esc, fmtclass;
};

struct Token
{
    int type, name, line, indent, pos_in_line;

    int c;
    int32_t number;
    double decimal;

    uint8_t* text;
    size_t text_capacity;
};

struct Tokenfactory
{
    Tokenclass_t* classes;
    size_t classes_num, classes_max;

    uint32_t* whitespaces;
    size_t whitespaces_num, whitespaces_max;

    int ownsinput;
    uint8_t* input;
    size_t inputlen, inputpos;
    int c;
    int line, line_begin, indent, pos_in_line;

    struct Token token;

    int stop_parse;

    TF_TOKEN_CB on_token;
};

struct AstNode
{
    int name;

    struct Token token;

    struct AstNode *left, *right, **children;
    size_t children_num, children_max;

    void* cust_data;
    void (*on_release)(AstNode_t* node);
};

struct Tokenbuffer
{
    Tokenfactory_t* tf;

    struct Token* tokens;
    size_t tokens_num, tokens_max;
};

TFFUNC void tokenclass_add(Tf_t* tf, const Tokenclass_t* classes_in, size_t count);
TFFUNC void tf_whitespace_add(Tf_t* tf, const uint32_t* chars_in, size_t count);
TFFUNC void tf_whitespace_common(Tf_t* tf);

TFFUNC void tf_clear_token(Token_t* token);
TFFUNC void tf_copy_token(Token_t* token, const Token_t* src);
TFFUNC void tf_input_string(Tf_t* tf, const uint8_t* string, intptr_t length, int makecopy);
TFFUNC void tf_move_token(Token_t* token, Token_t* src);
TFFUNC void tf_on_token(Tf_t* tf, TF_TOKEN_CB on_token);
TFFUNC int tf_parse_token(Tf_t* tf);
TFFUNC void tf_parse(Tf_t* tf);
TFFUNC void tf_release_token(Token_t* token);
TFFUNC void tf_stop_parse(Tf_t* tf);

TFFUNC char* tf_load_file(const char *filename, size_t *size_out);

TFFUNC Tf_t* tokenfactory();
TFFUNC void tokenfactory_del(Tf_t** tf_p);

/******************************************************************************/

TFFUNC int tb_accept(Tokenbuffer_t* tbuf, int name);
TFFUNC int tb_accept_sequence(Tokenbuffer_t* tbuf, int name, const char* text);
TFFUNC Token_t* tb_check(Tokenbuffer_t* tbuf, int name);
TFFUNC Token_t* tb_check_sequence(Tokenbuffer_t* tbuf, int name, const char* text);
TFFUNC Token_t* tb_current(Tokenbuffer_t* tbuf);
TFFUNC void tb_drop(Tokenbuffer_t* tbuf);
TFFUNC void tb_skip(Tokenbuffer_t *tbuf, int name);

TFFUNC Tokenbuffer_t* tokenbuffer(Tokenfactory_t* tf);
TFFUNC void tokenbuffer_del(Tokenbuffer_t** tbuf_p);

/******************************************************************************/

TFFUNC AstNode_t* ast_node(int name, Token_t* token);
TFFUNC AstNode_t* ast_node_2(int name, AstNode_t* left, AstNode_t* right);
TFFUNC void ast_release_node(AstNode_t** node_p);
TFFUNC void ast_addchild(AstNode_t* node, AstNode_t* child);
TFFUNC void ast_print(AstNode_t* node, int indent, const char** names, size_t num_names);
TFFUNC int ast_serialize_node(AstNode_t* node, FILE *f);
TFFUNC int ast_serialize(AstNode_t* node, const char* filename);
