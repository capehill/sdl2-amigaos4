/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "SDL_config.h"
#include "SDL_dynapi.h"

#if SDL_DYNAMIC_API

#include "SDL.h"

/* !!! FIXME: Shouldn't these be included in SDL.h? */
#include "SDL_shape.h"
#include "SDL_syswm.h"

/* This is the version of the dynamic API. This doesn't match the SDL version
   and should not change until there's been a major revamp in API/ABI.
   So 2.0.5 adds functions over 2.0.4? This number doesn't change;
   the sizeof (jump_table) changes instead. But 2.1.0 changes how a function
   works in an incompatible way or removes a function? This number changes,
   since sizeof (jump_table) isn't sufficient anymore. It's likely
   we'll forget to bump every time we add a function, so this is the
   failsafe switch for major API change decisions. Respect it and use it
   sparingly. */
#define SDL_DYNAPI_VERSION 1

static void SDL_InitDynamicAPI(void);


/* BE CAREFUL CALLING ANY SDL CODE IN HERE, IT WILL BLOW UP.
   Even self-contained stuff might call SDL_Error and break everything. */


/* behold, the macro salsa! */

/* !!! FIXME: ...disabled...until we write it.  :) */
#define DISABLE_JUMP_MAGIC 1

#if DISABLE_JUMP_MAGIC
/* Can't use the macro for varargs nonsense. This is atrocious. */
#define SDL_DYNAPI_VARARGS_LOGFN(_static, name, initcall, logname, prio) \
    _static void SDL_Log##logname##name(int category, SDL_PRINTF_FORMAT_STRING const char *fmt, ...) { \
        va_list ap; initcall; va_start(ap, fmt); \
        jump_table.SDL_LogMessageV(category, SDL_LOG_PRIORITY_##prio, fmt, ap); \
        va_end(ap); \
    }

#define SDL_DYNAPI_VARARGS(_static, name, initcall) \
    _static int SDL_SetError##name(SDL_PRINTF_FORMAT_STRING const char *fmt, ...) { \
        char buf[512]; /* !!! FIXME: dynamic allocation */ \
        va_list ap; initcall; va_start(ap, fmt); \
        jump_table.SDL_vsnprintf(buf, sizeof (buf), fmt, ap); \
        va_end(ap); \
        return jump_table.SDL_SetError("%s", buf); \
    } \
    _static int SDL_sscanf##name(const char *buf, SDL_SCANF_FORMAT_STRING const char *fmt, ...) { \
        int retval; va_list ap; initcall; va_start(ap, fmt); \
        retval = jump_table.SDL_vsscanf(buf, fmt, ap); \
        va_end(ap); \
        return retval; \
    } \
    _static int SDL_snprintf##name(SDL_OUT_Z_CAP(maxlen) char *buf, size_t maxlen, SDL_PRINTF_FORMAT_STRING const char *fmt, ...) { \
        int retval; va_list ap; initcall; va_start(ap, fmt); \
        retval = jump_table.SDL_vsnprintf(buf, maxlen, fmt, ap); \
        va_end(ap); \
        return retval; \
    } \
    _static void SDL_Log##name(SDL_PRINTF_FORMAT_STRING const char *fmt, ...) { \
        va_list ap; initcall; va_start(ap, fmt); \
        jump_table.SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, ap); \
        va_end(ap); \
    } \
    _static void SDL_LogMessage##name(int category, SDL_LogPriority priority, SDL_PRINTF_FORMAT_STRING const char *fmt, ...) { \
        va_list ap; initcall; va_start(ap, fmt); \
        jump_table.SDL_LogMessageV(category, priority, fmt, ap); \
        va_end(ap); \
    } \
    SDL_DYNAPI_VARARGS_LOGFN(_static, name, initcall, Verbose, VERBOSE) \
    SDL_DYNAPI_VARARGS_LOGFN(_static, name, initcall, Debug, DEBUG) \
    SDL_DYNAPI_VARARGS_LOGFN(_static, name, initcall, Info, INFO) \
    SDL_DYNAPI_VARARGS_LOGFN(_static, name, initcall, Warn, WARN) \
    SDL_DYNAPI_VARARGS_LOGFN(_static, name, initcall, Error, ERROR) \
    SDL_DYNAPI_VARARGS_LOGFN(_static, name, initcall, Critical, CRITICAL)
