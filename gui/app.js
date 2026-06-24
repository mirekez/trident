import { Development } from './Development.js';
import { PathSelector } from './PathSelector.js';
import { OpenFile } from './OpenFile.js';
import { ProjectOptions } from './ProjectOptions.js';
import { Filesystem } from './Filesysten.js';
import { FileSelect } from './FileSelect.js';
import { ClassGenerator } from './ClassGenerator.js';

const desktop = document.querySelector('#desktop');
const launchButtons = document.querySelector('#launch-buttons');
const template = document.querySelector('#window-template');
const connection = document.querySelector('#connection');

const rpcConfig = {
  createProject: {
    title: 'Create Project',
    buttonLabel: 'Create project',
    icon: newProjectIcon(),
    endpoint: '/rpc/list-folders',
    windowClass: 'path-window',
    render: (payload, node) => renderPathSelector(payload, node, 'create')
  },
  loadProject: {
    buttonLabel: 'Load project',
    icon: loadProjectIcon(),
    action: () => openProjectArchiveWindow()
  },
  saveProject: {
    buttonLabel: 'Save project',
    icon: saveProjectIcon(),
    action: saveProject
  },
  closeProject: {
    buttonLabel: 'Close project',
    icon: closeProjectIcon(),
    action: closeProject
  },
  projectOptions: {
    title: 'Project Settings',
    buttonLabel: 'Project settings',
    icon: projectSettingsIcon(),
    endpoint: '/rpc/get-project-settings',
    windowClass: 'project-options-window',
    render: renderProjectOptions
  },
  filesystem: {
    title: 'Filesystem',
    buttonLabel: 'Filesystem',
    icon: filesystemIcon(),
    endpoint: '/rpc/list-filesystem',
    windowClass: 'filesystem-window',
    render: renderFilesystem
  },
  classGenerator: {
    title: 'Class Generator',
    buttonLabel: 'Class Generator',
    icon: classGeneratorIcon(),
    endpoint: '/rpc/class-generator-defaults',
    windowClass: 'class-generator-window',
    render: renderClassGenerator
  },
  development: {
    title: 'Development',
    buttonLabel: 'Open Development',
    icon: developmentIcon(),
    endpoint: '/rpc/get-opened-file-list',
    windowClass: 'development-window',
    render: renderDevelopment
  }
};

let nextOffset = 0;
let nextZIndex = 10;
let nextPinnedZIndex = 100000;
let activeDevelopment = null;
const openMainWindows = new Map();
const restorableWindows = new Set(['development', 'filesystem']);
let layoutSaveTimer = 0;
let suppressLayoutSave = false;

createRpcButtons();
installPushHandlers();
startPushEvents();
restoreCurrentProjectLayout();

document.addEventListener('keydown', (event) => {
  if (event.key !== 'Escape') {
    return;
  }
  const windows = [...desktop.querySelectorAll('.internal-window')];
  const topWindow = windows.sort((a, b) => Number(b.style.zIndex || 0) - Number(a.style.zIndex || 0))[0];
  closeWindow(topWindow);
});

function createRpcButtons() {
  launchButtons.replaceChildren();
  Object.entries(rpcConfig).forEach(([key, config]) => {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'rpc-launch-button';
    button.dataset.window = key;
    button.title = config.buttonLabel;
    button.setAttribute('aria-label', config.buttonLabel);
    button.innerHTML = config.icon || '';
    button.addEventListener('click', () => {
      if (config.action) {
        config.action();
      } else {
        createWindow(key);
      }
    });
    launchButtons.appendChild(button);
  });
}

