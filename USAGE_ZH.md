# lua-ffi 使用指南

## 加载 FFI 库

```lua
local ffi = require("ffi")
```

模块会导出函数以及以下三个值：

- `ffi.VERSION`
- `ffi.nullptr`
- `ffi.C`

## ffi.cdef：声明 C 类型与函数

使用 `ffi.cdef` 提供 C 声明。

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

### `cdef` 支持内容

- 本项目支持的基础 C 标量类型。
- `typedef` 声明。
- `struct` 与 `union` 声明（包含嵌套/匿名成员）。
- `__attribute__((packed))` 解析。
- 函数声明与函数指针类型。
- 数组声明符，包括在 `ffi.new` 类型字符串中使用 `?` 的柔性形式。

### 注意事项

- 声明是可叠加的。
- 已知符号重复定义会报错。
- `cdef` 仅用于声明（不支持函数定义）。

### 默认支持的基础类型

以下内置基础 C 类型默认可用：

void bool char short int long float double

int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t

ino_t dev_t gid_t mode_t nlink_t uid_t off_t pid_t size_t ssize_t
useconds_t suseconds_t blksize_t blkcnt_t time_t

## 访问 C 库：ffi.C 与 ffi.load

### `ffi.C`

`ffi.C` 是 ffi 模块默认加载系统 C 库后返回的对象。你可以通过该对象访问已经声明的系统 C 函数。

```lua
ffi.cdef([[ int puts(const char *s); ]])
ffi.C.puts("hello")
```

### `ffi.load(path[, global])`

加载动态库并返回库对象。

```lua
local lib = ffi.load("./libdemo.so")
local lib_global = ffi.load("./libdemo.so", true)
```

- `path`：动态库路径。
- `global`（可选）：为 true 时使用全局符号可见性加载。

库对象行为：

- `lib.symbol` 会查找已声明函数。
- 若缺少声明：会报“缺少声明”错误。
- 若有声明但库中没有符号：会报“未定义函数”错误。

## 创建 C 值：ffi.new

签名：

```lua
ffi.new(ct [, init])
```

`ct` 可以是：

- C 类型字符串。
- 由 `ffi.typeof` 返回的 ctype 对象。

### 示例

```lua
local i = ffi.new("int", 123)
local p = ffi.new("struct Point", {1, 2})
local a = ffi.new("int[4]", {1, 2})
local b = ffi.new("int [?]", 8, {1, 2, 3})
```

初始化语义：

- 默认零初始化。
- 传入初始化值时，只接受一个初始化参数。
- 数组与结构体初始化支持 Lua table。

无效构造：

- 不能创建 `void` 值。
- 不能创建函数值。

## 值转换：ffi.cast

签名：

```lua
ffi.cast(ct, value)
```

常见用法：

```lua
local p = ffi.cast("int *", 0x1234)
local n = ffi.cast("int", p)
```

### 函数指针转换

要创建回调函数指针，可将 Lua 函数 cast 到函数指针类型：

```lua
local cb = ffi.cast("int (*)(int)", function(x)
    return x * 2
end)
```

重要行为：

- 函数指针 cast 期望传入 Lua function。
- 回调生命周期由返回的 cdata 对象跟踪。

## 类型工具

## `ffi.typeof(ct)`

返回规范化的 ctype 对象。

```lua
local t = ffi.typeof("struct Point")
```

## `ffi.istype(ct, obj)`

当 `obj` 与 `ct` 是完全相同的规范化 ctype 时返回 true。

```lua
local p = ffi.new("int", 1)
assert(ffi.istype("int", p))
```

## `ffi.sizeof(ct)`

返回类型或 cdata 值的字节大小。

```lua
local sz = ffi.sizeof("struct Point")
```

## `ffi.offsetof(ct, field)`

返回记录类型字段偏移。

```lua
local off = ffi.offsetof("struct Point", "y")
```

字段不存在时返回 `nil`。

## `ffi.addressof(cdata)`

返回指向底层存储的指针 cdata。

```lua
local v = ffi.new("int", 7)
local pv = ffi.addressof(v)
pv[0] = 99
```

## 数据转换辅助函数

## `ffi.tonumber(cdata)`

将数值标量 cdata 转成 Lua number/integer。

```lua
local v = ffi.new("int", 42)
assert(ffi.tonumber(v) == 42)
```

对非数值 cdata（例如 record）返回 `nil`。

## `ffi.string(cdata[, len])`

将字符指针/数组（或指定长度的原始内存）转换为 Lua 字符串。

```lua
local s = ffi.new("char[16]", "hello")
assert(ffi.string(s) == "hello")
assert(ffi.string(s, 3) == "hel")
```

传入显式 `len` 时，可将 pointer/array/record 内存按字节读取。

## `ffi.copy(dst, src[, len])`

向目标 cdata 拷贝内存。

```lua
local dst = ffi.new("char[16]")
ffi.copy(dst, "abc")       -- 同时写入 NUL 终止符
ffi.copy(dst, "xyz", 2)    -- 原样拷贝 2 个字节
```

## `ffi.fill(dst, len[, c])`

`memset` 风格填充。

```lua
local buf = ffi.new("uint8_t[4]")
ffi.fill(buf, 4, 0x5a)
```

## errno 辅助函数：ffi.errno

签名：

```lua
old = ffi.errno([new_errno])
```

- 返回当前 `errno`。
- 传入参数时会设置 `errno`，并返回旧值。

```lua
local prev = ffi.errno()
ffi.errno(2)
ffi.errno(prev)
```

## 生命周期管理：ffi.gc

为 cdata 绑定或移除 Lua 析构回调。

```lua
local p = ffi.gc(ffi.C.malloc(128), ffi.C.free)
-- 移除析构回调
ffi.gc(p, nil)
```

返回值仍是同一个 cdata 对象。

## 元类型：ffi.metatype

为记录类型关联元方法表。

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

运行时使用的元类型钩子：

- `__index`
- `__tostring`

## CData 行为与元方法语义

### 索引与赋值

- 记录 cdata：`obj.field`
- 指针/数组 cdata：`obj[index]`
- 指向记录的指针支持按字段名访问。

### const 保护

对 const 限定目标的赋值会被拒绝。

### 相等性

- 指针 cdata 比较指针值。
- 数值标量 cdata 按转换后的 Lua 数值比较。
- 与 `nil` 的比较可用于空指针判断。

### 调用函数 cdata

已声明的 C 函数会变成可调用 cdata。

```lua
ffi.cdef([[ int puts(const char *s); ]])
ffi.C.puts("hi")
```

支持可变参数声明。

### 长度运算符

数组支持 `#cdata`，返回元素个数。

## 导出变量

## `ffi.VERSION`

版本字符串。

## `ffi.nullptr`

规范的空指针 cdata。

空指针比较示例：

```lua
assert(ffi.nullptr == ffi.cast("void *", nil))
```

## 示例

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

## 常见陷阱

- 先声明再使用：函数调用需要匹配的 `cdef` 原型。
- 保持回调存活：只要 C 侧仍可能调用，就要持有回调 cdata。
- 明确所有权：为堆内存使用 `ffi.gc`。
- 正确使用 `ffi.string`：
  - NUL 结尾字符缓冲区用无长度形式；
  - 原始字节读取用带长度形式。
- 使用 `ffi.istype` 时应基于规范化类型进行比较。