#endif


/* Typedefs for function pointers for jump table, and predeclare funcs */
/* The DEFAULT funcs will init jump table and then call real function. */
/* The REAL funcs are the actual functions, name-mangled to not clash. */
#define SDL_DYNAPI_PROC(rc,fn,params,args,ret) \
    typedef rc (*SDL_DYNAPIFN_##fn) params; \
    static rc fn##_DEFAULT params; \
    extern rc fn##_REAL params;
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC

/* The jump table! */
typedef struct {
    #define SDL_DYNAPI_PROC(rc,fn,params,args,ret) SDL_DYNAPIFN_##fn fn;
    #include "SDL_dynapi_procs.h"
    #undef SDL_DYNAPI_PROC
} SDL_DYNAPI_jump_table;

/* Predeclare the default functions for initializing the jump table. */
#define SDL_DYNAPI_PROC(rc,fn,params,args,ret) static rc fn##_DEFAULT params;
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC

/* The actual jump table. */
static SDL_DYNAPI_jump_table jump_table = {
    #define SDL_DYNAPI_PROC(rc,fn,params,args,ret) fn##_DEFAULT,
    #include "SDL_dynapi_procs.h"
    #undef SDL_DYNAPI_PROC
};

/* Default functions init the function table then call right thing. */
#if DISABLE_JUMP_MAGIC
#define SDL_DYNAPI_PROC(rc,fn,params,args,ret) \
    static rc fn##_DEFAULT params { \
        SDL_InitDynamicAPI(); \
        ret jump_table.fn args; \
    }
#define SDL_DYNAPI_PROC_NO_VARARGS 1
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
#undef SDL_DYNAPI_PROC_NO_VARARGS
SDL_DYNAPI_VARARGS(static, _DEFAULT, SDL_InitDynamicAPI())
#else
/* !!! FIXME: need the jump magic. */
#error Write me.
#endif

/* Public API functions to jump into the jump table. */
#if DISABLE_JUMP_MAGIC
#define SDL_DYNAPI_PROC(rc,fn,params,args,ret) \
    rc fn params { ret jump_table.fn args; }
#define SDL_DYNAPI_PROC_NO_VARARGS 1
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
#undef SDL_DYNAPI_PROC_NO_VARARGS
SDL_DYNAPI_VARARGS(,,)
#else
/* !!! FIXME: need the jump magic. */
#error Write me.
#endif



/* Here's the exported entry point that fills in the jump table. */
/*  Use specific types when an "int" might suffice to keep this sane. */
typedef Sint32 (SDLCALL *SDL_DYNAPI_ENTRYFN)(Uint32 apiver, void *table, Uint32 tablesize);
extern DECLSPEC Sint32 SDLCALL SDL_DYNAPI_entry(Uint32, void *, Uint32);

Sint32
SDL_DYNAPI_entry(Uint32 apiver, void *table, Uint32 tablesize)
{
    SDL_DYNAPI_jump_table *output_jump_table = (SDL_DYNAPI_jump_table *) table;

    if (apiver != SDL_DYNAPI_VERSION) {
        /* !!! FIXME: can maybe handle older versions? */
        return -1;  /* not compatible. */
    } else if (tablesize > sizeof (jump_table)) {
        return -1;  /* newer version of SDL with functions we can't provide. */
    }

    /* Init our jump table first. */
    #define SDL_DYNAPI_PROC(rc,fn,params,args,ret) jump_table.fn = fn##_REAL;
    #include "SDL_dynapi_procs.h"
    #undef SDL_DYNAPI_PROC

    /* Then the external table... */
    if (output_jump_table != &jump_table) {
        jump_table.SDL_memcpy(output_jump_table, &jump_table, tablesize);
    }

    /* Safe to call SDL functions now; jump table is initialized! */

    return 0;  /* success! */
}


