/* SPDX-License-Identifier: MIT */
/*
 * Author: Jianhui Zhao <zhaojh329@gmail.com>
 */

#include <lauxlib.h>
#include <lualib.h>

#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <alloca.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <math.h>
#include <ffi.h>

#include "helper.h"
#include "config.h"
#include "token.h"
#include "lex.h"

#define MAX_RECORD_FIELDS   30
#define MAX_FUNC_ARGS       30

#define CDATA_MT    "cdata"
#define CTYPE_MT    "ctype"
#define CLIB_MT     "clib"

enum {
    CTYPE_BOOL,

    CTYPE_CHAR,
    CTYPE_UCHAR,

    CTYPE_SHORT,
    CTYPE_USHORT,

    CTYPE_INT,
    CTYPE_UINT,

    CTYPE_LONG,
    CTYPE_ULONG,

    CTYPE_LONGLONG,
    CTYPE_ULONGLONG,

    CTYPE_INT8_T,
    CTYPE_INT16_T,
    CTYPE_INT32_T,
    CTYPE_INT64_T,
    CTYPE_UINT8_T,
    CTYPE_UINT16_T,
    CTYPE_UINT32_T,
    CTYPE_UINT64_T,

    CTYPE_INO_T,
    CTYPE_DEV_T,
    CTYPE_GID_T,
    CTYPE_MODE_T,
    CTYPE_NLINK_T,
    CTYPE_UID_T,
    CTYPE_OFF_T,
    CTYPE_PID_T,
    CTYPE_SIZE_T,
    CTYPE_SSIZE_T,
    CTYPE_USECONDS_T,
    CTYPE_SUSECONDS_T,
    CTYPE_BLKSIZE_T,
    CTYPE_BLKCNT_T,
    CTYPE_TIME_T,

    CTYPE_FLOAT,
    CTYPE_DOUBLE,

    CTYPE_VOID,
    CTYPE_RECORD,
    CTYPE_ARRAY,
    CTYPE_PTR,
    CTYPE_FUNC,
};

struct crecord;
struct carray;
struct cfunc;

struct ctype {
    uint8_t type;
    uint8_t is_const:1;
    union {
        struct carray *array;
        struct crecord *rc;
        struct cfunc *func;
        struct ctype *ptr;
        ffi_type *ft;
    };
};

struct carray {
    size_t size;
    ffi_type ft;
    struct ctype *ct;
};

struct crecord_field {
    struct ctype *ct;
    size_t offset;
    char name[0];
};

struct crecord {
    ffi_type ft;
    uint8_t nfield:5;
    uint8_t is_union:1;
    uint8_t anonymous:1;
    struct crecord_field *fields[0];
};

struct cfunc {
    uint8_t va:1;
    uint8_t narg:5;
    struct ctype *rtype;
    struct ctype *args[0];
};

struct cdata {
    struct ctype *ct;
    int gc_ref;
    void *ptr;
};

struct clib {
    void *h;
};

static const char *crecord_registry;
static const char *carray_registry;
static const char *cfunc_registry;
static const char *ctype_registry;
static const char *ctdef_registry;
static const char *clib_registry;

#if LUA_VERSION_NUM < 503

/* LUA_TINT is defined in openwrt */
#ifndef LUA_TINT
static int lua_isinteger(lua_State *L, int idx)
{
    double number;

    if (!lua_isnumber(L, idx))
        return 0;

    number = lua_tonumber(L, idx);

    return floor(number) == number;
}
#endif

#if LUA_VERSION_NUM < 502
static void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup)
{
    luaL_checkstack(L, nup, "too many upvalues");
    for (; l->name != NULL; l++) {  /* fill the table with given functions */
        int i;
        for (i = 0; i < nup; i++)  /* copy upvalues to the top */
            lua_pushvalue(L, -nup);
        lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
        lua_setfield(L, -(nup + 2), l->name);
    }
    lua_pop(L, nup);  /* remove upvalues */
}

#define luaL_newlibtable(L, l) lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)
#define luaL_newlib(L, l) (luaL_newlibtable(L, l), luaL_setfuncs(L, l, 0))

#define ispseudo(i) ((i) <= LUA_REGISTRYINDEX)

static int lua_absindex(lua_State *L, int idx)
{
    return (idx > 0 || ispseudo(idx)) ? idx : lua_gettop(L) + idx + 1;
}

static int lua_rawgetp(lua_State *L, int idx, const void *p)
{
    lua_pushlightuserdata(L, (void *)p);
    lua_rawget(L, idx);
    return lua_type(L, -1);
}

static void lua_rawsetp(lua_State *L, int idx, const void *p)
{
    idx = lua_absindex(L, idx);
    lua_pushlightuserdata(L, (void *)p);
    lua_pushvalue(L, -2);
    lua_rawset(L, idx);
    lua_pop(L, 1);
}

static void *luaL_testudata (lua_State *L, int ud, const char *tname)
{
    void *p = lua_touserdata(L, ud);
    if (p != NULL) {  /* value is a userdata? */
        if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
            luaL_getmetatable(L, tname);  /* get correct metatable */
            if (!lua_rawequal(L, -1, -2))  /* not the same? */
                p = NULL;  /* value is a userdata with wrong metatable */
            lua_pop(L, 2);  /* remove both metatables */
            return p;
        }
    }
    return NULL;  /* value is not a userdata with a metatable */
}
#endif
#endif

#if LUA_VERSION_NUM > 501
#ifndef lua_equal
#define lua_equal(L,idx1,idx2) lua_compare(L,(idx1),(idx2),LUA_OPEQ)
#endif
#endif

static int lua_type_error(lua_State *L, int narg, char const *tname)
{
    lua_pushfstring(L, "%s expected, got %s", tname, luaL_typename(L, narg));
    luaL_argcheck(L, false, narg, lua_tostring(L, -1));
    return 0;
}

static ffi_type *ffi_type_of(size_t size, bool s)
{
    switch (size) {
    case 8:
        return s ? &ffi_type_sint64 : &ffi_type_uint64;
    case 4:
        return s ? &ffi_type_sint32 : &ffi_type_uint32;
    case 2:
        return s ? &ffi_type_sint16 : &ffi_type_uint16;
    default:
        return s ? &ffi_type_sint8 : &ffi_type_uint8;
    }
}

static const char *ctype_name(struct ctype *ct)
{
    switch (ct->type) {
    case CTYPE_BOOL:
        return "bool";

    case CTYPE_CHAR:
        return "char";
    case CTYPE_SHORT:
        return "short";
    case CTYPE_INT:
        return "int";
    case CTYPE_LONG:
        return "long";
    case CTYPE_LONGLONG:
        return "long long";

    case CTYPE_UCHAR:
        return "unsigned char";
    case CTYPE_USHORT:
        return "unsigned short";
    case CTYPE_UINT:
        return "unsigned int";
    case CTYPE_ULONG:
        return "unsigned long";
    case CTYPE_ULONGLONG:
        return "unsigned long long";

    case CTYPE_FLOAT:
        return "float";
    case CTYPE_DOUBLE:
        return "double";

    case CTYPE_INT8_T:
        return "int8_t";
    case CTYPE_INT16_T:
        return "int16_t";
    case CTYPE_INT32_T:
        return "int32_t";
    case CTYPE_INT64_T:
        return "int64_t";
    case CTYPE_UINT8_T:
        return "uint8_t";
    case CTYPE_UINT16_T:
        return "uint16_t";
    case CTYPE_UINT32_T:
        return "uint32_t";
    case CTYPE_UINT64_T:
        return "uint64_t";

    case CTYPE_INO_T:
        return "ino_t";
    case CTYPE_DEV_T:
        return "dev_t";
    case CTYPE_GID_T:
        return "gid_t";
    case CTYPE_MODE_T:
        return "mode_t";
    case CTYPE_NLINK_T:
        return "nlink_t";
    case CTYPE_UID_T:
        return "uid_t";
    case CTYPE_OFF_T:
        return "off_t";
    case CTYPE_PID_T:
        return "pid_t";
    case CTYPE_SIZE_T:
        return "size_t";
    case CTYPE_SSIZE_T:
        return "ssize_t";
    case CTYPE_USECONDS_T:
        return "useconds_t";
    case CTYPE_SUSECONDS_T:
        return "suseconds_t";
    case CTYPE_BLKSIZE_T:
        return "blksize_t";
    case CTYPE_BLKCNT_T:
        return "blkcnt_t";
    case CTYPE_TIME_T:
        return "time_t";

    case CTYPE_VOID:
        return "void";
    case CTYPE_RECORD:
        return ct->rc->is_union ? "union" : "struct";
    case CTYPE_ARRAY:
        return "array";
    case CTYPE_PTR:
        return "pointer";
    case CTYPE_FUNC:
        return "func";

    default:
        return "unknown";
    }
}

static ffi_type *ctype_ft(struct ctype *ct)
{
    switch (ct->type) {
    case CTYPE_ARRAY:
        return &ct->array->ft;
    case CTYPE_RECORD:
        return &ct->rc->ft;
    case CTYPE_PTR:
    case CTYPE_FUNC:
        return &ffi_type_pointer;
    default:
        return ct->ft;
    }
}

