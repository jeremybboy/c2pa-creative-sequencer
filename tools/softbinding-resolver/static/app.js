const fileInput = document.querySelector('#file');
const drop = document.querySelector('#drop');
const steps = document.querySelector('#steps');
const status = document.querySelector('#status');

function text(id, value) { document.querySelector(id).textContent = value; }

async function inspect(file) {
  steps.hidden = false;
  text('#embedded', 'Checking…');
  text('#watermark', 'Searching…');
  text('#binding', 'Waiting…');
  document.querySelector('#result').textContent = 'Waiting…';
  status.textContent = `Inspecting ${file.name}…`;
  try {
    const response = await fetch('/matches/byContent', {
      method: 'POST', headers: { 'Content-Type': file.type || 'audio/wav', 'X-Filename': file.name },
      body: await file.arrayBuffer()
    });
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || 'Inspection failed');
    text('#embedded', data.embeddedContentCredentials ? 'Present' : 'Not present');
    text('#watermark', `Search complete; ${data.decodedCandidateCount} candidate(s) decoded`);
    if (!data.matches.length) {
      text('#binding', 'No exact repository match');
      document.querySelector('#result').textContent = data.message;
      status.textContent = 'No recovery claim was made.';
      return;
    }
    const match = data.matches[0];
    text('#binding', `Audio watermark detected: ${match.value}`);
    document.querySelector('#result').innerHTML = `
      <strong>Content Credentials recovered via audio watermark</strong>
      <dl>
        <dt>Title</dt><dd>${match.title || '—'}</dd>
        <dt>Claim generator</dt><dd>${match.claimGenerator || '—'}</dd>
        <dt>Manifest ID</dt><dd>${match.manifestId}</dd>
        <dt>Algorithm</dt><dd>${match.algorithm}</dd>
        <dt>Binding value</dt><dd>${match.value}</dd>
      </dl>
      <p class="warning">Recovered through a soft binding. The derivative has not passed the original asset’s cryptographic hard binding.</p>
      <a href="/manifests/${encodeURIComponent(match.manifestId)}">Download recovered .c2pa</a>`;
    status.textContent = 'Recovery completed.';
  } catch (error) {
    status.textContent = error.message;
    document.querySelector('#result').textContent = 'Recovery failed.';
  }
}

fileInput.addEventListener('change', () => fileInput.files[0] && inspect(fileInput.files[0]));
for (const event of ['dragenter', 'dragover']) drop.addEventListener(event, e => { e.preventDefault(); drop.classList.add('active'); });
for (const event of ['dragleave', 'drop']) drop.addEventListener(event, e => { e.preventDefault(); drop.classList.remove('active'); });
drop.addEventListener('drop', e => e.dataTransfer.files[0] && inspect(e.dataTransfer.files[0]));
document.querySelector('#import').addEventListener('click', async () => {
  status.textContent = 'Importing Sequencer publications…';
  const response = await fetch('/imports/sequencer', { method: 'POST', body: new Uint8Array([1]) });
  const data = await response.json();
  status.textContent = `Imported ${data.imported}; already imported ${data.alreadyImported}; errors ${data.errors.length}.`;
});
