#!/usr/bin/env lua

local ffi = require 'ffi'

local function expect_error(fn, needle)
    local ok, err = pcall(fn)
    assert(not ok)
    assert(type(err) == 'string')
    if needle then
        assert(err:find(needle, 1, true), err)
    end
end

local function script_dir()
    local src = debug.getinfo(1, 'S').source
    if src:sub(1, 1) == '@' then
        src = src:sub(2)
    end
    return src:match('(.*/)') or './'
end

local LIB_PATH = script_dir() .. 'libtest.so'

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

    typedef struct md5_ctx {
        uint32_t lo, hi;
        uint32_t a, b, c, d;
        unsigned char buffer[64];
    } md5_ctx_t;

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

    struct packed_demo {
        char c;
        int i;
    } __attribute__((packed));

    struct normal_demo {
        char c;
        int i;
    };

    int sprintf(char *str, const char *format, ...);
    void *malloc(size_t size);
    void free(void *ptr);
    int open(const char *pathname, int flags);

    int student_get_age(struct student st);
    int student_get_age_ptr(struct student *st);
    const char *student_get_name(struct student *st);
    struct student *student_new(int age, const char *name);

    int *pass_array(int a[]);

    int cb_mul10(int i);
    int call_f0(int (*cb)(int));
    int call_f1(int (*cb)(int), int x);
    int call_f2(int (*cb)(int i), int x);
    int call_f3(int x, int (*cb)(int i));

    typedef int (*callback_t)(int);
    int call_f4(int x, callback_t cb);

    int missing_symbol(void);
]])

