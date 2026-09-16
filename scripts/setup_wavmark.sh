#!/bin/zsh
set -euo pipefail

WAVMARK_COMMIT="6ab3bf7ce0679e5b5cfeff3a62e8df9cd2024b37"
MODEL_SHA256="7a3d873d115c2c644685fb4cf109bdb299718296b88c871d3cf1c8e4cbc13144"
RUNTIME_ROOT="${C2PASEQ_WAVMARK_RUNTIME:-${HOME}/Library/Application Support/C2PA Creative Sequencer/WavMark}"
SCRIPT_DIR="${0:A:h}"

mkdir -p "${RUNTIME_ROOT}"
if [[ ! -d "${RUNTIME_ROOT}/source/.git" ]]; then
    git clone https://github.com/wavmark/wavmark.git "${RUNTIME_ROOT}/source"
fi
git -C "${RUNTIME_ROOT}/source" fetch --depth 1 origin "${WAVMARK_COMMIT}"
git -C "${RUNTIME_ROOT}/source" checkout --detach "${WAVMARK_COMMIT}"

python3 -m venv "${RUNTIME_ROOT}/venv"
"${RUNTIME_ROOT}/venv/bin/python3" -m pip install --upgrade pip
"${RUNTIME_ROOT}/venv/bin/python3" -m pip install -r "${SCRIPT_DIR}/wavmark-requirements.lock"
"${RUNTIME_ROOT}/venv/bin/python3" -m pip install --no-deps "${RUNTIME_ROOT}/source"
cp "${SCRIPT_DIR}/wavmark_helper.py" "${RUNTIME_ROOT}/wavmark_helper.py"

MODEL_PATH=$(HF_HOME="${RUNTIME_ROOT}/huggingface" "${RUNTIME_ROOT}/venv/bin/python3" -c \
  'from huggingface_hub import hf_hub_download; print(hf_hub_download(repo_id="M4869/WavMark", filename="step59000_snr39.99_pesq4.35_BERP_none0.30_mean1.81_std1.81.model.pkl"))')
cp "${MODEL_PATH}" "${RUNTIME_ROOT}/model.model.pkl"
ACTUAL_MODEL_SHA256=$(shasum -a 256 "${RUNTIME_ROOT}/model.model.pkl" | awk '{print $1}')
if [[ "${ACTUAL_MODEL_SHA256}" != "${MODEL_SHA256}" ]]; then
    echo "Unexpected WavMark model SHA-256: ${ACTUAL_MODEL_SHA256}" >&2
    exit 1
fi

"${RUNTIME_ROOT}/venv/bin/python3" - "${RUNTIME_ROOT}" <<'PY'
import pathlib, subprocess, sys
import numpy as np
import soundfile as sf

root = pathlib.Path(sys.argv[1])
rate = 48000
t = np.arange(rate * 2) / rate
stereo = np.column_stack((0.12 * np.sin(2*np.pi*220*t), 0.10 * np.sin(2*np.pi*330*t)))
source = root / "setup-source.wav"
output = root / "setup-watermarked.wav"
sf.write(source, stereo, rate, subtype="PCM_24")
command = [str(root / "venv/bin/python3"), str(root / "wavmark_helper.py"), "embed",
           "--model", str(root / "model.model.pkl"), "--input", str(source),
           "--output", str(output), "--payload", "A55A"]
subprocess.run(command, check=True)
source.unlink()
output.unlink()
PY

echo "WavMark runtime ready at ${RUNTIME_ROOT}"
