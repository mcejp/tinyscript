
#include "tokenfactory.h"

static int tb_pump_tokens(Tokenbuffer_t* tbuf, size_t count)
{
    while (count > 0)
    {
        if (tbuf->tokens_num + 1 > tbuf->tokens_max)
        {
            tbuf->tokens_max = (tbuf->tokens_max == 0) ? 4 : (tbuf->tokens_max * 2);
            tbuf->tokens = (Token_t*) realloc(tbuf->tokens, tbuf->tokens_max * sizeof(struct Token));
        }

        if (tf_parse_token(tbuf->tf) < 0)
            return -1;

        tf_move_token(&tbuf->tokens[tbuf->tokens_num], &tbuf->tf->token);
        tbuf->tokens_num++;

        count--;
    }

    return 0;
}

/******************************************************************************/

TFFUNC Token_t* tb_current(Tokenbuffer_t* tbuf);

TFFUNC int tb_accept(Tokenbuffer_t* tbuf, int name)
{
    if (tb_check(tbuf, name) != NULL)
    {
        tb_drop(tbuf);
        return 1;
    }
    else
        return 0;
}

TFFUNC int tb_accept_sequence(Tokenbuffer_t* tbuf, int name, const char* text)
{
    if (tb_check_sequence(tbuf, name, text) != NULL)
    {
        tb_drop(tbuf);
        return 1;
    }
    else
        return 0;
}

TFFUNC Token_t* tb_check(Tokenbuffer_t* tbuf, int name)
{
    Token_t* current;

    current = tb_current(tbuf);

    if (current == NULL)
        return NULL;

    if (current->name == name)
        return current;
    else
        return NULL;
}

TFFUNC Token_t* tb_check_sequence(Tokenbuffer_t* tbuf, int name, const char* text)
{
    Token_t* current;

    current = tb_current(tbuf);

    if (current == NULL)
        return NULL;

    if (current->type == TOKENCLASS_SEQUENCE && current->name == name && strcmp((const char*) current->text, text) == 0)
        return current;
    else
        return NULL;
}

TFFUNC Token_t* tb_current(Tokenbuffer_t* tbuf)
{
    if (tbuf->tokens_num < 1)
    {
        if (tb_pump_tokens(tbuf, 1) < 0)
            return NULL;
    }

    return &tbuf->tokens[0];
}

TFFUNC void tb_drop(Tokenbuffer_t* tbuf)
{
    /*TODO: check num_tokens > 0 */

    tf_release_token(&tbuf->tokens[0]);

    tbuf->tokens_num--;
    memmove(&tbuf->tokens[0], &tbuf->tokens[1], tbuf->tokens_num * sizeof(struct Token));
}

TFFUNC void tb_skip(Tokenbuffer_t *tbuf, int name)
{
    while (tb_accept(tbuf, name))
        ;
}

/******************************************************************************/

TFFUNC Tokenbuffer_t* tokenbuffer(Tokenfactory_t* tf)
{
    Tokenbuffer_t* tbuf;

    tbuf = (Tokenbuffer_t*) malloc( sizeof(Tokenbuffer_t) );

    tbuf->tf = tf;

    if (tbuf != NULL)
    {
        tbuf->tokens = NULL;
        tbuf->tokens_num = 0;
        tbuf->tokens_max = 0;
    }

    return tbuf;
}

TFFUNC void tokenbuffer_del(Tokenbuffer_t** tbuf_p)
{
    size_t i;

    if(*tbuf_p == NULL)
        return;

    for (i = 0; i < (*tbuf_p)->tokens_num; i++)
    {
        tf_release_token(&(*tbuf_p)->tokens[i]);
    }

    free( (*tbuf_p)->tokens );

    free(*tbuf_p);
    *tbuf_p = NULL;
}