async function createWindow(key, renderOptions = {}) {
  const config = rpcConfig[key];
  if (!config) {
    return;
  }
  const existing = openMainWindows.get(key);
  if (existing?.isConnected) {
    bringWindowToFront(existing);
    existing.scrollIntoView({ block: 'nearest', inline: 'nearest' });
    return;
  }
  openMainWindows.delete(key);
  const layout = renderOptions.layout || null;

  const node = template.content.firstElementChild.cloneNode(true);
  node.dataset.windowKey = key;
  const title = node.querySelector('h2');
  const content = node.querySelector('.window-content');
  const close = node.querySelector('.close-button');
  const maximize = node.querySelector('.maximize-button');
  const alwaysOnTop = node.querySelector('.always-on-top-button');
  initializeWindowButtons(node, maximize, alwaysOnTop);

  if (config.windowClass) {
    node.classList.add(config.windowClass);
  }
  title.textContent = config.title;
  content.innerHTML = '<div class="loading">Loading RPC response...</div>';
  node.style.left = `${Number.isFinite(layout?.x) ? Math.max(0, layout.x) : 32 + nextOffset}px`;
  node.style.top = `${Number.isFinite(layout?.y) ? Math.max(0, layout.y) : 32 + nextOffset}px`;
  if (Number.isFinite(layout?.width) && !layout?.maximized) {
    node.style.width = `${Math.max(260, layout.width)}px`;
  }
  if (Number.isFinite(layout?.height) && !layout?.maximized) {
    node.style.height = `${Math.max(180, layout.height)}px`;
  }
  if (layout?.maximized) {
    node.classList.add('maximized');
    setMaximizeButtonState(maximize, true);
  }
  if (layout?.alwaysOnTop) {
    node.classList.add('always-on-top');
    setAlwaysOnTopButtonState(alwaysOnTop, true);
  }
  bringWindowToFront(node);
  if (!layout) {
    nextOffset = (nextOffset + 28) % 140;
  }
  openMainWindows.set(key, node);

  close.addEventListener('click', () => {
    closeWindow(node);
  });
  maximize.addEventListener('click', () => toggleMaximized(node, maximize));
  alwaysOnTop.addEventListener('click', () => toggleAlwaysOnTop(node, alwaysOnTop));
  node.addEventListener('pointerdown', () => {
    bringWindowToFront(node);
  });
  makeDraggable(node);
  desktop.appendChild(node);

  try {
    connection.textContent = `Calling ${config.endpoint}`;
    const payload = await callRpc(config.endpoint);
    const rendered = config.render(payload, node, renderOptions);
    if (rendered.cleanup) {
      node._cleanup = rendered.cleanup;
    }
    content.replaceChildren(rendered.element || rendered);
    rendered.afterAttach?.();
    connection.textContent = 'Ready';
    persistWindowLayoutSoon();
  } catch (error) {
    content.replaceChildren(renderError(error));
    connection.textContent = 'RPC error';
  }
  return node;
}

function closeWindow(node, options = {}) {
  if (!node) {
    return;
  }
  const key = node.dataset.windowKey || '';
  const persistLayout = options.persistLayout !== false;
  node._cleanup?.();
  if (key) {
    openMainWindows.delete(key);
  }
  node.remove();
  if (persistLayout && isRestorableWindow(key)) {
    persistWindowLayoutSoon();
  }
}

function renderDevelopment(payload) {
  const development = new Development({
    fetchJson: callRpc,
    onStatus: (message) => {
      connection.textContent = message;
    },
    onOpenFileRequest: (tabs) => openFileWindow(tabs),
    onProjectSettingsRequired: (kind) => openProjectSettingsForFlow(kind)
  });
  return {
    element: development.element(),
    cleanup: () => {
      if (activeDevelopment === development) {
        activeDevelopment = null;
      }
      development.destroy();
    },
    afterAttach: async () => {
      activeDevelopment = development;
      await development.loadInitial(payload);
      development.focus();
    }
  };
}

function renderPathSelector(payload, windowNode, mode) {
  const selector = new PathSelector({
    payload,
    mode,
    fetchJson: callRpc,
    onStatus: (message) => {
      connection.textContent = message;
    },
    onProjectCreated: async (project) => {
      await activateProjectLayout(project);
      connection.textContent = `Project: ${project.path}`;
    },
    onDone: () => {
      closeWindow(windowNode);
    }
  });
  return selector.element();
}

function renderProjectOptions(payload, windowNode, renderOptions = {}) {
  const projectOptions = new ProjectOptions({
    payload,
    note: renderOptions.note || '',
    fetchJson: callRpc,
    onStatus: (message) => {
      connection.textContent = message;
    },
    onSelectAdditionalSources: () => openAdditionalSourcesWindow(payload.settings?.path || ''),
    onDone: () => {
      closeWindow(windowNode);
    }
  });
  return projectOptions.element();
}

function openProjectSettingsForFlow(kind) {
  const existing = openMainWindows.get('projectOptions');
  if (existing?.isConnected) {
    closeWindow(existing);
  }
  createWindow('projectOptions', {
    note: kind === 'run'
      ? 'For Run both Top module file and Main test file should be set.'
      : kind === 'synthesize'
        ? 'For Synthesize Top module name should be set.'
        : 'For compilation both Top module name and Top module file should be set.'
  });
}