local tests = {
    function()
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
        assert(tostring(ffi.typeof('DataUnion')) == 'ctype<union DataUnion>')
        assert(tostring(ffi.typeof('struct empty0')) == 'ctype<struct empty0>')
        assert(tostring(ffi.typeof('md5_ctx_t')) == 'ctype<struct md5_ctx>')
    end,
    function()
        assert(type(ffi.VERSION) == 'string')
        assert(#ffi.VERSION > 0)

        assert(tostring(ffi.C) == 'library: default')
        assert(ffi.nullptr == ffi.cast('void *', nil))
    end,
    function()
        local lib = ffi.load(LIB_PATH)
        assert(tostring(lib):find('library: ', 1, true))

        local libg = ffi.load(LIB_PATH, true)
        assert(libg.cb_mul10(9) == 90)

        expect_error(function()
            return lib.unknown_without_cdef
        end, 'missing declaration for function')

        expect_error(function()
            return lib.missing_symbol
        end, 'undefined function')
    end,
    function()
        local a = ffi.new('int [10]', {1, 2, 3})
        assert(a[0] == 1)
        assert(a[1] == 2)
        assert(a[2] == 3)

        a[9] = 123
        assert(a[9] == 123)
        assert(#a == 10)

        local flex = ffi.new('int [?]', 4, {7, 8, 9})
        assert(#flex == 4)
        assert(flex[0] == 7)
        assert(flex[1] == 8)
        assert(flex[2] == 9)
        assert(flex[3] == 0)
    end,
    function()
        local st = ffi.new('struct student2', { 12, 'bobo' })
        assert(st.age == 12)
        assert(ffi.string(st.name) == 'bobo')

        st = ffi.new('struct student2', { age = 25, name = 'anna' })
        assert(st.age == 25)
        assert(ffi.string(st.name) == 'anna')

        local an = ffi.new('struct anon_nest', {1, {2, {3, {4}}}})
        assert(an.a == 1)
        assert(an.b == 2)
        assert(an.c == 3)
        assert(an.d == 4)
    end,
    function()
        local c = ffi.new('coordinate', { 45.5, 67 })
        local cp = ffi.new('coordinate *', c)
        assert(cp.x == 45.5)
        assert(cp.y == 67)

        local p = ffi.cast('int *', 4567)
        assert(ffi.tonumber(ffi.cast('int', p)) == 4567)

        p = ffi.cast('int *', ffi.new('int', 789))
        assert(ffi.tonumber(ffi.cast('int', p)) == 789)
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
        p:add(1, 2)
        assert(p.x == 46)
        assert(p.y == 69)
    end,
    function()
        local t_int_ptr = ffi.typeof('int *')
        local v_int = ffi.new('int', 123)
        local v_int_ptr = ffi.new('int *', v_int)

        assert(tostring(t_int_ptr):find('ctype<', 1, true))

        assert(ffi.istype('int', v_int))
        assert(ffi.istype(t_int_ptr, v_int_ptr))
        assert(not ffi.istype('double', v_int))

        local addr = ffi.addressof(v_int)
        addr[0] = 999
        assert(ffi.tonumber(v_int) == 999)
    end,
    function()
        local psize = ffi.sizeof('struct packed_demo')
        local nsize = ffi.sizeof('struct normal_demo')
        assert(psize == 5)
        assert(nsize == 8)

        assert(ffi.offsetof('struct packed_demo', 'i') == 1)
        assert(ffi.offsetof('struct normal_demo', 'i') == 4)
        assert(ffi.offsetof('struct normal_demo', 'no_such') == nil)

        local p = ffi.new('struct Point')
        assert(ffi.sizeof(p) == ffi.sizeof('struct Point'))
    end,
    function()
        local x = ffi.new('int', 17)
        assert(ffi.tonumber(x) == 17)

        local pt = ffi.new('struct Point', {1, 2})
        assert(ffi.tonumber(pt) == nil)

        local chars = ffi.new('char [16]', 'hello')
        assert(ffi.string(chars) == 'hello')
        assert(ffi.string(chars, 3) == 'hel')

        local raw = ffi.new('char [8]')
        ffi.copy(raw, 'abcdef', 6)
        assert(ffi.string(raw, 3) == 'abc')

        local raw_ptr = ffi.cast('const char *', raw)
        assert(ffi.string(raw_ptr, 3) == 'abc')

        local bytes = ffi.string(pt, 4)
        assert(type(bytes) == 'string')
        assert(#bytes == 4)

        expect_error(function()
            ffi.string(x)
        end, 'cannot convert')
    end,
    function()
        local dst = ffi.new('char [16]')
        local n = ffi.copy(dst, 'abc')
        assert(n == 4)
        assert(ffi.string(dst) == 'abc')

        local src = ffi.new('char [8]', 'xyz')
        n = ffi.copy(dst, src, 2)
        assert(n == 2)
        assert(ffi.string(dst, 2) == 'xy')

        local u8 = ffi.new('uint8_t [4]')
        ffi.fill(u8, 4, 0x5a)
        assert(u8[0] == 0x5a)
        assert(u8[1] == 0x5a)
        assert(u8[2] == 0x5a)
        assert(u8[3] == 0x5a)
    end,
    function()
        local prev = ffi.errno()
        local got = ffi.errno(234)
        assert(got == prev)
        assert(ffi.errno() == 234)

        local fd = ffi.C.open('/definitely-not-exist/lua-ffi-test', 0)
        assert(fd == -1)
        assert(ffi.errno() ~= 0)

        ffi.errno(prev)
    end,
    function()
        local lib = ffi.load(LIB_PATH)

        local st = ffi.new('struct student', {12})
        assert(lib.student_get_age(st) == 12)
        assert(lib.student_get_age_ptr(st) == 12)

        local pst = ffi.gc(lib.student_new(32, 'bobo'), ffi.C.free)
        assert(lib.student_get_age_ptr(pst) == 32)
        assert(ffi.string(lib.student_get_name(pst)) == 'bobo')

        local arr = ffi.new('int [3]', {1, 2, 3})
        local parr = lib.pass_array(arr)
        assert(parr[0] == 1)
        assert(parr[1] == 2)
        assert(parr[2] == 3)

        parr[1] = 567
        assert(arr[1] == 567)
    end,
    function()
        local lib = ffi.load(LIB_PATH)

        assert(lib.call_f0(lib.cb_mul10) == 200)
        assert(lib.call_f1(lib.cb_mul10, 20) == 200)
        assert(lib.call_f2(lib.cb_mul10, 20) == 200)
        assert(lib.call_f3(20, lib.cb_mul10) == 200)

        local cb = ffi.cast('int (*)(int)', function(i)
            return i * 10
        end)
        assert(lib.call_f4(20, cb) == 200)

        expect_error(function()
            return ffi.cast('int (*)(int)', 0)
        end, 'function expected')
    end,
    function()
        local called = 0
        do
            local p = ffi.gc(ffi.new('int [1]'), function()
                called = called + 1
            end)
            ffi.gc(p, nil)
            p = nil
        end

        collectgarbage('collect')
        collectgarbage('collect')
        assert(called == 0)

        do
            local p = ffi.gc(ffi.new('int [1]'), function()
                called = called + 1
            end)
            p = nil
        end

        collectgarbage('collect')
        collectgarbage('collect')
        assert(called > 0)
    end,
    function()
        local buf = ffi.gc(ffi.C.malloc(128), ffi.C.free)
        local n = ffi.C.sprintf(buf, '%s %d %.2f %.2f', 'hello', 1, 2.1, 3.3)
        assert(n == 17)
        assert(ffi.string(ffi.cast('const char *', buf)) == 'hello 1 2.10 3.30')

        local a1 = ffi.new('int', 1)
        local a2 = ffi.new('double', 2.0)
        n = ffi.C.sprintf(buf, '%s %d %.2f %.2f', 'hello', a1, a2, 3.3)
        assert(n == 17)
        assert(ffi.string(ffi.cast('const char *', buf)) == 'hello 1 2.00 3.30')
    end,
}

for _, test in pairs(tests) do
    test()
end

print('Test PASS')
