
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include "parse.h"
#include <tinyapi.h>

#include <parse_args.h>

#ifdef TINYSCRIPT_SINGLE_FILE
#include <tokenbuffer.c>
#include <tokenfactory.c>
#endif

#ifdef _MSC_VER
#include <malloc.h>
#else
void *alloca(size_t size);
#endif

#ifdef _WIN32
#include <windows.h>
#endif

typedef struct
{
    TS_Val globals, locals, return_value;
    int should_break, should_return;
}
ast_context_t;

typedef struct
{
    AstNode_t* script;
}
ast_finalize_context_t;

static const char* TS_FunctionNodeRef_name = "TS.FunctionNodeRef";

TS_Val ast_eval(AstNode_t* node, ast_context_t* context);

TS_Val TS_func_load_module(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    TS_ModuleEntry_t entry;

#ifdef _WIN32
    char path[MAX_PATH];
    HMODULE library;
#endif

    if (num_arguments != 1 || arguments[0].type != TS_STRING)
    {
        printf("load_module: invalid argument(s)\n");
        return TS_null();
    }

#ifdef _WIN32
    snprintf(path, MAX_PATH, "module_%s.dll", arguments[0].string->bytes);

    library = LoadLibraryA(path);
    
    if (library == NULL)
    {
        printf("Warning: failed to open `%s`\n", path);
        return TS_null();
    }

    entry = (TS_ModuleEntry_t) GetProcAddress(library, "TS_ModuleEntry");

    if (entry == NULL)
    {
        printf("Warning: failed to load module `%s`\n", arguments[0].string->bytes);
        return TS_null();
    }

    return entry(arguments[0].string->bytes, ctx->globals);
#endif
}

TS_Val TS_func_say(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    size_t i;

    for (i = 0; i < num_arguments; i++)
    {
        if (arguments[i].type == TS_STRING)
            printf("%s", arguments[i].string->bytes);
        else
            TS_printvalue(arguments[i], 0);

        if (i + 1 < num_arguments)
            printf(" ");
    }

    printf("\n");
    return TS_null();
}

static const char* TS_File_type_name = "TS.File";

static void release_file(TS_Val val)
{
    fclose((FILE*) val.object->native->cust_data);
}

static FILE* unwrap_file(TS_Val val)
{
    if (val.type != TS_OBJECT || val.object->native == NULL || val.object->native->type_name != TS_File_type_name)
        return NULL;

    return (FILE*) val.object->native->cust_data;
}

TS_Val TS_func_File_read_line(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    FILE *file;
    static char buffer[2000];
    int num;

    file = unwrap_file(ctx->me);

    if (file == NULL || num_arguments != 0)
        return TS_null();

    /* TODO: buffer */
    if (!fgets(buffer, sizeof(buffer), file))
        return TS_null();

    num = strlen(buffer);

    if (num > 0 && buffer[num - 1] == '\n')
        buffer[num - 1] = 0;

    return TS_create_string(buffer);
}

TS_Val TS_func_File_write(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    FILE *file;
    size_t i;

    file = unwrap_file(ctx->me);

    if (file == NULL)
        return TS_null();

    for (i = 0; i < num_arguments; i++)
    {
        if (arguments[i].type == TS_STRING)
            fprintf(file, "%s", arguments[i].string->bytes);
        else if (arguments[i].type == TS_INT)
            fprintf(file, "%i", arguments[i].intval);
        else if (arguments[i].type == TS_FLOAT)
            fprintf(file, "%g", arguments[i].floatval);

        if (i + 1 < num_arguments)
            printf(" ");
    }

    fputc('\n', file);
    return TS_int(0);
}

static TS_Val wrap_file(FILE* file, int release)
{
    TS_Val native;

    if (file == NULL)
        return TS_null();

    native = TS_create_object(4);
    native.object->native = TS_create_native_struct(TS_File_type_name, file, release ? release_file : NULL);
    TS_set_member(native, "read_line", TS_native_function(TS_func_File_read_line));
    TS_set_member(native, "write", TS_native_function(TS_func_File_write));
    return native;
}

TS_Val TS_func_create_file(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 1 || arguments[0].type != TS_STRING)
        return TS_null();

    return wrap_file(fopen((const char *) arguments[0].string->bytes, "wb"), 1);
}

TS_Val TS_func_open_file(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 1 || arguments[0].type != TS_STRING)
        return TS_null();

    return wrap_file(fopen((const char *) arguments[0].string->bytes, "r"), 1);
}

