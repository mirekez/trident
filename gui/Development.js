import { EditorTabs } from './EditorTabs.js';
import { Console } from './Console.js';
import { FocusedControl } from './FocusedControl.js';
import { Log } from './Log.js';

const ENDPOINTS = {
  files: '/rpc/get-opened-file-list',
  log: '/rpc/refresh-dev-log',
  loadFile: '/rpc/load-file',
  run: '/rpc/run',
  compile: '/rpc/compile',
  synthesize: '/rpc/synthesize'
};

export class Development extends FocusedControl {
  constructor(options = {}) {
    super({ parentControl: options.parentControl });
    this.fetchJson = options.fetchJson || defaultFetchJson;
    this.onStatus = options.onStatus || (() => {});
    this.onOpenFileRequest = options.onOpenFileRequest || (() => {});
    this.onProjectSettingsRequired = options.onProjectSettingsRequired || (() => {});
    this.paneSizes = [58, 24, 18];
    this.drag = null;

    this.root = document.createElement('section');
    this.root.className = 'development';
    this.attachFocusRoot(this.root);

    this.toolbar = document.createElement('div');
    this.toolbar.className = 'development-toolbar';
    this.toolbar.append(
      this.toolbarButton('Save file', saveIcon(), () => this.saveFocusedFile()),
      this.toolbarButton('Save all', saveAllIcon(), () => this.saveAllFiles()),
      this.toolbarSpacer(),
      this.toolbarButton('Run', runIcon(), () => this.run()),
      this.toolbarButton('Compile', compileIcon(), () => this.compile()),
      this.toolbarButton('Synthesize', synthesizeIcon(), () => this.synthesize())
    );

    this.editorTabs = new EditorTabs({
      endpoint: ENDPOINTS.loadFile,
      fetchJson: this.fetchJson,
      onStatus: this.onStatus,
      parentControl: this,
      onOpenFileRequest: this.onOpenFileRequest
    });

    this.editorPane = this.createPane('development-pane development-editor-pane');
    this.logPane = this.createPane('development-pane development-log-pane');
    this.consolePane = this.createPane('development-pane development-console-pane');

    this.splitterA = this.createSplitter(0);
    this.splitterB = this.createSplitter(1);

    this.log = new Log();
    this.log.setText('Loading development log...');

    this.console = new Console({
      fetchJson: this.fetchJson,
      onStatus: this.onStatus,
      parentControl: this
    });

    this.editorPane.appendChild(this.editorTabs.element());
    this.logPane.appendChild(this.log.element());
    this.consolePane.appendChild(this.console.element());
    this.root.append(this.toolbar, this.editorPane, this.splitterA, this.logPane, this.splitterB, this.consolePane);
    this.applySizes();
  }

  element() {
    return this.root;
  }

  destroy() {
    this.console.destroy();
  }

  async loadInitial(filePayload) {
    this.editorTabs.loadInitial(filePayload);
    await this.refreshLog();
  }

  focus() {
    this.editorTabs.focus();
  }

  async refreshTabs() {
    await this.editorTabs.refreshFromBackend();
  }

  openFilePayload(payload) {
    this.editorTabs.addPayload(payload);
  }

  async saveModifiedEditors() {
    await this.editorTabs.saveModifiedWithPrompts();
  }

  async saveFocusedFile() {
    try {
      await this.editorTabs.saveFocusedEditor();
    } catch (error) {
      this.onStatus(error instanceof Error ? error.message : String(error));
    }
  }

  async saveAllFiles() {
    try {
      await this.editorTabs.saveAllFiles();
    } catch (error) {
      this.onStatus(error instanceof Error ? error.message : String(error));
    }
  }

  async compile() {
    await this.executeFlow({
      action: 'Compile',
      endpoint: ENDPOINTS.compile,
      runningText: 'Compiling...',
      missingError: 'compile_settings_required',
      missingText: 'Compile cancelled: Top module name and Top module file are required.',
      missingStatus: 'Compile settings required',
      successStatus: 'Compile finished',
      failureStatus: 'Compile failed',
      rpcErrorStatus: 'Compile RPC error',
      settingsKind: 'compile'
    });
  }

  async run() {
    await this.executeFlow({
      action: 'Run',
      endpoint: ENDPOINTS.run,
      runningText: 'Running...',
      missingError: 'run_settings_required',
      missingText: 'Run cancelled: Top module file and Main test file are required.',
      missingStatus: 'Run settings required',
      successStatus: 'Run finished',
      failureStatus: 'Run failed',
      rpcErrorStatus: 'Run RPC error',
      settingsKind: 'run'
    });
  }

  async synthesize() {
    await this.executeFlow({
      action: 'Synthesize',
      endpoint: ENDPOINTS.synthesize,
      runningText: 'Synthesizing...',
      missingError: 'synthesize_settings_required',
      missingText: 'Synthesize cancelled: Top module name is required.',
      missingStatus: 'Synthesize settings required',
      successStatus: 'Synthesize finished',
      failureStatus: 'Synthesize failed',
      rpcErrorStatus: 'Synthesize RPC error',
      settingsKind: 'synthesize'
    });
  }

