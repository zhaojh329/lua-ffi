/* SPDX-License-Identifier: MIT */
/*
 * Author: Jianhui Zhao <zhaojh329@gmail.com>
 */

#ifndef __LUA_FFI_HELPER_H
#define __LUA_FFI_HELPER_H

#define stack_dump(L, title)                                    \
    do {                                                        \
        int top = lua_gettop(L);                                \
        int i;                                                  \
        printf("--------stack dump:%s--------\n", title);       \
        for (i = 1; i <= top; i++) {                            \
            int t = lua_type(L, i);                             \
            printf("%2d: ", i);                                 \
            switch (t) {                                        \
            case LUA_TSTRING:                                   \
                printf("'%s'", lua_tostring(L, i));             \
                break;                                          \
            case LUA_TBOOLEAN:                                  \
                printf(lua_toboolean(L, i) ? "true" : "false"); \
                break;                                          \
            case LUA_TNUMBER:                                   \
                printf("%g", lua_tonumber(L, i));               \
                break;                                          \
            case LUA_TLIGHTUSERDATA:                            \
                printf("%p", lua_topointer(L, i));              \
                break;                                          \
            default:                                            \
                printf("%s", lua_typename(L, t));               \
                break;                                          \
            }                                                   \
            printf(" ");                                        \
        }                                                       \
        printf("\n");                                           \
        printf("++++++++++++++++++++++++++\n");                 \
    } while (0)

#endif