static inline size_t ctype_sizeof(struct ctype *ct)
{
    return ctype_ft(ct)->size;
}

static inline int cdata_type(struct cdata *cd)
{
    return cd->ct->type;
}

static inline void *cdata_ptr(struct cdata *cd)
{
    return cd->ptr ? cd->ptr : cd + 1;
}

static void *cdata_ptr_ptr(struct cdata *cd)
{
    int type = cdata_type(cd);

    if (type != CTYPE_PTR && type != CTYPE_FUNC)
        return NULL;

    return *(void **)cdata_ptr(cd);
}

static inline bool ctype_ptr_to(struct ctype *ct, int type)
{
    return ct->type != CTYPE_PTR ? false : ct->ptr->type == type;
}

static bool ctype_is_int(struct ctype *ct)
{
    return ct->type < CTYPE_FLOAT;
}

static bool ctype_is_num(struct ctype *ct)
{
    return ct->type < CTYPE_VOID;
}

static void cdata_ptr_set(struct cdata *cd, void *ptr)
{
    int type = cdata_type(cd);

    if (type != CTYPE_PTR && type != CTYPE_FUNC)
        return;

    *(void **)cdata_ptr(cd) = ptr;
}

static struct ctype *ctype_new(lua_State *L, bool keep)
{
    struct ctype *ct = lua_newuserdata(L, sizeof(struct ctype));

    ct->type = CTYPE_VOID;
    ct->is_const = false;

    luaL_getmetatable(L, CTYPE_MT);
    lua_setmetatable(L, -2);

    lua_rawgetp(L, LUA_REGISTRYINDEX, &ctype_registry);
    lua_pushvalue(L, -2);
    lua_rawsetp(L, -2, ct);

    if (keep)
        lua_pop(L, 1);
    else
        lua_pop(L, 2);

    return ct;
}

static bool ctype_equal(const struct ctype *ct1, const struct ctype *ct2)
{
    if (ct1->type != ct2->type)
        return false;

    if (ct1->is_const != ct2->is_const)
        return false;

    switch (ct1->type) {
    case CTYPE_RECORD:
        return ct1->rc == ct2->rc;
    case CTYPE_ARRAY:
        if (ct1->array->size != ct2->array->size)
            return false;
        return ctype_equal(ct1->array->ct, ct2->array->ct);
    case CTYPE_PTR:
        return ctype_equal(ct1->ptr, ct2->ptr);
    case CTYPE_FUNC:
        return false;
    default:
        break;
    }

    return true;
}

static struct ctype *ctype_lookup(lua_State *L, struct ctype *match, bool keep)
{
    struct ctype *ct;

    lua_rawgetp(L, LUA_REGISTRYINDEX, &ctype_registry);

    lua_pushnil(L);

    while (lua_next(L, -2) != 0) {
        ct = lua_touserdata(L, -1);
        if (ctype_equal(match, ct)) {
            if (keep) {
                lua_replace(L, -3);
                lua_pop(L, 1);
            } else {
                lua_pop(L, 3);
            }
            return ct;
        }
        lua_pop(L, 1);
    }

    lua_pop(L, 1);

    ct = ctype_new(L, keep);
    *ct = *match;

    return ct;
}

static struct carray *carray_lookup(lua_State *L, size_t size, struct ctype *ct)
{
    struct carray *a;

    lua_rawgetp(L, LUA_REGISTRYINDEX, &carray_registry);

    lua_pushnil(L);

    while (lua_next(L, -2) != 0) {
        a = lua_touserdata(L, -1);
        if (a->size == size && ctype_equal(a->ct, ct)) {
            lua_pop(L, 3);
            return a;
        }
        lua_pop(L, 1);
    }

    a = lua_newuserdata(L, sizeof(struct carray));
    if (!a)
        luaL_error(L, "no mem");

    lua_rawsetp(L, -2, a);
    lua_pop(L, 1);

    if (size) {
        a->ft.type = FFI_TYPE_STRUCT;
        a->ft.alignment = ctype_ft(ct)->alignment;
        a->ft.size = ctype_sizeof(ct) * size;
    }

    a->size = size;
    a->ct = ctype_lookup(L, ct, false);

    return a;
}

static const char *cstruct_lookup_name(lua_State *L, struct crecord *st)
{
    lua_rawgetp(L, LUA_REGISTRYINDEX, &crecord_registry);

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_topointer(L, -1) == st) {
            lua_pop(L, 1);
            lua_remove(L, -2);
            return lua_tostring(L, -1);
        }
        lua_pop(L, 1);
    }
    return NULL;
}

static void ctype_tostring(lua_State *L, struct ctype *ct, luaL_Buffer *b, bool *first_ptr)
{
    char buf[128];
    int i;

    if (ct->type != CTYPE_PTR && ct->is_const)
        luaL_addstring(b, "const ");

    switch (ct->type) {
    case CTYPE_PTR:
        ctype_tostring(L, ct->ptr, b, first_ptr);
        if (*first_ptr) {
            luaL_addchar(b, ' ');
            *first_ptr = false;
        }
        luaL_addchar(b, '*');
        if (ct->is_const)
            luaL_addstring(b, " const");
        break;
    case CTYPE_ARRAY:
        ctype_tostring(L, ct->array->ct, b, first_ptr);
        snprintf(buf, sizeof(buf), "%zd", ct->array->size);
        luaL_addchar(b, '[');
        luaL_addstring(b, buf);
        luaL_addchar(b, ']');
        break;
    case CTYPE_FUNC:
        ctype_tostring(L, ct->func->rtype, b, first_ptr);
        luaL_addstring(b, " (");
        for (i = 0; i < ct->func->narg; i++) {
            if (i > 0)
                luaL_addchar(b, ',');
            ctype_tostring(L, ct->func->args[i], b, first_ptr);
        }
        luaL_addchar(b, ')');
        break;
    default:
        luaL_addstring(b, ctype_name(ct));
        if (ct->type == CTYPE_RECORD && !ct->rc->anonymous) {
            luaL_addchar(b, ' ');
            luaL_addstring(b, cstruct_lookup_name(L, ct->rc));
            lua_pop(L, 1);
        }
        break;
    }
}

static struct cdata *cdata_new(lua_State *L, struct ctype *ct, void *ptr)
{
    struct cdata *cd = lua_newuserdata(L, sizeof(struct cdata) + (ptr ? 0 : ctype_sizeof(ct)));

    cd->gc_ref = LUA_REFNIL;
    cd->ptr = ptr;
    cd->ct = ct;

    luaL_getmetatable(L, CDATA_MT);
    lua_setmetatable(L, -2);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, cd);

    if (!ptr)
        memset(cdata_ptr(cd), 0, ctype_sizeof(ct));

    return cd;
}

static int __cdata_tostring(lua_State *L, struct cdata *cd)
{
    void *ptr = cdata_type(cd) == CTYPE_PTR ? cdata_ptr_ptr(cd) : cdata_ptr(cd);
    bool first_ptr = true;
    luaL_Buffer b;
    char buf[128];

    luaL_buffinit(L, &b);
    luaL_addstring(&b, "cdata<");
    ctype_tostring(L, cd->ct, &b, &first_ptr);
    snprintf(buf, sizeof(buf), ">: %p", ptr);
    luaL_addstring(&b, buf);
    luaL_pushresult(&b);

    return 1;
}

static int cdata_tostring(lua_State *L)
{
    struct cdata *cd = luaL_checkudata(L, 1, CDATA_MT);
    return __cdata_tostring(L, cd);
}

static int __ctype_tostring(lua_State *L, struct ctype *ct)
{
    bool first_ptr = true;
    luaL_Buffer b;

    luaL_buffinit(L, &b);
    ctype_tostring(L, ct, &b, &first_ptr);
    luaL_pushresult(&b);

    return 1;
}

#define PUSH_INTEGER(L, type, ptr) \
    do { \
        type v; \
        memcpy(&v, ptr, sizeof(v)); \
        lua_pushinteger(L, v); \
    } while (0)

#define PUSH_NUMBER(L, type, ptr) \
    do { \
        type v; \
        memcpy(&v, ptr, sizeof(v)); \
        lua_pushnumber(L, v); \
    } while (0)

