# lua-ffi

[1]: https://img.shields.io/badge/license-MIT-brightgreen.svg?style=plastic
[2]: /LICENSE
[3]: https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=plastic
[4]: https://github.com/zhaojh329/lua-ffi/pulls
[5]: https://img.shields.io/badge/Issues-welcome-brightgreen.svg?style=plastic
[6]: https://github.com/zhaojh329/lua-ffi/issues/new
[7]: https://img.shields.io/badge/release-1.0.0-blue.svg?style=plastic
[8]: https://github.com/zhaojh329/lua-ffi/releases
[9]: https://github.com/zhaojh329/lua-ffi/workflows/build/badge.svg

[![license][1]][2]
[![PRs Welcome][3]][4]
[![Issue Welcome][5]][6]
[![Release Version][7]][8]
![Build Status][9]
![visitors](https://visitor-badge.laobi.icu/badge?page_id=zhaojh329.lua-ffi)

[LuaJIT]: https://luajit.org/
[cffi-lua]: https://github.com/q66/cffi-lua
[libffi]: https://sourceware.org/libffi/

A foreign function interface (FFI) is a mechanism by which a program written in one programming language
can call routines or make use of services written or compiled in another one. An FFI is often used in
contexts where calls are made into binary dynamic-link library.

Lua-ffi is a portable lightweight C FFI for Lua, based on [libffi] and aiming to be mostly compatible
with [LuaJIT] FFI, but written from scratch in C language.

## Features

* portable - Used in Lua5.1, Lua5.2, Lua5.3 and Lua5.4.
* lightweight - Written in C language, very small, only about 50KB.

## Example

```lua
local ffi = require 'ffi'

ffi.cdef([[
    struct timeval {
        time_t      tv_sec;     /* seconds */
        suseconds_t tv_usec;    /* microseconds */
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

## Basic types supported

void bool char short int long float double

int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t

ino_t dev_t gid_t mode_t nlink_t uid_t off_t pid_t size_t ssize_t
useconds_t suseconds_t blksize_t blkcnt_t time_t

## Requirements

* [libffi] - A portable foreign-function interface library.
* Lua 5.1 or newer (tested up to and including 5.4).

## Build

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

## [Testing](/tests)

## Acknowledgements

This project was inspired by the following repositories:

- [cffi-lua]

Thanks to the authors of these repositories for their excellent work.
