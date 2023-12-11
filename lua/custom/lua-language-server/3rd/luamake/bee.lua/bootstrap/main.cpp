#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#    define _CRT_SECURE_NO_WARNINGS
#endif

#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/unreachable.h>
#include <bee/utility/path_helper.h>
#include <binding/binding.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.hpp>
#include <string_view>

#if !defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#if !defined(LUA_PROGNAME)
#    define LUA_PROGNAME "lua"
#endif

static lua_State *globalL = NULL;

static const char *progname = LUA_PROGNAME;
/*
** Hook set by signal function to stop the interpreter.
*/
static void lstop(lua_State *L, lua_Debug *ar) {
    (void)ar;                   /* unused arg. */
    lua_sethook(L, NULL, 0, 0); /* reset hook */
    luaL_error(L, "interrupted!");
}

/*
** Function to be called at a C signal. Because a C signal cannot
** just change a Lua state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction(int i) {
    signal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
    lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message(const char *pname, const char *msg) {
    if (pname) lua_writestringerror("%s: ", pname);
    lua_writestringerror("%s\n", msg);
}

/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by Lua or by 'msghandler'.
*/
static int report(lua_State *L, int status) {
    if (status != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        l_message(progname, msg);
        lua_pop(L, 1); /* remove message */
    }
    return status;
}

/*
** Message handler used to run all chunks
*/
static int msghandler(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == NULL) {                           /* is error object not a string? */
        if (luaL_callmeta(L, 1, "__tostring") && /* does it have a metamethod */
            lua_type(L, -1) == LUA_TSTRING)      /* that produces a string? */
            return 1;                            /* that is the message */
        else
            msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1); /* append a standard traceback */
    return 1;                     /* return the traceback */
}

/*
** Interface to 'lua_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall(lua_State *L, int narg, int nres) {
    int status;
    int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, msghandler); /* push message handler */
    lua_insert(L, base);              /* put it under function and args */
    globalL = L;                      /* to be available to 'laction' */
    signal(SIGINT, laction);          /* set C-signal handler */
    status = lua_pcall(L, narg, nres, base);
    signal(SIGINT, SIG_DFL); /* reset C-signal handler */
    lua_remove(L, base);     /* remove message handler from the stack */
    return status;
}

static void createargtable(lua_State *L, char **argv, int argc) {
    int i;
    lua_createtable(L, argc - 1, 2);
    lua_pushstring(L, argv[0]);
    lua_rawseti(L, -2, -1);
    lua_pushstring(L, "!main.lua");
    lua_rawseti(L, -2, 0);
    for (i = 1; i < argc; i++) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i);
    }
    lua_setglobal(L, "arg");
}

/*
** Push on the stack the contents of table 'arg' from 1 to #arg
*/
static int pushargs(lua_State *L) {
    int i, n;
    if (lua_getglobal(L, "arg") != LUA_TTABLE)
        luaL_error(L, "'arg' is not a table");
    n = (int)luaL_len(L, -1);
    luaL_checkstack(L, n + 3, "too many arguments to script");
    for (i = 1; i <= n; i++)
        lua_rawgeti(L, -i, i);
    lua_remove(L, -i); /* remove table from the stack */
    return n;
}

template <typename CharT>
static std::string_view tostrview(const std::basic_string<CharT> &u8str) {
    static_assert(sizeof(CharT) == sizeof(char));
    return { reinterpret_cast<const char *>(u8str.data()), u8str.size() };
}

static fs::path pushprogdir(lua_State *L) {
    auto r = bee::path_helper::exe_path();
    if (!r) {
        luaL_error(L, "unable to get progdir: %s\n", r.error().c_str());
        std::unreachable();
    }
    return r.value().remove_filename();
}

static void init_cpath(lua_State *L) {
    lua_getglobal(L, "package");
    auto progdir = pushprogdir(L);
#if defined(_WIN32)
    progdir /= L"?.dll";
#else
    progdir /= L"?.so";
#endif
    auto str     = progdir.generic_u8string();
    auto strview = tostrview(str);
    lua_pushlstring(L, strview.data(), strview.size());
    lua_setfield(L, -2, "cpath");
    lua_pop(L, 1);
}

typedef struct LoadF {
    int n;             /* number of pre-read characters */
    FILE *f;           /* file being read */
    char buff[BUFSIZ]; /* area for reading file */
} LoadF;

static const char *getF(lua_State *L, void *ud, size_t *size) {
    LoadF *lf = (LoadF *)ud;
    (void)L;           /* not used */
    if (lf->n > 0) {   /* are there pre-read characters to be read? */
        *size = lf->n; /* return them (chars already in buffer) */
        lf->n = 0;     /* no more pre-read characters */
    }
    else { /* read a block from file */
        /* 'fread' can return > 0 *and* set the EOF flag. If next call to
           'getF' called 'fread', it might still wait for user input.
           The next check avoids this problem. */
        if (feof(lf->f)) return NULL;
        *size = fread(lf->buff, 1, sizeof(lf->buff), lf->f); /* read block */
    }
    return lf->buff;
}