function renderFilesystem(payload) {
  const filesystem = new Filesystem({
    payload,
    fetchJson: callRpc,
    onStatus: (message) => {
      connection.textContent = message;
    },
    onOpenFile: (filePayload) => {
      if (!activeDevelopment) {
        connection.textContent = 'Open Development window before opening files';
        return;
      }
      activeDevelopment.openFilePayload(filePayload);
      const developmentWindow = openMainWindows.get('development');
      if (developmentWindow?.isConnected) {
        bringWindowToFront(developmentWindow);
      }
      connection.textContent = `Opened: ${filePayload.path}`;
    },
    onOpenProject: (path) => {
      openProject(path);
    }
  });
  return filesystem.element();
}

function renderClassGenerator(payload) {
  const generator = new ClassGenerator({
    payload,
    fetchJson: callRpc,
    onStatus: (message) => {
      connection.textContent = message;
    },
    onGenerated: (filePayload) => {
      if (activeDevelopment) {
        activeDevelopment.openFilePayload(filePayload);
        const developmentWindow = openMainWindows.get('development');
        if (developmentWindow?.isConnected) {
          bringWindowToFront(developmentWindow);
        }
      }
    }
  });
  return generator.element();
}

function installPushHandlers() {
  window.pushSource = async (filename) => {
    if (!filename) {
      connection.textContent = 'pushSource ignored: empty filename';
      return false;
    }
    if (!activeDevelopment) {
      connection.textContent = 'pushSource ignored: Development window is not open';
      return false;
    }

    try {
      connection.textContent = `pushSource: ${filename}`;
      const payload = await callRpc('/rpc/open-file', { path: filename });
      activeDevelopment.openFilePayload(payload);
      const developmentWindow = openMainWindows.get('development');
      if (developmentWindow?.isConnected) {
        bringWindowToFront(developmentWindow);
      }
      connection.textContent = `Opened: ${payload.path}`;
      return true;
    } catch (error) {
      connection.textContent = error instanceof Error ? error.message : String(error);
      return false;
    }
  };

  window.pushProjectSettings = async (settings = {}) => {
    try {
      const payload = {
        topModuleName: settings.topModuleName || '',
        topModuleFile: settings.topModuleFile || '',
        mainTestFile: settings.mainTestFile || '',
        additionalSources: settings.additionalSources || ''
      };
      connection.textContent = 'pushProjectSettings';
      const response = await callRpc('/rpc/save-project-settings', payload);
      connection.textContent = `Project settings saved: ${response.settings.path}`;
      return true;
    } catch (error) {
      connection.textContent = error instanceof Error ? error.message : String(error);
      return false;
    }
  };
}

async function startPushEvents() {
  while (true) {
    try {
      const response = await fetch('/rpc/push-events', { method: 'POST' });
      if (!response.ok || !response.body) {
        throw new Error(`Push stream failed with HTTP ${response.status}`);
      }
      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let buffer = '';
      while (true) {
        const { value, done } = await reader.read();
        if (done) {
          break;
        }
        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split('\n');
        buffer = lines.pop() || '';
        for (const line of lines) {
          handlePushEvent(line);
        }
      }
      if (buffer.trim()) {
        handlePushEvent(buffer);
      }
    } catch (error) {
      connection.textContent = error instanceof Error ? error.message : String(error);
      await delay(1000);
    }
  }
}

function handlePushEvent(line) {
  if (!line.trim()) {
    return;
  }
  try {
    const event = JSON.parse(line);
    if (event.action === 'pushSource') {
      window.pushSource(event.filename);
    } else if (event.action === 'pushProjectSettings') {
      window.pushProjectSettings({
        topModuleName: event.topModuleName,
        topModuleFile: event.topModuleFile,
        mainTestFile: event.mainTestFile,
        additionalSources: event.additionalSources
      });
    }
  } catch (error) {
    connection.textContent = error instanceof Error ? error.message : String(error);
  }
}

