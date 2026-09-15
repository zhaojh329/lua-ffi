#!/bin/bash
# Build the module and its C oracle with one target compiler, then run under QEMU.
set -euo pipefail

build_dir=$(mkdir -p "$1" && cd "$1" && pwd)
target=$2
compiler=$3
repo_dir=$(cd "$(dirname "$0")/.." && pwd)
toolchain=${TOOLCHAIN_PREFIX:-/usr}
jobs=${JOBS:-2}

case "$target" in
    i686-linux-gnu) emulator=qemu-i386 ;;
    arm-linux-gnueabihf) emulator=qemu-arm ;;
    aarch64-linux-gnu) emulator=qemu-aarch64 ;;
    mips-linux-gnu) emulator=qemu-mips ;;
    mipsel-linux-gnu) emulator=qemu-mipsel ;;
    riscv64-linux-gnu) emulator=qemu-riscv64 ;;
    *) printf 'Unsupported test target: %s\n' "$target" >&2; exit 1 ;;
esac

# Keep compiler flags in a wrapper so configure, Lua make and CMake agree.
case "$compiler" in
    gcc) compiler_cmd=("$toolchain/bin/$target-gcc") ;;
    clang) compiler_cmd=(clang "--target=$target" "--gcc-toolchain=$toolchain") ;;
    *) printf 'Expected gcc or clang\n' >&2; exit 1 ;;
esac
if [[ -n ${CROSS_SYSROOT:-} ]]; then
    compiler_cmd+=("--sysroot=$CROSS_SYSROOT")
fi
{
    printf '#!/bin/bash\nexec '
    printf '%q ' "${compiler_cmd[@]}"
    printf '"$@"\n'
} > "$build_dir/cc"
chmod +x "$build_dir/cc"
export CC="$build_dir/cc"
export AR="$toolchain/bin/$target-ar"
export RANLIB="$toolchain/bin/$target-ranlib"

archive_dir=${DISTFILES:-$build_dir/downloads}
mkdir -p "$archive_dir"
if [[ ! -f "$archive_dir/lua-5.4.8.tar.gz" ]]; then
    curl -fL --retry 3 https://www.lua.org/ftp/lua-5.4.8.tar.gz \
        -o "$archive_dir/lua-5.4.8.tar.gz"
fi
if [[ ! -f "$archive_dir/libffi-3.4.7.tar.gz" ]]; then
    curl -fL --retry 3 \
        https://github.com/libffi/libffi/releases/download/v3.4.7/libffi-3.4.7.tar.gz \
        -o "$archive_dir/libffi-3.4.7.tar.gz"
fi
(cd "$archive_dir" && sha256sum -c <<'EOF'
4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae  lua-5.4.8.tar.gz
138607dee268bdecf374adf9144c00e839e38541f75f24a1fcf18b78fda48b2d  libffi-3.4.7.tar.gz
EOF
)

tar -xzf "$archive_dir/lua-5.4.8.tar.gz" -C "$build_dir"
tar -xzf "$archive_dir/libffi-3.4.7.tar.gz" -C "$build_dir"
make -C "$build_dir/lua-5.4.8" -j"$jobs" linux CC="$CC" AR="$AR rcu" RANLIB="$RANLIB"
(
    cd "$build_dir/libffi-3.4.7"
    ./configure --host="$target" --build="$(./config.guess)" \
        --prefix="$build_dir/deps" --disable-docs
    make -j"$jobs"
    make install
)

export PKG_CONFIG_LIBDIR="$build_dir/deps/lib/pkgconfig"
cmake -S "$repo_dir" -B "$build_dir/module" \
    -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR="$target" \
    -DCMAKE_C_COMPILER="$CC" -DLUA_INCLUDE_DIR="$build_dir/lua-5.4.8/src" \
    -DCMAKE_MODULE_LINKER_FLAGS="-L$build_dir/deps/lib" \
    -DCMAKE_BUILD_RPATH="$build_dir/deps/lib"
cmake --build "$build_dir/module" -j"$jobs"
python3 "$repo_dir/tests/gen_bitfields.py" "$build_dir/oracle.c"
"$CC" -shared -fPIC "$build_dir/oracle.c" -o "$build_dir/oracle.so"

export LUA_CPATH="$build_dir/module/?.so;;"
"$emulator" -L "$toolchain/$target" \
    -E "LD_LIBRARY_PATH=$toolchain/$target/lib:$build_dir/deps/lib" \
    "$build_dir/lua-5.4.8/src/lua" \
    "$repo_dir/tests/bitfield.lua" "$build_dir/oracle.so"