static int cdata_to_lua(lua_State *L, struct ctype *ct, void *ptr)
{
    switch (ct->type) {
    case CTYPE_RECORD:
    case CTYPE_ARRAY:
    case CTYPE_PTR:
        cdata_new(L, ct, ptr);
        return 1;
    }

    switch (ct->ft->type) {
    case FFI_TYPE_SINT8:
        PUSH_INTEGER(L, int8_t, ptr);
        break;
    case FFI_TYPE_UINT8:
        PUSH_INTEGER(L, uint8_t, ptr);
        break;
    case FFI_TYPE_SINT16:
        PUSH_INTEGER(L, int16_t, ptr);
        break;
    case FFI_TYPE_UINT16:
        PUSH_INTEGER(L, uint16_t, ptr);
        break;
    case FFI_TYPE_SINT32:
        PUSH_INTEGER(L, int32_t, ptr);
        break;
    case FFI_TYPE_UINT32:
        PUSH_INTEGER(L, uint32_t, ptr);
        break;
    case FFI_TYPE_SINT64:
        PUSH_INTEGER(L, int64_t, ptr);
        break;
    case FFI_TYPE_UINT64:
        PUSH_INTEGER(L, uint64_t, ptr);
        break;
    case FFI_TYPE_FLOAT:
        PUSH_NUMBER(L, float, ptr);
        break;
    case FFI_TYPE_DOUBLE:
        PUSH_NUMBER(L, double, ptr);
        break;
    default:
        return 0;
    }

    return 1;
}

static int cdata_from_lua(lua_State *L, struct ctype *ct, void *ptr, int idx, bool cast);

static lua_Integer from_lua_num_int(lua_State *L, int idx)
{
    if (lua_isinteger(L, idx))
        return lua_tointeger(L, idx);
    else if (lua_isboolean(L, idx))
        return lua_toboolean(L, idx);
    else
        return luaL_checknumber(L, idx);
}

static lua_Number from_lua_num_num(lua_State *L, int idx)
{
    if (lua_isboolean(L, idx))
        return lua_toboolean(L, idx);
    else
        return luaL_checknumber(L, idx);
}

