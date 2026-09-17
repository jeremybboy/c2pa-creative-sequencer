#!/bin/sh
set -eu

AUDIOWMARK_COMMIT=c204998c92931285efdf6670c81cefd199298895
ZITA_COMMIT=cfea03f129f067c13f2453db89e10f19309cf45d
RUNTIME_ROOT=${C2PASEQ_AUDIOWMARK_RUNTIME:-"${HOME}/Library/Application Support/C2PA Creative Sequencer/AudioWMark"}
SOURCE_ROOT="${RUNTIME_ROOT}/source"
ZITA_SOURCE="${SOURCE_ROOT}/zita-resampler"
AUDIOWMARK_SOURCE="${SOURCE_ROOT}/audiowmark"
DEPENDENCY_PREFIX="${RUNTIME_ROOT}/deps"
CONFIGURE_ROOT=$(mktemp -d /private/tmp/c2paseq-audiowmark.XXXXXX)
trap 'rm -rf "${CONFIGURE_ROOT}"' EXIT
ln -s "${RUNTIME_ROOT}" "${CONFIGURE_ROOT}/runtime"
CONFIGURE_RUNTIME="${CONFIGURE_ROOT}/runtime"

for command in git cmake make autoreconf automake autoconf glibtoolize pkg-config; do
    command -v "${command}" >/dev/null 2>&1 || {
        echo "Missing build dependency: ${command}" >&2
        exit 1
    }
done

mkdir -p "${SOURCE_ROOT}" "${DEPENDENCY_PREFIX}"
if [ ! -d "${ZITA_SOURCE}/.git" ]; then
    git clone https://github.com/digital-stage/zita-resampler.git "${ZITA_SOURCE}"
fi
git -C "${ZITA_SOURCE}" fetch --tags --force origin
git -C "${ZITA_SOURCE}" checkout --detach "${ZITA_COMMIT}"

cmake -S "${ZITA_SOURCE}" -B "${RUNTIME_ROOT}/build-zita" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${DEPENDENCY_PREFIX}" \
    -DBUILD_SHARED_LIBS=OFF
cmake --build "${RUNTIME_ROOT}/build-zita" --parallel 4
cmake --install "${RUNTIME_ROOT}/build-zita"
mkdir -p "${DEPENDENCY_PREFIX}/include/zita-resampler"
cp "${ZITA_SOURCE}"/source/zita-resampler/*.h \
   "${DEPENDENCY_PREFIX}/include/zita-resampler/"

if [ ! -d "${AUDIOWMARK_SOURCE}/.git" ]; then
    git clone https://github.com/swesterfeld/audiowmark.git "${AUDIOWMARK_SOURCE}"
fi
git -C "${AUDIOWMARK_SOURCE}" fetch --tags --force origin
git -C "${AUDIOWMARK_SOURCE}" checkout --detach "${AUDIOWMARK_COMMIT}"

cd "${AUDIOWMARK_SOURCE}"
./autogen.sh --prefix="${CONFIGURE_RUNTIME}" \
    --with-libzita-resampler-prefix="${CONFIGURE_RUNTIME}/deps" \
    --without-docs
make -j4
make install

TEST_DIRECTORY="${RUNTIME_ROOT}/self-test"
mkdir -p "${TEST_DIRECTORY}"
python3 - "${TEST_DIRECTORY}/input.wav" <<'PY'
import math
import struct
import sys
import wave

with wave.open(sys.argv[1], "wb") as output:
    output.setnchannels(2)
    output.setsampwidth(2)
    output.setframerate(48000)
    for index in range(48000 * 15):
        left = int(10000 * math.sin(2 * math.pi * 220 * index / 48000))
        right = int(10000 * math.sin(2 * math.pi * 277 * index / 48000))
        output.writeframesraw(struct.pack("<hh", left, right))
PY

PAYLOAD=0123456789abcdef0011223344556677
"${RUNTIME_ROOT}/bin/audiowmark" add --strict \
    "${TEST_DIRECTORY}/input.wav" "${TEST_DIRECTORY}/marked.wav" "${PAYLOAD}"
DECODED=$("${RUNTIME_ROOT}/bin/audiowmark" get --strict \
    "${TEST_DIRECTORY}/marked.wav")
echo "${DECODED}" | grep "${PAYLOAD}" >/dev/null || {
    echo "AudioWMark self-test did not recover the exact 128-bit payload" >&2
    exit 1
}

echo "AudioWMark 0.6.5 runtime ready at ${RUNTIME_ROOT}"
