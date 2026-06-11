export class Filesystem {
  constructor(options = {}) {
    this.payload = options.payload || {};
    this.fetchJson = options.fetchJson || defaultFetchJson;
    this.onStatus = options.onStatus || (() => {});
    this.onOpenFile = options.onOpenFile || (() => {});
    this.onOpenProject = options.onOpenProject || (() => {});
    this.selected = null;

    this.root = document.createElement('section');
    this.root.className = 'filesystem-view';

    this.toolbar = document.createElement('div');
    this.toolbar.className = 'filesystem-toolbar';
    this.toolbar.append(
      this.toolButton('Up', upIcon(), () => this.loadPath(this.payload.parent)),
      this.spacer(),
      this.toolButton('Create file', createIcon(), () => this.createFile()),
      this.toolButton('Delete', deleteIcon(), () => this.deleteFile()),
      this.toolButton('Rename', renameIcon(), () => this.renameFile())
    );
    this.upButton = this.toolbar.firstElementChild;

    this.pathLine = document.createElement('div');
    this.pathLine.className = 'filesystem-path';

    this.header = document.createElement('div');
    this.header.className = 'filesystem-header';
    this.header.append(
      this.headerCell('Name'),
      this.headerCell('Type'),
      this.headerCell('Size')
    );

    this.list = document.createElement('div');
    this.list.className = 'filesystem-list';

    this.message = document.createElement('div');
    this.message.className = 'filesystem-message';

    this.root.append(this.toolbar, this.pathLine, this.header, this.list, this.message);
    this.render();
  }

  element() {
    return this.root;
  }

  render() {
    this.selected = null;
    this.pathLine.textContent = this.payload.path || '';
    this.upButton.disabled = !this.payload.parent || this.payload.parent === this.payload.path;
    this.list.replaceChildren();

    const items = this.payload.items || [];
    items.forEach((item) => {
      const row = document.createElement('button');
      row.type = 'button';
      row.className = `filesystem-row ${item.kind === 'folder' ? 'folder' : 'file'}`;
      row.append(
        this.cell(item.name, 'name'),
        this.cell(item.kind === 'folder' ? 'File folder' : fileType(item.name), 'type'),
        this.cell(item.kind === 'folder' ? '' : formatSize(item.size), 'size')
      );
      row.addEventListener('click', () => this.select(item, row));
      row.addEventListener('dblclick', () => this.openItem(item));
      this.list.appendChild(row);
    });

    if (items.length === 0) {
      const empty = document.createElement('div');
      empty.className = 'filesystem-empty';
      empty.textContent = 'This folder is empty';
      this.list.appendChild(empty);
    }
  }

  select(item, row) {
    this.selected = item;
    this.list.querySelectorAll('.filesystem-row.selected').forEach((node) => node.classList.remove('selected'));
    row.classList.add('selected');
  }

  async openItem(item) {
    if (item.kind === 'folder') {
      await this.loadPath(item.path);
      return;
    }
    if (item.name.endsWith('.trident')) {
      this.onOpenProject(item.path);
      return;
    }

    this.onStatus('Calling /rpc/open-file');
    try {
      const payload = await this.fetchJson('/rpc/open-file', { path: item.path });
      this.onOpenFile(payload);
      this.message.textContent = `Opened ${item.name}`;
      this.onStatus('Ready');
    } catch (error) {
      this.setError(error);
    }
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

  async createFile() {
    const name = window.prompt('Create file');
    if (!name?.trim()) {
      return;
    }
    this.onStatus('Calling /rpc/create-filesystem-file');
    try {
      const payload = await this.fetchJson('/rpc/create-filesystem-file', {
        path: this.payload.path,
        name: name.trim()
      });
      this.payload = payload.directory;
      this.message.textContent = `Created ${name.trim()}`;
      this.render();
      this.onStatus('Ready');
    } catch (error) {
      this.setError(error);
    }
  }

  async deleteFile() {
    if (!this.selected) {
      this.message.textContent = 'Select a file or folder to delete';
      return;
    }
    if (!window.confirm(`Delete ${this.selected.name}?`)) {
      return;
    }
    this.onStatus('Calling /rpc/delete-filesystem-file');
    try {
      const payload = await this.fetchJson('/rpc/delete-filesystem-file', { path: this.selected.path });
      this.payload = payload.directory;
      this.message.textContent = `Deleted ${this.selected.name}`;
      this.render();
      this.onStatus('Ready');
    } catch (error) {
      this.setError(error);
    }
  }

  async renameFile() {
    if (!this.selected) {
      this.message.textContent = 'Select a file or folder to rename';
      return;
    }
    const name = window.prompt('Rename', this.selected.name);
    if (!name?.trim() || name.trim() === this.selected.name) {
      return;
    }
    this.onStatus('Calling /rpc/rename-filesystem-file');
    try {
      const payload = await this.fetchJson('/rpc/rename-filesystem-file', {
        path: this.selected.path,
        name: name.trim()
      });
      this.payload = payload.directory;
      this.message.textContent = `Renamed to ${name.trim()}`;
      this.render();
      this.onStatus('Ready');
    } catch (error) {
      this.setError(error);
    }
  }

  toolButton(label, icon, onClick) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'filesystem-tool-button';
    button.title = label;
    button.setAttribute('aria-label', label);
    button.innerHTML = icon;
    button.addEventListener('click', onClick);
    return button;
  }

  spacer() {
    const node = document.createElement('span');
    node.className = 'filesystem-toolbar-spacer';
    return node;
  }

  headerCell(text) {
    const cell = document.createElement('div');
    cell.textContent = text;
    return cell;
  }

  cell(text, role) {
    const cell = document.createElement('span');
    cell.className = `filesystem-cell ${role}`;
    cell.textContent = text;
    return cell;
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

function fileType(name) {
  const dot = name.lastIndexOf('.');
  if (dot <= 0 || dot === name.length - 1) {
    return 'File';
  }
  return `${name.slice(dot + 1).toUpperCase()} File`;
}

function formatSize(size) {
  const bytes = Number(size || 0);
  if (bytes < 1024) {
    return `${bytes} bytes`;
  }
  if (bytes < 1024 * 1024) {
    return `${Math.round(bytes / 1024)} KB`;
  }
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

function upIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M12 5l-7 7"/>
      <path d="M12 5l7 7"/>
      <path d="M12 6v13"/>
    </svg>`;
}

function createIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M6 3h8l4 4v14H6V3z"/>
      <path d="M14 3v5h4"/>
      <path d="M12 11v6"/>
      <path d="M9 14h6"/>
    </svg>`;
}

function deleteIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M5 7h14"/>
      <path d="M9 7V4h6v3"/>
      <path d="M8 10v9"/>
      <path d="M16 10v9"/>
      <path d="M6 7l1 14h10l1-14"/>
    </svg>`;
}

function renameIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M4 20h16"/>
      <path d="M14 5l5 5"/>
      <path d="M13 6L6 13l-1 5 5-1 7-7"/>
    </svg>`;
}
