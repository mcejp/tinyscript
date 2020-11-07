
#ifdef _WIN32
#include <windows.h>
#endif

#include <SDL.h>
#include <SDL_syswm.h>

#include "..\..\include\tinyapi.h"

#include "..\..\src\tsval.c"

static const char* SDL_Surface_type_name = "SDL_Surface";

static void release_surface(TS_Val val)
{
    SDL_FreeSurface((SDL_Surface*) val.native->cust_data);
}

static int unwrap_rect(TS_Val val, SDL_Rect* rect)
{
    if (val.type != TS_OBJECT)
        return -1;

    rect->x = TS_get_member(val, "x").intval;
    rect->y = TS_get_member(val, "y").intval;
    rect->w = TS_get_member(val, "w").intval;
    rect->h = TS_get_member(val, "h").intval;

    return 0;
}

static SDL_Surface* unwrap_surface(TS_Val val)
{
    if (val.type != TS_NATIVE || val.native->type_name != SDL_Surface_type_name)
        return NULL;

    return (SDL_Surface*) val.native->cust_data;
}

static TS_Val wrap_surface(SDL_Surface* surface, int release)
{
    if (surface == NULL)
        return TS_null();

    return TS_create_native("SDL_Surface", surface, release ? release_surface : NULL);
}

static TS_Val BlitSurface(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    SDL_Surface* src, * dst;
    SDL_Rect srcrect, dstrect;

    int have_srcrect, have_dstrect;

    if (num_arguments != 4 || (src = unwrap_surface(arguments[0])) == NULL || (dst = unwrap_surface(arguments[2])) == NULL)
        return TS_null();

    have_srcrect = (unwrap_rect(arguments[1], &srcrect) >= 0);
    have_dstrect = (unwrap_rect(arguments[3], &dstrect) >= 0);

    return TS_int(SDL_BlitSurface(src, have_srcrect ? &srcrect : NULL, dst, have_dstrect ? &dstrect : NULL));
}

static TS_Val CreateRGBSurface(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    SDL_Surface* surface;

    if (num_arguments != 8 || arguments[0].type != TS_INT || arguments[1].type != TS_INT
            || arguments[2].type != TS_INT || arguments[3].type != TS_INT
            || arguments[4].type != TS_INT || arguments[5].type != TS_INT
            || arguments[6].type != TS_INT || arguments[7].type != TS_INT)
        return TS_null();

    surface = SDL_CreateRGBSurface(arguments[0].intval, arguments[1].intval, arguments[2].intval, arguments[3].intval,
            arguments[4].intval, arguments[5].intval, arguments[6].intval, arguments[7].intval);

    return wrap_surface(surface, 0);
}

static TS_Val Delay(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 1 || arguments[0].type != TS_INT)
        return TS_null();

    SDL_Delay(arguments[0].intval);
    return TS_int(0);
}

static TS_Val FillRectWrap(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    SDL_Surface* surface;
    SDL_Rect rect;

    if (num_arguments != 3 || arguments[2].type != TS_INT)
        return TS_null();

    surface = unwrap_surface(arguments[0]);

    if (surface == NULL)
        return TS_null();

    if (unwrap_rect(arguments[1], &rect) < 0)
        return TS_null();

    return TS_int(SDL_FillRect(surface, &rect, arguments[2].intval));
}

static TS_Val Flip(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    SDL_Surface* surface;

    if (num_arguments != 1)
        return TS_null();

    surface = unwrap_surface(arguments[0]);

    if (surface == NULL)
        return TS_null();

    return TS_int(SDL_Flip(surface));
}

static TS_Val GL_SwapBuffers(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 0)
        return TS_null();

    SDL_GL_SwapBuffers();
    return TS_int(0);
}

static TS_Val Init(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 1 || arguments[0].type != TS_INT)
        return TS_null();

    return TS_int(SDL_Init(arguments[0].intval));
}

static TS_Val LoadBMP(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 1 || arguments[0].type != TS_STRING)
        return TS_null();

    return wrap_surface(SDL_LoadBMP((const char *) arguments[0].string->bytes), 1);
}

