#pragma once

#include <stdint.h>
#include <stdlib.h>

enum { TS_NULL, TS_BOOL, TS_FLOAT, TS_INT, TS_LIST, TS_NATIVE, TS_NATIVEFUNC, TS_OBJECT, /*TS_STR_CONST,*/ TS_STRING };

#define TS_IS_INT(val_) ((val_).type == TS_INT)
#define TS_IS_NUMERIC(val_) ((val_).type == TS_FLOAT || (val_).type == TS_INT)
#define TS_IS_NUMERIC_OR_BOOL(val_) ((val_).type == TS_BOOL || (val_).type == TS_FLOAT || (val_).type == TS_INT)
#define TS_NUMERIC_AS_INT(val_) (((val_).type == TS_FLOAT) ? (int)(val_).floatval : (val_).intval)
#define TS_NUMERIC_AS_FLOAT(val_) (((val_).type == TS_FLOAT) ? (val_).floatval : (float)(val_).intval)

typedef void (*TS_Callback_t)();
typedef struct TS_CallContext TS_CallContext;
typedef struct TS_List TS_List;
typedef struct TS_Native TS_Native;
typedef struct TS_Object TS_Object;
typedef struct TS_String TS_String;

typedef struct TS_ObjectMember TS_ObjectMember;

typedef struct
{
    size_t num_references;
}
TS_GC;

typedef struct
{
    int type;

    union {
        int intval;
        float floatval;

        TS_List* list;
        TS_Native* native;
        TS_Callback_t native_func;
        TS_Object* object;
        TS_String* string;
    };
}
TS_Val;

typedef TS_Val (*TS_NativeFunction_t)(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments);

struct TS_CallContext
{
    TS_Val globals;
    TS_Val me;
};

struct TS_ObjectMember
{
    TS_Val key, val;

    TS_Val (*cust_get_val)(TS_ObjectMember*);
    void (*cust_set_val)(TS_ObjectMember*, TS_Val);
};

struct TS_List
{
    TS_GC gc;

    size_t num_items, capacity;
    TS_Val* items;
};

struct TS_Native
{
    TS_GC gc;

    const char* type_name;
    void* cust_data;

    uint32_t (*get_hash)(TS_Val val);
    TS_Val (*get_member)(TS_Val val, const char* name);
    TS_Val (*invoke)(TS_Val val, TS_Val globals, TS_Val* arguments, size_t num_arguments);
    void (*on_destroy)(TS_Val val);
    void (*printvalue)(TS_Val val);
};

struct TS_Object
{
    TS_GC gc;

    size_t num_members, max_members;
    TS_ObjectMember* members;

    TS_Native* native;
};

struct TS_String
{
    TS_GC gc;

    size_t num_bytes, max_bytes;
    uint8_t* bytes;
};

/* create */
TS_Val TS_null();
TS_Val TS_bool(int intval);
TS_Val TS_float(float floatval);
TS_Val TS_int(int intval);
TS_Val TS_create_list(size_t capacity);
TS_Val TS_create_native(const char* type_name, void* cust_data, void (*on_destroy)(TS_Val val));
TS_Native* TS_create_native_struct(const char* type_name, void* cust_data, void (*on_destroy)(TS_Val val));
TS_Val TS_create_object(size_t max_members);
TS_Val TS_create_string(const char* string);
TS_Val TS_create_string_using(uint8_t *bytes, size_t num_bytes);

/* general */
TS_Val TS_get_entry(TS_Val val, TS_Val key);
int TS_is_zero(TS_Val val);
void TS_printvalue(TS_Val val, int indent);
TS_Val TS_reference(TS_Val val);
void TS_rlsvalue(TS_Val val);
void TS_set_entry(TS_Val val, TS_Val key, TS_Val value);

/* operations */
TS_Val TS_add(TS_Val left, TS_Val right);
TS_Val TS_bin_or(TS_Val left, TS_Val right);
TS_Val TS_divide(TS_Val left, TS_Val right);
int TS_equals(TS_Val left, TS_Val right);
int TS_equals(TS_Val left, TS_Val right);
TS_Val TS_multiply(TS_Val left, TS_Val right);
TS_Val TS_negative(TS_Val val);
TS_Val TS_not(TS_Val val);
TS_Val TS_subtract(TS_Val left, TS_Val right);

/* lists*/
void TS_add_item(TS_List *list, TS_Val item);

/* objects */
TS_ObjectMember* TS_find_member(TS_Val val, const char* name);
TS_ObjectMember* TS_find_member_2(TS_Val val, TS_Val key);
TS_Val TS_get_member(TS_Val val, const char* name);
int TS_set_member(TS_Val val, const char* name, TS_Val new_val);

TS_Val TS_native_function(TS_NativeFunction_t invoke);

/* string */
void TS_string_appendchar(TS_String *str, char c);
void TS_string_appendutf8(TS_String *str, const char* string);