// TS> String expand(String str, Object dictionary)
TS_Val TS_func_expand(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    TS_Val newstr;
    size_t i;
    char c;

    if (num_arguments != 2 || arguments[0].type != TS_STRING || arguments[1].type != TS_OBJECT)
        return TS_null();

    newstr = TS_create_string(NULL);

    for (i = 0; i < arguments[0].string->num_bytes; i++)
    {
        c = arguments[0].string->bytes[i];

        if (c == '$')
        {
            char FIXME_buffer[100];
            size_t j;

            TS_Val replacement;

            i++;

            if (i == arguments[0].string->num_bytes)
                break;

            c = arguments[0].string->bytes[i]; /* '{' */
            i++;

            j = 0;

            for ( ; i < arguments[0].string->num_bytes; i++)
            {
                c = arguments[0].string->bytes[i];

                if (c == '}')
                    break;

                FIXME_buffer[j++] = c;
            }

            FIXME_buffer[j] = 0;

            replacement = TS_get_member(arguments[1], FIXME_buffer);

            if (replacement.type == TS_STRING)
                TS_string_appendutf8(newstr.string, replacement.string->bytes);

            TS_rlsvalue(replacement);
        }
        else
            TS_string_appendchar(newstr.string, c);
    }

    TS_string_appendchar(newstr.string, 0);
    newstr.string->num_bytes--;
    return newstr;
}

TS_Val TS_func__strdrop(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 2 || arguments[0].type != TS_STRING || arguments[1].type != TS_INT)
        return TS_null();

    return TS_create_string(arguments[0].string->bytes + TS_NUMERIC_AS_INT(arguments[1]));
}

typedef struct
{
    int is_global;
}
ident_cust_data;

static void node_on_release_struct(AstNode_t* node)
{
    free(node->cust_data);
}

static void node_on_release_tsval(AstNode_t* node)
{
    TS_rlsvalue( *((TS_Val*) node->cust_data) );
    free(node->cust_data);
}

void ast_finalize(AstNode_t* node, ast_finalize_context_t* context)
{
    size_t i;

    switch (node->name)
    {
        case SN_IDENT:
        {
            ast_properties_t *properties;
            ident_cust_data *cust_data;

            cust_data = (ident_cust_data *) malloc(sizeof(ident_cust_data));
            cust_data->is_global = 0;

            properties = (ast_properties_t *) context->script->cust_data;

            for (i = 0; i < properties->num_globals; i++)
            {
                if (strcmp(properties->globals[i], (const char*) node->token.text) == 0)
                {
                    cust_data->is_global = 1;
                    break;
                }
            }

            node->cust_data = cust_data;
            node->on_release = node_on_release_struct;
            break;
        }

        case SN_MEMBER:
            if (node->right == NULL || node->right->name != SN_IDENT || node->right->token.text == NULL)
            {
                printf("Validation error: right node of SN_MEMBER must be SN_IDENT\n");
                abort();
            }

            break;

        case SN_FUNCTION:
        {
            TS_Val obj;

            /* validate argument list */
            if (node->right != NULL)
                for (i = 0; i < node->right->children_num; i++)
                    if (node->right->children[i]->name != SN_IDENT || node->right->children[i]->token.text == NULL)
                    {
                        printf("Validation error: expected SN_IDENT in function argument declaration\n");
                        abort();
                    }

            obj = TS_create_native(TS_FunctionNodeRef_name, node, NULL);

            node->cust_data = malloc(sizeof(TS_Val));
            memcpy(node->cust_data, &obj, sizeof(TS_Val));
            node->on_release = node_on_release_tsval;
            break;
        }
    }

    if (node->left != NULL)
        ast_finalize(node->left, context);

    if (node->right != NULL)
        ast_finalize(node->right, context);

    for (i = 0; i < node->children_num; i++)
        ast_finalize(node->children[i], context);
}

void ast_store_to(AstNode_t* node, ast_context_t* context, TS_Val val)
{
    /* remember to ALWAYS add a reference if the 'val' will be saved */

    switch (node->name)
    {
        case SN_IDENT:
        {
            TS_ObjectMember* member;

            member = TS_find_member(context->locals, (const char*) node->token.text);

            if (member != NULL)
            {
                TS_rlsvalue(member->val);
                member->val = TS_reference(val);
                break;
            }

            member = TS_find_member(context->globals, (const char*) node->token.text);

            if (member != NULL)
            {
                TS_rlsvalue(member->val);
                member->val = TS_reference(val);
                break;
            }

            if ( ((ident_cust_data*) node->cust_data)->is_global )
                TS_set_member(context->globals, (const char*) node->token.text, TS_reference(val));
            else
                TS_set_member(context->locals, (const char*) node->token.text, TS_reference(val));
            break;
        }

        case SN_INDEX:
        {
            TS_Val obj, key;

            obj = ast_eval(node->left, context);
            key = ast_eval(node->right, context);
            TS_set_entry(obj, key, TS_reference(val));
            TS_rlsvalue(obj);
            break;
        }

        case SN_MEMBER:
        {
            TS_Val obj;

            obj = ast_eval(node->left, context);
            TS_set_member(obj, (const char*) node->right->token.text, TS_reference(val));
            TS_rlsvalue(obj);
            break;
        }

        default:
            printf("Error: can't store to node type %i\n", node->name);
    }
}

