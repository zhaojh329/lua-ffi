local ffi = require 'ffi'

ffi.cdef[[
int bf_count(void);
const char *bf_decl(int c);
const char *bf_name(int c);
size_t bf_size(int c);
size_t bf_align(int c);
int bf_fields(int c);
const char *bf_field(int c, int f);
const char *bf_type(int c, int f);
size_t bf_offset(int c, int f);
long long bf_set(int c, void *p, int f, long long v);
long long bf_get(int c, void *p, int f);
int bf_equal(int c, void *a, void *b);
double bf_number(int c, void *p, int f);
void bf_pattern(int c, void *p, int f, unsigned char byte);
void *bf_alloc(int c);
void bf_free(void *p);
void *bf_guard(void);
void bf_unguard(void *p);
]]

local lib = ffi.load(assert(arg[1], 'expected C oracle library path'))

for c = 0, lib.bf_count() - 1 do
    local name = ffi.string(lib.bf_name(c))
    local ct = 'struct ' .. name

    ffi.cdef(ffi.string(lib.bf_decl(c)))
    assert(ffi.sizeof(ct) == lib.bf_size(c), name .. ': size')
    assert(ffi.offsetof('struct align_' .. name, 'value') == lib.bf_align(c), name .. ': alignment')

    local storage = lib.bf_alloc(c)
    assert(storage ~= nil)

    local native = ffi.cast(ct .. ' *', storage)
    local array = ffi.new(ct .. '[2]')
    local value = array[0]
    local value_ptr = ffi.cast('void *', value)
    local neighbor_ptr = ffi.cast('void *', array[1])

    for f = 0, lib.bf_fields(c) - 1 do
        local field = ffi.string(lib.bf_field(c, f))
        local base = ffi.string(lib.bf_type(c, f))
        local label = name .. '.' .. field

        assert(ffi.offsetof(ct, field) == lib.bf_offset(c, f), label .. ': offset')

        for _, input in ipairs({0, 1, 2, 5, 42}) do
            local expected = lib.bf_set(c, native, f, input)
            local output = native[field]

            if base == 'bool' then
                assert(output == (expected ~= 0), label .. ': C to Lua bool')
            else
                assert(output == expected, label .. ': C to Lua')
            end

            value[field] = input
            assert(lib.bf_equal(c, value_ptr, native) == 1, label .. ': Lua to C')
            assert(lib.bf_get(c, neighbor_ptr, f) == 0, name .. ': array neighbor')
        end

        -- Preserve all 64 bits on writes; compare numeric reads at Lua's precision.
        for _, byte in ipairs({85, 170, 255}) do
            if base == 'bool' then
                value[field] = true
            else
                local pattern = ffi.new(base)

                ffi.fill(pattern, ffi.sizeof(base), byte)
                value[field] = pattern
            end

            lib.bf_pattern(c, native, f, byte)
            assert(lib.bf_equal(c, value_ptr, native) == 1,
                   label .. ': bit pattern')

            if base ~= 'bool' then
                assert(native[field] + 0.0 == lib.bf_number(c, native, f),
                       label .. ': numeric pattern')
            end
        end

        value[field] = 0
        lib.bf_set(c, native, f, 0)
    end

    -- Nested views must use the same corrected size and layout.
    local first = ffi.string(lib.bf_field(c, 0))
    local wrapped = ffi.new('struct align_' .. name)

    value[first] = 1
    wrapped.value = value
    assert(lib.bf_equal(c, ffi.cast('void *', wrapped.value), value_ptr) == 1,
           name .. ': nested')
    lib.bf_free(storage)

    local initialized = ffi.new(ct, {[first] = 1})
    assert(lib.bf_equal(c, ffi.cast('void *', initialized), value_ptr) == 1,
           name .. ': initializer')
end

local page = lib.bf_guard()
assert(page ~= nil, 'guard-page allocation failed')

local tiny = ffi.cast('struct tiny *', page)
tiny.a = 5
tiny.b = 17
assert(tiny.a == 5 and tiny.b == 17)
lib.bf_unguard(page)

local ok = pcall(ffi.cdef, 'struct invalid_bool_width { bool a:2; };')
assert(not ok, 'bool bitfield width must be at most one')

print('bitfield C/Lua oracle: ' .. lib.bf_count() .. ' layouts passed')
