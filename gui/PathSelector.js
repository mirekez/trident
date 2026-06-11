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
    this.selectButton = this.button(this.mode === 'load' ? 'Load' : 'Create folder', () => this.selectCurrentPath());
    this.toolbar.append(this.upButton, this.selectButton);

    this.list = document.createElement('div');
    this.list.className = 'path-selector-list';

    this.createRow = document.createElement('form');
    this.createRow.className = 'path-selector-create';
    this.projectInput = document.createElement('input');
    this.projectInput.type = 'text';
    this.projectInput.placeholder = 'Project name';
    this.projectInput.autocomplete = 'off';
    this.projectInput.spellcheck = false;
    this.createButton = document.createElement('button');
    this.createButton.type = 'submit';
    this.createButton.textContent = 'Create project';
    this.createRow.append(this.projectInput, this.createButton);
    this.createRow.addEventListener('submit', (event) => this.createProject(event));

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

  async createFolder(name) {
    if (!name) {
      this.showFolderQuestion();
      return;
    }
    this.onStatus('Calling /rpc/create-folder');
    try {
      this.payload = await this.fetchJson('/rpc/create-folder', {
        path: this.payload.path,
        name
      });
      this.message.textContent = '';
      this.render();
      this.onStatus('Ready');
    } catch (error) {
      this.setError(error);
    }
  }

  async createProject(event) {
    event.preventDefault();
    const name = this.projectInput.value.trim();
    if (!name) {
      this.message.textContent = 'Enter project name';
      return;
    }
    if (name.includes('/') || name.includes('\\') || name === '.' || name === '..') {
      this.message.textContent = 'Invalid project name';
      return;
    }
    this.onStatus('Calling /rpc/create-folder');
    try {
      const createdFolder = await this.fetchJson('/rpc/create-folder', {
        path: this.payload.path,
        name
      });
      const created = await this.createProjectAtPath(createdFolder.path);
      if (!created) {
        return;
      }
      this.onStatus('Ready');
      this.onDone();
    } catch (error) {
      this.setError(error);
    }
  }

  async selectCurrentPath() {
    if (this.mode !== 'load') {
      this.showFolderQuestion();
      return;
    }
    this.onStatus('Calling /rpc/load-project');
    try {
      const loaded = await this.fetchJson('/rpc/load-project', { path: this.payload.path });
      this.message.textContent = `Project loaded from ${loaded.project.path}`;
      this.onProjectCreated(loaded.project);
      this.onStatus('Ready');
      this.onDone();
    } catch (error) {
      this.setError(error);
    }
  }

  async createProjectAtPath(path) {
    const request = { path };
    let created = null;
    try {
      created = await this.fetchJson('/rpc/create-project', request);
    } catch (error) {
      if (String(error.message || error) !== 'project_exists') {
        throw error;
      }
      if (!window.confirm('Project metadata already exists. Overwrite and delete the existing .trident folder?')) {
        this.message.textContent = 'Project creation cancelled';
        this.onStatus('Ready');
        return false;
      }
      created = await this.fetchJson('/rpc/create-project', { ...request, overwrite: true });
    }
    this.projectInput.value = '';
    this.message.textContent = `Project created in ${created.project.path}`;
    this.onProjectCreated(created.project);
    return true;
  }

  showFolderQuestion() {
    const overlay = document.createElement('div');
    overlay.className = 'path-selector-question';

    const dialog = document.createElement('form');
    dialog.className = 'path-selector-question-dialog';

    const title = document.createElement('div');
    title.className = 'path-selector-question-title';
    title.textContent = 'Create folder';

    const input = document.createElement('input');
    input.type = 'text';
    input.placeholder = 'Folder name';
    input.autocomplete = 'off';
    input.spellcheck = false;

    const actions = document.createElement('div');
    actions.className = 'path-selector-question-actions';

    const ok = document.createElement('button');
    ok.type = 'submit';
    ok.textContent = 'OK';

    const discard = document.createElement('button');
    discard.type = 'button';
    discard.textContent = 'Discard';
    discard.addEventListener('click', () => overlay.remove());

    actions.append(ok, discard);
    dialog.append(title, input, actions);
    overlay.appendChild(dialog);
    this.root.appendChild(overlay);
    input.focus();

    dialog.addEventListener('submit', async (event) => {
      event.preventDefault();
      const name = input.value.trim();
      if (!name) {
        input.focus();
        return;
      }
      overlay.remove();
      await this.createFolder(name);
    });
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