async function saveProject(options = {}) {
  try {
    const settings = await currentProjectSettings();
    if (!settings) {
      connection.textContent = 'No project to save';
      return false;
    }
    connection.textContent = 'Checking editors';
    await activeDevelopment?.saveModifiedEditors();
    await persistWindowLayoutNow();
    let projectName = options.projectName || settings.projectName || '';
    let overwrite = Boolean(options.overwrite);
    if (!projectName) {
      const selected = await openProjectSaveNameWindow(settings);
      if (!selected) {
        connection.textContent = 'Project save cancelled';
        return false;
      }
      projectName = selected.projectName;
      overwrite = Boolean(selected.overwrite);
    } else if (options.overwrite === undefined) {
      overwrite = window.confirm(`Overwrite ${projectName}.trident?`);
      if (!overwrite) {
        connection.textContent = 'Project save cancelled';
        return false;
      }
    }
    connection.textContent = 'Calling /rpc/save-project';
    let payload;
    try {
      payload = await callRpc('/rpc/save-project', { projectName, overwrite });
    } catch (error) {
      if (String(error.message || error) !== 'archive_exists') {
        throw error;
      }
      if (!window.confirm(`${projectName}.trident already exists. Overwrite it?`)) {
        connection.textContent = 'Project save cancelled';
        return false;
      }
      payload = await callRpc('/rpc/save-project', { projectName, overwrite: true });
    }
    connection.textContent = `Saved: ${payload.archive}`;
    return true;
  } catch (error) {
    connection.textContent = error instanceof Error ? error.message : String(error);
    return false;
  }
}

async function openProject(path) {
  try {
    const current = await currentProjectSettings();
    if (current && window.confirm('Save current project before opening another project?')) {
      const saved = await saveProject();
      if (!saved) {
        connection.textContent = 'Open project cancelled';
        return false;
      }
    }
    connection.textContent = 'Calling /rpc/load-project';
    const payload = await callRpc('/rpc/load-project', { path });
    await activateProjectLayout(payload.project);
    connection.textContent = `Project: ${payload.project.path}`;
    closeWindow(openMainWindows.get('loadProject'));
    closeWindow(openMainWindows.get('saveProjectName'));
    return true;
  } catch (error) {
    connection.textContent = error instanceof Error ? error.message : String(error);
    return false;
  }
}

async function closeProject() {
  try {
    const current = await currentProjectSettings();
    if (!current) {
      connection.textContent = 'No project to close';
      return false;
    }
    if (!window.confirm('Save and close current project?')) {
      connection.textContent = 'Close project cancelled';
      return false;
    }
    const saved = await saveProject();
    if (!saved) {
      connection.textContent = 'Close project cancelled';
      return false;
    }
    connection.textContent = 'Calling /rpc/close-project';
    await callRpc('/rpc/close-project');
    closeRestorableWindows(false);
    closeWindow(openMainWindows.get('projectOptions'), { persistLayout: false });
    connection.textContent = 'Project closed';
    return true;
  } catch (error) {
    connection.textContent = error instanceof Error ? error.message : String(error);
    return false;
  }
}

async function currentProjectSettings() {
  try {
    const payload = await callRpc('/rpc/get-project-settings');
    return payload.settings;
  } catch {
    return null;
  }
}

async function restoreCurrentProjectLayout() {
  const settings = await currentProjectSettings();
  if (settings) {
    await activateProjectLayout(settings);
  }
}

async function openProjectArchiveWindow() {
  return openFileSelectWindow({
    key: 'loadProject',
    title: 'Load Project',
    mode: 'open-project',
    initialPath: '',
    onSelect: async (selection) => {
      if (selection?.path) {
        await openProject(selection.path);
      }
    }
  });
}

async function openProjectSaveNameWindow(settings) {
  closeWindow(openMainWindows.get('saveProjectName'));
  return new Promise((resolve) => {
    openFileSelectWindow({
      key: 'saveProjectName',
      title: 'Save Project',
      mode: 'save-project',
      initialPath: settings.path,
      onSelect: (selection) => {
        closeWindow(openMainWindows.get('saveProjectName'));
        resolve(selection || false);
      },
      onCancel: () => {
        closeWindow(openMainWindows.get('saveProjectName'));
        resolve(false);
      }
    }).catch((error) => {
      connection.textContent = error instanceof Error ? error.message : String(error);
      resolve(false);
    });
  });
}

