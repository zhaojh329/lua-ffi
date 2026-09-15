# Bitfield interoperability tests

The generator emits C declarations and native compiler accessors, not calculated
expected layouts. The Lua test compares sizes, alignments, field byte offsets,
C-to-Lua reads and Lua-to-C writes, including packed, nested and array objects.
Patterns exercise all 64 bits and a packed field spanning nine bytes. Exact-size
allocations and a guard page check access bounds. Defined field values are
compared; unspecified padding bits are not compared.

```sh
cmake -S . -B /tmp/lua-ffi-tests -DUSE_LUA54=ON -DCMAKE_C_COMPILER=gcc
cmake --build /tmp/lua-ffi-tests
python3 tests/gen_bitfields.py /tmp/lua-ffi-tests/oracle.c
gcc -shared -fPIC /tmp/lua-ffi-tests/oracle.c -o /tmp/lua-ffi-tests/oracle.so
LUA_CPATH='/tmp/lua-ffi-tests/?.so;;' lua5.4 tests/bitfield.lua /tmp/lua-ffi-tests/oracle.so
```

Use a separate build directory with `-DCMAKE_C_COMPILER=clang` to test Clang.
Compiling the oracle with the other compiler also checks interoperability between
GCC and Clang. The dedicated `bitfields.yml` workflow covers Lua 5.1–5.4 with both compilers.
ASan/UBSan jobs instrument the module and oracle; leak detection is excluded from
these access-bounds checks.

## Cross architecture execution

In Debian bookworm, install `build-essential clang cmake flex pkg-config python3
curl ca-certificates qemu-user` and the relevant `gcc-<target>` package. Then run:

```sh
bash tests/run-bitfields-cross.sh /tmp/lua-ffi-mips mips-linux-gnu gcc
bash tests/run-bitfields-cross.sh /tmp/lua-ffi-mips-clang mips-linux-gnu clang
```

The script builds Lua 5.4.8, libffi 3.4.7, the module and the oracle for the target,
then executes the Lua test through QEMU. Dependency downloads are checksum-pinned.
It accepts `i686-linux-gnu`, `arm-linux-gnueabihf`, `aarch64-linux-gnu`,
`mips-linux-gnu`, `mipsel-linux-gnu`, and `riscv64-linux-gnu`. Both compiler choices
are exercised for every target in CI. Each target/compiler pair needs its own
build directory.

For an extracted toolchain, `TOOLCHAIN_PREFIX` selects the directory containing
`bin/<target>-gcc` and `<target>/lib`; optional `CROSS_SYSROOT` is passed to the
compiler. `DISTFILES` selects a shared download cache and `JOBS` controls build
parallelism. The compiler and QEMU executables must be runnable on the host.

The layout algorithm uses the named-bitfield rules shared by these target ABIs:
type-aligned containers for ordinary fields and contiguous allocation for packed
fields. See the [Arm ABI specifications](https://github.com/ARM-software/abi-aa)
and [Clang's record layout implementation](https://github.com/llvm/llvm-project/blob/main/clang/lib/AST/RecordLayoutBuilder.cpp).
