#class_begin

static const char* ${fullname_c}_type_name = "${fullname}";

static void release_${fullname_c}(TS_Val val)
{
    //fclose((FILE*) val.native->cust_data);
}

static ${native_name}* unwrap_${fullname_c}(TS_Val val)
{
    if (val.type != TS_OBJECT || val.object->native == NULL || val.object->native->type_name != ${fullname_c}_type_name)
        return NULL;

    return (${native_name}*) val.object->native->cust_data;
}

#class_end_1
static TS_Val wrap_${fullname_c}(${native_name}* native, int release)
{
    TS_Val object;

    if (native == NULL)
        return TS_null();

    object = TS_create_object(4);
    object.object->native = TS_create_native_struct(${fullname_c}_type_name, native, release ? release_${fullname_c} : NULL);

#class_end_method
    TS_set_member(object, "${name}", TS_native_function(${native_name}));

#class_end_2
    return object;
}