static void ft_from_lua_num(lua_State *L, ffi_type *ft, void *ptr, int idx)
{
    switch (ft->type) {
    case FFI_TYPE_SINT8:
        *(int8_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_UINT8:
        *(uint8_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_SINT16:
        *(int16_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_UINT16:
        *(uint16_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_SINT32:
        *(int32_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_UINT32:
        *(uint32_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_SINT64:
        *(int64_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_UINT64:
        *(uint64_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_FLOAT:
        *(float *)ptr = from_lua_num_num(L, idx);
        break;
    case FFI_TYPE_DOUBLE:
        *(double *)ptr = from_lua_num_num(L, idx);
        break;
    }
}

static bool cdata_from_lua_num(lua_State *L, struct ctype *ct, void *ptr, int idx, bool cast)
{
    if (ct->type == CTYPE_PTR) {
        if (!cast)
            return false;
        *(void **)ptr = (void *)(intptr_t)lua_tointeger(L, idx);
        return true;
    }

    if (!ctype_is_num(ct))
        return false;

    ft_from_lua_num(L, ct->ft, ptr, idx);

    if (ct->type == CTYPE_BOOL)
        *(int8_t *)ptr = !!*(int8_t *)ptr;

    return true;
}

static bool cdata_from_lua_cdata_ptr(lua_State *L, struct ctype *ct, void *ptr,
                struct ctype *from_ct, void *from_ptr, bool cast)
{
    if (ct->type == CTYPE_PTR && (cast || ctype_equal(ct->ptr, from_ct)
            || ctype_ptr_to(ct, CTYPE_VOID) || from_ct->type == CTYPE_VOID)) {
        *(void **)ptr = from_ptr;
        return true;
    }

    if (cast && ctype_is_int(ct)) {
        lua_pushinteger(L, (intptr_t)from_ptr);
        cdata_from_lua_num(L, ct, ptr, -1, true);
        lua_pop(L, 1);
        return true;
    }

    return false;
}

static bool cdata_from_lua_cdata(lua_State *L, struct ctype *ct, void *ptr, int idx, bool cast)
{
    struct cdata *cd = lua_touserdata(L, idx);

    switch (cdata_type(cd)) {
    case CTYPE_ARRAY:
        return cdata_from_lua_cdata_ptr(L, ct, ptr, cd->ct->array->ct, cdata_ptr(cd), cast);
    case CTYPE_PTR:
        return cdata_from_lua_cdata_ptr(L, ct, ptr, cd->ct->ptr, cdata_ptr_ptr(cd), cast);
    case CTYPE_RECORD:
        if (ct->type == CTYPE_PTR && (cast || ctype_equal(cd->ct, ct->ptr))) {
            *(void **)ptr = cdata_ptr(cd);
            return true;
        }

        if (ctype_equal(cd->ct, ct)) {
            memcpy(ptr, cdata_ptr(cd), ctype_sizeof(ct));
            return true;
        }
        break;
    default:
        if (ctype_is_num(cd->ct)) {
            cdata_to_lua(L, cd->ct, cdata_ptr(cd));
            cdata_from_lua_num(L, ct, ptr, -1, cast);
            lua_pop(L, 1);
            return true;
        }
        break;
    }

    return false;
}

static bool cdata_from_lua_table(lua_State *L, struct ctype *ct, void *ptr, int idx, bool cast)
{
    int i = 0;

    if (ct->type == CTYPE_ARRAY) {
        while (i < ct->array->size) {
            lua_rawgeti(L, idx, i + 1);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                break;
            }
            cdata_from_lua(L, ct->array->ct, ptr + ctype_sizeof(ct->array->ct) * i++, lua_absindex(L, -1), cast);
            lua_pop(L, 1);
        }
        return true;
    } else if (ct->type == CTYPE_RECORD) {
        while (i < ct->rc->nfield) {
            struct crecord_field *field = ct->rc->fields[i++];

            lua_rawgeti(L, idx, i);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_getfield(L, idx, field->name);
            }

            if (!lua_isnil(L, -1))
                cdata_from_lua(L, field->ct, ptr + field->offset, lua_absindex(L, -1), cast);
            lua_pop(L, 1);
        }
        return true;
    }

    return false;
}

static int cdata_from_lua(lua_State *L, struct ctype *ct, void *ptr, int idx, bool cast)
{
    switch (ct->type) {
    case CTYPE_FUNC:
    case CTYPE_VOID:
        luaL_error(L, "invalid C type");
        break;
    case CTYPE_ARRAY:
    case CTYPE_RECORD:
        if (cast)
            luaL_error(L, "invalid C type");
        break;
    default:
        break;
    }

    switch (lua_type(L, idx)) {
    case LUA_TNIL:
        if (ct->type == CTYPE_PTR) {
            *(void **)ptr = NULL;
            return 0;
        }
        break;
    case LUA_TNUMBER:
    case LUA_TBOOLEAN:
        if (cdata_from_lua_num(L, ct, ptr, idx, cast))
            return 0;
        break;
    case LUA_TSTRING:
        if (cast || ((ctype_ptr_to(ct, CTYPE_CHAR) || ctype_ptr_to(ct, CTYPE_VOID))
            && ct->ptr->is_const)) {
            *(const char **)ptr = (const char *)lua_tostring(L, idx);
            return 0;
        }

        if (ct->type == CTYPE_ARRAY && ct->array->ct->type == CTYPE_CHAR) {
            size_t len;
            const char *str = lua_tolstring(L, idx, &len);
            memcpy(ptr, str, len + 1);
            return 0;
        }
        break;
    case LUA_TUSERDATA:
        if (luaL_testudata(L, idx, CDATA_MT)) {
            if (cdata_from_lua_cdata(L, ct, ptr, idx, cast))
                return 0;
        } else if (ct->type == CTYPE_PTR) {
            void *ud = lua_touserdata(L, idx);

            if (luaL_testudata(L, idx, LUA_FILEHANDLE)) {
                *(void **)ptr = *(void **)ud;
                return 0;
            }

            if (cast || ctype_ptr_to(ct, CTYPE_VOID)) {
                *(void **)ptr = ud;
                return 0;
            }
        }
        break;
    case LUA_TLIGHTUSERDATA:
        if (ct->type == CTYPE_PTR) {
            *(void **)ptr = lua_touserdata(L, idx);
            return 0;
        }
        break;
    case LUA_TTABLE:
        if (cdata_from_lua_table(L, ct, ptr, idx, cast))
            return 0;
        break;
    default:
        break;
    }

    if (luaL_testudata(L, idx, CDATA_MT)) {
        struct cdata *cd = lua_touserdata(L, idx);
        __ctype_tostring(L, cd->ct);
        __ctype_tostring(L, ct);
        lua_pushfstring(L, "cannot convert '%s' to '%s'", lua_tostring(L, -2), lua_tostring(L, -1));
    } else {
        __ctype_tostring(L, ct);
        lua_pushfstring(L, "cannot convert '%s' to '%s'", luaL_typename(L, idx), lua_tostring(L, -1));
    }
    return luaL_argerror(L, idx, lua_tostring(L, -1));
}

static int cdata_index_ptr(lua_State *L, struct cdata *cd, struct ctype *ct, bool to)
{
    void *ptr = cdata_type(cd) == CTYPE_PTR ? cdata_ptr_ptr(cd) : cdata_ptr(cd);
    int idx;

    if (ct->type == CTYPE_VOID) {
        __ctype_tostring(L, cd->ct);
        return luaL_error(L, "ctype '%s' cannot be indexed", lua_tostring(L, -1));
    }

    if (!lua_isinteger(L, 2)) {
        __ctype_tostring(L, cd->ct);
        return luaL_error(L, "ctype '%s' cannot be indexed with %s", lua_tostring(L, -1), luaL_typename(L, 2));
    }

    idx = lua_tointeger(L, 2);

    if (to) {
        lua_rawgetp(L, LUA_REGISTRYINDEX, cd);
        lua_rawgeti(L, -1, idx);
        if (!lua_isnil(L, -1)) {
            lua_remove(L, -2);
            return 1;
        }
        lua_pop(L, 2);

        cdata_to_lua(L, ct, ptr + ctype_sizeof(ct) * idx);

        if (luaL_testudata(L, -1, CDATA_MT)) {
            lua_rawgetp(L, LUA_REGISTRYINDEX, cd);
            lua_pushvalue(L, -2);
            lua_rawseti(L, -2, idx);
            lua_pop(L, 1);
        }
        return 1;
    } else {
        return cdata_from_lua(L, ct, ptr + ctype_sizeof(ct) * idx, 3, false);
    }
}

static struct crecord_field *cdata_crecord_find_field(
        struct crecord_field **fields, int nfield, const char *name, size_t *offset)
{
    int i;

    for (i = 0; i < nfield; i++) {
        struct crecord_field *field = fields[i];

        if (field->name[0]) {
            if (!strcmp(field->name, name)) {
                *offset += field->offset;
                return field;
            }
        } else {
            field = cdata_crecord_find_field(field->ct->rc->fields, field->ct->rc->nfield, name, offset);
            if (field) {
                *offset += fields[i]->offset;
                return field;
            }
        }
    }

    return NULL;
}

static int cdata_index_crecord(lua_State *L, struct cdata *cd, struct ctype *ct, bool to)
{
    void *ptr = cdata_type(cd) == CTYPE_PTR ? cdata_ptr_ptr(cd) : cdata_ptr(cd);
    struct crecord_field *field;
    size_t offset = 0;
    const char *name;

    if (lua_type(L, 2) != LUA_TSTRING)
        return luaL_error(L, "struct must be indexed with string");

    name = lua_tostring(L, 2);

    if (to) {
        lua_rawgetp(L, LUA_REGISTRYINDEX, cd);
        lua_getfield(L, -1, name);
        if (!lua_isnil(L, -1)) {
            lua_remove(L, -2);
            return 1;
        }
        lua_pop(L, 2);
    }

    field = cdata_crecord_find_field(ct->rc->fields, ct->rc->nfield, name, &offset);
    if (!field) {
        __ctype_tostring(L, ct);
        return luaL_error(L, "ctype '%s' has no member named '%s'", lua_tostring(L, -1), name);
    }

    if (to) {
        cdata_to_lua(L, field->ct, ptr + offset);
        if (luaL_testudata(L, -1, CDATA_MT)) {
            lua_rawgetp(L, LUA_REGISTRYINDEX, cd);
            lua_pushvalue(L, -2);
            lua_setfield(L, -2, name);
            lua_pop(L, 1);
        }
        return 1;
    } else {
        return cdata_from_lua(L, field->ct, ptr + offset, 3, false);
    }
}

static int cdata_index_common(lua_State *L, bool to)
{
    struct cdata *cd = luaL_checkudata(L, 1, CDATA_MT);
    struct ctype *ct = cd->ct;

    if (!to && ct->is_const)
        return luaL_error(L, "assignment of read-only variable");

    switch (ct->type) {
    case CTYPE_RECORD:
        return cdata_index_crecord(L, cd, ct, to);
    case CTYPE_PTR:
        if (ct->ptr->type == CTYPE_RECORD && lua_type(L, 2) == LUA_TSTRING)
            return cdata_index_crecord(L, cd, ct->ptr, to);
        return cdata_index_ptr(L, cd, ct->ptr, to);
    case CTYPE_ARRAY:
        return cdata_index_ptr(L, cd, ct->array->ct, to);
    default:
        __ctype_tostring(L, cd->ct);
        return luaL_error(L, "ctype '%s' cannot be indexed", lua_tostring(L, -1));
    }
}

static int cdata_index(lua_State *L)
{
    return cdata_index_common(L, true);
}

static int cdata_newindex(lua_State *L)
{
    return cdata_index_common(L, false);
}

static int cdata_eq(lua_State *L)
{
    struct cdata *cd = luaL_checkudata(L, 1, CDATA_MT);
    int type = cdata_type(cd);
    struct cdata *a;
    bool eq = false;

    switch (type) {
    case CTYPE_RECORD:
    case CTYPE_ARRAY:
    case CTYPE_FUNC:
        break;
    case CTYPE_PTR:
        if (lua_isnil(L, 2)) {
            eq = cdata_ptr_ptr(cd) == NULL;
            break;
        }

        a = luaL_testudata(L, 2, CDATA_MT);
        if (a && cdata_type(a) == CTYPE_PTR)
            eq = cdata_ptr_ptr(cd) == cdata_ptr_ptr(a);

        break;
    default:
        cdata_to_lua(L, cd->ct, cdata_ptr(cd));
        eq = lua_equal(L, 2, -1);
        lua_pop(L, 1);
    }

    lua_pushboolean(L, eq);
    return 1;
}

static ffi_type *lua_to_vararg(lua_State *L, int idx)
{
    struct cdata *cd;

    switch (lua_type(L, idx)) {
    case LUA_TBOOLEAN:
        return &ffi_type_sint; /* cannot be less than the size of int, due to limited in libffi */
    case LUA_TNUMBER:
        if (lua_isinteger(L, idx))
            return ffi_type_of(sizeof(lua_Integer), false);
        return &ffi_type_double;
    case LUA_TNIL:
    case LUA_TSTRING:
    case LUA_TLIGHTUSERDATA:
        return &ffi_type_pointer;
    case LUA_TUSERDATA:
        cd = luaL_testudata(L, idx, CDATA_MT);
        if (!cd || cdata_type(cd) == CTYPE_RECORD || cdata_type(cd) == CTYPE_ARRAY)
            return &ffi_type_pointer;
        return ctype_ft(cd->ct);
    default:
        return NULL;
    }
}

static int cdata_call(lua_State *L)
{
    struct cdata *cd = luaL_checkudata(L, 1, CDATA_MT);
    ffi_type *args[MAX_FUNC_ARGS] = {};
    void *values[MAX_FUNC_ARGS] = {};
    struct ctype *ct = cd->ct;
    int i, status, narg;
    struct cfunc *func;
    struct ctype *rtype;
    ffi_cif cif;
    void *sym;

    if (ct->type != CTYPE_FUNC) {
        __ctype_tostring(L, ct);
        return luaL_error(L, "'%s' is not callable", lua_tostring(L, -1));
    }

    sym = cdata_ptr_ptr(cd);
    func = ct->func;
    rtype = func->rtype;

    narg = lua_gettop(L) - 1;

    if (func->va) {
        if (narg < func->narg)
            return luaL_error(L, "wrong number of arguments for function call");
    } else if (narg != func->narg) {
        return luaL_error(L, "wrong number of arguments for function call");
    }

    for (i = 0; i < func->narg; i++) {
        args[i] = ctype_ft(func->args[i]);
        values[i] = alloca(args[i]->size);
        cdata_from_lua(L, func->args[i], values[i], i + 2, false);
    }

    if (func->va) {
        for (i = func->narg; i < narg; i++) {
            args[i] = lua_to_vararg(L, i + 2);
            if (!args[i])
                return luaL_error(L, "unsupported type '%s'", luaL_typename(L, i + 2));
            values[i] = alloca(args[i]->size);
        }

        for (i = func->narg; i < narg; i++) {
            switch (lua_type(L, i + 2)) {
            case LUA_TBOOLEAN:
            case LUA_TNUMBER:
                ft_from_lua_num(L, args[i], values[i], i + 2);
                break;
            case LUA_TNIL:
                *(void **)values[i] = NULL;
                break;
            case LUA_TSTRING:
                *(void **)values[i] = (void *)luaL_checkstring(L, i + 2);
                break;
            case LUA_TLIGHTUSERDATA:
                *(void **)values[i] = (void *)lua_topointer(L, i + 2);
                break;
            case LUA_TUSERDATA:
                cd = luaL_testudata(L, i + 2, CDATA_MT);
                if (!cd)
                    *(void **)values[i] = lua_touserdata(L, i + 2);
                else if (cdata_type(cd) == CTYPE_RECORD || cdata_type(cd) == CTYPE_ARRAY)
                    *(void **)values[i] = cdata_ptr(cd);
                else if (cdata_type(cd) == CTYPE_FUNC || cdata_type(cd) == CTYPE_PTR)
                    *(void **)values[i] = cdata_ptr_ptr(cd);
                else {
                    cdata_to_lua(L, cd->ct, cdata_ptr(cd));
                    ft_from_lua_num(L, args[i], values[i], -1);
                    lua_pop(L, 1);
                }
                break;
            }
        }
    }

    if (func->va)
        status = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, func->narg, narg, ctype_ft(rtype), args);
    else
        status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, func->narg, ctype_ft(rtype), args);
    if (status)
        return luaL_error(L, "ffi_prep_cif fail: %d", status);

    if (rtype->type == CTYPE_RECORD || rtype->type == CTYPE_PTR) {
        if (rtype->type == CTYPE_PTR) {
            void *rvalue;
            ffi_call(&cif, FFI_FN(sym), &rvalue, values);
            cdata_ptr_set(cdata_new(L, rtype, NULL), rvalue);
        } else {
            cd = cdata_new(L, rtype, NULL);
            ffi_call(&cif, FFI_FN(sym), cdata_ptr(cd), values);
        }

        return 1;
    }

    if (rtype->type <= CTYPE_RECORD) {
        void *rvalue = NULL;

        if (rtype->type != CTYPE_VOID)
            rvalue = alloca(ctype_sizeof(rtype));

        ffi_call(&cif, FFI_FN(sym), rvalue, values);

        return cdata_to_lua(L, rtype, rvalue);
    }

    return luaL_error(L, "unsupported return type '%s'", ctype_name(rtype));
}

static int cdata_gc(lua_State *L)
{
    struct cdata *cd = luaL_checkudata(L, 1, CDATA_MT);
    int gc_ref = cd->gc_ref;

    if (gc_ref != LUA_REFNIL) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, gc_ref);
        lua_pushvalue(L, 1);
        if (lua_pcall(L, 1, 0, 0))
            lua_pop(L, 1);
        luaL_unref(L, LUA_REGISTRYINDEX, gc_ref);
    }

    lua_pushnil(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, cd);

    return 0;
}

static const luaL_Reg cdata_methods[] = {
    {"__tostring", cdata_tostring},
    {"__index", cdata_index},
    {"__newindex", cdata_newindex},
    {"__eq", cdata_eq},
    {"__call", cdata_call},
    {"__gc", cdata_gc},
    {NULL, NULL}
};

static int lua_ctype_tostring(lua_State *L)
{
    struct ctype *ct = luaL_checkudata(L, 1, CTYPE_MT);

    lua_pushliteral(L, "ctype<");
    __ctype_tostring(L, ct);
    lua_pushliteral(L, ">");
    lua_concat(L, 3);

    return 1;
}

static int ctype_gc(lua_State *L)
{
    struct ctype *ct = luaL_checkudata(L, 1, CTYPE_MT);
    int type = ct->type;

    if (type == CTYPE_RECORD && ct->rc->anonymous) {
        int i;
        for (i = 0; i < ct->rc->nfield; i++)
            free(ct->rc->fields[i]);
        free(ct->rc);
    }

    return 0;
}

static const luaL_Reg ctype_methods[] = {
    {"__tostring", lua_ctype_tostring},
    {"__gc", ctype_gc},
    {NULL, NULL}
};

static int clib_index(lua_State *L)
{
    struct clib *lib = luaL_checkudata(L, 1, CLIB_MT);
    const char *name = luaL_checkstring(L, 2);
    struct ctype match = { .type = CTYPE_FUNC };
    struct ctype *ct;
    void *sym;

    lua_rawgetp(L, LUA_REGISTRYINDEX, lib);
    lua_getfield(L, -1, name);
    if (!lua_isnil(L, -1))
        goto done;
    lua_pop(L, 1);

    lua_rawgetp(L, LUA_REGISTRYINDEX, &cfunc_registry);
    lua_getfield(L, -1, name);

    if (lua_isnil(L, -1))
        return luaL_error(L, "missing declaration for function '%s", name);

    match.func = (struct cfunc *)lua_topointer(L, -1);
    lua_pop(L, 2);

    ct = ctype_lookup(L, &match, false);

    sym = dlsym(lib->h, name);
    if (!sym)
        return luaL_error(L, "undefined function '%s'", name);

    cdata_ptr_set(cdata_new(L, ct, NULL), sym);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, name);

done:
    lua_remove(L, -2);
    return 1;
}

static int clib_tostring(lua_State *L)
{
    struct clib *lib = luaL_checkudata(L, 1, CLIB_MT);
    if (lib->h == RTLD_DEFAULT)
        lua_pushliteral(L, "library: default");
    else
        lua_pushfstring(L, "library: %p", lib->h);
    return 1;
}

static int clib_gc(lua_State *L)
{
    struct clib *lib = luaL_checkudata(L, 1, CLIB_MT);
    void *h = lib->h;

    if (h != RTLD_DEFAULT)
        dlclose(h);

    lua_pushnil(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, lib);

    return 0;
}

static const luaL_Reg clib_methods[] = {
    {"__index", clib_index},
    {"__tostring", clib_tostring},
    {"__gc", clib_gc},
    {NULL, NULL}
};

static int cparse_expected_error(lua_State *L, int tok, const char *s)
{
    if (tok)
        return luaL_error(L, "%d:'%s' expected before '%s'", yyget_lineno(), s, yyget_text());
    else
        return luaL_error(L, "%d:identifier expected", yyget_lineno());
}

static void ctype_to_ptr(lua_State *L, struct ctype *ct)
{
    struct ctype *ptr = ctype_lookup(L, ct, false);
    ct->type = CTYPE_PTR;
    ct->is_const = false;
    ct->ptr = ptr;
}

extern char *lex_err;

static inline int cparse_check_tok(lua_State *L, int tok)
{
    if (!tok && lex_err)
        return luaL_error(L, "%d:%s", yyget_lineno(), lex_err);
    return tok;
}

static int cparse_pointer(lua_State *L, int tok, struct ctype *ct)
{
    while (cparse_check_tok(L, tok) == '*') {
        ctype_to_ptr(L, ct);
        tok = yylex();
    }

    if (cparse_check_tok(L, tok) == TOK_CONST) {
        ct->is_const = true;
        tok = yylex();
    }

    return tok;
}

static int cparse_array(lua_State *L, int tok, bool *flexible, int *size)
{
    *size = -1;

    if (cparse_check_tok(L, tok) != '[') {
        *flexible = false;
        return tok;
    }

    tok = yylex();

    if (!*flexible && cparse_check_tok(L, tok) != TOK_INTEGER)
        return luaL_error(L, "%d:flexible array not supported at here", yyget_lineno());

    *flexible = false;

    if (cparse_check_tok(L, tok) == TOK_INTEGER || cparse_check_tok(L, tok) == '?') {
        if (cparse_check_tok(L, tok) == TOK_INTEGER) {
            *size = atoi(yyget_text());
            if (*size < 0)
                return luaL_error(L, "%d:size of array is negative", yyget_lineno());
        } else {
            *flexible = true;
        }
        tok = yylex();
    } else {
        *flexible = true;
    }

    if (cparse_check_tok(L, tok) != ']')
        return cparse_expected_error(L, tok, "]");

    return yylex();
}

static int cparse_basetype(lua_State *L, int tok, struct ctype *ct);

static void init_ft_struct(lua_State *L, ffi_type *ft, ffi_type **elements, size_t *offsets)
{
    int status;

    ft->type = FFI_TYPE_STRUCT;
    ft->elements = elements;

    status = ffi_get_struct_offsets(FFI_DEFAULT_ABI, ft, offsets);
    if (status)
        luaL_error(L, "ffi_get_struct_offsets fail: %d", status);
}

static void cparse_new_array(lua_State *L, size_t array_size, struct ctype *ct)
{
    struct carray *a = carray_lookup(L, array_size, ct);

    ct->type = CTYPE_ARRAY;
    ct->is_const = false;
    ct->array = a;
}

static void check_void_forbidden(lua_State *L, struct ctype *ct, int tok)
{
    if (ct->type != CTYPE_VOID)
        return;

    if (tok)
        luaL_error(L, "%d:void type in forbidden context near '%s'",
                yyget_lineno(), yyget_text());
    else
        luaL_error(L, "%d:void type in forbidden context", yyget_lineno());
}

static int cparse_record(lua_State *L, struct ctype *ct, bool is_union);

static int cparse_record_field(lua_State *L, struct crecord_field **fields)
{
    int nfield = 0;
    int tok, i;

    while (true) {
        struct crecord_field *field;
        struct ctype ct = {};
        bool flexible = false;
        int array_size;
        char *name;

        tok = yylex();

        if (cparse_check_tok(L, tok) == '}')
            return nfield;

        if (cparse_check_tok(L, tok) == TOK_STRUCT || cparse_check_tok(L, tok) == TOK_UNION) {
            tok = cparse_record(L, &ct, cparse_check_tok(L, tok) == TOK_UNION);
            if (tok == ';') {
                field = calloc(1, sizeof(struct crecord_field) + 1);
                if (!field)
                    return luaL_error(L, "no mem");
                goto add;
            }
        } else {
            tok = cparse_basetype(L, tok, &ct);
        }

        tok = cparse_pointer(L, tok, &ct);

        check_void_forbidden(L, &ct, tok);

        if (cparse_check_tok(L, tok) != TOK_NAME)
            return cparse_expected_error(L, tok, "identifier");

        name = yyget_text();

        for (i = 0; i < nfield; i++)
            if (!strcmp(fields[i]->name, name))
                return luaL_error(L, "%d:duplicate member'%s'", yyget_lineno(), name);

        field = calloc(1, sizeof(struct crecord_field) + yyget_leng() + 1);
        if (!field)
            return luaL_error(L, "no mem");

        memcpy(field->name, name, yyget_leng());

        tok = cparse_array(L, yylex(), &flexible, &array_size);
        if (cparse_check_tok(L, tok) != ';')
            return cparse_expected_error(L, tok, ";");

        if (array_size >= 0)
            cparse_new_array(L, array_size, &ct);

add:
        field->ct = ctype_lookup(L, &ct, false);
        fields[nfield++] = field;
    }
}

static inline bool ctype_is_zero_array(struct ctype *ct)
{
    return ct->type == CTYPE_ARRAY && ct->array->size == 0;
}

static int cparse_record(lua_State *L, struct ctype *ct, bool is_union)
{
    bool named = false;
    int tok = yylex();

    ct->type = CTYPE_RECORD;

    if (cparse_check_tok(L, tok) == TOK_NAME) {
        named = true;
        lua_pushstring(L, yyget_text());
        tok = yylex();
    }

    if (cparse_check_tok(L, tok) == '{') {
        struct crecord_field *fields[MAX_RECORD_FIELDS];
        size_t offsets[MAX_RECORD_FIELDS];
        ffi_type **elements;
        size_t nfield = 0;
        int i, j, nelement;

        if (named) {
            lua_rawgetp(L, LUA_REGISTRYINDEX, &crecord_registry);
            lua_pushvalue(L, -2);
            lua_gettable(L, -2);

            if (!lua_isnil(L, -1))
                return luaL_error(L, "%d:redefinition of symbol '%s'", yyget_lineno(), lua_tostring(L, -3));
            lua_pop(L, 1);
        }

        nfield = cparse_record_field(L, fields);

        if (is_union) {
            nelement = 2;
        } else {
            nelement = nfield + 1;

            for (i = 0; i < nfield; i++) {
                if (ctype_is_zero_array(fields[i]->ct))
                    nelement--;
            }
        }

        ct->rc = calloc(1, sizeof(struct crecord)
                        + sizeof(struct crecord_field *) * nfield
                        + sizeof(ffi_type *) * nelement);
        if (!ct->rc)
            return luaL_error(L, "no mem");

        memcpy(ct->rc->fields, fields, sizeof(struct crecord_field *) * nfield);

        if (named) {
            lua_pushvalue(L, -2);
            lua_pushlightuserdata(L, ct->rc);
            lua_settable(L, -3);
            lua_pop(L, 2);
        } else {
            ct->rc->anonymous = true;
        }

        ct->rc->is_union = is_union;
        ct->rc->nfield = nfield;

        elements = (ffi_type **)&ct->rc->fields[nfield];

        if (is_union) {
            for (i = 0; i < nfield; i++) {
                if (!ctype_is_zero_array(fields[i]->ct)) {
                    ffi_type *ft = ctype_ft(fields[i]->ct);
                    if (i == 0 || ft->size > elements[0]->size)
                        elements[0] = ft;
                }
            }

            if (!elements[0])
                nelement--;
        } else {
            for (i = 0, j = 0; i < nfield; i++) {
                if (ctype_is_zero_array(fields[i]->ct))
                    continue;
                elements[j++] = ctype_ft(fields[i]->ct);
            }
        }

        if (nelement > 1)
            init_ft_struct(L, &ct->rc->ft, elements, offsets);

        if (!is_union) {
            for (i = 0, j = 0; i < nfield; i++) {
                if (ctype_is_zero_array(fields[i]->ct)) {
                    if (i > 0)
                        ct->rc->fields[i]->offset = fields[i - 1]->offset + ctype_sizeof(fields[i - 1]->ct);
                } else {
                    ct->rc->fields[i]->offset = offsets[j++];
                }
            }
        }

        return yylex();
    } else {
        if (!named)
            return cparse_expected_error(L, tok, "identifier");

        lua_rawgetp(L, LUA_REGISTRYINDEX, &crecord_registry);
        lua_pushvalue(L, -2);
        lua_gettable(L, -2);

        if (lua_isnil(L, -1))
            return luaL_error(L, "%d:undeclared of symbol '%s", yyget_lineno(), lua_tostring(L, -3));

        ct->rc = (struct crecord *)lua_topointer(L, -1);
        lua_pop(L, 3);
    }

    return tok;
}

static int cparse_squals(int type, int squals, struct ctype *ct, ffi_type *s, ffi_type *u)
{
    ct->type = s ? ++type : type;
    ct->ft = squals == TOK_SIGNED ? s : u;
    return yylex();
}

static int cparse_basetype(lua_State *L, int tok, struct ctype *ct)
{
    ct->is_const = false;

    if (cparse_check_tok(L, tok) == TOK_CONST) {
        ct->is_const = true;
        tok = yylex();
    }

    if (cparse_check_tok(L, tok) == TOK_SIGNED || cparse_check_tok(L, tok) == TOK_UNSIGNED) {
        int squals = tok;

        tok = yylex();

        switch (tok) {
        case TOK_CHAR:
            tok = cparse_squals(CTYPE_CHAR, squals, ct, &ffi_type_schar, &ffi_type_uchar);
            break;
        case TOK_SHORT:
            tok = cparse_squals(CTYPE_SHORT, squals, ct, &ffi_type_sshort, &ffi_type_ushort);
            break;
        case TOK_INT:
            tok = cparse_squals(CTYPE_INT, squals, ct, &ffi_type_sint, &ffi_type_uint);
            break;
        case TOK_LONG:
            tok = cparse_squals(CTYPE_LONG, squals, ct, &ffi_type_slong, &ffi_type_ulong);
            break;
        default:
            ct->type = CTYPE_INT;
            ct->ft = (squals == TOK_SIGNED) ? &ffi_type_sint : &ffi_type_uint;
            break;
        }
    } else if (cparse_check_tok(L, tok) == TOK_STRUCT || cparse_check_tok(L, tok) == TOK_UNION) {
        tok = cparse_record(L, ct, cparse_check_tok(L, tok) == TOK_UNION);
    } else {
#define INIT_TYPE(t1, t2) \
            ct->type = t1; \
            ct->ft = &t2; \
            break

#define INIT_TYPE_T(t1, t2, s) \
            ct->type = t1; \
            ct->ft = ffi_type_of(sizeof(t2), s); \
            break

        switch (tok) {
        case TOK_VOID:
            INIT_TYPE(CTYPE_VOID, ffi_type_void);
        case TOK_BOOL:
            INIT_TYPE(CTYPE_BOOL, ffi_type_sint8);
        case TOK_CHAR:
            INIT_TYPE(CTYPE_CHAR, ffi_type_schar);
        case TOK_SHORT:
            INIT_TYPE(CTYPE_SHORT, ffi_type_sshort);
        case TOK_INT:
            INIT_TYPE(CTYPE_INT, ffi_type_sint);
        case TOK_LONG:
            INIT_TYPE(CTYPE_LONG, ffi_type_slong);
        case TOK_FLOAT:
            INIT_TYPE(CTYPE_FLOAT, ffi_type_float);
        case TOK_DOUBLE:
            INIT_TYPE(CTYPE_DOUBLE, ffi_type_double);
        case TOK_INT8_T:
            INIT_TYPE_T(CTYPE_INT8_T, int8_t, true);
        case TOK_INT16_T:
            INIT_TYPE_T(CTYPE_INT16_T, int16_t, true);
        case TOK_INT32_T:
            INIT_TYPE_T(CTYPE_INT32_T, int32_t, true);
        case TOK_INT64_T:
            INIT_TYPE_T(CTYPE_INT64_T, int64_t, true);
        case TOK_UINT8_T:
            INIT_TYPE_T(CTYPE_UINT8_T, uint8_t, false);
        case TOK_UINT16_T:
            INIT_TYPE_T(CTYPE_UINT16_T, uint16_t, false);
        case TOK_UINT32_T:
            INIT_TYPE_T(CTYPE_UINT32_T, uint32_t, false);
        case TOK_UINT64_T:
            INIT_TYPE_T(CTYPE_UINT64_T, uint64_t, false);
        case TOK_OFF_T:
            INIT_TYPE_T(CTYPE_OFF_T, off_t, true);
        case TOK_INO_T:
            INIT_TYPE_T(CTYPE_INO_T, ino_t, false);
        case TOK_DEV_T:
            INIT_TYPE_T(CTYPE_DEV_T, dev_t, false);
        case TOK_GID_T:
            INIT_TYPE_T(CTYPE_GID_T, gid_t, false);
        case TOK_MODE_T:
            INIT_TYPE_T(CTYPE_MODE_T, mode_t, false);
        case TOK_NLINK_T:
            INIT_TYPE_T(CTYPE_NLINK_T, nlink_t, false);
        case TOK_UID_T:
            INIT_TYPE_T(CTYPE_UID_T, uid_t, false);
        case TOK_PID_T:
            INIT_TYPE_T(CTYPE_PID_T, pid_t, true);
        case TOK_SIZE_T:
            INIT_TYPE_T(CTYPE_SIZE_T, size_t, false);
        case TOK_SSIZE_T:
            INIT_TYPE_T(CTYPE_SSIZE_T, ssize_t, true);
        case TOK_USECONDS_T:
            INIT_TYPE_T(CTYPE_USECONDS_T, useconds_t, false);
        case TOK_SUSECONDS_T:
            INIT_TYPE_T(CTYPE_SUSECONDS_T, suseconds_t, true);
        case TOK_BLKSIZE_T:
            INIT_TYPE_T(CTYPE_BLKSIZE_T, blksize_t, true);
        case TOK_BLKCNT_T:
            INIT_TYPE_T(CTYPE_BLKCNT_T, blkcnt_t, true);
        case TOK_TIME_T:
            INIT_TYPE_T(CTYPE_TIME_T, time_t, true);
        case TOK_NAME:
            lua_rawgetp(L, LUA_REGISTRYINDEX, &ctdef_registry);
            lua_getfield(L, -1, yyget_text());
            if (!lua_isnil(L, -1)) {
                *ct = *(struct ctype *)lua_touserdata(L, -1);
                lua_pop(L, 2);
                break;
            }
        default:
            return luaL_error(L, "%d:unknown type name '%s'", yyget_lineno(), yyget_text());
        }
        tok = yylex();
#undef INIT_TYPE
#undef INIT_TYPE_T
    }

    if (cparse_check_tok(L, tok) == TOK_INT) {
        switch (ct->type) {
        case CTYPE_LONG:
        case CTYPE_ULONG:
            tok = yylex();
            break;
        }
    } else if (cparse_check_tok(L, tok) == TOK_LONG) {
        switch (ct->type) {
        case CTYPE_INT:
            ct->type = CTYPE_LONG;
            ct->ft = &ffi_type_slong;
            tok = yylex();
            break;
        case CTYPE_UINT:
            ct->type = CTYPE_ULONG;
            ct->ft = &ffi_type_ulong;
            tok = yylex();
            break;
        case CTYPE_LONG:
            ct->type = CTYPE_LONGLONG;
            ct->ft = &ffi_type_sint64;
            tok = yylex();
            break;
        case CTYPE_ULONG:
            ct->type = CTYPE_ULONGLONG;
            ct->ft = &ffi_type_uint64;
            tok = yylex();
            break;
        }
    }

    if (cparse_check_tok(L, tok) == TOK_CONST) {
        ct->is_const = true;
        tok = yylex();
    }

    return tok;
}

static int cparse_function(lua_State *L, int tok, struct ctype *rtype)
{
    struct ctype args[MAX_FUNC_ARGS] = {};
    struct cfunc *func;
    int i, narg = 0;
    bool va = false;

    tok = cparse_pointer(L, tok, rtype);

    if (cparse_check_tok(L, tok) != TOK_NAME)
        return cparse_expected_error(L, tok, "identifier");

    lua_pushstring(L, yyget_text());

    lua_rawgetp(L, LUA_REGISTRYINDEX, &cfunc_registry);
    lua_pushvalue(L, -2);
    lua_gettable(L, -2);

    if (!lua_isnil(L, -1))
        return luaL_error(L, "%d:redefinition of function '%s'", yyget_lineno(), lua_tostring(L, -3));

    lua_pop(L, 1);

    tok = yylex();

    if (cparse_check_tok(L, tok) != '(')
        return cparse_expected_error(L, tok, "(");

    while (true) {
        bool flexible = true;
        int array_size;

        tok = yylex();
        if (cparse_check_tok(L, tok) == ')')
            break;

        if (cparse_check_tok(L, tok) == TOK_STRUCT || cparse_check_tok(L, tok) == TOK_UNION) {
            tok = cparse_record(L, &args[narg], cparse_check_tok(L, tok) == TOK_UNION);
        } else if (cparse_check_tok(L, tok) == TOK_VAL) {
            tok = yylex();
            if (cparse_check_tok(L, tok) != ')')
                return cparse_expected_error(L, tok, ")");
            va = true;
            break;
        } else {
            tok = cparse_basetype(L, tok, &args[narg]);
        }

        tok = cparse_pointer(L, tok, &args[narg]);

        if (cparse_check_tok(L, tok) != ')')
            check_void_forbidden(L, &args[narg], tok);
        else
            break;

        if (cparse_check_tok(L, tok) == TOK_NAME)
            tok = yylex();

        tok = cparse_array(L, tok, &flexible, &array_size);

        if (flexible || array_size >= 0)
            ctype_to_ptr(L, &args[narg]);

        narg++;

        if (cparse_check_tok(L, tok) == ')')
            break;

        if (cparse_check_tok(L, tok) != ',')
            return cparse_expected_error(L, tok, ",");
    }

    tok = yylex();
    if (cparse_check_tok(L, tok) != ';')
        return cparse_expected_error(L, tok, ";");

    func = calloc(1, sizeof(struct cfunc) + sizeof(struct ctype *) * narg);
    if (!func)
        return luaL_error(L, "no mem");

    func->narg = narg;
    func->va = va;

    for (i = 0;  i < narg; i++)
        func->args[i] = ctype_lookup(L, &args[i], false);

    func->rtype = ctype_lookup(L, rtype, false);

    lua_pushvalue(L, -2);
    lua_pushlightuserdata(L, func);
    lua_settable(L, -3);
    lua_pop(L, 2);

    return 0;
}

static int lua_ffi_cdef(lua_State *L)
{
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);
    lua_Debug ar;
    int tok;

    lua_getstack(L, 1, &ar);
    lua_getinfo(L, "nSl", &ar);

    yy_scan_bytes(str, len);
    yyset_lineno(ar.currentline);

    while ((tok = yylex())) {
        bool tdef = false;
        struct ctype ct;

        if (cparse_check_tok(L, tok) == ';')
            continue;

        if (cparse_check_tok(L, tok) == TOK_TYPEDEF) {
            tdef = true;
            tok = yylex();
        }

        tok = cparse_basetype(L, tok, &ct);

        if (tdef) {
            const char *name;

            tok = cparse_pointer(L, tok, &ct);

            if (cparse_check_tok(L, tok) != TOK_NAME)
                return cparse_expected_error(L, tok, "identifier");

            name = yyget_text();

            lua_rawgetp(L, LUA_REGISTRYINDEX, &ctdef_registry);
            lua_getfield(L, -1, name);

            if (!lua_isnil(L, -1))
                return luaL_error(L, "%d:redefinition of symbol '%s'", yyget_lineno(), name);

            lua_pop(L, 1);
            ctype_lookup(L, &ct, true);
            lua_setfield(L, -2, name);
            lua_pop(L, 1);

            if (cparse_check_tok(L, yylex()) != ';')
                return cparse_expected_error(L, tok, ";");

            continue;
        }

        if (cparse_check_tok(L, tok) == ';')
            continue;

        cparse_function(L, tok, &ct);
    }

    yylex_destroy();

    return 0;
}

