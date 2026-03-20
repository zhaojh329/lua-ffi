# lua-ffi Usage Guide

## Loading the FFI Library

```lua
local ffi = require("ffi")
```

The module exports functions plus three values:

- `ffi.VERSION`
- `ffi.nullptr`
- `ffi.C`

## ffi.cdef: Declaring C Types and Functions

Use `ffi.cdef` to provide C declarations.

```lua
ffi.cdef([[ 
    struct Point {
        int x;
        int y;
    };

    typedef struct Point Point;
    typedef int (*callback_t)(int);

    int puts(const char *s);
]])
```

### What `cdef` supports

- Basic C scalar types supported by this project.
- `typedef` declarations.
- `struct` and `union` declarations (including nested/anonymous members).
- Packed attribute parsing (`__attribute__((packed))`).
- Function declarations and function pointer types.
- Array declarators, including flexible form `?` in type strings used by `ffi.new`.

### Notes

- Declarations are additive.
- Redefinition of known symbols is rejected.
- `cdef` is for declarations only (no function definitions).

### Default basic types supported

The following built-in basic C types are available by default:

void bool char short int long float double

int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t

ino_t dev_t gid_t mode_t nlink_t uid_t off_t pid_t size_t ssize_t
useconds_t suseconds_t blksize_t blkcnt_t time_t

## Accessing C Libraries: ffi.C and ffi.load

### `ffi.C`

`ffi.C` is the object returned by the ffi module's default load of the system C library.
Use this object to access declared system C functions.

```lua
ffi.cdef([[ int puts(const char *s); ]])
ffi.C.puts("hello")
```

### `ffi.load(path[, global])`

Loads a shared library and returns a library object.

```lua
local lib = ffi.load("./libdemo.so")
local lib_global = ffi.load("./libdemo.so", true)
```

- `path`: shared library path.
- `global` (optional): if true, loads with global symbol visibility.

Library object behavior:

- `lib.symbol` looks up a declared function.
- If declaration is missing: error for missing declaration.
- If declaration exists but symbol is absent in library: undefined function error.

## Creating C Values: ffi.new

Signature:

```lua
ffi.new(ct [, init])
```

`ct` can be:

- A C type string.
- A ctype object from `ffi.typeof`.

### Examples

```lua
local i = ffi.new("int", 123)
local p = ffi.new("struct Point", {1, 2})
local a = ffi.new("int[4]", {1, 2})
local b = ffi.new("int [?]", 8, {1, 2, 3})
```

Initialization semantics:

- Zero-initialized by default.
- Exactly one initializer is accepted (when provided).
- Array and struct initializers accept Lua tables.

Invalid constructions:

- `void` value creation is invalid.
- Function value creation is invalid.

## Converting Values: ffi.cast

Signature:

```lua
ffi.cast(ct, value)
```

Typical uses:

```lua
local p = ffi.cast("int *", 0x1234)
local n = ffi.cast("int", p)
```

### Function pointer cast

To create a callback function pointer, cast a Lua function:

```lua
local cb = ffi.cast("int (*)(int)", function(x)
    return x * 2
end)
```

Important behavior:

- Function pointer cast expects a Lua function.
- Callback lifetime is tracked by the resulting cdata object.

## Type Utilities

## `ffi.typeof(ct)`

Returns canonical ctype object.

```lua
local t = ffi.typeof("struct Point")
```

## `ffi.istype(ct, obj)`

Returns true if `obj` has exactly the same canonical ctype.

```lua
local p = ffi.new("int", 1)
assert(ffi.istype("int", p))
```

## `ffi.sizeof(ct)`

Returns byte size of a type or cdata value.

```lua
local sz = ffi.sizeof("struct Point")
```

## `ffi.offsetof(ct, field)`

Returns field offset for record types.

```lua
local off = ffi.offsetof("struct Point", "y")
```

Returns `nil` when field is not found.

## `ffi.addressof(cdata)`

Returns a pointer cdata to the underlying storage.

```lua
local v = ffi.new("int", 7)
local pv = ffi.addressof(v)
pv[0] = 99
```

