
#include "parse.h"

#include <stdarg.h>
#include <stdio.h>

enum { ST_APPEND, ST_ASSIGN, ST_BIN_AND, ST_BIN_OR, ST_COLON, ST_COMMA, ST_DIVIDE, ST_EQUALS, ST_GLOBAL, ST_IDENT, ST_INT, ST_LBRACKET, ST_LCURLY, ST_LSQUARE,
        ST_NEWLINE, ST_NOT, ST_NOT_EQUALS, ST_MINUS, ST_MULTIPLY, ST_PERIOD, ST_PLUS, ST_RBRACKET, ST_RCURLY, ST_REAL, ST_RSQUARE, ST_STRINGLIT };

static const char* node_names[] = { "ADD", "APPEND", "ASSIGN", "BIN_AND", "BIN_OR", "BLOCK",
    "BREAK", "CALL", "DIVIDE", "EQUALS", "FALSE", "FUNCTION",
    "IDENT", "IF", "INDEX", "INT", "ITERATE", "LIST", "MEMBER", "MULTIPLY", "NOT", "NOT_EQUALS", "NULL", "OBJECT",
    "REAL", "RETURN", "SCRIPT", "STRING", "SUBTRACT", "TRUE", "WHILE" };

static const uint32_t ident_ranges[] = {
    'a', 'z',
    'A', 'Z',
    '0', '9',
    '_', '_'
};

static const Tokenclass_t script_tokens[] = {
    TOKEN_CLASS_STRING(CLASS_COMMENT, -1,           '#', '\n', -1, -1),
    TOKEN_CLASS_WORD(CLASS_TOKEN, ST_APPEND,        ".."),
    TOKEN_CLASS_WORD(CLASS_TOKEN, ST_EQUALS,        "=="),
    TOKEN_CLASS_WORD(CLASS_TOKEN, ST_GLOBAL,        "global"),
    TOKEN_CLASS_WORD(CLASS_TOKEN, ST_NOT_EQUALS,    "!="),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_ASSIGN,        '='),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_BIN_AND,       '&'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_BIN_OR,        '|'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_COLON,         ':'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_COMMA,         ','),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_DIVIDE,        '/'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_LBRACKET,      '('),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_LCURLY,        '{'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_LSQUARE,       '['),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_MINUS,         '-'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_MULTIPLY,      '*'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_NEWLINE,       '\n'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_NOT,           '!'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_PERIOD,        '.'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_PLUS,          '+'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_RBRACKET,      ')'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_RCURLY,        '}'),
    TOKEN_CLASS_CHAR(CLASS_TOKEN, ST_RSQUARE,       ']'),
    TOKEN_CLASS_NUMBER(CLASS_TOKEN, ST_INT,         '.', ST_REAL, -1, 10),
    TOKEN_CLASS_NUMBER(CLASS_TOKEN, ST_INT,         -1, -1, '$', 16),
    TOKEN_CLASS_SEQUENCE(CLASS_TOKEN, ST_IDENT,     (uint32_t*) ident_ranges, sizeof(ident_ranges) / (2 * sizeof(uint32_t))),
    TOKEN_CLASS_STRING(CLASS_TOKEN, ST_STRINGLIT,   '\'', '\'', '\\', TF_FMTCLASS_C)
};

typedef struct
{
    const uint8_t* file_name;

    Tokenbuffer_t* tokenbuffer;
    AstNode_t* script;

    char** globals;
    size_t num_globals, max_globals;

    int failed;
    uint8_t* error_desc;
    size_t error_capacity;

    int dont_check_newline;
}
parsing_context_t;

#define tbuf (p->tokenbuffer)

#define skip_newlines(p) tb_skip((p)->tokenbuffer, ST_NEWLINE)

#define ERROR_FORMAT        "%s\n\t(%s, line %i)\n\n", err_desc, p->file_name
#define ERROR_FORMAT_W_POS  "%s\n\t(%s, line %i)\n\n%s\n%s\n", err_desc, p->file_name, last_token->line, line, spaces

