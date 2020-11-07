
#include "tokenfactory.h"

#include <limits.h>

/*TODO: move me */
enum { C_UNK, C_WS, C_TOKEN };

static int tf_fetch_byte(Tf_t* tf, uint8_t* byte)
{
    if (tf->inputpos >= tf->inputlen)
        return -1;

    *byte = tf->input[tf->inputpos++];
    return 1;
}

static int tf_is_in_ranges(int32_t c, Tokenclass_t* tc)
{
    size_t i;

    if (tc->type == TOKENCLASS_NUMBER)
    {
        if (tc->decimal_c > 0 && c == tc->decimal_c)
            return 2;

        if (tc->base > 0 && c >= '0' && c < '0' + TFMIN(tc->base, 10))
            return 1;

        if (tc->base > 10 && c >= 'a' && c < 'a' + TFMIN(tc->base, 26))
            return 1;

        if (tc->base > 10 && c >= 'A' && c < 'A' + TFMIN(tc->base, 26))
            return 1;
    }
    else if (tc->type == TOKENCLASS_SEQUENCE)
    {
        for (i = 0; i + 1 < tc->num_ranges * 2; i += 2)
        {
            if (c >= (int) tc->ranges[i] && c <= (int) tc->ranges[i + 1])
                return 1;
        }
    }

    return 0;
}

static int tf_is_token(Tf_t* tf, Tokenclass_t* tc)
{
    tf->token.type = tc->type;
    tf->token.name = tc->name;
    tf->token.indent = tf->indent;

    return C_TOKEN;
}

static int tf_read_char(Tf_t* tf, uint32_t* c_p)
{
    uint32_t c;
    uint8_t byte_in;
    int num_bytes, i;

    if (tf->c > 0)
    {
        *c_p = tf->c;
        tf->c = -1;
        return 1;
    }

    *c_p = -1;

    if (tf_fetch_byte(tf, &byte_in) < 0)
        return -1;

    /* 0XXX XXXX one byte */
    if (byte_in <= 0x7F)
    {
        c = byte_in;
        num_bytes = 1;
    }
    /* 110X XXXX  two bytes */
    else if ((byte_in & 0xE0) == 0xC0)
    {
        c = byte_in & 31;
        num_bytes = 2;
    }
    /* 1110 XXXX  three bytes */
    else if ((byte_in & 0xF0) == 0xE0)
    {
        c = byte_in & 15;
        num_bytes = 3;
    }
    /* 1111 0XXX  four bytes */
    else if ((byte_in & 0xF8) == 0xF0)
    {
        c = byte_in & 7;
        num_bytes = 4;
    }
    /* 1111 10XX  five bytes */
    /*else if ((byte_in & 0xFC) == 0xF8)
    {
        c = byte_in & 3;
        num_bytes = 5;
    }*/
    /* 1111 110X  six bytes */
    /*else if ((byte_in & 0xFE) == 0xFC)
    {
        c = byte_in & 1;
        num_bytes = 6;
    }*/
    else
    {
        /* not a valid first byte of a UTF-8 sequecce */
        /*c = byte_in;
        num_bytes = 1;*/
        return -1;
    }

    for (i = 0; i < num_bytes - 1; i++)
    {
        if (tf_fetch_byte(tf, &byte_in) < 0)
            return -1;

        if ((byte_in & 0xC0) != 0x80)
            return -1;

        c = (c << 6) | (byte_in & 0x3F);
    }

    if (c == '\n')
    {
        tf->line++;
        tf->line_begin = tf->inputpos;
        tf->indent = 0;
        tf->pos_in_line = 0;
    }
    else
        tf->pos_in_line++;

    *c_p = c;
    return num_bytes;
}

static int tf_read_word(Tf_t* tf, const char* word)
{
    uint32_t cache, c;
    size_t oldpos;
    int old_pos_in_line;

    cache = tf->c;
    oldpos = tf->inputpos;
    old_pos_in_line = tf->pos_in_line;

    while (*word)
    {
        if (tf_read_char(tf, &c) < 0 || c != *word)
        {
            tf->c = cache;
            tf->inputpos = oldpos;
            tf->pos_in_line = old_pos_in_line;
            return -1;
        }

        word++;
    }

    return 0;
}

/******************************************************************************/

TFFUNC void tf_clear_token(Token_t* token)
{
    token->type = -1;
    token->name = -1;
    token->text = NULL;
}

/*TFFUNC void tf_copy_token(Token_t* token, const Token_t* src)
{
    token->type = src->type;
    token->name = src->name;
    token->c = src->c;

    if (src->text != NULL)
    {
        size_t length;

        length = strlen((const char*) src->text) + 1;
        token->text = (uint8_t*) malloc(length);
        memcpy(token->text, src->text, length);
    }
}*/

