#!/usr/bin/env bash
#
# mayhem/build.sh — build rive-runtime's fuzz harnesses and the functional oracle.
#
# Upstream builds with premake5 (gmake). premake would git-clone its dependencies at generate time;
# the Dockerfile cached them under $RIVE_DEPENDENCIES and premake reuses them (DEPENDENCIES=...),
# so this script runs fully offline and is re-runnable on an already-built tree.
#
#   1) the runtime (librive + harfbuzz, SheenBidi, yoga, miniaudio) and the decoders (libpng, zlib,
#      libjpeg, libwebp, KTX2) with $SANITIZER_FLAGS $DEBUG_FLAGS  -> mayhem/build/fuzz{,-dec}
#   2) each harness in mayhem/harnesses/ twice: libFuzzer binary + -standalone reproducer -> /mayhem
#   3) the same libraries with upstream's normal flags + mayhem/oracle.cpp -> mayhem/build/oracle
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${RIVE_PREMAKE:=/opt/toolchains/deps/premake-5.0.0-beta7/bin/premake5}"
: "${RIVE_DEPENDENCIES:=/opt/toolchains/deps/rive-deps}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
export DEPENDENCIES="$RIVE_DEPENDENCIES" PREMAKE_PATH="$SRC/build"
B="$SRC/mayhem/build"
mkdir -p "$B"

RIVE_OPTS=(--with_rive_text --with_rive_layout --with_rive_audio=system --no-lto --config=release)

# premake_libs <outdir-suffix> <cflags>: generate + build runtime and decoders into $B/<suffix>{,-dec}
premake_libs() {
  local out="$B/$1" flags="$2" decflags="${3:-}"
  # premake resolves --out relative to its working directory.
  (cd "$SRC" && "$RIVE_PREMAKE" --file=premake5_v2.lua gmake "${RIVE_OPTS[@]}" --out="mayhem/build/$1")
  (cd "$SRC/decoders" && "$RIVE_PREMAKE" --file=premake5_v2.lua gmake --no-lto --config=release --out="../mayhem/build/$1-dec")
  make -C "$out" -j"$MAYHEM_JOBS" CC="$CC" CXX="$CXX" AR="${AR:-llvm-ar}" \
       CFLAGS="$flags" CXXFLAGS="$flags"
  make -C "$out-dec" -j"$MAYHEM_JOBS" CC="$CC" CXX="$CXX" AR="${AR:-llvm-ar}" \
       CFLAGS="$flags $decflags" CXXFLAGS="$flags $decflags"
}

# The defines librive was compiled with (class layouts depend on them).
rive_defines() { sed -n 's/^ *DEFINES += //p' "$1/rive.make" | head -1; }

link_libs() {
  local out="$1"
  echo "$out/librive.a $out/librive_harfbuzz.a $out/librive_sheenbidi.a $out/librive_yoga.a" \
       "$out/libminiaudio.a $out-dec/librive_decoders.a $out-dec/liblibpng.a $out-dec/liblibjpeg.a" \
       "$out-dec/liblibwebp.a $out-dec/libzlib.a -lpthread -ldl -lm"
}

MINIAUDIO_INC="$(ls -d "$RIVE_DEPENDENCIES"/rive-app_miniaudio_* | head -1)"
INCS=(-I"$SRC/include" -I"$SRC/decoders/include" -I"$SRC" -I"$MINIAUDIO_INC")

# ---- 1) sanitized libraries -----------------------------------------------------------------
# -fsanitize=fuzzer-no-link: SanCov edges in the library, not only in the harness.
# UBSan relaxations (ASan and every other UBSan check stay on and halting):
#  - pointer-overflow: SimpleArray::resize (`m_ptr + size` on a null m_ptr), miniaudio's
#    ma_offset_ptr(NULL, 0) and similar null+offset idioms fire on every valid input;
#  - shift-base, decoders only: libjpeg's Huffman bit buffer (jdhuff.c) left-shifts a signed long
#    by design on every JPEG.
# Only effective when sanitizers are on; an empty SANITIZER_FLAGS keeps the build sanitizer-free.
UB_RELAX=""; DEC_RELAX=""
case "$SANITIZER_FLAGS" in *undefined*) UB_RELAX="-fno-sanitize=pointer-overflow"; DEC_RELAX="-fno-sanitize=shift-base";; esac
case "$SANITIZER_FLAGS" in *-fsanitize=*) COV="-fsanitize=fuzzer-no-link";; *) COV="";; esac
premake_libs fuzz "$SANITIZER_FLAGS $UB_RELAX $DEBUG_FLAGS $COV" "$DEC_RELAX"
FDEFS="$(rive_defines "$B/fuzz")"

# ---- 2) harnesses ---------------------------------------------------------------------------
HFLAGS=(-std=c++17 -fno-rtti -fno-exceptions $FDEFS "${INCS[@]}" -I"$B")
$CC  $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$B/lsan_off.o"
$CXX $SANITIZER_FLAGS $UB_RELAX $DEBUG_FLAGS $COV "${HFLAGS[@]}" -c "$SRC/utils/no_op_factory.cpp" -o "$B/no_op_factory.o"
FLIBS="$(link_libs "$B/fuzz")"
# fuzz_text_shape's fixed font, embedded as a C array (regenerated each run from the committed asset).
{ echo "static const unsigned char kEmbeddedFont[] = {"
  od -An -v -tu1 "$SRC/tests/unit_tests/assets/IBMPlexSansArabic-Regular.ttf" | sed 's/ \+/,/g; s/^,//; s/$/,/'
  echo "};"; } > "$B/embedded_font.h.tmp"
cmp -s "$B/embedded_font.h.tmp" "$B/embedded_font.h" 2>/dev/null || mv "$B/embedded_font.h.tmp" "$B/embedded_font.h"
rm -f "$B/embedded_font.h.tmp"
for h in "$SRC"/mayhem/harnesses/fuzz_*.cpp; do
  n="$(basename "$h" .cpp)"
  $CXX $SANITIZER_FLAGS $UB_RELAX $DEBUG_FLAGS $COV "${HFLAGS[@]}" -c "$h" -o "$B/$n.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$B/$n.o" "$B/lsan_off.o" \
       "$B/no_op_factory.o" $FLIBS -o "$SRC/$n"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$B/$n.o" "$B/standalone_main.o" "$B/lsan_off.o" \
       "$B/no_op_factory.o" $FLIBS -o "$SRC/$n-standalone"
done

# ---- 3) functional oracle, upstream's normal (release) flags --------------------------------
premake_libs oracle "$COVERAGE_FLAGS"
ODEFS="$(rive_defines "$B/oracle")"
$CXX -std=c++17 -fno-rtti -fno-exceptions -O2 $COVERAGE_FLAGS $ODEFS "${INCS[@]}" \
     "$SRC/mayhem/oracle.cpp" "$SRC/utils/no_op_factory.cpp" $(link_libs "$B/oracle") \
     -o "$B/rive_oracle"
echo "build.sh: done"
