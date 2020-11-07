
#ifdef _WIN32
#include <windows.h>
#endif

#include <gl/gl.h>

#include "..\..\include\tinyapi.h"

#include "..\..\src\tsval.c"

static TS_Val Begin(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 1 || !TS_IS_INT(arguments[0]))
        return TS_null();

    glBegin(arguments[0].intval);

    return TS_int(0);
}

static TS_Val Clear(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 1 || !TS_IS_INT(arguments[0]))
        return TS_null();

    glClear(arguments[0].intval);

    return TS_int(0);
}

static TS_Val ClearColor(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 4
            || !TS_IS_NUMERIC(arguments[0]) || !TS_IS_NUMERIC(arguments[1])
            || !TS_IS_NUMERIC(arguments[2]) || !TS_IS_NUMERIC(arguments[3]))
        return TS_null();

    glClearColor(TS_NUMERIC_AS_FLOAT(arguments[0]),
            TS_NUMERIC_AS_FLOAT(arguments[1]),
            TS_NUMERIC_AS_FLOAT(arguments[2]),
            TS_NUMERIC_AS_FLOAT(arguments[3]));

    return TS_int(0);
}

static TS_Val Color3bv(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    int value;

    if (num_arguments != 1 || !TS_IS_NUMERIC(arguments[0]))
        return TS_null();

    // assumes little-endian
    value = TS_NUMERIC_AS_INT(arguments[0]);
    glColor3bv((const GLbyte *) &value);

    return TS_int(0);
}

static TS_Val Color4bv(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    int value;

    if (num_arguments != 1 || !TS_IS_NUMERIC(arguments[0]))
        return TS_null();

    // assumes little-endian
    value = TS_NUMERIC_AS_INT(arguments[0]);
    glColor4bv((const GLbyte *) &value);

    return TS_int(0);
}

static TS_Val Colorf(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments < 3 || num_arguments > 4 || !TS_IS_NUMERIC(arguments[0]) || !TS_IS_NUMERIC(arguments[1]) || !TS_IS_NUMERIC(arguments[2]))
        return TS_null();

    if (num_arguments == 3)
        glColor3f(TS_NUMERIC_AS_FLOAT(arguments[0]), TS_NUMERIC_AS_FLOAT(arguments[1]), TS_NUMERIC_AS_FLOAT(arguments[2]));
    else if (num_arguments == 4)
    {
        if (!TS_IS_NUMERIC(arguments[3]))
            return TS_null();

        glColor4f(TS_NUMERIC_AS_FLOAT(arguments[0]), TS_NUMERIC_AS_FLOAT(arguments[1]), TS_NUMERIC_AS_FLOAT(arguments[2]), TS_NUMERIC_AS_FLOAT(arguments[3]));
    }

    return TS_int(0);
}

static TS_Val End(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 0)
        return TS_null();

    glEnd();

    return TS_int(0);
}

static TS_Val LoadIdentity(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 0)
        return TS_null();

    glLoadIdentity();

    return TS_int(0);
}

static TS_Val Ortho(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 6
            || !TS_IS_NUMERIC(arguments[0]) || !TS_IS_NUMERIC(arguments[1])
            || !TS_IS_NUMERIC(arguments[2]) || !TS_IS_NUMERIC(arguments[3])
            || !TS_IS_NUMERIC(arguments[4]) || !TS_IS_NUMERIC(arguments[5]))
        return TS_null();

    glOrtho(TS_NUMERIC_AS_FLOAT(arguments[0]),
            TS_NUMERIC_AS_FLOAT(arguments[1]),
            TS_NUMERIC_AS_FLOAT(arguments[2]),
            TS_NUMERIC_AS_FLOAT(arguments[3]),
            TS_NUMERIC_AS_FLOAT(arguments[4]),
            TS_NUMERIC_AS_FLOAT(arguments[5]));

    return TS_int(0);
}

static TS_Val Vertex(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments < 2 || num_arguments > 4 || !TS_IS_NUMERIC(arguments[0]) || !TS_IS_NUMERIC(arguments[1]))
        return TS_null();

    if (num_arguments == 2)
        glVertex2f(TS_NUMERIC_AS_FLOAT(arguments[0]), TS_NUMERIC_AS_FLOAT(arguments[1]));
    else if (num_arguments == 3)
    {
        if (!TS_IS_NUMERIC(arguments[2]))
            return TS_null();

        glVertex3f(TS_NUMERIC_AS_FLOAT(arguments[0]), TS_NUMERIC_AS_FLOAT(arguments[1]), TS_NUMERIC_AS_FLOAT(arguments[2]));
    }
    else if (num_arguments == 4)
    {
        if (!TS_IS_NUMERIC(arguments[2]) || !TS_IS_NUMERIC(arguments[3]))
            return TS_null();

        glVertex4f(TS_NUMERIC_AS_FLOAT(arguments[0]), TS_NUMERIC_AS_FLOAT(arguments[1]), TS_NUMERIC_AS_FLOAT(arguments[2]), TS_NUMERIC_AS_FLOAT(arguments[3]));
    }

    return TS_int(0);
}

static TS_Val Viewport(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 4
            || !TS_IS_NUMERIC(arguments[0]) || !TS_IS_NUMERIC(arguments[1])
            || !TS_IS_NUMERIC(arguments[2]) || !TS_IS_NUMERIC(arguments[3]))
        return TS_null();

    glViewport(TS_NUMERIC_AS_INT(arguments[0]),
            TS_NUMERIC_AS_INT(arguments[1]),
            TS_NUMERIC_AS_INT(arguments[2]),
            TS_NUMERIC_AS_INT(arguments[3]));

    return TS_int(0);
}

#define SET_CONST(name_) TS_set_member(gl, #name_, TS_int(GL_##name_));
#define SET_FUNC(name_) TS_set_member(gl, #name_, TS_native_function(name_));
#define SET_FUNC2(name_) TS_set_member(gl, #name_, TS_native_function(name_##Wrap));

__declspec(dllexport) TS_Val TS_ModuleEntry(const uint8_t* referenced_name, TS_Val globals)
{
    TS_Val gl;

    gl = TS_create_object(32);
    TS_set_member(globals, "gl", gl);

    SET_CONST(COLOR_BUFFER_BIT)
    SET_CONST(DEPTH_BUFFER_BIT)
    SET_CONST(LINES)
    SET_CONST(QUADS)
    SET_CONST(TRIANGLES)
    SET_CONST(TRIANGLE_STRIP)

    SET_FUNC(Begin)
    SET_FUNC(ClearColor)
    SET_FUNC(Clear)
    SET_FUNC(Color3bv)
    SET_FUNC(Color4bv)
    SET_FUNC(Colorf)
    SET_FUNC(LoadIdentity)
    SET_FUNC(Ortho)
    SET_FUNC(End)
    SET_FUNC(Vertex)
    SET_FUNC(Viewport)

    return TS_null();
}

#ifdef _WIN32
BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    return TRUE;
}
#endif