TFFUNC void tf_input_string(Tf_t* tf, const uint8_t* string, intptr_t length, int makecopy)
{
    if (tf->ownsinput)
        free(tf->input);

    tf->inputpos = 0;
    tf->line = 1;
    tf->line_begin = 0;
    tf->indent = 0;
    tf->pos_in_line = 0;

    if (length < 0)
        tf->inputlen = strlen((const char*) string);
    else if (length == 0)
    {
        tf->inputlen = 0;
        return;
    }

    if (!makecopy)
    {
        tf->input = (uint8_t*) string;
    }
    else
    {
        tf->input = (uint8_t*) malloc(tf->inputlen + 1);
        memcpy(tf->input, string, tf->inputlen);
        tf->input[tf->inputlen] = 0;
    }

    return;
}

TFFUNC char* tf_load_file(const char *filename, size_t *size_out)
{
    FILE *f;
    long size;
    char* buffer;

    f = fopen(filename, "rb");

    if (f == NULL)
        return NULL;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if ((buffer = (char *)malloc(size + 1)) == NULL)
    {
        fclose(f);
        return NULL;
    }

    size = fread(buffer, 1, size, f);
    fclose(f);

    buffer[size] = 0;

    if (size_out != NULL)
        *size_out = size;

    return buffer;
}

TFFUNC void tf_move_token(Token_t* token, Token_t* src)
{
    memcpy(token, src, sizeof(struct Token));
    tf_clear_token(src);
}

TFFUNC void tf_on_token(Tf_t* tf, TF_TOKEN_CB on_token)
{
    tf->on_token = on_token;
}

TFFUNC int tf_parse_token(Tf_t* tf)
{
    uint32_t c;
    size_t i;
    int type;
    int token_line, token_pos;

    tf_release_token(&tf->token);

    _restart:

    do
    {
        token_line = tf->line;
        token_pos = tf->pos_in_line;

        if (tf_read_char(tf, &c) < 0)
            return -1;

        type = C_UNK;

        /*printf("char: %c\n", c);*/

        for (i = 0; i < tf->whitespaces_num; i++)
        {
            if (c == tf->whitespaces[i])
            {
                tf->indent++;
                type = C_WS;
                break;
            }
        }
    }
    while (type == C_WS);

    for (i = 0; type == C_UNK && i < tf->classes_num; i++)
    {
        Tokenclass_t* tc = &tf->classes[i];

        switch (tc->type)
        {
            case TOKENCLASS_CHAR:
                if (c == tc->c)
                {
                    type = tf_is_token(tf, tc);
                    tf->token.c = c;
                }
                break;

            case TOKENCLASS_NUMBER:
            {
                /* if there is an opening character required, check for it */
                if (tc->c > 0)
                {
                    if (c != tc->c || tf_read_char(tf, &c) < 0)
                        break;
                }

                if (tf_is_in_ranges(c, tc))
                {
                    char tmp[400];
                    size_t j;
                    int is_decimal, is_in_range;

                    type = tf_is_token(tf, tc);
                    is_decimal = 0;

                    /* push init char back to cache */
                    tf->c = c;

                    j = 0;
                    while (1)
                    {
                        if (tf_read_char(tf, &c) < 0)
                            break;

                        is_in_range = tf_is_in_ranges(c, tc);

                        if (!is_in_range)
                            break;
                        else if (is_in_range == 1)
                        {
                            tmp[j++] = c;
                        }
                        else
                        {
                            if (is_decimal)
                                printf( "Error: double decimal ALSO FIXME\n");

                            is_decimal = 1;
                            tmp[j++] = '.';
                        }
                    }

                    tf->c = c;
                    tmp[j++] = 0;

                    if (is_decimal)
                    {
                        if (tc->base != 10)
                            printf( "Error: base incompatible with decimal number\n");

                        tf->token.name = tc->decimal_name;
                        tf->token.decimal = strtod(tmp, NULL);
                    }
                    else
                    {
                        tf->token.number = strtol(tmp, NULL, tc->base);

                        if (tf->token.number == INT_MAX)
                            tf->token.number = strtoul(tmp, NULL,tc->base);
                    }
                }
                break;
            }

            case TOKENCLASS_SEQUENCE:
                if (tf_is_in_ranges(c, tc))
                {
                    char tmp[400];
                    size_t j;

                    type = tf_is_token(tf, tc);

                    j = 0;
                    tmp[j++] = c;
                    while (1)
                    {
                        if (tf_read_char(tf, &c) < 0)
                            break;

                        if (!tf_is_in_ranges(c, tc))
                            break;
                        else
                        {
                            tmp[j++] = c;
                        }
                    }

                    tf->c = c;
                    tmp[j++] = 0;

                    tf->token.text = (uint8_t*) malloc(j);
                    memcpy(tf->token.text, tmp, j);
                }
                break;

            case TOKENCLASS_STRING:
                if (c == tc->oq)
                {
                    char tmp[400];
                    size_t j;
                    int escape;

                    j = 0;
                    escape = 0;
                    while (1)
                    {
                        if (tf_read_char(tf, &c) < 0)
                            /* error actually */
                            break;

                        if (!escape && c == tc->esc)
                            escape = 1;
                        else if (!escape && c == tc->cq)
                            break;
                        else
                        {
                            if(escape && tc->fmtclass == TF_FMTCLASS_C)
                            {
                                if (c == 'n')
                                    c = '\n';
                                else if (c == 't')
                                    c = '\t';
                            }

                            tmp[j++] = c;
                            escape = 0;
                        }
                    }

                    tmp[j++] = 0;

                    if (tc->purpose == CLASS_COMMENT)
                        goto _restart;

                    type = tf_is_token(tf, tc);
                    tf->token.text = (uint8_t*) malloc(j);
                    memcpy(tf->token.text, tmp, j);
                }
                break;

            case TOKENCLASS_WORD:
                if (c != tc->word[0])
                    break;

                if (tf_read_word(tf, tc->word + 1) != 0)
                    break;

                type = tf_is_token(tf, tc);
                break;
        }
    }

    if (type == C_TOKEN)
    {
        tf->token.line = token_line;
        tf->token.pos_in_line = token_pos;

        if (tf->on_token != NULL)
            tf->on_token(tf);
    }
    else if(type == C_UNK)
    {
        printf( "ERROR (change to cb): character %c [%i]\n", c, c);
    }

    return 0;
}