static TS_Val MaximizeWindow(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    SDL_SysWMinfo info;

    if (num_arguments != 0)
        return TS_null();

    SDL_VERSION(&info.version);
    SDL_GetWMInfo(&info);
    ShowWindow(info.window, SW_MAXIMIZE);
    return TS_int(0);
}

static TS_Val PollEvent(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    TS_Val eventobj;
    SDL_Event event;

    if (num_arguments != 0)
        return TS_null();

    if (SDL_PollEvent(&event))
    {
        eventobj = TS_create_object(2);
        TS_set_member(eventobj, "type", TS_int(event.type));

        if (event.type == SDL_MOUSEMOTION)
        {
            TS_Val motionobj;

            motionobj = TS_create_object(2);
            TS_set_member(eventobj, "motion", motionobj);

            TS_set_member(motionobj, "x", TS_int(event.motion.x));
            TS_set_member(motionobj, "y", TS_int(event.motion.y));
        }
        else if (event.type == SDL_VIDEORESIZE)
        {
            TS_Val resizeobj;

            resizeobj = TS_create_object(2);
            TS_set_member(eventobj, "resize", resizeobj);

            TS_set_member(resizeobj, "w", TS_int(event.resize.w));
            TS_set_member(resizeobj, "h", TS_int(event.resize.h));
        }

        return eventobj;
    }
    else
        return TS_null();
}

static TS_Val Rect(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    TS_Val rectobj;

    if (num_arguments != 4)
        return TS_null();

    rectobj = TS_create_object(4);
    TS_set_member(rectobj, "x", TS_reference(arguments[0]));
    TS_set_member(rectobj, "y", TS_reference(arguments[1]));
    TS_set_member(rectobj, "w", TS_reference(arguments[2]));
    TS_set_member(rectobj, "h", TS_reference(arguments[3]));
    return rectobj;
}

static TS_Val Quit(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    if (num_arguments != 0)
        return TS_null();

    SDL_Quit();
    return TS_int(0);
}

static TS_Val SetVideoMode(TS_CallContext *ctx, TS_Val* arguments, size_t num_arguments)
{
    SDL_Surface* surface;

    if (num_arguments != 4 || arguments[0].type != TS_INT || arguments[1].type != TS_INT
            || arguments[2].type != TS_INT || arguments[3].type != TS_INT)
        return TS_null();

    surface = SDL_SetVideoMode(arguments[0].intval, arguments[1].intval, arguments[2].intval, arguments[3].intval);

    return wrap_surface(surface, 0);
}

#define SET_CONST(name_)    TS_set_member(SDL, #name_, TS_int(SDL_##name_));
#define SET_FUNC(name_)     TS_set_member(SDL, #name_, TS_native_function(name_));
#define SET_FUNC2(name_)    TS_set_member(SDL, #name_, TS_native_function(name_##Wrap));

__declspec(dllexport) TS_Val TS_ModuleEntry(const uint8_t* referenced_name, TS_Val globals)
{
    TS_Val SDL;

    SDL = TS_create_object(32);
    TS_set_member(globals, "SDL", SDL);

    SET_CONST(HWSURFACE)
    SET_CONST(INIT_VIDEO)
    SET_CONST(MOUSEMOTION)
    SET_CONST(OPENGL)
    SET_CONST(QUIT)
    SET_CONST(NOFRAME)
    SET_CONST(RESIZABLE)
    SET_CONST(SWSURFACE)
    SET_CONST(VIDEORESIZE)

    SET_FUNC(BlitSurface)
    SET_FUNC(CreateRGBSurface)
    SET_FUNC(Delay)
    SET_FUNC2(FillRect)
    SET_FUNC(Flip)
    SET_FUNC(GL_SwapBuffers)
    SET_FUNC(Init)
    SET_FUNC(LoadBMP)
    SET_FUNC(MaximizeWindow)
    SET_FUNC(PollEvent)
    SET_FUNC(Quit)
    SET_FUNC(Rect)
    SET_FUNC(SetVideoMode)

    return TS_null();
}

#ifdef _WIN32
BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    return TRUE;
}
#endif