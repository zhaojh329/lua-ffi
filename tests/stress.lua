#!/usr/bin/env lua

local ffi = require 'ffi'

ffi.cdef([[
    struct student {
        int age;
        char name[0];
    };

    int printf(const char *format, ...);
    void *malloc(size_t size);
    void free(void *ptr);
    int usleep(useconds_t usec);
]])

local function calc_mem()
    local n = collectgarbage('count')

    if n > 1024 * 1024 then
        return n / 1024 * 1024, 'G'
    end

    if n > 1024 then
        return n / 1024, 'M'
    end

    return n, 'K'
end

collectgarbage('setpause', 100)

while true do
    ffi.new('int [10]', {1, 2, 3})

    local p = ffi.gc(ffi.C.malloc(ffi.sizeof('struct student') + 128), ffi.C.free)
    local st = ffi.new('struct student *', p)
    st.age = 45
    st.name = 'asd'
    assert(st.age == 45)
    assert(ffi.string(st.name) == 'asd')

    ffi.C.usleep(100)

    ffi.C.printf('mem: %.2f %sB\n', calc_mem())
end