async function openAdditionalSourcesWindow(initialPath) {
  closeWindow(openMainWindows.get('additionalSources'));
  return new Promise((resolve) => {
    openFileSelectWindow({
      key: 'additionalSources',
      title: 'Select Additional Sources',
      mode: 'select-files',
      initialPath,
      onSelect: (selection) => {
        closeWindow(openMainWindows.get('additionalSources'));
        resolve(selection?.files || []);
      },
      onCancel: () => {
        closeWindow(openMainWindows.get('additionalSources'));
        resolve([]);
      }
    }).catch((error) => {
      connection.textContent = error instanceof Error ? error.message : String(error);
      resolve([]);
    });
  });
}

async function openFileSelectWindow({ key, title, mode, initialPath, onSelect, onCancel }) {
  const existing = openMainWindows.get(key);
  if (existing?.isConnected) {
    bringWindowToFront(existing);
    existing.scrollIntoView({ block: 'nearest', inline: 'nearest' });
    return;
  }
  openMainWindows.delete(key);

  const node = template.content.firstElementChild.cloneNode(true);
  node.dataset.windowKey = key;
  node.classList.add('file-select-window');
  node.querySelector('h2').textContent = title;
  const maximize = node.querySelector('.maximize-button');
  const alwaysOnTop = node.querySelector('.always-on-top-button');
  initializeWindowButtons(node, maximize, alwaysOnTop);
  node.querySelector('.close-button').addEventListener('click', () => {
    onCancel?.();
    closeWindow(node);
  });
  maximize.addEventListener('click', () => toggleMaximized(node, maximize));
  alwaysOnTop.addEventListener('click', () => toggleAlwaysOnTop(node, alwaysOnTop));
  node.addEventListener('pointerdown', () => {
    bringWindowToFront(node);
  });
  makeDraggable(node);
  node.style.left = `${32 + nextOffset}px`;
  node.style.top = `${32 + nextOffset}px`;
  bringWindowToFront(node);
  nextOffset = (nextOffset + 28) % 140;
  openMainWindows.set(key, node);

  const content = node.querySelector('.window-content');
  content.innerHTML = '<div class="loading">Loading files...</div>';
  desktop.appendChild(node);

  connection.textContent = 'Calling /rpc/list-filesystem';
  const payload = await callRpc('/rpc/list-filesystem', initialPath ? { path: initialPath } : undefined);
  const selector = new FileSelect({
    payload,
    mode,
    fetchJson: callRpc,
    onStatus: (message) => {
      connection.textContent = message;
    },
    onSelect,
    onCancel: () => {
      onCancel?.();
      closeWindow(node);
    }
  });
  content.replaceChildren(selector.element());
  connection.textContent = 'Ready';
}

async function openFileWindow(tabs) {
  const node = template.content.firstElementChild.cloneNode(true);
  node.classList.add('path-window');
  node.querySelector('h2').textContent = 'Open File';
  const maximize = node.querySelector('.maximize-button');
  const alwaysOnTop = node.querySelector('.always-on-top-button');
  initializeWindowButtons(node, maximize, alwaysOnTop);
  node.querySelector('.close-button').addEventListener('click', () => node.remove());
  maximize.addEventListener('click', () => toggleMaximized(node, maximize));
  alwaysOnTop.addEventListener('click', () => toggleAlwaysOnTop(node, alwaysOnTop));
  node.addEventListener('pointerdown', () => {
    bringWindowToFront(node);
  });
  makeDraggable(node);
  node.style.left = `${32 + nextOffset}px`;
  node.style.top = `${32 + nextOffset}px`;
  bringWindowToFront(node);
  nextOffset = (nextOffset + 28) % 140;
  const content = node.querySelector('.window-content');
  content.innerHTML = '<div class="loading">Loading project files...</div>';
  desktop.appendChild(node);

  try {
    connection.textContent = 'Calling /rpc/list-project-files';
    const payload = await callRpc('/rpc/list-project-files');
    const picker = new OpenFile({
      payload,
      fetchJson: callRpc,
      onStatus: (message) => {
        connection.textContent = message;
      },
      onOpen: (filePayload) => {
        tabs.addPayload(filePayload);
        node.remove();
      }
    });
    content.replaceChildren(picker.element());
    connection.textContent = 'Ready';
  } catch (error) {
    content.replaceChildren(renderError(error));
    connection.textContent = 'RPC error';
  }
}

async function callRpc(endpoint, body = undefined) {
  return callRpcWithBody(endpoint, body);
}