static int load_lib(lua_State *L, const char *path, bool global)
{
    struct clib *lib;
    void *h;

    if (path) {
        h = dlopen(path, RTLD_LAZY | (global ? RTLD_GLOBAL : RTLD_LOCAL));
        if (!h) {
            const char *err = dlerror();
            return luaL_error(L, err ? err : "dlopen() failed");
        }
    } else {
        h = RTLD_DEFAULT;
    }

    lib = lua_newuserdata(L, sizeof(struct clib));
    lib->h = h;

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, lib);

    luaL_getmetatable(L, CLIB_MT);
    lua_setmetatable(L, -2);

    if (global) {
        lua_rawgetp(L, LUA_REGISTRYINDEX, &clib_registry);
        lua_pushvalue(L, -2);
        lua_rawsetp(L, -2, lib);
        lua_pop(L, 1);
    }

    return 1;
}

static int lua_ffi_load(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    bool global = lua_toboolean(L, 2);
    return load_lib(L, path, global);
}

static struct ctype *lua_check_ct(lua_State *L, bool *va, bool keep)
{
    struct cdata *cd;
    struct ctype *ct;

    if (lua_type(L, 1) == LUA_TSTRING) {
        size_t len;
        const char *str = luaL_checklstring(L, 1, &len);
        bool flexible = false;
        struct ctype match;
        int array_size;
        lua_Debug ar;
        int tok;

        lua_getstack(L, 1, &ar);
        lua_getinfo(L, "nSl", &ar);

        yy_scan_bytes(str, len);

        yyset_lineno(ar.currentline - 1);

        if (va)
            flexible = *va;

        tok = cparse_basetype(L, yylex(), &match);
        tok = cparse_pointer(L, tok, &match);
        tok = cparse_array(L, tok, &flexible, &array_size);

        if (tok)
            luaL_error(L, "%d:unexpected '%s'", yyget_lineno(), yyget_text());

        if (flexible || array_size >= 0) {
            if (flexible) {
                array_size = luaL_checkinteger(L, 2);
                luaL_argcheck(L, 2, array_size > 0, "array size must great than 0");
            }

            cparse_new_array(L, array_size, &match);
        }

        if (va)
            *va = flexible;

        yylex_destroy();

        return ctype_lookup(L, &match, keep);
    }

