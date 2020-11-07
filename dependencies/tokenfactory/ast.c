
#include "tokenfactory.h"

static void ast_indent(int indent)
{
    int i;

    for (i = 0; i < indent + 1; i++)
        printf("  ");
}

TFFUNC void ast_addchild(AstNode_t* node, AstNode_t* child)
{
    if (node->children_num + 1 > node->children_max)
    {
        node->children_max = (node->children_max == 0) ? 4 : (node->children_max * 2);
        node->children = (AstNode_t**) realloc(node->children, node->children_max * sizeof(AstNode_t*));
    }

    node->children[node->children_num++] = child;
}

TFFUNC AstNode_t* ast_node(int name, Token_t* token)
{
    AstNode_t* node;

    node = (AstNode_t*) malloc( sizeof(struct AstNode) );

    node->name = name;

    node->left = NULL;
    node->right = NULL;
    node->children = NULL;
    node->children_num = 0;
    node->children_max = 0;
    node->cust_data = NULL;
    node->on_release = NULL;

    if (token != NULL)
        tf_move_token(&node->token, token);
    else
        tf_clear_token(&node->token);

    return node;
}

TFFUNC AstNode_t* ast_node_2(int name, AstNode_t* left, AstNode_t* right)
{
    AstNode_t* node;

    node = ast_node(name, NULL);
    node->left = left;
    node->right = right;

    return node;
}

TFFUNC void ast_print(AstNode_t* node, int indent, const char** names, size_t num_names)
{
    size_t i;

    ast_indent(indent);

    printf("- [%i", node->name);

    if (node->name >= 0 && (size_t)node->name < num_names)
        printf("%s", names[node->name]);

    printf("]");

    if (node->token.text != NULL)
        printf(" `%s`", node->token.text);

    printf("\n");

    if (node->left != NULL)
        ast_print(node->left, indent + 1, names, num_names);

    if (node->right != NULL)
        ast_print(node->right, indent + 1, names, num_names);

    for (i = 0; i < node->children_num; i++)
        ast_print(node->children[i], indent + 1, names, num_names);
}

TFFUNC void ast_release_node(AstNode_t** node_p)
{
    size_t i;

    if ((*node_p) == NULL)
        return;

    if ((*node_p)->on_release != NULL)
        (*node_p)->on_release(*node_p);

    tf_release_token( &(*node_p)->token );

    if ((*node_p)->left != NULL)
        ast_release_node(&(*node_p)->left);

    if ((*node_p)->right != NULL)
        ast_release_node(&(*node_p)->right);

    for (i = 0; i < (*node_p)->children_num; i++)
        ast_release_node(&(*node_p)->children[i]);

    free((*node_p)->children);

    free(*node_p);
    *node_p = NULL;
}

TFFUNC int ast_serialize_node(AstNode_t* node, FILE *f)
{
    uint8_t flags;
    int16_t name;
    uint16_t children_num;

    int32_t token_c, token_number;
    float token_dec;
    uint16_t token_text_len;

    size_t i;

    flags = 0;

    if (node->left != NULL)
        flags |= 1;

    if (node->right != NULL)
        flags |= 2;

    name = node->name;
    children_num = node->children_num;

    token_c = node->token.c;
    token_number = node->token.number;
    token_dec = (float) node->token.decimal;

    fwrite(&flags, 1, 1, f);
    fwrite(&name, 2, 1, f);
    fwrite(&children_num, 2, 1, f);

    fwrite(&token_c, 4, 1, f);
    fwrite(&token_number, 4, 1, f);
    fwrite(&token_dec, 4, 1, f);

    if (node->left != NULL)
        ast_serialize_node(node->left, f);

    if (node->right != NULL)
        ast_serialize_node(node->right, f);

    for (i = 0; i < node->children_num; i++)
        ast_serialize_node(node->children[i], f);

    if (node->token.text != NULL)
    {
        token_text_len = strlen((const char*) node->token.text);
        fwrite(&token_text_len, 2, 1, f);
        fwrite(node->token.text, 1, token_text_len, f);
    }
    else
    {
        token_text_len = 0;
        fwrite(&token_text_len, 2, 1, f);
    }

    return 0;
}

TFFUNC int ast_serialize(AstNode_t* node, const char* filename)
{
    FILE *f;

    f = fopen(filename, "wb");

    if (f == NULL)
        return -1;

    ast_serialize_node(node, f);

    fclose(f);
    return 0;
}