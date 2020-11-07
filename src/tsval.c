
#include <tsval.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void do_indent(int indent)
{
    int i;

    for (i = 0; i < indent; i++)
        printf("  ");
}

/*static char *strdup_ (const char *s) {
    char *d = (char*) malloc (strlen (s) + 1);   // Space for length plus nul
    if (d == NULL) return NULL;          // No memory
    strcpy (d,s);                        // Copy the characters
    return d;                            // Return the new string
}*/

static void TS_rlslist(TS_List* list);
static void TS_rlsobject(TS_Object* obj);

TS_Val TS_null()
{
    TS_Val val;

    val.type = TS_NULL;
    val.intval = 0;

    return val;
}

TS_Val TS_bool(int intval)
{
    TS_Val val;

    val.type = TS_BOOL;
    val.intval = intval;

    return val;
}

TS_Val TS_float(float floatval)
{
    TS_Val val;

    val.type = TS_FLOAT;
    val.floatval = floatval;

    return val;
}

TS_Val TS_int(int intval)
{
    TS_Val val;

    val.type = TS_INT;
    val.intval = intval;

    return val;
}

TS_Val TS_create_list(size_t capacity)
{
    TS_Val val;

    val.type = TS_LIST;
    val.list = (TS_List*) malloc(sizeof(TS_List));
    
    val.list->gc.num_references = 1;

    val.list->num_items = 0;
    val.list->capacity = capacity;

    if (capacity == 0)
        val.list->items = NULL;
    else
        val.list->items = (TS_Val*) malloc(capacity * sizeof(TS_Val));

    return val;
}

TS_Val TS_create_native(const char* type_name, void* cust_data, void (*on_destroy)(TS_Val val))
{
    TS_Val val;

    val.type = TS_NATIVE;
    val.native = TS_create_native_struct(type_name, cust_data, on_destroy);

    return val;
}

TS_Native* TS_create_native_struct(const char* type_name, void* cust_data, void (*on_destroy)(TS_Val val))
{
    TS_Native* native;

    native = (TS_Native*) malloc(sizeof(TS_Native));
    
    native->gc.num_references = 1;

    native->type_name = type_name;
    native->cust_data = cust_data;

    native->get_hash = NULL;
    native->get_member = NULL;
    native->invoke = NULL;
    native->on_destroy = on_destroy;
    native->printvalue = NULL;

    return native;
}

TS_Val TS_create_object(size_t max_members)
{
    TS_Val val;

    val.type = TS_OBJECT;
    val.object = (TS_Object*) malloc(sizeof(TS_Object));
    
    val.object->gc.num_references = 1;

    val.object->num_members = 0;
    val.object->max_members = max_members;

    if (max_members == 0)
        val.object->members = NULL;
    else
        val.object->members = (TS_ObjectMember*) malloc(max_members * sizeof(TS_ObjectMember));

    val.object->native = NULL;

    return val;
}

TS_Val TS_create_string(const char* string)
{
    if (string == NULL)
        return TS_create_string_using(NULL, 0);
    else
    {
        uint8_t *bytes;
        size_t num_bytes;

        num_bytes = strlen(string);
        bytes = (uint8_t *) malloc(num_bytes + 1);
        memcpy(bytes, string, num_bytes + 1);

        return TS_create_string_using(bytes, num_bytes);
    }
}

TS_Val TS_create_string_using(uint8_t *bytes, size_t num_bytes)
{
    TS_Val val;

    val.type = TS_STRING;
    val.string = (TS_String*) malloc(sizeof(TS_String));
    
    val.string->gc.num_references = 1;

    val.string->num_bytes = num_bytes;
    val.string->max_bytes = num_bytes;
    val.string->bytes = bytes;

    return val;
}

/* --- general --- */

void TS_obj_addmember(TS_Object* obj, TS_Val key, TS_Val val);