    if (va)
        *va = false;

    ct = luaL_testudata(L, 1, CTYPE_MT);
    if (ct)
        return ct;

    cd = luaL_testudata(L, 1, CDATA_MT);
    if (cd) {
        if (keep) {
            lua_rawgetp(L, LUA_REGISTRYINDEX, &ctype_registry);
            lua_pushlightuserdata(L, cd->ct);
            lua_gettable(L, -2);
            lua_remove(L, -2);
        }
        return cd->ct;
    }

    lua_type_error(L, 1, "C type");

    return NULL;
}

static int lua_ffi_new(lua_State *L)
{
    bool va = true;
    struct ctype *ct = lua_check_ct(L, &va, false);
    struct cdata *cd = cdata_new(L, ct, NULL);
    int idx = va ? 3 : 2;
    int ninit;

    ninit = lua_gettop(L) - idx;

    if (ninit == 1) {
        cdata_from_lua(L, cd->ct, cdata_ptr(cd), idx, false);
    } else if (ninit != 0) {
        __ctype_tostring(L, ct);
        return luaL_error(L, "too many initializers for '%s'", lua_tostring(L, -1));
    }

    return 1;
}

static int lua_ffi_cast(lua_State *L)
{
    struct ctype *ct = lua_check_ct(L, NULL, false);
    struct cdata *cd = cdata_new(L, ct, NULL);

    cdata_from_lua(L, ct, cdata_ptr(cd), 2, true);

    return 1;
}