static void parse_error(parsing_context_t *p, const char* format, ...)
{
    va_list args;

    size_t err_desc_length;
    char* err_desc;

    Token_t *last_token;
    int i;
    char *line, *spaces;

    size_t errorlength;

    va_start(args, format);
    err_desc_length = 1 + vsnprintf(NULL, 0, format, args);
    err_desc = (char*) malloc(err_desc_length);
    vsnprintf(err_desc, err_desc_length, format, args);
    va_end(args);

    last_token = tb_current(tbuf);

    if (last_token != NULL)
    {
        for (i = tbuf->tf->line_begin; tbuf->tf->input[i] != '\n' && tbuf->tf->input[i] != 0; i++)
            ;

        i -= tbuf->tf->line_begin;

        line = (char*) malloc(i);
        memcpy(line, tbuf->tf->input + tbuf->tf->line_begin, i - 1);
        line[i-1] = 0;

        spaces = (char*) malloc(last_token->pos_in_line + 2);
        for (i = 0; i < last_token->pos_in_line; i++)
            spaces[i] = ' ';
        spaces[i++] = '^';
        spaces[i] = 0;
    }
    else
    {
        line = NULL;
        spaces = NULL;
    }

    if (last_token != NULL)
        errorlength = 1 + snprintf(NULL, 0, ERROR_FORMAT_W_POS);
    else
        errorlength = 1 + snprintf(NULL, 0, ERROR_FORMAT);

    if (p->error_capacity < errorlength)
    {
        p->error_capacity = errorlength;
        p->error_desc = (uint8_t*) malloc(p->error_capacity);
    }

    if (last_token != NULL)
        snprintf(p->error_desc, p->error_capacity, ERROR_FORMAT_W_POS);
    else
        snprintf(p->error_desc, p->error_capacity, ERROR_FORMAT);

    free(err_desc);
    free(line);
    free(spaces);

    p->failed = 1;
}

static void register_global(parsing_context_t *p, const char *name)
{
    size_t length;

    if (p->num_globals + 1 >= p->max_globals)
    {
        p->max_globals = (p->max_globals == 0) ? 4 : (p->max_globals * 2);
        p->globals = (char **) realloc(p->globals, p->max_globals * sizeof(char*));
    }

    length = strlen(name);

    p->globals[p->num_globals] = (char *) malloc(length + 1);
    memcpy(p->globals[p->num_globals], name, length + 1);

    p->num_globals++;
}

AstNode_t* ast_block(parsing_context_t *p, int indent);
static AstNode_t* ast_list(parsing_context_t *p);
AstNode_t* ast_logexpr(parsing_context_t *p);

#define ast_expression ast_logexpr

static AstNode_t* ast_ident(parsing_context_t *p)
{
    AstNode_t* ident;
    Token_t* token;

    if ((token = tb_check(tbuf, ST_IDENT)) != NULL)
    {
        ident = ast_node(SN_IDENT, token);
        tb_drop(tbuf);
        return ident;
    }

    return NULL;
}