TS_Val TS_get_entry(TS_Val val, TS_Val key)
{
    if (val.type == TS_LIST)
    {
        if (TS_IS_NUMERIC(key))
        {
            int index;

            index = TS_NUMERIC_AS_INT(key);

            if (index >= 0 && (unsigned) index < val.list->num_items)
                return TS_reference(val.list->items[index]);
        }
    }
    else if (val.type == TS_OBJECT)
    {
        TS_ObjectMember *member;

        member = TS_find_member_2(val, key);

        if (member != NULL)
            return TS_reference(member->val);
        else
            return TS_null();
    }
    else if (val.type == TS_STRING)
    {
        if (TS_IS_NUMERIC(key))
        {
            int index;

            index = TS_NUMERIC_AS_INT(key);

            if (index >= 0 && (unsigned) index < val.string->num_bytes)
                return TS_int(val.string->bytes[index]);
        }
    }
    
    return TS_null();
}

void TS_set_entry(TS_Val val, TS_Val key, TS_Val value)
{
    if (val.type == TS_OBJECT)
    {
        TS_ObjectMember* member;

        member = TS_find_member_2(val, key);

        if (member != NULL)
        {
            TS_rlsvalue(member->val);
            member->val = value;
        }
        else
        {
            TS_obj_addmember(val.object, key, value);
            return;
        }
    }

    TS_rlsvalue(key);
}

int TS_equals(TS_Val left, TS_Val right)
{
    if (left.type != right.type)
        return 0;

    switch (left.type)
    {
        case TS_BOOL:
        case TS_INT:
            return left.intval == right.intval;

        case TS_STRING:
            if (left.string->num_bytes != right.string->num_bytes)
                return 0;

            return memcmp(left.string->bytes, right.string->bytes, right.string->num_bytes) == 0;

        case TS_NULL:
            return 1;
    }

    return 0;
}

void TS_printvalue(TS_Val val, int indent)
{
    switch (val.type)
    {
        case TS_BOOL:
            printf( "%s", val.intval ? "true" : "false" );
            break;

        case TS_FLOAT:
            printf("%g", val.floatval);
            break;

        case TS_INT:
            printf("%i", val.intval);
            break;

        case TS_LIST:
        {
            size_t i;

            printf( "(" );

            for (i = 0; i < val.list->num_items; i++)
            {
                TS_printvalue(val.list->items[i], indent + 1);

                if (i + 1 < val.list->num_items)
                    printf( ", " );
            }

            printf( ")" );
            break;
        }

        case TS_NATIVE:
            if (val.native->printvalue != NULL)
                val.native->printvalue(val);
            else
                printf( "<native: %s>", val.native->type_name );
            break;

        case TS_NATIVEFUNC:
            printf( "<native function @ %p>", val.native_func );
            break;

        case TS_NULL:
            printf( "null" );
            break;

        case TS_OBJECT:
        {
            size_t i;

            printf( "{\n" );

            for (i = 0; i < val.object->num_members; i++)
            {
                do_indent(indent + 1);
                TS_printvalue(val.object->members[i].key, 0);
                printf( ": " );
                TS_printvalue(val.object->members[i].val, indent + 1);

                if (i + 1 < val.object->num_members)
                    printf( ",\n" );
                else
                    printf( "\n" );
            }

            do_indent(indent);
            printf( "}" );
            break;
        }

        case TS_STRING:
            printf( "'%s'", val.string->bytes );
            break;
    }
}

TS_Val TS_reference(TS_Val val)
{
    switch (val.type)
    {
        case TS_LIST: val.list->gc.num_references++; break;
        case TS_NATIVE: val.native->gc.num_references++; break;
        case TS_OBJECT: val.object->gc.num_references++; break;
        case TS_STRING: val.string->gc.num_references++; break;
    }

    return val;
}

void TS_rlsvalue(TS_Val val)
{
    switch (val.type)
    {
        case TS_LIST:
            if (val.list->gc.num_references == 1)
                TS_rlslist(val.list);
            else
                val.list->gc.num_references--;
            break;

        case TS_NATIVE:
            if (val.native->gc.num_references == 1)
            {
                if (val.native->on_destroy != NULL)
                    val.native->on_destroy(val);

                free(val.native);
            }
            else
                val.native->gc.num_references--;
            break;

        case TS_OBJECT:
            if (val.object->gc.num_references == 1)
            {
                if (val.object->native != NULL)
                {
                    if (val.object->native->gc.num_references == 1)
                    {
                        if (val.object->native->on_destroy != NULL)
                            val.object->native->on_destroy(val);

                        free(val.object->native);
                    }
                    else
                        val.object->native->gc.num_references--;
                }

                TS_rlsobject(val.object);
            }
            else
                val.object->gc.num_references--;
            break;

        case TS_STRING:
            if (val.string->gc.num_references == 1)
            {
                free(val.string->bytes);
                free(val.string);
            }
            else
                val.string->gc.num_references--;
            break;
    }
}