static int lua_ffi_typeof(lua_State *L)
{
    lua_check_ct(L, NULL, true);
    return 1;
}

static int lua_ffi_addressof(lua_State *L)
{
    struct cdata *cd = luaL_checkudata(L, 1, CDATA_MT);
    struct ctype match = {
        .type = CTYPE_PTR,
        .ptr = cd->ct
    };
    struct ctype *ct = ctype_lookup(L, &match, false);
    cdata_ptr_set(cdata_new(L, ct, NULL), cdata_ptr(cd));
    return 1;
}

static int lua_ffi_gc(lua_State *L)
{
    struct cdata *cd = luaL_checkudata(L, 1, CDATA_MT);

    if (lua_isnil(L, 2)) {
        if (cd->gc_ref != LUA_REFNIL) {
            luaL_unref(L, LUA_REGISTRYINDEX, cd->gc_ref);
            cd->gc_ref = LUA_REFNIL;
        }
    } else {
        lua_pushvalue(L, 2);
        cd->gc_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    lua_settop(L, 1);
    return 1;
}

static int lua_ffi_sizeof(lua_State *L)
{
    struct ctype *ct = lua_check_ct(L, NULL, false);
    lua_pushinteger(L, ctype_sizeof(ct));
    return 1;
}

static int lua_ffi_offsetof(lua_State *L)
{
    struct ctype *ct = lua_check_ct(L, NULL, false);
    char const *name = luaL_checkstring(L, 2);
    struct crecord_field **fields;
    int i;

    if (ct->type != CTYPE_RECORD)
        return 0;

    fields = ct->rc->fields;

    for (i = 0; i < ct->rc->nfield; i++) {
        if (!strcmp(fields[i]->name, name)) {
            lua_pushinteger(L, fields[i]->offset);
            return 1;
        }
    }

    return 0;
}

static int lua_ffi_istype(lua_State *L)
{
    struct ctype *ct = lua_check_ct(L, NULL, false);
    struct cdata *cd = luaL_checkudata(L, 2, CDATA_MT);
    lua_pushboolean(L, ct == cd->ct);
    return 1;
}

static int lua_ffi_tonumber(lua_State *L)
{
    struct cdata *cd = luaL_checkudata(L, 1, CDATA_MT);
    struct ctype *ct = cd->ct;

    if (ct->type < CTYPE_VOID)
        return cdata_to_lua(L, ct, cdata_ptr(cd));
    lua_pushnil(L);
    return 1;
}

static int lua_ffi_string(lua_State *L)
{
    struct cdata *cd = luaL_checkudata(L, 1, CDATA_MT);
    struct carray *array = NULL;
    struct ctype *ct = cd->ct;
    const char *ptr = ct->type == CTYPE_PTR ? cdata_ptr_ptr(cd) : cdata_ptr(cd);
    size_t len;

    if (lua_gettop(L) > 1) {
        len = luaL_checkinteger(L, 2);

        switch (ct->type) {
        case CTYPE_PTR:
        case CTYPE_ARRAY:
        case CTYPE_RECORD:
            lua_pushlstring(L, ptr, len);
            return 1;
        default:
            goto converr;
        }
    }

    switch (ct->type) {
    case CTYPE_PTR:
        ct = ct->ptr;
        break;
    case CTYPE_ARRAY:
        array = ct->array;
        ct = array->ct;
        break;
    default:
        goto converr;
    }

    switch (ct->type) {
    case CTYPE_VOID:
    case CTYPE_CHAR:
    case CTYPE_UCHAR:
        break;
    default:
        goto converr;
    }

    if (array && array->size) {
        char *p = memchr(ptr, '\0', array->ft.size);
        len = p ? p - ptr : array->ft.size;
        lua_pushlstring(L, ptr, len);
    } else {
        lua_pushstring(L, ptr);
    }

    return 1;

converr:
    __cdata_tostring(L, cd);
    lua_pushfstring(L, "cannot convert '%s' to 'string'", lua_tostring(L, -1));
    luaL_argcheck(L, false, 1, lua_tostring(L, -1));
    return 0;
}

static int lua_ffi_copy(lua_State *L)
{
    struct cdata *cd = luaL_checkudata(L, 1, CDATA_MT);
    void *dst = cdata_ptr(cd);
    const void *src;
    size_t len;

    if (lua_gettop(L) < 3) {
        src = luaL_checklstring(L, 2, &len);
        memcpy(dst, src, len);
        ((char *)dst)[len++] = '\0';
    } else {
        len = luaL_checkinteger(L, 3);

        if (lua_type(L, 2) == LUA_TSTRING)
            src = lua_tostring(L, 2);
        else
            src = cdata_ptr(luaL_checkudata(L, 2, CDATA_MT));

        memcpy(dst, src, len);
    }

    lua_pushinteger(L, len);

    return 1;
}

static int lua_ffi_fill(lua_State *L)
{
    struct cdata *cd = luaL_checkudata(L, 1, CDATA_MT);
    int len = luaL_checkinteger(L, 2);
    int c = luaL_optinteger(L, 3, 0);

    memset(cdata_ptr(cd), c, len);

    return 0;
}

static int lua_ffi_errno(lua_State *L)
{
    int cur = errno;

    if (lua_gettop(L) > 0)
        errno = luaL_checkinteger(L, 1);

    lua_pushinteger(L, cur);
    return 1;
}

static const luaL_Reg methods[] = {
    {"cdef", lua_ffi_cdef},
    {"load", lua_ffi_load},

    {"new", lua_ffi_new},
    {"cast", lua_ffi_cast},
    {"typeof", lua_ffi_typeof},
    {"addressof", lua_ffi_addressof},
    {"gc", lua_ffi_gc},

    {"sizeof", lua_ffi_sizeof},
    {"offsetof", lua_ffi_offsetof},
    {"istype", lua_ffi_istype},

    {"tonumber", lua_ffi_tonumber},
    {"string", lua_ffi_string},
    {"copy", lua_ffi_copy},
    {"fill", lua_ffi_fill},
    {"errno", lua_ffi_errno},

    {NULL, NULL}
};

static void createmetatable(lua_State *L, const char *name, const struct luaL_Reg regs[])
{
    luaL_newmetatable(L, name);
    luaL_setfuncs(L, regs, 0);
    lua_pop(L, 1);
}

static void create_nullptr(lua_State *L)
{
    struct ctype *ct = ctype_new(L, false);
    ctype_to_ptr(L, ct);
    cdata_ptr_set(cdata_new(L, ct, NULL), NULL);
}

int luaopen_ffi(lua_State *L)
{
    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &crecord_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &carray_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &cfunc_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &ctype_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &ctdef_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &clib_registry);

    createmetatable(L, CDATA_MT, cdata_methods);
    createmetatable(L, CTYPE_MT, ctype_methods);
    createmetatable(L, CLIB_MT, clib_methods);

    luaL_newlib(L, methods);

    lua_pushstring(L, LUA_FFI_VERSION_STRING);
    lua_setfield(L, -2, "VERSION");

    create_nullptr(L);
    lua_setfield(L, -2, "nullptr");

    load_lib(L, NULL, true);
    lua_setfield(L, -2, "C");

    return 1;
}
