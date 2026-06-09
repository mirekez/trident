export class OpenFile {
  constructor(options = {}) {
    this.payload = options.payload || {};
    this.fetchJson = options.fetchJson;
    this.onStatus = options.onStatus || (() => {});
    this.onOpen = options.onOpen || (() => {});

    this.root = document.createElement('section');
    this.root.className = 'open-file';

    this.pathLine = document.createElement('div');
    this.pathLine.className = 'open-file-current';

    this.toolbar = document.createElement('div');
    this.toolbar.className = 'open-file-toolbar';
    this.upButton = this.button('Up', () => this.loadPath(this.payload.parent));
    this.toolbar.appendChild(this.upButton);

    this.list = document.createElement('div');
    this.list.className = 'open-file-list';

    this.createForm = document.createElement('form');
    this.createForm.className = 'open-file-create';
    this.fileInput = document.createElement('input');
    this.fileInput.type = 'text';
    this.fileInput.placeholder = 'New file name';
    this.fileInput.autocomplete = 'off';
    this.fileInput.spellcheck = false;
    this.createButton = document.createElement('button');
    this.createButton.type = 'submit';
    this.createButton.textContent = 'Create';
    this.createForm.append(this.fileInput, this.createButton);
    this.createForm.addEventListener('submit', (event) => this.createFile(event));

    this.message = document.createElement('div');
    this.message.className = 'open-file-message';

    this.root.append(this.pathLine, this.toolbar, this.list, this.createForm, this.message);
    this.render();
  }

  element() {
    return this.root;
  }

  render() {
    this.pathLine.textContent = this.payload.path || '';
    this.upButton.disabled = !this.payload.parent || this.payload.parent === this.payload.path;
    this.list.replaceChildren();

    (this.payload.folders || []).forEach((folder) => {
      const row = this.row(folder.name, 'open-file-folder');
      row.addEventListener('click', () => this.loadPath(folder.path));
      this.list.appendChild(row);
    });

    (this.payload.files || []).forEach((file) => {
      const row = this.row(file.name, 'open-file-file');
      row.addEventListener('click', () => this.openFile(file.path));
      this.list.appendChild(row);
    });

    if ((this.payload.folders || []).length === 0 && (this.payload.files || []).length === 0) {
      const empty = document.createElement('div');
      empty.className = 'open-file-empty';
      empty.textContent = 'No files';
      this.list.appendChild(empty);
    }
  }

  async loadPath(path) {
    if (!path) return;
    this.onStatus('Calling /rpc/list-project-files');
    try {
      this.payload = await this.fetchJson('/rpc/list-project-files', { path });
      this.message.textContent = '';
      this.render();
      this.onStatus('Ready');
    } catch (error) {
      this.setError(error);
    }
  }

  async openFile(path) {
    this.onStatus('Calling /rpc/open-file');
    try {
      const payload = await this.fetchJson('/rpc/open-file', { path });
      this.onOpen(payload);
      this.onStatus('Ready');
    } catch (error) {
      this.setError(error);
    }
  }

  async createFile(event) {
    event.preventDefault();
    const name = this.fileInput.value.trim();
    if (!name) return;
    this.onStatus('Calling /rpc/create-file');
    try {
      const payload = await this.fetchJson('/rpc/create-file', {
        path: this.payload.path,
        name
      });
      this.fileInput.value = '';
      this.onOpen(payload);
      this.onStatus('Ready');
    } catch (error) {
      this.setError(error);
    }
  }

  row(label, className) {
    const row = document.createElement('button');
    row.type = 'button';
    row.className = className;
    row.textContent = label;
    return row;
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
