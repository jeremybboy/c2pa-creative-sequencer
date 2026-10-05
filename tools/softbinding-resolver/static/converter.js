const source = document.querySelector('#derivative-file');
const preset = document.querySelector('#derivative-preset');
const create = document.querySelector('#create-derivative');
const status = document.querySelector('#derivative-status');
const download = document.querySelector('#derivative-download');
let downloadUrl = null;

function outputName(file, presetId) {
  const stem = file.name.replace(/\.[^.]+$/, '') || 'audio';
  return `${stem}-${presetId}.mp3`;
}

async function loadPresets() {
  try {
    const response = await fetch('/derivatives/presets');
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || 'Could not inspect derivative runtime');
    preset.replaceChildren(...data.presets.map(item => {
      const option = document.createElement('option');
      option.value = item.id;
      option.textContent = `${item.label} — ${item.description}`;
      return option;
    }));
    preset.disabled = !data.available;
    create.disabled = !data.available || !source.files[0];
    status.textContent = data.message;
  } catch (error) {
    status.textContent = error.message;
  }
}

source.addEventListener('change', () => {
  create.disabled = preset.disabled || !source.files[0];
  download.hidden = true;
});

create.addEventListener('click', async () => {
  const file = source.files[0];
  if (!file) return;
  create.disabled = true;
  download.hidden = true;
  status.textContent = `Creating ${preset.options[preset.selectedIndex].textContent}…`;
  try {
    const response = await fetch('/derivatives', {
      method: 'POST',
      headers: {
        'Content-Type': file.type || 'application/octet-stream',
        'X-Filename': file.name,
        'X-Derivative-Preset': preset.value,
      },
      body: await file.arrayBuffer(),
    });
    if (!response.ok) {
      const data = await response.json();
      throw new Error(data.error || 'Derivative creation failed');
    }
    if (downloadUrl) URL.revokeObjectURL(downloadUrl);
    downloadUrl = URL.createObjectURL(await response.blob());
    download.href = downloadUrl;
    download.download = outputName(file, preset.value);
    download.hidden = false;
    download.click();
    status.textContent = 'Derivative created locally. Test this same MP3 on both recovery pages.';
  } catch (error) {
    status.textContent = error.message;
  } finally {
    create.disabled = preset.disabled || !source.files[0];
  }
});

window.addEventListener('pagehide', () => downloadUrl && URL.revokeObjectURL(downloadUrl));
loadPresets();
