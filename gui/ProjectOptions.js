export class ProjectOptions {
  constructor(options = {}) {
    this.payload = options.payload || {};
    this.fetchJson = options.fetchJson || defaultFetchJson;
    this.onStatus = options.onStatus || (() => {});
    this.onDone = options.onDone || (() => {});
    this.noteText = options.note || '';

    const settings = this.payload.settings || {};

    this.root = document.createElement('form');
    this.root.className = 'project-options';

    this.message = document.createElement('div');
    this.message.className = 'project-options-message';
    this.message.textContent = 'Project settings';

    this.pathValue = document.createElement('div');
    this.pathValue.className = 'project-options-readonly';
    this.pathValue.textContent = settings.path || '';

    this.topModuleInput = document.createElement('input');
    this.topModuleInput.type = 'text';
    this.topModuleInput.name = 'topModuleName';
    this.topModuleInput.autocomplete = 'off';
    this.topModuleInput.spellcheck = false;
    this.topModuleInput.value = settings.topModuleName || '';

    this.topModuleFileInput = document.createElement('input');
    this.topModuleFileInput.type = 'text';
    this.topModuleFileInput.name = 'topModuleFile';
    this.topModuleFileInput.autocomplete = 'off';
    this.topModuleFileInput.spellcheck = false;
    this.topModuleFileInput.value = settings.topModuleFile || '';

    this.mainTestFileInput = document.createElement('input');
    this.mainTestFileInput.type = 'text';
    this.mainTestFileInput.name = 'mainTestFile';
    this.mainTestFileInput.autocomplete = 'off';
    this.mainTestFileInput.spellcheck = false;
    this.mainTestFileInput.value = settings.mainTestFile || '';

    const fields = document.createElement('div');
    fields.className = 'project-options-fields';
    fields.append(
      this.fieldRow('Project path', this.pathValue),
      this.fieldRow('Top module name', this.topModuleInput),
      this.fieldRow('Top module file', this.topModuleFileInput),
      this.fieldRow('Main test file', this.mainTestFileInput)
    );

    const actions = document.createElement('div');
    actions.className = 'project-options-actions';

    this.saveButton = document.createElement('button');
    this.saveButton.type = 'submit';
    this.saveButton.textContent = 'Save';

    this.ignoreButton = document.createElement('button');
    this.ignoreButton.type = 'button';
    this.ignoreButton.textContent = 'Ignore';
    this.ignoreButton.addEventListener('click', () => this.onDone());

    this.note = document.createElement('div');
    this.note.className = 'project-options-note';
    this.note.textContent = this.noteText;
    this.note.hidden = !this.noteText;

    actions.append(this.saveButton, this.ignoreButton);
    this.root.append(this.message, fields, this.note, actions);
    this.root.addEventListener('submit', (event) => this.save(event));
  }

  element() {
    return this.root;
  }

  fieldRow(label, control) {
    const row = document.createElement('label');
    row.className = 'project-options-row';
    const name = document.createElement('span');
    name.textContent = label;
    row.append(name, control);
    return row;
  }

  async save(event) {
    event.preventDefault();
    this.saveButton.disabled = true;
    this.message.textContent = 'Saving project settings...';
    this.onStatus('Calling /rpc/save-project-settings');
    try {
      const payload = await this.fetchJson('/rpc/save-project-settings', {
        topModuleName: this.topModuleInput.value.trim(),
        topModuleFile: this.topModuleFileInput.value.trim(),
        mainTestFile: this.mainTestFileInput.value.trim()
      });
      this.message.textContent = 'Saved';
      this.onStatus(`Project settings saved: ${payload.settings.path}`);
      this.onDone(payload.settings);
    } catch (error) {
      this.message.textContent = error instanceof Error ? error.message : String(error);
      this.onStatus('Project settings save failed');
      this.saveButton.disabled = false;
    }
  }
}

async function defaultFetchJson(endpoint, body = undefined) {
  const options = { method: 'POST' };
  if (body !== undefined) {
    options.headers = { 'Content-Type': 'application/json' };
    options.body = JSON.stringify(body);
  }
  const response = await fetch(endpoint, options);
  if (!response.ok) {
    throw new Error(`RPC failed with HTTP ${response.status}`);
  }
  return response.json();
}
