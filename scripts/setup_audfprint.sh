#!/bin/sh
set -eu

REPOSITORY="https://github.com/dpwe/audfprint.git"
COMMIT="cb03ba99feafd41b8874307f0f4e808a6ce34362"
RUNTIME_ROOT=${C2PASEQ_AUDFPRINT_RUNTIME:-"${HOME}/Library/Application Support/C2PA Creative Sequencer/Fingerprint"}
SCRIPT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_ROOT="${RUNTIME_ROOT}/source/audfprint"

command -v git >/dev/null 2>&1 || { echo "git is required" >&2; exit 1; }
command -v ffmpeg >/dev/null 2>&1 || { echo "ffmpeg is required (brew install ffmpeg)" >&2; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "python3 is required" >&2; exit 1; }

mkdir -p "${RUNTIME_ROOT}/source" "${RUNTIME_ROOT}/bin" "${RUNTIME_ROOT}/lib"
if [ ! -d "${SOURCE_ROOT}/.git" ]; then
    git clone "${REPOSITORY}" "${SOURCE_ROOT}"
fi
git -C "${SOURCE_ROOT}" fetch --quiet origin "${COMMIT}"
git -C "${SOURCE_ROOT}" checkout --quiet --detach "${COMMIT}"

python3 -m venv "${RUNTIME_ROOT}/venv"
"${RUNTIME_ROOT}/venv/bin/python" -m pip install --upgrade pip
"${RUNTIME_ROOT}/venv/bin/python" -m pip install \
    "numpy==2.3.3" "scipy==1.16.2" "docopt==0.6.2" "joblib==1.5.2" "psutil==7.1.0"
install -m 755 "${SCRIPT_ROOT}/audfprint_helper.py" "${RUNTIME_ROOT}/lib/audfprint_helper.py"
install -m 755 "${SCRIPT_ROOT}/c2paseq-audfprint" "${RUNTIME_ROOT}/bin/c2paseq-audfprint"

echo "audfprint runtime ${COMMIT} ready at ${RUNTIME_ROOT}"