/* --- operations --- */
TS_Val TS_add(TS_Val left, TS_Val right)
{
    /* int + int => int */
    if ((left.type == TS_BOOL || left.type == TS_INT) && (right.type == TS_BOOL || right.type == TS_INT))
        return TS_int(left.intval + right.intval);

    /* any num + any num => real */
    if (!TS_IS_NUMERIC_OR_BOOL(left) || !TS_IS_NUMERIC_OR_BOOL(right))
        return TS_null();

    return TS_float(TS_NUMERIC_AS_FLOAT(left) + TS_NUMERIC_AS_FLOAT(right));
}

TS_Val TS_bin_or(TS_Val left, TS_Val right)
{
    /* any num as int | any num as int => int */
    if (!TS_IS_NUMERIC_OR_BOOL(left) || !TS_IS_NUMERIC_OR_BOOL(right))
        return TS_null();

    return TS_int(TS_NUMERIC_AS_INT(left) | TS_NUMERIC_AS_INT(right));
}

TS_Val TS_divide(TS_Val left, TS_Val right)
{
    /* num * num => real */
    if (!TS_IS_NUMERIC(left) || !TS_IS_NUMERIC(right))
        return TS_null();

    return TS_float(TS_NUMERIC_AS_FLOAT(left) / TS_NUMERIC_AS_FLOAT(right));
}

int TS_is_zero(TS_Val val)
{
    return val.type == TS_NULL || ((val.type == TS_BOOL || val.type == TS_INT) && val.intval == 0) || (val.type == TS_FLOAT && val.floatval == 0.0f);
}

TS_Val TS_multiply(TS_Val left, TS_Val right)
{
    /* int * int => int */
    if ((left.type == TS_BOOL || left.type == TS_INT) && (right.type == TS_BOOL || right.type == TS_INT))
        return TS_int(left.intval * right.intval);

    /* any num * any num => real */
    if (!TS_IS_NUMERIC_OR_BOOL(left) || !TS_IS_NUMERIC_OR_BOOL(right))
        return TS_null();

    return TS_float(TS_NUMERIC_AS_FLOAT(left) * TS_NUMERIC_AS_FLOAT(right));
}

TS_Val TS_negative(TS_Val val)
{
    if (val.type == TS_INT)
        return TS_int(-val.intval);
    else if (val.type == TS_FLOAT)
        return TS_float(-val.floatval);
    else
        return TS_null();
}

TS_Val TS_not(TS_Val val)
{
    if (val.type == TS_BOOL || val.type == TS_INT || val.type == TS_NULL)
        return TS_int(!val.intval);
    else if (val.type == TS_FLOAT)
        return TS_float(val.floatval == 0.0f ? 1.0f : 0.0f);
    else
        return TS_null();
}

TS_Val TS_subtract(TS_Val left, TS_Val right)
{
    /* int - int => int */
    if ((left.type == TS_BOOL || left.type == TS_INT) && (right.type == TS_BOOL || right.type == TS_INT))
        return TS_int(left.intval - right.intval);

    /* any num - any num => real */
    if (!TS_IS_NUMERIC_OR_BOOL(left) || !TS_IS_NUMERIC_OR_BOOL(right))
        return TS_null();

    return TS_float(TS_NUMERIC_AS_FLOAT(left) - TS_NUMERIC_AS_FLOAT(right));
}

/* --- list --- */
void TS_add_item(TS_List *list, TS_Val item)
{
    if (list->num_items + 1 > list->capacity)
    {
        list->capacity = (list->capacity == 0) ? 4 : (list->capacity * 2);
        list->items = (TS_Val*) realloc(list->items, list->capacity * sizeof(TS_Val));
    }

    list->items[list->num_items] = item;
    list->num_items++;
}