#define AST_EVAL_BINARY_OP(node_name_, function_)\
        case node_name_:\
        {\
            TS_Val left, right, val;\
\
            left = ast_eval(node->left, context);\
            right = ast_eval(node->right, context);\
\
            val = function_(left, right);\
\
            TS_rlsvalue(left);\
            TS_rlsvalue(right);\
            return val;\
        }

TS_Val ast_eval(AstNode_t* node, ast_context_t* context)
{
    switch (node->name)
    {
        AST_EVAL_BINARY_OP(SN_ADD, TS_add)

        case SN_APPEND:
        {
            TS_Val left, right;

            left = ast_eval(node->left, context);
            right = ast_eval(node->right, context);
            
            if (left.type == TS_LIST)
            {
                TS_add_item(left.list, right);
                return left;
            }
            else if (left.type == TS_STRING && right.type == TS_STRING)
            {
                uint8_t *joined;
                size_t length;

                length = left.string->num_bytes + right.string->num_bytes;
                joined = (uint8_t *)malloc(length + 1);
                memcpy(joined, left.string->bytes, left.string->num_bytes);
                memcpy(joined + left.string->num_bytes, right.string->bytes, right.string->num_bytes + 1);

                TS_rlsvalue(left);
                TS_rlsvalue(right);
                return TS_create_string_using(joined, length);
            }
            else
            {
                TS_rlsvalue(left);
                TS_rlsvalue(right);
                return TS_null();
            }
        }

        case SN_ASSIGN:
        {
            TS_Val val;

            val = ast_eval(node->right, context);

            ast_store_to(node->left, context, val);
            return val;
        }

        //AST_EVAL_BINARY_OP(SN_BIN_AND, TS_bin_and)
        AST_EVAL_BINARY_OP(SN_BIN_OR, TS_bin_or)

        case SN_BREAK:
            context->should_break = 1;
            break;

        case SN_CALL:
        {
            TS_Val function, retval;

            function = ast_eval(node->left, context);

            if (function.type == TS_NATIVE && function.native->type_name == TS_FunctionNodeRef_name)
            {
                AstNode_t* func;
                ast_context_t new_context;
                size_t i;

                func = (AstNode_t*) function.native->cust_data;

                new_context.globals = TS_reference(context->globals);
                new_context.locals = TS_create_object(4);
                new_context.return_value = TS_null();
                new_context.should_break = 0;
                new_context.should_return = 0;

                if (node->left->name == SN_MEMBER)
                    TS_set_member(new_context.locals, "me", ast_eval(node->left->left, context));

                if (func->right != NULL)
                {
                    for (i = 0; i < func->right->children_num; i++)
                    {
                        if (i < node->right->children_num)
                            TS_set_member(new_context.locals, (const char*) func->right->children[i]->token.text, ast_eval(node->right->children[i], context));
                    }
                }

                /* should be null anyway */
                TS_rlsvalue(ast_eval( func->children[0], &new_context ));

                retval = new_context.return_value;

                TS_rlsvalue(new_context.globals);
                TS_rlsvalue(new_context.locals);
            }
            /*else if (function.type == TS_NATIVE && function.native->invoke != NULL)
            {
                TS_Val *arguments;
                size_t num_arguments, i;

                num_arguments = node->right->children_num;

#ifdef _MSC_VER
                arguments = (TS_Val*) _malloca(num_arguments * sizeof(TS_Val));
#else
                arguments = (TS_Val*) alloca(num_arguments * sizeof(TS_Val));
#endif

                for (i = 0; i < num_arguments; i++)
                    arguments[i] = ast_eval(node->right->children[i], context);

                retval = function.native->invoke(function, context->globals, arguments, num_arguments);

                for (i = 0; i < num_arguments; i++)
                    TS_rlsvalue(arguments[i]);

#ifdef _MSC_VER
                _freea(arguments);
#endif
            }*/
            else if (function.type == TS_NATIVEFUNC && function.native_func != NULL)
            {
                TS_CallContext ctx;
                TS_Val *arguments;
                size_t num_arguments, i;

                num_arguments = node->right->children_num;

#ifdef _MSC_VER
                arguments = (TS_Val*) _malloca(num_arguments * sizeof(TS_Val));
#else
                arguments = (TS_Val*) alloca(num_arguments * sizeof(TS_Val));
#endif

                for (i = 0; i < num_arguments; i++)
                    arguments[i] = ast_eval(node->right->children[i], context);

                ctx.globals = context->globals;

                if (node->left->name == SN_MEMBER)
                    ctx.me = ast_eval(node->left->left, context);
                else
                    ctx.me = TS_null();

                retval = ((TS_NativeFunction_t) function.native_func)(&ctx, arguments, num_arguments);

                TS_rlsvalue(ctx.me);

                for (i = 0; i < num_arguments; i++)
                    TS_rlsvalue(arguments[i]);

#ifdef _MSC_VER
                _freea(arguments);
#endif
            }
            else
            {
                printf("Error: uninvokable expression\n");
                abort();
            }

            TS_rlsvalue(function);
            return retval;
        }

        AST_EVAL_BINARY_OP(SN_DIVIDE, TS_divide)

        case SN_EQUALS:
        {
            TS_Val left, right;
            int is_zero;

            left = ast_eval(node->left, context);
            right = ast_eval(node->right, context);

            is_zero = TS_equals(left, right);

            TS_rlsvalue(left);
            TS_rlsvalue(right);

            return TS_bool(is_zero);
        }

        case SN_FALSE:
            return TS_bool(0);

        case SN_FUNCTION:
            return TS_reference( *((TS_Val*) node->cust_data) );

        case SN_IDENT:
        {
            TS_ObjectMember* member;

            member = TS_find_member(context->locals, (const char*) node->token.text);

            if (member != NULL)
                return TS_reference(member->val);

            member = TS_find_member(context->globals, (const char*) node->token.text);

            if (member != NULL)
                return TS_reference(member->val);

            break;
        }

        case SN_IF:
        {
            TS_Val condition;
            int is_zero;

            condition = ast_eval(node->left, context);
            is_zero = TS_is_zero(condition);
            TS_rlsvalue(condition);

            if (!is_zero)
                TS_rlsvalue(ast_eval(node->right, context));
            else if (node->children_num > 0)
                TS_rlsvalue(ast_eval(node->children[0], context));
            break;
        }

        case SN_INDEX:
        {
            TS_Val array, key, val;

            array = ast_eval(node->left, context);
            key = ast_eval(node->right, context);

            val = TS_get_entry(array, key);

            TS_rlsvalue(array);
            TS_rlsvalue(key);
            return val;
        }

        case SN_INT:
            return TS_int(node->token.number);

        case SN_ITERATE:
        {
            TS_Val list;
            size_t i;

            list = ast_eval(node->right, context);

            if (list.type == TS_LIST)
            {
                for(i = 0; i < list.list->num_items; i++)
                {
                    TS_set_member(context->locals, (const char *) node->left->token.text, TS_reference(list.list->items[i]));

                    TS_rlsvalue(ast_eval(node->children[0], context));
                }
            }
            else if (list.type == TS_STRING)
            {
                for(i = 0; i < list.string->num_bytes; i++)
                {
                    TS_set_member(context->locals, (const char *) node->left->token.text, TS_int(list.string->bytes[i]));

                    TS_rlsvalue(ast_eval(node->children[0], context));
                }
            }

            TS_rlsvalue(list);
            break;
        }

        case SN_LIST:
            if(node->children_num == 1)
                return ast_eval(node->children[0], context);
            else
            {
                TS_Val val;
                size_t i;

                val = TS_create_list(node->children_num + 3);

                for (i = 0; i < node->children_num; i++)
                    TS_add_item(val.list, ast_eval(node->children[i], context));

                return val;
            }

        case SN_MEMBER:
        {
            TS_Val val, member;

            val = ast_eval(node->left, context);
            member = TS_get_member(val, (const char*) node->right->token.text);
            TS_rlsvalue(val);

            return member;
        }

        AST_EVAL_BINARY_OP(SN_MULTIPLY, TS_multiply)

        case SN_NOT:
        {
            TS_Val left, val;

            left = ast_eval(node->left, context);
            val = TS_not(left);

            TS_rlsvalue(left);
            return val;
        }

        case SN_NOT_EQUALS:
        {
            TS_Val left, right;
            int is_zero;

            left = ast_eval(node->left, context);
            right = ast_eval(node->right, context);
            is_zero = !TS_equals(left, right);
            TS_rlsvalue(left);
            TS_rlsvalue(right);

            return TS_bool(is_zero);
        }

        case SN_NULL:
            return TS_null();

        case SN_OBJECT:
        {
            TS_Val val;
            size_t i;

            val = TS_create_object(node->children_num);

            for (i = 0; i < node->children_num; i++)
                TS_set_member(val, (const char *) node->children[i]->left->token.text, ast_eval(node->children[i]->right, context));

            return val;
        }

        case SN_REAL:
            return TS_float((float) node->token.decimal);

        case SN_RETURN:
            if (node->left != NULL)
                context->return_value = ast_eval(node->left, context);

            context->should_return = 1;
            break;

        case SN_BLOCK:
        case SN_SCRIPT:
        {
            size_t i;

            for (i = 0; i < node->children_num; i++)
            {
                TS_rlsvalue(ast_eval(node->children[i], context));

                if (context->should_break > 0 || context->should_return > 0)
                    break;
            }

            break;
        }

        case SN_STRING:
            /* TODO: reference a TS_Val stored in the AST Node */
            return TS_create_string((const char*) node->token.text);

        case SN_SUBTRACT:
        {
            TS_Val left, right, val;

            right = ast_eval(node->right, context);
            
            if (node->left != NULL)
            {
                left = ast_eval(node->left, context);
                val = TS_subtract(left, right);
                TS_rlsvalue(left);
            }
            else
                val = TS_negative(right);

            TS_rlsvalue(right);
            return val;
        }

        case SN_TRUE:
            return TS_bool(1);

        case SN_WHILE:
        {
            while (1)
            {
                TS_Val condition;
                int is_zero;

                condition = ast_eval(node->left, context);
                is_zero = TS_is_zero(condition);
                TS_rlsvalue(condition);

                if (is_zero)
                    break;

                TS_rlsvalue(ast_eval(node->right, context));

                if (context->should_break > 0)
                {
                    context->should_break--;
                    break;
                }
            }

            break;
        }
    }

    return TS_null();
}