  async executeFlow(options) {
    try {
      const ready = await this.ensureFlowSettings(options.settingsKind);
      if (!ready) {
        this.log.setText(options.missingText);
        this.onStatus(options.missingStatus);
        return;
      }

      this.onStatus(`Calling ${options.endpoint}`);
      this.log.setText(options.runningText);
      const payload = await this.fetchJson(options.endpoint);
      const header = [
        `Action: ${payload.action || options.action}`,
        `Exit code: ${payload.exitCode}`,
        payload.command ? `Command: ${payload.command}` : ''
      ].filter(Boolean).join('\n');
      this.log.setText(`${header}\n\n${payload.output || ''}`);
      this.onStatus(payload.exitCode === 0 ? options.successStatus : options.failureStatus);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      if (message === options.missingError || message === 'compile_settings_required' || message === 'synthesize_settings_required' || message === 'top_module_required') {
        this.onProjectSettingsRequired(options.settingsKind);
        this.log.setText(options.missingText);
        this.onStatus(options.missingStatus);
        return;
      }
      this.log.setText(error instanceof Error ? error.message : String(error));
      this.onStatus(options.rpcErrorStatus);
    }
  }

  async ensureFlowSettings(kind) {
    this.onStatus('Checking project settings');
    const payload = await this.fetchJson('/rpc/get-project-settings');
    const settings = payload.settings || {};
    const ready = kind === 'run'
      ? settings.topModuleFile && settings.mainTestFile
      : kind === 'synthesize'
        ? settings.topModuleName
        : settings.topModuleName && settings.topModuleFile;
    if (ready) {
      return true;
    }

    this.onProjectSettingsRequired(kind);
    return false;
  }

  async refreshLog() {
    this.onStatus(`Calling ${ENDPOINTS.log}`);
    try {
      const payload = await this.fetchJson(ENDPOINTS.log);
      this.log.setText(payload.log || '');
      this.onStatus('Ready');
    } catch (error) {
      this.log.setText(error instanceof Error ? error.message : String(error));
      this.onStatus('RPC error');
    }
  }

  createPane(className) {
    const pane = document.createElement('div');
    pane.className = className;
    return pane;
  }

  toolbarButton(label, icon, onClick) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'development-tool-button';
    button.title = label;
    button.setAttribute('aria-label', label);
    button.innerHTML = icon;
    button.addEventListener('click', onClick);
    return button;
  }

  toolbarSpacer() {
    const spacer = document.createElement('span');
    spacer.className = 'development-toolbar-spacer';
    return spacer;
  }

  createSplitter(index) {
    const splitter = document.createElement('div');
    splitter.className = 'development-splitter';
    splitter.setAttribute('role', 'separator');
    splitter.setAttribute('aria-orientation', 'horizontal');
    splitter.addEventListener('pointerdown', (event) => this.beginResize(event, index));
    splitter.addEventListener('pointermove', (event) => this.resize(event));
    splitter.addEventListener('pointerup', () => this.endResize());
    return splitter;
  }

  beginResize(event, index) {
    this.drag = {
      index,
      startY: event.clientY,
      sizes: [...this.paneSizes],
      height: this.root.getBoundingClientRect().height
    };
    event.currentTarget.setPointerCapture(event.pointerId);
    this.root.classList.add('resizing');
  }

  resize(event) {
    if (!this.drag || this.drag.height <= 0) {
      return;
    }
    const delta = ((event.clientY - this.drag.startY) / this.drag.height) * 100;
    const first = this.drag.index;
    const second = first + 1;
    const next = [...this.drag.sizes];
    next[first] = clamp(this.drag.sizes[first] + delta, 12, 78);
    next[second] = clamp(this.drag.sizes[second] - delta, 12, 78);
    if (next[first] + next[second] !== this.drag.sizes[first] + this.drag.sizes[second]) {
      return;
    }
    this.paneSizes = next;
    this.applySizes();
  }

  endResize() {
    this.drag = null;
    this.root.classList.remove('resizing');
  }

  applySizes() {
    this.editorPane.style.flexBasis = `${this.paneSizes[0]}%`;
    this.logPane.style.flexBasis = `${this.paneSizes[1]}%`;
    this.consolePane.style.flexBasis = `${this.paneSizes[2]}%`;
  }
}

async function defaultFetchJson(endpoint) {
  const response = await fetch(endpoint, { method: 'POST' });
  if (!response.ok) {
    throw new Error(`RPC failed with HTTP ${response.status}`);
  }
  return response.json();
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function saveIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M5 3h12l2 2v16H5V3z"/>
      <path d="M8 3v6h8V3"/>
      <path d="M8 15h8v6H8v-6z"/>
    </svg>`;
}

function saveAllIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M7 3h10l2 2v14H7V3z"/>
      <path d="M10 3v5h6V3"/>
      <path d="M10 14h6v5h-6v-5z"/>
      <path d="M4 6v15h12"/>
    </svg>`;
}

function compileIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M4 20h16"/>
      <path d="M7 17l8-8"/>
      <path d="M13 5l6 6"/>
      <path d="M14 4l6 6"/>
      <path d="M5 18l3 3"/>
    </svg>`;
}

function runIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M8 5v14l11-7-11-7z"/>
    </svg>`;
}

function synthesizeIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <rect x="4" y="5" width="5" height="5" rx="1"/>
      <rect x="15" y="5" width="5" height="5" rx="1"/>
      <rect x="4" y="14" width="5" height="5" rx="1"/>
      <rect x="15" y="14" width="5" height="5" rx="1"/>
      <path d="M9 7.5h6"/>
      <path d="M9 16.5h6"/>
      <path d="M6.5 10v4"/>
      <path d="M17.5 10v4"/>
    </svg>`;
}