function delay(milliseconds) {
  return new Promise((resolve) => window.setTimeout(resolve, milliseconds));
}

async function callRpcWithBody(endpoint, body = undefined) {
  const options = { method: 'POST' };
  if (body !== undefined) {
    options.headers = { 'Content-Type': 'application/json' };
    options.body = JSON.stringify(body);
  }
  const response = await fetch(endpoint, options);
  if (!response.ok) {
    let detail = '';
    try {
      const payload = await response.json();
      detail = payload.error || payload.message || '';
    } catch {
      detail = await response.text().catch(() => '');
    }
    throw new Error(detail || `RPC failed with HTTP ${response.status}`);
  }
  return response.json();
}

function renderError(error) {
  const node = document.createElement('pre');
  node.className = 'error';
  node.textContent = error instanceof Error ? error.message : String(error);
  return node;
}

function makeDraggable(windowNode) {
  const titlebar = windowNode.querySelector('.window-titlebar');
  let startX = 0;
  let startY = 0;
  let baseX = 0;
  let baseY = 0;
  let dragging = false;

  titlebar.addEventListener('pointerdown', (event) => {
    if (event.target.closest('button')) {
      return;
    }
    if (windowNode.classList.contains('maximized')) {
      return;
    }
    dragging = true;
    startX = event.clientX;
    startY = event.clientY;
    baseX = windowNode.offsetLeft;
    baseY = windowNode.offsetTop;
    titlebar.setPointerCapture(event.pointerId);
  });

  titlebar.addEventListener('pointermove', (event) => {
    if (!dragging) {
      return;
    }
    windowNode.style.left = `${Math.max(0, baseX + event.clientX - startX)}px`;
    windowNode.style.top = `${Math.max(0, baseY + event.clientY - startY)}px`;
  });

  titlebar.addEventListener('pointerup', () => {
    if (dragging) {
      persistWindowLayoutSoon();
    }
    dragging = false;
  });
}

function toggleMaximized(windowNode, button) {
  const maximized = windowNode.classList.toggle('maximized');
  setMaximizeButtonState(button, maximized);

  if (!maximized) {
    windowNode.style.left = `${32 + nextOffset}px`;
    windowNode.style.top = `${32 + nextOffset}px`;
    nextOffset = (nextOffset + 28) % 140;
  }

  bringWindowToFront(windowNode);
  persistWindowLayoutSoon();
}

function toggleAlwaysOnTop(windowNode, button) {
  const enabled = windowNode.classList.toggle('always-on-top');
  setAlwaysOnTopButtonState(button, enabled);
  bringWindowToFront(windowNode);
  persistWindowLayoutSoon();
}

function initializeWindowButtons(windowNode, maximizeButton, alwaysOnTopButton) {
  setMaximizeButtonState(maximizeButton, windowNode.classList.contains('maximized'));
  setAlwaysOnTopButtonState(alwaysOnTopButton, windowNode.classList.contains('always-on-top'));
}

function setMaximizeButtonState(button, maximized) {
  button.innerHTML = maximized ? restoreWindowIcon() : maximizeWindowIcon();
  button.title = maximized ? 'Restore' : 'Maximize';
  button.setAttribute('aria-label', maximized ? 'Restore window' : 'Maximize window');
}

function setAlwaysOnTopButtonState(button, enabled) {
  button.innerHTML = alwaysOnTopIcon();
  button.title = enabled ? 'Disable always on top' : 'Always on top';
  button.setAttribute('aria-label', enabled ? 'Disable always on top' : 'Always on top');
  button.setAttribute('aria-pressed', enabled ? 'true' : 'false');
}

function bringWindowToFront(windowNode) {
  if (!windowNode) {
    return;
  }
  if (windowNode.classList.contains('always-on-top')) {
    windowNode.style.zIndex = String(nextPinnedZIndex++);
  } else {
    windowNode.style.zIndex = String(nextZIndex++);
  }
}

function maximizeWindowIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <rect x="6" y="6" width="12" height="12"/>
    </svg>`;
}

function restoreWindowIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <rect x="8" y="8" width="10" height="10"/>
      <path d="M6 16V6h10"/>
    </svg>`;
}

function alwaysOnTopIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M12 4v10"/>
      <path d="M8 8h8"/>
      <path d="M9 14h6l-3 6-3-6z"/>
    </svg>`;
}

async function activateProjectLayout(project) {
  closeRestorableWindows(false);
  await restoreProjectWindows(project?.windows || []);
  await persistWindowLayoutNow();
}

function closeRestorableWindows(persistLayout = true) {
  suppressLayoutSave = suppressLayoutSave || !persistLayout;
  try {
    [...openMainWindows.entries()].forEach(([key, node]) => {
      if (isRestorableWindow(key)) {
        closeWindow(node, { persistLayout });
      }
    });
  } finally {
    if (!persistLayout) {
      suppressLayoutSave = false;
    }
  }
}

async function restoreProjectWindows(windows) {
  for (const layout of windows) {
    if (!layout?.open || !isRestorableWindow(layout.key)) {
      continue;
    }
    await createWindow(layout.key, { layout, restoring: true });
  }
}

function isRestorableWindow(key) {
  return restorableWindows.has(key);
}

function collectWindowLayout() {
  return [...openMainWindows.entries()]
    .filter(([key, node]) => isRestorableWindow(key) && node?.isConnected)
    .map(([key, node]) => ({
      key,
      open: true,
      x: Math.max(0, Math.round(node.offsetLeft)),
      y: Math.max(0, Math.round(node.offsetTop)),
      width: Math.max(0, Math.round(node.offsetWidth)),
      height: Math.max(0, Math.round(node.offsetHeight)),
      maximized: node.classList.contains('maximized'),
      alwaysOnTop: node.classList.contains('always-on-top')
    }));
}

function persistWindowLayoutSoon() {
  if (suppressLayoutSave) {
    return;
  }
  window.clearTimeout(layoutSaveTimer);
  layoutSaveTimer = window.setTimeout(() => {
    persistWindowLayoutNow();
  }, 250);
}

async function persistWindowLayoutNow() {
  if (suppressLayoutSave) {
    return false;
  }
  window.clearTimeout(layoutSaveTimer);
  layoutSaveTimer = 0;
  try {
    await callRpc('/rpc/save-window-layout', { windows: collectWindowLayout() });
    return true;
  } catch {
    return false;
  }
}

function newProjectIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M3 6h7l2 2h9v11H3V6z"/>
      <path d="M12 13h6"/>
      <path d="M15 10v6"/>
    </svg>`;
}

function loadProjectIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M3 7h7l2 2h9v10H3V7z"/>
      <path d="M12 14h6"/>
      <path d="M15 11l3 3-3 3"/>
    </svg>`;
}

function saveProjectIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M5 3h12l2 2v16H5V3z"/>
      <path d="M8 3v6h8V3"/>
      <path d="M8 15h8v6H8v-6z"/>
    </svg>`;
}

function closeProjectIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M3 7h7l2 2h9v10H3V7z"/>
      <path d="M9 12l6 6"/>
      <path d="M15 12l-6 6"/>
    </svg>`;
}

function projectSettingsIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M12 8.5a3.5 3.5 0 1 0 0 7 3.5 3.5 0 0 0 0-7z"/>
      <path d="M19 12a7 7 0 0 0-.1-1.1l2-1.5-2-3.4-2.4 1a7 7 0 0 0-1.9-1.1L14.3 3h-4.6l-.3 2.9A7 7 0 0 0 7.5 7L5.1 6l-2 3.4 2 1.5A7 7 0 0 0 5 12c0 .4 0 .8.1 1.1l-2 1.5 2 3.4 2.4-1a7 7 0 0 0 1.9 1.1l.3 2.9h4.6l.3-2.9a7 7 0 0 0 1.9-1.1l2.4 1 2-3.4-2-1.5c.1-.3.1-.7.1-1.1z"/>
    </svg>`;
}

function filesystemIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M4 5h16v14H4V5z"/>
      <path d="M8 9h8"/>
      <path d="M8 12h8"/>
      <path d="M8 15h5"/>
    </svg>`;
}

function classGeneratorIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M5 4h14v16H5V4z"/>
      <path d="M8 8h8"/>
      <path d="M8 12h5"/>
      <path d="M8 16h8"/>
      <path d="M16 11l2 2-2 2"/>
    </svg>`;
}

function developmentIcon() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M4 5h16v14H4V5z"/>
      <path d="M4 10h16"/>
      <path d="M8 15l-2-2 2-2"/>
      <path d="M16 11l2 2-2 2"/>
      <path d="M11 16l2-6"/>
    </svg>`;
}