static int errfile(lua_State *L, const char *what, int fnameindex) {
    const char *serr     = strerror(errno);
    const char *filename = lua_tostring(L, fnameindex) + 1;
    lua_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
    lua_remove(L, fnameindex);
    return LUA_ERRFILE;
}

static int loadfile(lua_State *L, const fs::path &filename, const char *chunkname) {
    LoadF lf;
    int status, readstatus;
    int fnameindex = lua_gettop(L) + 1; /* index of filename on the stack */
    lua_pushstring(L, chunkname);
#if defined(_WIN32)
    lf.f = _wfopen(filename.c_str(), L"r");
#else
    lf.f = fopen(filename.c_str(), "r");
#endif
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
    lf.n       = 0;
    status     = lua_load(L, getF, &lf, lua_tostring(L, -1), "t");
    readstatus = ferror(lf.f);
    if (readstatus) {
        lua_settop(L, fnameindex); /* ignore results from 'lua_load' */
        return errfile(L, "read", fnameindex);
    }
    lua_remove(L, fnameindex);
    return status;
}

static int handle_script(lua_State *L) {
    auto progdir = pushprogdir(L);
    int status   = loadfile(L, progdir / "main.lua", "=(bootstrap.lua)");
    if (status == LUA_OK) {
        int n  = pushargs(L); /* push arguments to script */
        status = docall(L, n, LUA_MULTRET);
    }
    return report(L, status);
}

/*
** Main body of stand-alone interpreter (to be called in protected mode).
** Reads the options and handles them all.
*/
static int pmain(lua_State *L) {
    int argc    = (int)lua_tointeger(L, 1);
    char **argv = (char **)lua_touserdata(L, 2);
    luaL_checkversion(L); /* check that interpreter has correct version */
    if (argv[0] && argv[0][0]) progname = argv[0];
    lua_pushboolean(L, 1); /* signal for libraries to ignore env. vars. */
    lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
    luaL_openlibs(L); /* open standard libraries */
    ::bee::lua::preload_module(L);
    init_cpath(L);
    createargtable(L, argv, argc); /* create table 'arg' */
    lua_gc(L, LUA_GCGEN, 0, 0);    /* GC in generational mode */
    if (handle_script(L) != LUA_OK)
        return 0;
    lua_pushboolean(L, 1); /* signal no errors */
    return 1;
}

#if defined(_WIN32)
int umain(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
    int status, result;
    lua_State *L = luaL_newstate(); /* create state */
    if (L == NULL) {
        l_message(argv[0], "cannot create state: not enough memory");
        return EXIT_FAILURE;
    }
    lua_pushcfunction(L, &pmain);   /* to call 'pmain' in protected mode */
    lua_pushinteger(L, argc);       /* 1st argument */
    lua_pushlightuserdata(L, argv); /* 2nd argument */
    status = lua_pcall(L, 2, 1, 0); /* do the call */
    result = lua_toboolean(L, -1);  /* get result */
    report(L, status);
    lua_close(L);
    return (result && status == LUA_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#if defined(_WIN32)

#    include <Windows.h>
#    include <wchar.h>

extern "C" wchar_t *u2w(const char *str);
extern "C" char *w2u(const wchar_t *str);

void enable_vtmode_(HANDLE h) {
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) {
        return;
    }
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(h, mode);
}

void enable_vtmode() {
    enable_vtmode_(GetStdHandle(STD_OUTPUT_HANDLE));
    enable_vtmode_(GetStdHandle(STD_ERROR_HANDLE));
}

int wmain(int argc, wchar_t **wargv) {
    enable_vtmode();
    char **argv = (char **)calloc(argc + 1, sizeof(char *));
    if (!argv) {
        return EXIT_FAILURE;
    }
    for (int i = 0; i < argc; ++i) {
        argv[i] = w2u(wargv[i]);
    }
    argv[argc] = 0;
    int ret    = umain(argc, argv);
    for (int i = 0; i < argc; ++i) {
        free(argv[i]);
    }
    free(argv);
    return ret;
}

#    if defined(__MINGW32__)

#        include <stdlib.h>

extern int _CRT_glob;
extern
#        ifdef __cplusplus
    "C"
#        endif
    void
    __wgetmainargs(int *, wchar_t ***, wchar_t ***, int, int *);

int main() {
    wchar_t **enpv, **argv;
    int argc, si = 0;
    __wgetmainargs(&argc, &argv, &enpv, _CRT_glob, &si);
    return wmain(argc, argv);
}

#    endif

#endif