void ast_exec(AstNode_t* ast)
{
    ast_finalize_context_t finalize_context;
    ast_context_t context;

    /* TODO: wrap the whole script in a function */

    finalize_context.script = ast;
    ast_finalize(ast, &finalize_context);

    /* set up context */
    context.globals = TS_create_object(4);
    context.locals = TS_create_object(4);
    context.return_value = TS_null();
    context.should_break = 0;
    context.should_return = 0;

    TS_set_member(context.globals, "create_file", TS_native_function(TS_func_create_file));
    TS_set_member(context.globals, "load_module", TS_native_function(TS_func_load_module));
    TS_set_member(context.globals, "open_file", TS_native_function(TS_func_open_file));
    TS_set_member(context.globals, "say", TS_native_function(TS_func_say));
    TS_set_member(context.globals, "_strdrop", TS_native_function(TS_func__strdrop));
    TS_set_member(context.globals, "_strexpand", TS_native_function(TS_func_expand));

    TS_rlsvalue(ast_eval(ast, &context));

    printf("\n");
    TS_printvalue(context.globals, 0);

    TS_rlsvalue(context.globals);
    TS_rlsvalue(context.locals);
}

int do_script(const char* filename)
{
    char* script;
    AstNode_t* ast;

    script = tf_load_file(filename, NULL);

    if (script == NULL)
        return -1;

    ast = parse(filename, script);
    free(script);

    if (ast != NULL)
    {
        //ast_serialize(ast, "ast.txt");
        ast_exec(ast);
        ast_release_node(&ast);
    }

    return 0;
}

int ssscanf(const char* string, int flags, const char* format, ...);

static const char *input_filename = NULL;

static int on_arg(int type, const char* arg, const char* ext)
{
    if (type == ARG_DEFAULT)
        /* TODO: What to do if multiple */
        input_filename = arg;

    return 0;
}

static void on_err(const char* arg)
{
    fprintf(stderr, "tinyscript: unrecognized argument `%s`\n", arg);
}

parse_args_t tinyscript_args = {
    "", "",
    NULL, NULL,

    on_arg,
    on_err
};

int main(int argc, char **argv)
{
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetBreakAlloc();
#endif

    if (parse_args(argc - 1, argv + 1, &tinyscript_args) < 0)
        return -1;

    if (input_filename == NULL)
    {
        fprintf(stderr, "tinyscript: Nothing to do.\n");
        return 0;
    }

    do_script(input_filename);
    return 0;
}