TFFUNC void tf_parse(Tf_t* tf)
{
    tf->stop_parse = 0;

    while (!tf->stop_parse)
    {
        if (tf_parse_token(tf) < 0)
            break;
    }
}

TFFUNC void tf_release_token(Token_t* token)
{
    free(token->text);
    token->text = NULL;
}

TFFUNC void tf_stop_parse(Tf_t* tf)
{
    tf->stop_parse = 1;
}

TFFUNC void tf_whitespace_add(Tf_t* tf, const uint32_t* chars_in, size_t count)
{
    if (tf->whitespaces_num + count >= tf->whitespaces_max)
    {
        tf->whitespaces_max = tf->whitespaces_num + count + 3;
        tf->whitespaces = (uint32_t*) realloc( tf->whitespaces, tf->whitespaces_max * sizeof(uint32_t) );
    }

    memcpy(tf->whitespaces + tf->whitespaces_num, chars_in, count * sizeof(uint32_t) );
    tf->whitespaces_num += count;
}

TFFUNC void tf_whitespace_common(Tf_t* tf)
{
    static const uint32_t chars[] = {' ', '\t', '\r', '\n'};

    tf_whitespace_add(tf, chars, 4);
}

TFFUNC void tokenclass_add(Tf_t* tf, const Tokenclass_t* classes_in, size_t count)
{
    if (tf->classes_num + count >= tf->classes_max)
    {
        tf->classes_max = tf->classes_num + count + 3;
        tf->classes = (Tokenclass_t*) realloc( tf->classes, tf->classes_max * sizeof(Tokenclass_t) );
    }

    memcpy(tf->classes + tf->classes_num, classes_in, count * sizeof(Tokenclass_t) );
    tf->classes_num += count;
}

TFFUNC Tf_t* tokenfactory()
{
    Tf_t* tf;

    tf = (Tf_t*) malloc( sizeof(Tokenfactory_t) );

    if (tf != NULL)
    {
        tf->classes = NULL;
        tf->classes_num = 0;
        tf->classes_max = 0;

        tf->whitespaces = NULL;
        tf->whitespaces_num = 0;
        tf->whitespaces_max = 0;

        tf->ownsinput = 0;
        tf->input = NULL;
        tf->inputlen = 0;
        tf->inputpos = 0;
        tf->c = -1;
        tf->line = -1;
        tf->indent = -1;

        tf_clear_token(&tf->token);

        tf->stop_parse = -1;

        tf->on_token = NULL;
    }

    return tf;
}

TFFUNC void tokenfactory_del(Tf_t** tf_p)
{
    if(*tf_p == NULL)
        return;

    tf_release_token(&(*tf_p)->token);

    free( (*tf_p)->classes );
    free( (*tf_p)->whitespaces );

    if( (*tf_p)->ownsinput )
        free( (*tf_p)->input );

    free(*tf_p);
    *tf_p = NULL;
}
