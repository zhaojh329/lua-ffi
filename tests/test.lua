#!/usr/bin/env lua

local ffi = require 'ffi'

ffi.cdef([[
    struct Point {
        int x;
        int y;
    };

    struct student {
        int age;
        char name[0];
    };

    struct student2 {
        int age;
        char name[128];
    };

    typedef struct {
        double x;
        double y;
    } coordinate;

    struct anon_nest {
        int a;
        struct {
            int b;
            struct {
                int c;
                struct {
                    int d;
                };
            };
        };
    };

    int sprintf(char *str, const char *format, ...);

    void *malloc(size_t size);
    void free(void *ptr);
]])

local tests = {
    function()
        local a = ffi.new('int [10]', {1, 2, 3})
        assert(a[0] == 1)
        assert(a[1] == 2)
        assert(a[2] == 3)
        a[9] = 123
        assert(a[9] == 123)
    end,
    function()
        local st = ffi.new('struct student2', { 12, 'bobo' })
        assert(st.age == 12)
        assert(ffi.string(st.name) == 'bobo')
    end,
    function()
        local st = ffi.new('struct student2', { age = 12, name = 'bobo' })
        assert(st.age == 12)
        assert(ffi.string(st.name) == 'bobo')

        ffi.copy(st.name, 'wen')
        assert(ffi.string(st.name) == 'wen')
    end,
    function()
        local c = ffi.new('coordinate', { 45.6, 67 })
        assert(c.x == 45.6)
        assert(c.y == 67)
    end,
    function()
        local c = ffi.new('coordinate', { 45.6, 67 })
        local p = ffi.new('coordinate *', c)
        assert(p.x == 45.6)
        assert(p.y == 67)
    end,
    function()
        -- int *p = (int *)4567
        local p = ffi.cast('int *', 4567)

        -- int n = (int)p
        assert(ffi.tonumber(ffi.cast('int', p)) == 4567)

        p = ffi.cast('int *', ffi.new('int', 4567))
        assert(ffi.tonumber(ffi.cast('int', p)) == 4567)
    end,
    function()
        -- void *p = malloc(sizeof(struct student) + 128)
        -- struct student *st = p
        local p = ffi.gc(ffi.C.malloc(ffi.sizeof('struct student') + 128), ffi.C.free)
        local st = ffi.new('struct student *', p)
        st.age = 45
        st.name = 'asd'
        assert(st.age == 45)
        assert(ffi.string(st.name) == 'asd')
    end,
    function()
        local an = ffi.new('struct anon_nest', {1, { 2, { 3, { 4 } }}})
        assert(an.a == 1)
        assert(an.b == 2)
        assert(an.c == 3)
        assert(an.d == 4)
    end,
    function()
        local buf = ffi.gc(ffi.C.malloc(128), ffi.C.free)

        -- note: for lua 5.1 and 5.2, numbers like 2.0, 3.0, will be treated as integers
        -- but in OpenWrt, there is a patch for lua5.1 to support distinguish integer type.
        local n = ffi.C.sprintf(buf, '%s %d %.2f %.2f', 'hello', 1, 2.1, 3.3)
        assert(n == 17)
        assert(ffi.string(buf) == 'hello 1 2.10 3.30')
    end,
    function()
        local buf = ffi.gc(ffi.C.malloc(128), ffi.C.free)

        local a1 = ffi.new('int', 1)
        local a2 = ffi.new('double', 2.0)

        local n = ffi.C.sprintf(buf, '%s %d %.2f %.2f', 'hello', a1, a2, 3.3)
        assert(n == 17)
        assert(ffi.string(buf) == 'hello 1 2.00 3.30')
    end,
    function()
        ffi.cdef([[
            int student_get_age(struct student st);
            int student_get_age_ptr(struct student *st);
            const char *student_get_name(struct student *st);
            struct student *student_new(int age, const char *name);
        ]])

        local lib = ffi.load('./libtest.so')
        local st = ffi.new('struct student', { 12 })
        assert(lib.student_get_age(st) == 12)
        assert(lib.student_get_age_ptr(st) == 12)

        st = ffi.gc(lib.student_new(32, 'bobo'), ffi.C.free)
        assert(lib.student_get_age_ptr(st) == 32)
        assert(ffi.string(lib.student_get_name(st)) == 'bobo')
    end,
    function()
        ffi.cdef([[
            int *pass_array(int a[]);
        ]])
        local lib = ffi.load('./libtest.so')
        local a = ffi.new('int[3]', { 1, 2, 3})
        local p = lib.pass_array(a)
        assert(p[0] == 1)
        assert(p[1] == 2)
        assert(p[2] == 3)

        p[1] = 567
        assert(a[1] == 567)
    end,
    function()
        local tp = ffi.metatype(ffi.typeof('struct Point'), {
            __tostring = function(self)
                return string.format('x:%d,y:%d', self.x, self.y)
            end,
            __index = {
                add = function(self, x, y)
                    self.x = self.x + x
                    self.y = self.y + y
                end
            }
        })

        local p = ffi.new(tp, {45, 67})
        assert(tostring(p) == 'x:45,y:67')

        p:add(1, 1)
        assert(p.x == 46 and p.y == 68)
    end
}

for _, test in pairs(tests) do
    test()
end

print('Test PASS')
