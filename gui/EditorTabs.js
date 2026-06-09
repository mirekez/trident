import { CodeEditor } from './CodeEditor.js';
import { FocusedControl } from './FocusedControl.js';

export class EditorTabs extends FocusedControl {
  constructor(options = {}) {
    super({ parentControl: options.parentControl });
    this.endpoint = options.endpoint;
    this.fetchJson = options.fetchJson || defaultFetchJson;
    this.onStatus = options.onStatus || (() => {});
    this.onOpenFileRequest = options.onOpenFileRequest || (() => this.loadFile());
    this.tabs = [];
    this.activeId = 0;
    this.nextId = 1;

    this.root = document.createElement('section');
    this.root.className = 'editor-tabs';
    this.attachFocusRoot(this.root);

    this.tabbar = document.createElement('div');
    this.tabbar.className = 'editor-tabs-bar';
    this.tabbar.setAttribute('role', 'tablist');

    this.tabStrip = document.createElement('div');
    this.tabStrip.className = 'editor-tabs-strip';

    this.addButton = document.createElement('button');
    this.addButton.type = 'button';
    this.addButton.className = 'editor-tab-add';
    this.addButton.textContent = '+';
    this.addButton.title = 'Open another file';
    this.addButton.setAttribute('aria-label', 'Open another file');
    this.addButton.addEventListener('click', () => this.onOpenFileRequest(this));

    this.panel = document.createElement('div');
    this.panel.className = 'editor-tabs-panel';

    this.tabbar.append(this.tabStrip, this.addButton);
    this.root.append(this.tabbar, this.panel);
  }

  element() {
    return this.root;
  }

  focus() {
    const active = this.tabs.find((tab) => tab.id === this.activeId);
    active?.editor.focus();
  }

  async loadInitial(payload) {
    if (Array.isArray(payload?.files)) {
      this.loadFiles(payload.files);
      return;
    }
    this.addPayload(payload);
  }

  async refreshFromBackend() {
    this.onStatus('Calling /rpc/update-development-tabs');
    const payload = await this.fetchJson('/rpc/update-development-tabs');
    this.loadFiles(payload.files || []);
    this.onStatus('Ready');
  }

  async saveModifiedWithPrompts() {
    const modified = this.tabs.filter((entry) => entry.modified);
    for (const entry of modified) {
      if (window.confirm(`Save changes to ${entry.path}?`)) {
        await this.saveTab(entry);
      }
    }
  }

  async saveFocusedEditor() {
    const focusedEditor = this.focusedChild();
    const entry = this.tabs.find((tab) => tab.editor === focusedEditor) ||
      this.tabs.find((tab) => tab.id === this.activeId);
    if (!entry) {
      this.onStatus('No focused editor');
      return;
    }
    await this.saveTab(entry);
  }

  async saveAllFiles() {
    for (const entry of this.tabs) {
      await this.saveTab(entry);
    }
  }

  loadFiles(files) {
    this.tabs = [];
    this.activeId = 0;
    this.nextId = 1;
    this.tabStrip.replaceChildren();
    this.panel.replaceChildren();

    files.forEach((file) => this.addPayload(file));
    if (files.length === 0) {
      this.showError('No opened files');
    }
  }

  async loadFile() {
    const index = this.tabs.length + 1;
    this.addButton.disabled = true;
    this.onStatus(`Calling ${this.endpoint}`);
    try {
      const payload = await this.fetchJson(this.endpoint);
      this.addPayload({ ...payload, tabSuffix: index });
      this.onStatus('Ready');
    } catch (error) {
      this.onStatus('RPC error');
      this.showError(error);
    } finally {
      this.addButton.disabled = false;
    }
  }

  addPayload(payload) {
    const id = this.nextId++;
    const basePath = payload.path || payload.title || 'untitled';
    const path = payload.tabSuffix ? pathWithIndex(basePath, payload.tabSuffix) : basePath;
    const entry = {
      id,
      path,
      modified: false,
      tab: null,
      editor: null
    };
    const editor = new CodeEditor({
      value: payload.content || '',
      path,
      language: payload.language || 'cpp',
      parentControl: this,
      onChange: () => this.setModified(entry, true)
    });
    entry.editor = editor;

    const tab = document.createElement('button');
    tab.type = 'button';
    tab.className = 'editor-tab';
    tab.title = editor.path;
    tab.setAttribute('role', 'tab');

    const label = document.createElement('span');
    label.className = 'editor-tab-label';
    label.textContent = shortPath(editor.path);

    const close = document.createElement('span');
    close.className = 'editor-tab-close';
    close.textContent = 'x';
    close.title = 'Close tab';
    close.addEventListener('click', (event) => {
      event.stopPropagation();
      this.requestClose(id);
    });

    tab.append(label, close);
    tab.addEventListener('click', () => this.activate(id));
    entry.tab = tab;

    this.tabs.push(entry);
    this.tabStrip.appendChild(tab);
    this.activate(id);
  }

