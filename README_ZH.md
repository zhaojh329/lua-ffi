# lua-ffi

[1]: https://img.shields.io/badge/license-MIT-brightgreen.svg?style=plastic
[2]: /LICENSE
[3]: https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=plastic
[4]: https://github.com/zhaojh329/lua-ffi/pulls
[5]: https://img.shields.io/badge/Issues-welcome-brightgreen.svg?style=plastic
[6]: https://github.com/zhaojh329/lua-ffi/issues/new
[7]: https://img.shields.io/badge/release-1.2.0-blue.svg?style=plastic
[8]: https://github.com/zhaojh329/lua-ffi/releases
[9]: https://github.com/zhaojh329/lua-ffi/workflows/build/badge.svg
[10]: https://img.shields.io/github/downloads/zhaojh329/lua-ffi/total

[![license][1]][2]
[![PRs Welcome][3]][4]
[![Issue Welcome][5]][6]
[![Release Version][7]][8]
![Build Status][9]
![Downloads][10]
![visitors](https://visitor-badge.laobi.icu/badge?page_id=zhaojh329.lua-ffi)

[LuaJIT]: https://luajit.org/
[cffi-lua]: https://github.com/q66/cffi-lua
[libffi]: https://sourceware.org/libffi/

外部函数接口（FFI）是一种机制，使一种编程语言编写的程序可以调用另一种语言实现或编译出的函数/服务。FFI 常用于调用二进制动态链接库中的能力。

lua-ffi 是一个面向 Lua 的可移植、轻量级 C FFI，基于 [libffi] 实现，目标是尽量兼容 [LuaJIT] FFI，并且由 C 语言从零实现。

## 特性

* 可移植：可用于 Lua5.1、Lua5.2、Lua5.3 和 Lua5.4。
* 轻量：使用 C 语言实现，体积很小，仅约 50KB。

## 示例

```lua
local ffi = require 'ffi'

ffi.cdef([[
    struct timeval {
        time_t      tv_sec;     /* seconds */
        suseconds_t tv_usec;    /* microseconds */
    };

    struct timezone {
        int tz_minuteswest;     /* minutes west of Greenwich */
        int tz_dsttime;         /* type of DST correction */
    };

    int gettimeofday(struct timeval *tv, struct timezone *tz);
    char *strerror(int errnum);
]])

local function strerror(errno)
    return ffi.string(ffi.C.strerror(errno))
end

local tv = ffi.new('struct timeval')

if ffi.C.gettimeofday(tv, nil) < 0 then
    print('gettimeofday fail:', strerror(ffi.errno()))
else
    print('tv.tv_sec:', tv.tv_sec, 'tv.tv_usec:', tv.tv_usec)
end
```

## [使用指南](USAGE_ZH.md)

## 依赖

* [libffi] - 一个可移植的外部函数接口库。
* Lua 5.1 或更新版本（已测试到 5.4）。

## 构建

### Ubuntu

    sudo apt install -y liblua5.3-dev libffi-dev
    git clone https://github.com/zhaojh329/lua-ffi.git
    cd lua-ffi && mkdir build && cd build
    cmake .. && sudo make install

### OpenWrt

    Languages  --->
        Lua  --->
            <>  lua-ffi............ A portable lightweight C FFI for lua5.1, based on libffi
            <>  lua-ffi-lua5.3..... A portable lightweight C FFI for lua5.3, based on libffi
            <*> lua-ffi-lua5.4..... A portable lightweight C FFI for lua5.4, based on libffi

## [测试](/tests)

## 致谢

本项目受以下仓库启发：

- [cffi-lua]

感谢这些项目作者的优秀工作。