static AstNode_t* ast_atomic(parsing_context_t *p)
{
    AstNode_t* expr;
    Token_t* token;

    if (tb_accept_sequence(tbuf, ST_IDENT, "null"))
        return ast_node(SN_NULL, NULL);
    else if (tb_accept_sequence(tbuf, ST_IDENT, "false"))
        return ast_node(SN_FALSE, NULL);
    else if (tb_accept_sequence(tbuf, ST_IDENT, "function"))
    {
        AstNode_t *block, *func;

        func = ast_node(SN_FUNCTION, NULL);

        /* name */
        func->left = ast_ident(p);

        if (p->failed)
        {
            ast_release_node(&func);
            return NULL;
        }

        /* arguments */
        func->right = ast_list(p);

        if (p->failed)
        {
            ast_release_node(&func);
            return NULL;
        }

        block = ast_block(p, -1);

        if (p->failed)
        {
            ast_release_node(&func);
            return NULL;
        }

        if (block != NULL)
            ast_addchild(func, block);
        else
            ast_addchild(func, ast_node(SN_NULL, NULL));

        if (func->left != NULL)
        {
            register_global(p, (const char *) func->left->token.text);

            func = ast_node_2(SN_ASSIGN, func->left, func);
            func->right->left = NULL;
        }

        return func;
    }
    else if (tb_accept_sequence(tbuf, ST_IDENT, "true"))
        return ast_node(SN_TRUE, NULL);
    else if (tb_accept(tbuf, ST_LCURLY))
    {
        AstNode_t *name, *value;
        int ignore_rcurly;

        expr = ast_node(SN_OBJECT, NULL);
        ignore_rcurly = 0;

        while (1)
        {
            skip_newlines(p);

            if (expr->children_num == 0 && tb_check(tbuf, ST_RCURLY))
                break;

            name = ast_ident(p);

            if (p->failed)
            {
                ast_release_node(&expr);
                return NULL;
            }

            if (name == NULL)
            {
                parse_error(p, "Expected member name");
                ast_release_node(&expr);
                return NULL;
            }

            skip_newlines(p);
            if (!tb_accept(tbuf, ST_COLON))
            {
                parse_error(p, "Expected ':'");
                ast_release_node(&name);
                ast_release_node(&expr);
                return NULL;
            }

            skip_newlines(p);
            value = ast_expression(p);

            if (p->failed)
            {
                ast_release_node(&name);
                ast_release_node(&expr);
                return NULL;
            }

            if (value == NULL)
            {
                parse_error(p, "Expected expression");
                ast_release_node(&name);
                ast_release_node(&expr);
                return NULL;
            }

            ast_addchild(expr, ast_node_2(SN_ASSIGN, name, value));

            if (value->name == SN_FUNCTION)
            {
                skip_newlines(p);

                if (tb_accept(tbuf, ST_RCURLY))
                {
                    ignore_rcurly = 1;
                    break;
                }
                else
                    continue;
            }

            if (!tb_accept(tbuf, ST_COMMA))
                break;
        }

        skip_newlines(p);

        if (!ignore_rcurly && !tb_accept(tbuf, ST_RCURLY))
        {
            parse_error(p, "Expected ',' or '}'");
            ast_release_node(&expr);
        }

        return expr;
    }
    else if ((token = tb_check(tbuf, ST_INT)) != NULL)
    {
        expr = ast_node(SN_INT, token);
        tb_drop(tbuf);
        return expr;
    }
    else if ((token = tb_check(tbuf, ST_REAL)) != NULL)
    {
        expr = ast_node(SN_REAL, token);
        tb_drop(tbuf);
        return expr;
    }
    else if ((token = tb_check(tbuf, ST_STRINGLIT)) != NULL)
    {
        expr = ast_node(SN_STRING, token);
        tb_drop(tbuf);
        return expr;
    }
    else if ((expr = ast_ident(p)) != NULL || (expr = ast_list(p)) != NULL)
        return expr;

    return NULL;
}

static AstNode_t* ast_list(parsing_context_t *p)
{
    AstNode_t* list, * next;

    if (!tb_accept(tbuf, ST_LBRACKET))
        return NULL;

    skip_newlines(p);

    list = ast_node(SN_LIST, NULL);

    if (tb_accept(tbuf, ST_RBRACKET))
        return list;

    do
    {
        skip_newlines(p);

        if ( (next = ast_expression(p)) == NULL )
            next = ast_node(SN_NULL, NULL);

        ast_addchild(list, next);

        if (p->failed)
        {
            ast_release_node(&list);
            return NULL;
        }
    }
    while (tb_accept(tbuf, ST_COMMA));

    skip_newlines(p);

    if (!tb_accept(tbuf, ST_RBRACKET))
    {
        parse_error(p, "Expected ',' or ')'");
        ast_release_node(&list);
    }

    return list;
}

AstNode_t* ast_nearbound(parsing_context_t *p)
{
    AstNode_t* expr, *right;

    expr = ast_atomic(p);

    if (expr == NULL)
        return NULL;

    while (1)
    {
        if (tb_accept(tbuf, ST_LSQUARE))
        {
            skip_newlines(p);
            right = ast_expression(p);

            if (p->failed)
            {
                ast_release_node(&expr);
                return NULL;
            }

            if (right == NULL)
            {
                parse_error(p, "Expected an expression");
                ast_release_node(&expr);
                return NULL;
            }

            skip_newlines(p);

            if (!tb_accept(tbuf, ST_RSQUARE))
            {
                parse_error(p, "Expected ']'");
                ast_release_node(&expr);
                return NULL;
            }

            expr = ast_node_2(SN_INDEX, expr, right);
        }
        else if (tb_accept(tbuf, ST_PERIOD))
        {
            skip_newlines(p);
            right = ast_ident(p);

            if (p->failed)
            {
                ast_release_node(&expr);
                return NULL;
            }

            if (right == NULL)
            {
                parse_error(p, "Expected member name");
                ast_release_node(&expr);
                return NULL;
            }

            expr = ast_node_2(SN_MEMBER, expr, right);
        }
        else if ((right = ast_list(p)) != NULL)
            expr = ast_node_2(SN_CALL, expr, right);
        else
            break;
    }

    return expr;
}