## Data Conversion Helpers

## `ffi.tonumber(cdata)`

Converts scalar numeric cdata to Lua number/integer.

```lua
local v = ffi.new("int", 42)
assert(ffi.tonumber(v) == 42)
```

Returns `nil` for non-numeric cdata (for example records).

## `ffi.string(cdata[, len])`

Converts char pointer/array (or raw memory with explicit length) to Lua string.

```lua
local s = ffi.new("char[16]", "hello")
assert(ffi.string(s) == "hello")
assert(ffi.string(s, 3) == "hel")
```

With explicit `len`, pointer/array/record memory can be read as bytes.

## `ffi.copy(dst, src[, len])`

Copies memory to destination cdata.

```lua
local dst = ffi.new("char[16]")
ffi.copy(dst, "abc")       -- writes NUL terminator too
ffi.copy(dst, "xyz", 2)    -- raw copy of 2 bytes
```

## `ffi.fill(dst, len[, c])`

`memset` style fill.

```lua
local buf = ffi.new("uint8_t[4]")
ffi.fill(buf, 4, 0x5a)
```

## Error Number Helper: ffi.errno

Signature:

```lua
old = ffi.errno([new_errno])
```

- Returns current `errno`.
- If argument is provided, sets `errno` and returns previous value.

```lua
local prev = ffi.errno()
ffi.errno(2)
ffi.errno(prev)
```

## Lifetime Management: ffi.gc

Attach or remove Lua finalizer for cdata.

```lua
local p = ffi.gc(ffi.C.malloc(128), ffi.C.free)
-- remove finalizer
ffi.gc(p, nil)
```

Returns the same cdata object.

## Metatypes: ffi.metatype

Associates metamethod table with a record type.

```lua
ffi.cdef([[
    struct Point { int x; int y; };
]])

local Point = ffi.metatype(ffi.typeof("struct Point"), {
    __tostring = function(self)
        return string.format("(%d,%d)", self.x, self.y)
    end,
    __index = {
        add = function(self, dx, dy)
            self.x = self.x + dx
            self.y = self.y + dy
        end,
    },
})

local p = ffi.new(Point, {1, 2})
p:add(10, 20)
```

Supported metatype hooks used by runtime:

- `__index`
- `__tostring`

## CData Behaviors and Metamethod Semantics

### Indexing and assignment

- Record cdata: `obj.field`
- Pointer/array cdata: `obj[index]`
- Pointer-to-record supports field access by name.

### Const protection

Assignment to const-qualified targets is rejected.

### Equality

- Pointer cdata compares pointer values.
- Numeric scalar cdata compares by converted Lua numeric value.
- `nil` comparison works for pointer-null checks.

### Calling function cdata

Declared C functions become callable cdata values.

```lua
ffi.cdef([[ int puts(const char *s); ]])
ffi.C.puts("hi")
```

Vararg declarations are supported.

### Length operator

`#cdata` is supported for arrays and returns element count.

## Exported Variables

## `ffi.VERSION`

Version string.

## `ffi.nullptr`

Canonical null pointer cdata.

Example null comparison:

```lua
assert(ffi.nullptr == ffi.cast("void *", nil))
```

## Example

```lua
local ffi = require("ffi")

ffi.cdef([[
    struct student {
        int age;
        char name[0];
    };

    void *malloc(size_t size);
    void free(void *ptr);

    int printf(const char *format, ...);
]])

local p = ffi.gc(ffi.C.malloc(ffi.sizeof("struct student") + 32), ffi.C.free)
local st = ffi.new("struct student *", p)

st.age = 18
ffi.copy(st.name, "alice")

ffi.C.printf("name=%s age=%d\n", st.name, st.age)
```

## Common Pitfalls

- Declare before use: function calls require matching `cdef` prototypes.
- Keep callbacks alive: store callback cdata as long as C may call it.
- Be explicit with ownership: use `ffi.gc` for allocated memory.
- Use the correct `ffi.string` form:
  - no length for NUL-terminated char buffers,
  - with length for raw bytes.
- Use canonical types when checking with `ffi.istype`.