/* Obviously we can't use SDL_LoadObject() to load SDL.  :)  */
/* Also obviously, we never close the loaded library. */
#if defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
static SDL_INLINE void *get_sdlapi_entry(const char *fname, const char *sym)
{
    HANDLE lib = LoadLibraryA(fname);
    void *retval = NULL;
    if (lib) {
        retval = GetProcAddress(lib, sym);
        if (retval == NULL) {
            FreeLibrary(lib);
        }
    }
    return retval;
}

#elif defined(__HAIKU__)
#include <os/kernel/image.h>
static SDL_INLINE void *get_sdlapi_entry(const char *fname, const char *sym)
{
    image_id lib = load_add_on(fname);
    void *retval = NULL;
    if (lib >= 0) {
        if (get_image_symbol(lib, sym, B_SYMBOL_TYPE_TEXT, &retval) != B_NO_ERROR) {
            unload_add_on(lib);
            retval = NULL;
        }
    }
    return retval;
}
#elif defined(unix) || defined(__unix__) || defined(__APPLE__) //|| defined(__amigaos4__)
#include <dlfcn.h>
static SDL_INLINE void *get_sdlapi_entry(const char *fname, const char *sym)
{
    void *lib = dlopen(fname, RTLD_NOW | RTLD_LOCAL);
    void *retval = NULL;
    if (lib != NULL) {
        retval = dlsym(lib, sym);
        if (retval == NULL) {
            dlclose(lib);
        }
    }
    return retval;
}
#else
#error Please define your platform.
#endif


static void
SDL_InitDynamicAPILocked(void)
{
    const char *libname = SDL_getenv_REAL("SDL_DYNAMIC_API");
    SDL_DYNAPI_ENTRYFN entry = SDL_DYNAPI_entry;  /* funcs from here by default. */

    if (libname) {
        entry = (SDL_DYNAPI_ENTRYFN) get_sdlapi_entry(libname, "SDL_DYNAPI_entry");
        if (!entry) {
            /* !!! FIXME: fail to startup here instead? */
            /* !!! FIXME: definitely warn user. */
            /* Just fill in the function pointers from this library. */
            entry = SDL_DYNAPI_entry;
        }
    }

    if (entry(SDL_DYNAPI_VERSION, &jump_table, sizeof (jump_table)) < 0) {
        /* !!! FIXME: fail to startup here instead? */
        /* !!! FIXME: definitely warn user. */
        /* Just fill in the function pointers from this library. */
        if (entry != SDL_DYNAPI_entry) {
            if (!SDL_DYNAPI_entry(SDL_DYNAPI_VERSION, &jump_table, sizeof (jump_table))) {
                /* !!! FIXME: now we're screwed. Should definitely abort now. */
            }
        }
    }

    /* we intentionally never close the newly-loaded lib, of course. */
}

static void
SDL_InitDynamicAPI(void)
{
    /* So the theory is that every function in the jump table defaults to
     *  calling this function, and then replaces itself with a version that
     *  doesn't call this function anymore. But it's possible that, in an
     *  extreme corner case, you can have a second thread hit this function
     *  while the jump table is being initialized by the first.
     * In this case, a spinlock is really painful compared to what spinlocks
     *  _should_ be used for, but this would only happen once, and should be
     *  insanely rare, as you would have to spin a thread outside of SDL (as
     *  SDL_CreateThread() would also call this function before building the
     *  new thread).
     */
    static volatile SDL_bool already_initialized = SDL_FALSE;

    /* SDL_AtomicLock calls SDL mutex functions to emulate if
       SDL_ATOMIC_DISABLED, which we can't do here, so in such a
       configuration, you're on your own. */
    #if !SDL_ATOMIC_DISABLED
    static SDL_SpinLock lock = 0;
    SDL_AtomicLock_REAL(&lock);
    #endif

    if (!already_initialized) {
        SDL_InitDynamicAPILocked();
        already_initialized = SDL_TRUE;
    }

    #if !SDL_ATOMIC_DISABLED
    SDL_AtomicUnlock_REAL(&lock);
    #endif
}

#endif  /* SDL_DYNAMIC_API */

/* vi: set ts=4 sw=4 expandtab: */

