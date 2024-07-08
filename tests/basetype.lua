#!/usr/bin/env lua

local ffi = require 'ffi'

ffi.cdef([[
    typedef struct Point {
        int x;
        int y;
    } Point;

    typedef union DataUnion {
        int i;
        float f;
        char str[20];
    } DataUnion;

    struct ComplexStruct {
        int id;
        char name[50];

        Point *location;

        int scores[10];

        struct {
            struct Point topLeft;
            struct Point bottomRight;
        } boundingBox;

        DataUnion data;

        struct NestedStruct {
            int a;
            int b;
        } *nestedStructPtr;

        struct {
            int a;
            int b;
            struct {
                int c;
            };
        };
    };

    struct empty0 {};

    struct empty1 {
        char name[0];
    };

    union empty2 {
        char name[0];
    };

    typedef struct md5_ctx {
        uint32_t lo, hi;
        uint32_t a, b, c, d;
        unsigned char buffer[64];
    } md5_ctx_t;

    int printf(const char *format, ...);
    struct ComplexStruct *fun1();
    struct ComplexStruct *fun2(int a[]);
]])

assert(tostring(ffi.typeof('void')) == 'ctype<void>')
assert(tostring(ffi.typeof('char')) == 'ctype<char>')
assert(tostring(ffi.typeof('short')) == 'ctype<short>')
assert(tostring(ffi.typeof('int')) == 'ctype<int>')
assert(tostring(ffi.typeof('long')) == 'ctype<long>')

assert(tostring(ffi.typeof('unsigned char')) == 'ctype<unsigned char>')
assert(tostring(ffi.typeof('unsigned short')) == 'ctype<unsigned short>')
assert(tostring(ffi.typeof('unsigned int')) == 'ctype<unsigned int>')
assert(tostring(ffi.typeof('unsigned long')) == 'ctype<unsigned long>')

assert(tostring(ffi.typeof('long int')) == 'ctype<long>')
assert(tostring(ffi.typeof('int long')) == 'ctype<long>')
assert(tostring(ffi.typeof('long long')) == 'ctype<long long>')

assert(tostring(ffi.typeof('float')) == 'ctype<float>')
assert(tostring(ffi.typeof('double')) == 'ctype<double>')

assert(tostring(ffi.typeof('int8_t')) == 'ctype<int8_t>')
assert(tostring(ffi.typeof('int16_t')) == 'ctype<int16_t>')
assert(tostring(ffi.typeof('int32_t')) == 'ctype<int32_t>')
assert(tostring(ffi.typeof('int64_t')) == 'ctype<int64_t>')
assert(tostring(ffi.typeof('uint8_t')) == 'ctype<uint8_t>')
assert(tostring(ffi.typeof('uint16_t')) == 'ctype<uint16_t>')
assert(tostring(ffi.typeof('uint32_t')) == 'ctype<uint32_t>')
assert(tostring(ffi.typeof('uint64_t')) == 'ctype<uint64_t>')

assert(tostring(ffi.typeof('ino_t')) == 'ctype<ino_t>')
assert(tostring(ffi.typeof('dev_t')) == 'ctype<dev_t>')
assert(tostring(ffi.typeof('gid_t')) == 'ctype<gid_t>')
assert(tostring(ffi.typeof('mode_t')) == 'ctype<mode_t>')
assert(tostring(ffi.typeof('nlink_t')) == 'ctype<nlink_t>')
assert(tostring(ffi.typeof('uid_t')) == 'ctype<uid_t>')
assert(tostring(ffi.typeof('off_t')) == 'ctype<off_t>')
assert(tostring(ffi.typeof('pid_t')) == 'ctype<pid_t>')
assert(tostring(ffi.typeof('size_t')) == 'ctype<size_t>')
assert(tostring(ffi.typeof('ssize_t')) == 'ctype<ssize_t>')
assert(tostring(ffi.typeof('useconds_t')) == 'ctype<useconds_t>')
assert(tostring(ffi.typeof('suseconds_t')) == 'ctype<suseconds_t>')
assert(tostring(ffi.typeof('blksize_t')) == 'ctype<blksize_t>')
assert(tostring(ffi.typeof('blkcnt_t')) == 'ctype<blkcnt_t>')
assert(tostring(ffi.typeof('time_t')) == 'ctype<time_t>')

assert(tostring(ffi.typeof('struct ComplexStruct')) == 'ctype<struct ComplexStruct>')
assert(tostring(ffi.typeof('Point')) == 'ctype<struct Point>')
