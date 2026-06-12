export class FileSelect {
  constructor(options = {}) {
    this.payload = options.payload || {};
    this.mode = options.mode || 'open-project';
    this.fetchJson = options.fetchJson || defaultFetchJson;
    this.onStatus = options.onStatus || (() => {});
    this.onSelect = options.onSelect || (() => {});
    this.onCancel = options.onCancel || (() => {});
    this.selectedFiles = new Set();

    this.root = document.createElement('section');
    this.root.className = `file-select ${this.mode}`;

    this.pathLine = document.createElement('div');
    this.pathLine.className = 'file-select-current';

    this.toolbar = document.createElement('div');
    this.toolbar.className = 'file-select-toolbar';
    this.upButton = this.button('Up', () => this.loadPath(this.payload.parent));
    this.cancelButton = this.button('Cancel', () => this.onCancel());
    this.addButton = this.button('Add selected', () => this.submitSelectedFiles());
    this.toolbar.append(this.upButton);
    if (this.mode === 'select-files') {
      this.toolbar.appendChild(this.addButton);
    }
    this.toolbar.appendChild(this.cancelButton);

    this.list = document.createElement('div');
    this.list.className = 'file-select-list';

    this.nameForm = document.createElement('form');
    this.nameForm.className = 'file-select-name';
    this.nameInput = document.createElement('input');
    this.nameInput.type = 'text';
    this.nameInput.placeholder = 'Project name';
    this.nameInput.autocomplete = 'off';
    this.nameInput.spellcheck = false;
    this.saveButton = document.createElement('button');
    this.saveButton.type = 'submit';
    this.saveButton.textContent = 'Save';
    this.nameForm.append(this.nameInput, this.saveButton);
    this.nameForm.addEventListener('submit', (event) => this.submitName(event));

    this.message = document.createElement('div');
    this.message.className = 'file-select-message';

    this.root.append(this.pathLine, this.toolbar, this.list);
    if (this.mode === 'save-project') {
      this.root.appendChild(this.nameForm);
    }
    this.root.appendChild(this.message);
    this.render();
  }

  element() {
    return this.root;
  }

  render() {
    this.pathLine.textContent = this.payload.path || '';
    this.upButton.disabled = !this.payload.parent || this.payload.parent === this.payload.path;
    this.list.replaceChildren();

    const items = this.payload.items || [];
    const visible = items.filter((item) => {
      if (item.kind === 'folder') {
        return true;
      }
      return this.mode === 'select-files' ? item.kind === 'file' : isProjectArchive(item.name);
    });
    visible.forEach((item) => {
      const row = this.mode === 'select-files' && item.kind === 'file'
        ? document.createElement('label')
        : document.createElement('button');
      if (row.tagName === 'BUTTON') {
        row.type = 'button';
      }
      row.className = item.kind === 'folder' ? 'file-select-folder' : 'file-select-file';
      row.title = item.path;
      if (this.mode === 'select-files' && item.kind === 'file') {
        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.checked = this.selectedFiles.has(item.path);
        checkbox.addEventListener('change', () => {
          if (checkbox.checked) {
            this.selectedFiles.add(item.path);
          } else {
            this.selectedFiles.delete(item.path);
          }
          this.updateAddButton();
        });
        const name = document.createElement('span');
        name.textContent = item.name;
        row.append(checkbox, name);
      } else {
        row.textContent = item.name;
      }
      row.addEventListener('click', () => {
        if (item.kind === 'folder') {
          this.loadPath(item.path);
        }
      });
      row.addEventListener('dblclick', () => {
        if (item.kind === 'folder') {
          this.loadPath(item.path);
        } else if (this.mode !== 'select-files') {
          this.onSelect({ path: item.path });
        }
      });
      this.list.appendChild(row);
    });

    if (visible.length === 0) {
      const empty = document.createElement('div');
      empty.className = 'file-select-empty';
      empty.textContent = this.mode === 'select-files'
        ? 'No files here'
        : this.mode === 'save-project' ? 'No project archives' : 'No project archives here';
      this.list.appendChild(empty);
    }
    this.updateAddButton();
  }

  async loadPath(path) {
    if (!path) {
      return;
    }
    this.onStatus('Calling /rpc/list-filesystem');
    try {
      this.payload = await this.fetchJson('/rpc/list-filesystem', { path });
      this.message.textContent = '';
      this.render();
      this.onStatus('Ready');
    } catch (error) {
      this.setError(error);
    }
  }

  submitName(event) {
    event.preventDefault();
    const projectName = normalizeProjectName(this.nameInput.value);
    if (!projectName) {
      this.message.textContent = 'Enter project name';
      return;
    }

    const archiveName = `${projectName}.trident`;
    const exists = (this.payload.items || []).some((item) => item.kind === 'file' && item.name === archiveName);
    let overwrite = false;
    if (exists) {
      overwrite = window.confirm(`${archiveName} already exists. Overwrite it?`);
      if (!overwrite) {
        this.message.textContent = 'Project save cancelled';
        this.onSelect(false);
        return;
      }
    }
    this.onSelect({ projectName, overwrite });
  }

  submitSelectedFiles() {
    const files = [...this.selectedFiles];
    if (files.length === 0) {
      this.message.textContent = 'Select one or more files';
      return;
    }
    this.onSelect({ files });
  }

  updateAddButton() {
    if (this.addButton) {
      this.addButton.disabled = this.mode === 'select-files' && this.selectedFiles.size === 0;
    }
  }

  button(label, onClick) {
    const button = document.createElement('button');
    button.type = 'button';
    button.textContent = label;
    button.addEventListener('click', onClick);
    return button;
  }

  setError(error) {
    this.message.textContent = error instanceof Error ? error.message : String(error);
    this.onStatus('RPC error');
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

function isProjectArchive(name) {
  return name.endsWith('.trident');
}

function normalizeProjectName(name) {
  let result = String(name || '').trim();
  if (result.endsWith('.trident')) {
    result = result.slice(0, -'.trident'.length);
  }
  if (result.includes('/') || result.includes('\\') || result === '.' || result === '..') {
    return '';
  }
  return result;
}