  activate(id) {
    this.activeId = id;
    const active = this.tabs.find((tab) => tab.id === id);
    if (!active) {
      return;
    }

    this.tabs.forEach((entry) => {
      const selected = entry.id === id;
      entry.tab.classList.toggle('active', selected);
      entry.tab.setAttribute('aria-selected', selected ? 'true' : 'false');
    });

    this.panel.replaceChildren(active.editor.element());
    active.editor.focus();
  }

  setModified(entry, modified) {
    entry.modified = modified;
    entry.tab?.classList.toggle('modified', modified);
  }

  requestClose(id) {
    const entry = this.tabs.find((tab) => tab.id === id);
    if (!entry) {
      return;
    }
    if (!entry.modified) {
      this.closeTab(id);
      return;
    }
    this.showClosePrompt(entry);
  }

  async saveTab(entry) {
    this.onStatus('Calling /rpc/save-file');
    await this.fetchJson('/rpc/save-file', {
      path: entry.path,
      content: entry.editor.getValue()
    });
    this.setModified(entry, false);
    this.onStatus('Ready');
  }

  async saveAndClose(entry, prompt) {
    try {
      await this.saveTab(entry);
      prompt.remove();
      this.closeTab(entry.id);
    } catch (error) {
      this.onStatus('RPC error');
      const message = prompt.querySelector('.editor-tabs-prompt-message');
      message.textContent = error instanceof Error ? error.message : String(error);
    }
  }

  closeTab(id) {
    const index = this.tabs.findIndex((tab) => tab.id === id);
    if (index === -1) {
      return;
    }
    const [closed] = this.tabs.splice(index, 1);
    this.fetchJson('/rpc/close-file', { path: closed.path }).catch(() => {});
    closed.tab.remove();
    if (this.activeId === id) {
      const next = this.tabs[Math.min(index, this.tabs.length - 1)];
      if (next) {
        this.activate(next.id);
      } else {
        this.activeId = 0;
        this.panel.replaceChildren();
      }
    }
  }

  showClosePrompt(entry) {
    const overlay = document.createElement('div');
    overlay.className = 'editor-tabs-prompt';

    const dialog = document.createElement('div');
    dialog.className = 'editor-tabs-prompt-dialog';

    const title = document.createElement('div');
    title.className = 'editor-tabs-prompt-title';
    title.textContent = 'Save changes?';

    const message = document.createElement('div');
    message.className = 'editor-tabs-prompt-message';
    message.textContent = entry.path;

    const actions = document.createElement('div');
    actions.className = 'editor-tabs-prompt-actions';

    const save = document.createElement('button');
    save.type = 'button';
    save.textContent = 'Save';
    save.addEventListener('click', () => this.saveAndClose(entry, overlay));

    const ignore = document.createElement('button');
    ignore.type = 'button';
    ignore.textContent = 'Ignore';
    ignore.addEventListener('click', () => {
      overlay.remove();
      this.closeTab(entry.id);
    });

    const cancel = document.createElement('button');
    cancel.type = 'button';
    cancel.textContent = 'Cancel';
    cancel.addEventListener('click', () => overlay.remove());

    actions.append(save, ignore, cancel);
    dialog.append(title, message, actions);
    overlay.appendChild(dialog);
    this.root.appendChild(overlay);
  }

  showError(error) {
    const item = document.createElement('div');
    item.className = 'editor-tabs-error';
    item.textContent = error instanceof Error ? error.message : String(error);
    this.panel.replaceChildren(item);
  }
}

async function defaultFetchJson(endpoint) {
  const response = await fetch(endpoint, { method: 'POST' });
  if (!response.ok) {
    throw new Error(`RPC failed with HTTP ${response.status}`);
  }
  return response.json();
}

function pathWithIndex(path, index) {
  if (index <= 1) {
    return path;
  }

  const slash = path.lastIndexOf('/');
  const dir = slash === -1 ? '' : path.slice(0, slash + 1);
  const name = slash === -1 ? path : path.slice(slash + 1);
  const dot = name.lastIndexOf('.');
  if (dot <= 0) {
    return `${dir}${name}-${index}`;
  }
  return `${dir}${name.slice(0, dot)}-${index}${name.slice(dot)}`;
}

function shortPath(path) {
  const parts = path.split('/');
  return parts.slice(-2).join('/');
}