/* --- string --- */
void TS_string_appendchar(TS_String *str, char c)
{
    if (str->num_bytes + 1 >= str->max_bytes)
    {
        str->max_bytes = (str->max_bytes == 0) ? 4 : (str->max_bytes * 2);
        str->bytes = (uint8_t *)realloc(str->bytes, str->max_bytes);
    }

    str->bytes[str->num_bytes++] = c;
    str->bytes[str->num_bytes] = 0;
}

void TS_string_appendutf8(TS_String *str, const char* string)
{
    size_t l;

    l = strlen(string);

    if (str->num_bytes + l + 1 >= str->max_bytes)
    {
        while (str->num_bytes + l + 1 >= str->max_bytes)
            str->max_bytes = (str->max_bytes == 0) ? 4 : (str->max_bytes * 2);

        str->bytes = (uint8_t *)realloc(str->bytes, str->max_bytes);
    }

    memcpy(str->bytes + str->num_bytes, string, l + 1);
    str->num_bytes += l;
}

/* --- objects --- */

TS_ObjectMember* TS_find_member(TS_Val val, const char* name)
{
    size_t i;

    if (val.type != TS_OBJECT)
        return NULL;

    for (i = 0; i < val.object->num_members; i++)
    {
        if (val.object->members[i].key.type == TS_STRING
                && strcmp((const char*) val.object->members[i].key.string->bytes, name) == 0)
        {
            return &val.object->members[i];
        }
    }

    return NULL;
}

TS_ObjectMember* TS_find_member_2(TS_Val val, TS_Val key)
{
    size_t i;

    if (val.type != TS_OBJECT)
        return NULL;

    for (i = 0; i < val.object->num_members; i++)
    {
        if (TS_equals(val.object->members[i].key, key))
            return &val.object->members[i];
    }

    return NULL;
}

TS_Val TS_get_member(TS_Val val, const char* name)
{
    TS_ObjectMember* member;

    if (val.type == TS_NATIVE)
    {
        if (val.native->get_member != NULL)
            return val.native->get_member(val, name);
        else
            return TS_null();
    }

    member = TS_find_member(val, name);

    if (member != NULL)
        return TS_reference(member->val);
    else
        return TS_null();
}

void TS_obj_addmember(TS_Object* obj, TS_Val key, TS_Val val)
{
    if (obj->num_members + 1 > obj->max_members)
    {
        obj->max_members = (obj->max_members == 0) ? 4 : (obj->max_members * 2);
        obj->members = (TS_ObjectMember*) realloc(obj->members, obj->max_members * sizeof(TS_ObjectMember));
    }

    obj->members[obj->num_members].key = key;
    obj->members[obj->num_members].val = val;
    obj->members[obj->num_members].cust_get_val = NULL;
    obj->members[obj->num_members].cust_set_val = NULL;
    obj->num_members++;
}

int TS_set_member(TS_Val val, const char* name, TS_Val new_val)
{
    if (val.type == TS_OBJECT)
    {
        TS_ObjectMember* member;

        member = TS_find_member(val, name);

        if (member != NULL)
        {
            TS_rlsvalue(member->val);
            member->val = new_val;
        }
        else
            TS_obj_addmember(val.object, TS_create_string(name), new_val);

        return 0;
    }

    return -1;
}

/* --- releasing --- */

static void TS_rlslist(TS_List* list)
{
    size_t i;

    for (i = 0; i < list->num_items; i++)
        TS_rlsvalue(list->items[i]);

    free(list->items);
    free(list);
}

static void TS_rlsobject(TS_Object* obj)
{
    size_t i;

    for (i = 0; i < obj->num_members; i++)
    {
        TS_rlsvalue(obj->members[i].key);
        TS_rlsvalue(obj->members[i].val);
    }

    free(obj->members);
    free(obj);
}

TS_Val TS_native_function(TS_NativeFunction_t invoke)
{
    TS_Val native_function;

    native_function.type = TS_NATIVEFUNC;
    native_function.native_func = (TS_Callback_t) invoke;

    return native_function;
}