AstNode_t* ast_prefixednearbound(parsing_context_t *p)
{
    if (tb_accept(tbuf, ST_MINUS))
    {
        AstNode_t* node;

        node = ast_prefixednearbound(p);

        if (node == NULL)
        {
            parse_error(p, "Expected expression following '-'");
            return NULL;
        }

        return ast_node_2(SN_SUBTRACT, NULL, node);
    }
    else if (tb_accept(tbuf, ST_NOT))
    {
        AstNode_t* node;

        node = ast_prefixednearbound(p);

        if (node == NULL)
        {
            parse_error(p, "Expected expression following '!'");
            return NULL;
        }

        return ast_node_2(SN_NOT, node, NULL);
    }
    else
        return ast_nearbound(p);
}

#define MUL_RULE(token_) else if (tb_accept(tbuf, ST_##token_))\
    {\
        skip_newlines(p);\
\
        right = ast_prefixednearbound(p);\
\
        if (p->failed)\
        {\
            ast_release_node(&expr);\
            return NULL;\
        }\
\
        if (right == NULL)\
        {\
            parse_error(p, "Expected expression");\
            ast_release_node(&expr);\
            return NULL;\
        }\
\
        expr = ast_node_2(SN_##token_, expr, right);\
    }

AstNode_t* ast_mulexpr(parsing_context_t *p)
{
    AstNode_t *expr, *right;

    expr = ast_prefixednearbound(p);

    if (expr == NULL)
        return NULL;

    while (1)
    {
        if (0);
        MUL_RULE(MULTIPLY)
        MUL_RULE(DIVIDE)
        else
            break;
    }

    return expr;
}

#define ADD_RULE(token_, rule_, operator_) else if (tb_accept(tbuf, ST_##token_))\
    {\
        skip_newlines(p);\
\
        right = ast_mulexpr(p);\
\
        if (p->failed)\
        {\
            ast_release_node(&expr);\
            return NULL;\
        }\
\
        if (right == NULL)\
        {\
            parse_error(p, "Expected expression following " operator_);\
            ast_release_node(&expr);\
            return NULL;\
        }\
\
        expr = ast_node_2(SN_##rule_, expr, right);\
    }

AstNode_t* ast_addexpr(parsing_context_t *p)
{
    AstNode_t *expr, *right;

    expr = ast_mulexpr(p);

    if (expr == NULL)
        return NULL;

    while (1)
    {
        if (0);
        ADD_RULE(APPEND,    APPEND,     "'..'")
        ADD_RULE(PLUS,      ADD,        "'+'")
        ADD_RULE(MINUS,     SUBTRACT,   "'-'")
        else
            break;
    }

    return expr;
}

AstNode_t* ast_assignexpr(parsing_context_t *p)
{
    AstNode_t *expr, *right;

    expr = ast_addexpr(p);

    if (expr == NULL)
        return NULL;

    while (tb_accept(tbuf, ST_ASSIGN))
    {
        skip_newlines(p);

        right = ast_assignexpr(p);

        if (p->failed)
        {
            ast_release_node(&expr);
            return NULL;
        }

        if (right == NULL)
        {
            parse_error(p, "Expected expression following '='");
            ast_release_node(&expr);
            return NULL;
        }

        expr = ast_node_2(SN_ASSIGN, expr, right);
    }

    return expr;
}

#define CMP_RULE(rule_, operator_) else if (tb_accept(tbuf, ST_##rule_))\
    {\
        skip_newlines(p);\
\
        right = ast_assignexpr(p);\
\
        if (p->failed)\
        {\
            ast_release_node(&expr);\
            return NULL;\
        }\
\
        if (right == NULL)\
        {\
            parse_error(p, "Expected expression following " operator_);\
            ast_release_node(&expr);\
            return NULL;\
        }\
\
        expr = ast_node_2(SN_##rule_, expr, right);\
    }

AstNode_t* ast_cmpexpr(parsing_context_t *p)
{
    AstNode_t *expr, *right;

    expr = ast_assignexpr(p);

    if (expr == NULL)
        return NULL;

    while (1)
    {
        if (0);
        CMP_RULE(EQUALS,        "'=='")
        CMP_RULE(NOT_EQUALS,    "'!='")
        else
            break;
    }

    return expr;
}

#define LOG_RULE(rule_, operator_) else if (tb_accept(tbuf, ST_##rule_))\
    {\
        skip_newlines(p);\
\
        right = ast_cmpexpr(p);\
\
        if (p->failed)\
        {\
            ast_release_node(&expr);\
            return NULL;\
        }\
\
        if (right == NULL)\
        {\
            parse_error(p, "Expected expression following " operator_);\
            ast_release_node(&expr);\
            return NULL;\
        }\
\
        expr = ast_node_2(SN_##rule_, expr, right);\
    }

AstNode_t* ast_logexpr(parsing_context_t *p)
{
    AstNode_t *expr, *right;

    expr = ast_cmpexpr(p);

    if (expr == NULL)
        return NULL;

    while (1)
    {
        if (0);
        LOG_RULE(BIN_AND,   "'&'")
        LOG_RULE(BIN_OR,    "'|'")
        else
            break;
    }

    return expr;
}

AstNode_t* ast_statement(parsing_context_t *p)
{
    if (tb_accept_sequence(tbuf, ST_IDENT, "break"))
    {
        return ast_node(SN_BREAK, NULL);
    }
    else if (tb_accept_sequence(tbuf, ST_IDENT, "if"))
    {
        AstNode_t *node;

        node = ast_node_2(SN_IF, NULL, NULL);

        /* condition */
        skip_newlines(p);
        node->left = ast_expression(p);

        if (p->failed)
        {
            ast_release_node(&node);
            return NULL;
        }

        if (node->left == NULL)
        {
            parse_error(p, "Expected expression following 'if'");
            ast_release_node(&node);
            return NULL;
        }

        /* block */
        node->right = ast_block(p, -1);

        if (p->failed)
        {
            ast_release_node(&node);
            return NULL;
        }

        if (node->right == NULL)
        {
            parse_error(p, "Expected code block");
            ast_release_node(&node);
            return NULL;
        }

        skip_newlines(p);
        if (tb_accept_sequence(tbuf, ST_IDENT, "else"))
        {
            AstNode_t* else_block;

            else_block = ast_block(p, -1);

            if (p->failed)
            {
                ast_release_node(&node);
                return NULL;
            }

            if (else_block == NULL)
            {
                parse_error(p, "Expected code block following 'else'");
                ast_release_node(&node);
                return NULL;
            }

            ast_addchild(node, else_block);
        }

        return node;
    }
    else if (tb_accept_sequence(tbuf, ST_IDENT, "iterate"))
    {
        AstNode_t *node, *block;

        node = ast_node_2(SN_ITERATE, NULL, NULL);

        /* iterator name */
        skip_newlines(p);
        node->left = ast_ident(p);

        if (p->failed)
        {
            ast_release_node(&node);
            return NULL;
        }

        if (node->left == NULL)
        {
            parse_error(p, "Expected iterator name");
            ast_release_node(&node);
            return NULL;
        }

        skip_newlines(p);

        if (!tb_accept_sequence(tbuf, ST_IDENT, "in"))
        {
            parse_error(p, "Expected 'in'");
            ast_release_node(&node);
            return NULL;
        }

        /* list */
        skip_newlines(p);
        node->right = ast_expression(p);

        if (p->failed)
        {
            ast_release_node(&node);
            return NULL;
        }

        if (node->right == NULL)
        {
            parse_error(p, "Expected expression");
            ast_release_node(&node);
            return NULL;
        }

        block = ast_block(p, -1);

        if (p->failed)
        {
            ast_release_node(&node);
            return NULL;
        }

        if (block == NULL)
        {
            parse_error(p, "Expected code block following 'iterate'");
            ast_release_node(&node);
            return NULL;
        }

        ast_addchild(node, block);
        return node;
    }
    else if (tb_accept_sequence(tbuf, ST_IDENT, "return"))
    {
        AstNode_t *value;

        value = ast_expression(p);

        if (p->failed)
            return NULL;

        return ast_node_2(SN_RETURN, value, NULL);
    }
    else if (tb_accept_sequence(tbuf, ST_IDENT, "while"))
    {
        AstNode_t *node;

        node = ast_node_2(SN_WHILE, NULL, NULL);

        /* condition */
        skip_newlines(p);
        node->left = ast_expression(p);

        if (p->failed)
        {
            ast_release_node(&node);
            return NULL;
        }

        if (node->left == NULL)
        {
            parse_error(p, "Expected expression following 'while'");
            ast_release_node(&node);
            return NULL;
        }

        /* block */
        node->right = ast_block(p, -1);

        if (p->failed)
        {
            ast_release_node(&node);
            return NULL;
        }

        if (node->right == NULL)
        {
            parse_error(p, "Expected code block");
            ast_release_node(&node);
            return NULL;
        }

        return node;
    }
    else 
        return ast_expression(p);
}

AstNode_t* ast_block(parsing_context_t *p, int i)
{
    AstNode_t *block;
    Token_t *tok;
    int indent;

    indent = -1;
    block = NULL;

    skip_newlines(p);

    while (1)
    {
        AstNode_t* statement;

        tok = tb_current(tbuf);

        if (tok == NULL)
            break;

        if (indent < 0)
            indent = tok->indent;
        else if (tok->indent < indent)
            break;

        statement = ast_statement(p);
        
        if (statement == NULL)
            break;

        if (block == NULL)
            block = ast_node(SN_BLOCK, NULL);

        ast_addchild(block, statement);

        if (!p->dont_check_newline && !tb_accept(tbuf, ST_NEWLINE) && tb_current(tbuf) != NULL)
        {
            ast_print(statement, 0, NULL, 0);
            parse_error(p, "Expected new line after statement");
        }

        skip_newlines(p);
        p->dont_check_newline = 0;
    }

    if (p->failed)
    {
        ast_release_node(&block);
        return NULL;
    }

    p->dont_check_newline = 1;
    return block;
}

int ast_script(parsing_context_t *p)
{
    p->script = ast_node(SN_SCRIPT, NULL);

    while (!p->failed)
    {
        AstNode_t* child;

        skip_newlines(p);

        if (tb_accept(tbuf, ST_GLOBAL))
        {
            Token_t* token;

            do
            {
                skip_newlines(p);

                if ((token = tb_check(tbuf, ST_IDENT)) != NULL)
                {
                    register_global(p, (const char *) token->text);
                    tb_drop(tbuf);
                }
                else
                    break;
            }
            while (tb_accept(tbuf, ST_COMMA));

            if (p->failed)
                break;

            if (!tb_accept(tbuf, ST_NEWLINE))
                parse_error(p, "Expected ',' or new line");
        }
        else if ((child = ast_block(p, -1)) != NULL)
            ast_addchild(p->script, child);
        else
            break;
    }

    if (!p->failed && tb_current(tbuf) != NULL)
        parse_error(p, "Unrecognized token in input");

    return 0;
}

static void node_on_release_script(AstNode_t* node)
{
    ast_properties_t *ast_properties;
    size_t i;

    ast_properties = (ast_properties_t *) node->cust_data;

    for (i = 0; i < ast_properties->num_globals; i++)
        free(ast_properties->globals[i]);

    free(ast_properties->globals);
    free(ast_properties);
}

AstNode_t* parse(const char* filename, const char* script)
{
    Tokenfactory_t *tf;
    parsing_context_t p;
    ast_properties_t *ast_properties;
    size_t i;

    static const uint32_t chars[] = {' ', '\t', '\r'};

    tf = tokenfactory();
    tokenclass_add(tf, script_tokens, sizeof(script_tokens) / sizeof(script_tokens[0]));
    tf_whitespace_add(tf, chars, 3);
    
    tf_input_string(tf, (uint8_t*) script, -1, 0);

    p.file_name = filename;

    p.tokenbuffer = tokenbuffer(tf);
    p.script = NULL;

    p.globals = NULL;
    p.num_globals = 0;
    p.max_globals = 0;

    p.failed = 0;
    p.error_desc = NULL;
    p.error_capacity = 0;

    p.dont_check_newline = 0;

    ast_script(&p);

    if (p.failed)
    {
        printf("\nPARSE ERROR:\n%s", p.error_desc);
        ast_release_node(&p.script);

        for (i = 0; i < p.num_globals; i++)
            free(p.globals[i]);

        free(p.globals);
    }
    else
    {
        //ast_print(p.script, 0, node_names, sizeof(node_names) / sizeof(*node_names));

        ast_properties = (ast_properties_t *) malloc(sizeof(ast_properties_t));
        ast_properties->globals = p.globals;
        ast_properties->num_globals = p.num_globals;
        
        p.script->cust_data = ast_properties;
        p.script->on_release = node_on_release_script;
    }

    tokenbuffer_del(&p.tokenbuffer);
    tokenfactory_del(&tf);

    free(p.error_desc);

    return p.script;
}
