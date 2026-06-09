export class PathSelector {
  constructor(options = {}) {
    this.payload = options.payload || {};
    this.fetchJson = options.fetchJson;
    this.onStatus = options.onStatus || (() => {});
    this.onProjectCreated = options.onProjectCreated || (() => {});
    this.onDone = options.onDone || (() => {});
    this.mode = options.mode || 'create';

    this.root = document.createElement('section');
    this.root.className = 'path-selector';

    this.pathLine = document.createElement('div');
    this.pathLine.className = 'path-selector-current';

    this.toolbar = document.createElement('div');
    this.toolbar.className = 'path-selector-toolbar';

    this.upButton = this.button('Up', () => this.loadPath(this.payload.parent));
    this.selectButton = this.button(this.mode === 'load' ? 'Load' : 'Select', () => this.selectCurrentPath());
    this.toolbar.append(this.upButton, this.selectButton);

    this.list = document.createElement('div');
    this.list.className = 'path-selector-list';

    this.createRow = document.createElement('form');
    this.createRow.className = 'path-selector-create';
    this.folderInput = document.createElement('input');
    this.folderInput.type = 'text';
    this.folderInput.placeholder = 'New folder';
    this.folderInput.autocomplete = 'off';
    this.folderInput.spellcheck = false;
    this.createButton = document.createElement('button');
    this.createButton.type = 'submit';
    this.createButton.textContent = 'Create';
    this.createRow.append(this.folderInput, this.createButton);
    this.createRow.addEventListener('submit', (event) => this.createFolder(event));

    this.message = document.createElement('div');
    this.message.className = 'path-selector-message';

    this.root.append(this.pathLine, this.toolbar, this.list);
    if (this.mode !== 'load') {
      this.root.appendChild(this.createRow);
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

    const folders = this.payload.folders || [];
    if (folders.length === 0) {
      const empty = document.createElement('div');
      empty.className = 'path-selector-empty';
      empty.textContent = 'No folders';
      this.list.appendChild(empty);
      return;
    }

    folders.forEach((folder) => {
      const row = document.createElement('button');
      row.type = 'button';
      row.className = 'path-selector-folder';
      row.textContent = folder.name;
      row.title = folder.path;
      row.addEventListener('click', () => this.loadPath(folder.path));
      this.list.appendChild(row);
    });
  }

  async loadPath(path) {
    if (!path) {
      return;
    }
    this.onStatus('Calling /rpc/list-folders');
    try {
      this.payload = await this.fetchJson('/rpc/list-folders', { path });
      this.message.textContent = '';
      this.render();
      this.onStatus('Ready');
    } catch (error) {
      this.setError(error);
    }
  }

  async createFolder(event) {
    event.preventDefault();
    const name = this.folderInput.value.trim();
    if (!name) {
      return;
    }
    this.onStatus('Calling /rpc/create-folder');
    try {
      this.payload = await this.fetchJson('/rpc/create-folder', {
        path: this.payload.path,
        name
      });
      this.folderInput.value = '';
      this.message.textContent = '';
      this.render();
      this.onStatus('Ready');
    } catch (error) {
      this.setError(error);
    }
  }

  async selectCurrentPath() {
    this.onStatus(this.mode === 'load' ? 'Calling /rpc/load-project' : 'Calling /rpc/create-project');
    try {
      if (this.mode === 'load') {
        const loaded = await this.fetchJson('/rpc/load-project', { path: this.payload.path });
        this.message.textContent = `Project loaded from ${loaded.project.path}`;
        this.onProjectCreated(loaded.project);
      } else {
        const request = { path: this.payload.path };
        try {
          await this.fetchJson('/rpc/create-project', request);
        } catch (error) {
          if (String(error.message || error) !== 'project_exists') {
            throw error;
          }
          if (!window.confirm('Project metadata already exists. Overwrite and delete the existing .trident folder?')) {
            this.message.textContent = 'Project creation cancelled';
            this.onStatus('Ready');
            return;
          }
          await this.fetchJson('/rpc/create-project', { ...request, overwrite: true });
        }
        const saved = await this.fetchJson('/rpc/save-project');
        this.message.textContent = `Project saved in ${saved.project.path}/.trident`;
        this.onProjectCreated(saved.project);
      }
      this.onStatus('Ready');
      this.onDone();
    } catch (error) {
      this.setError(error);
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
