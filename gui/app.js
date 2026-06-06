import { EditorTabs } from './EditorTabs.js';
import { Development } from './Development.js';
import { PathSelector } from './PathSelector.js';
import { OpenFile } from './OpenFile.js';

const desktop = document.querySelector('#desktop');
const launchButtons = document.querySelector('#launch-buttons');
const template = document.querySelector('#window-template');
const connection = document.querySelector('#connection');
const splitter = document.querySelector('#splitter');

const rpcConfig = {
  createProject: {
    title: 'Create Project',
    buttonLabel: 'Create project',
    endpoint: '/rpc/list-folders',
    windowClass: 'path-window',
    render: (payload, node) => renderPathSelector(payload, node, 'create')
  },
  loadProject: {
    title: 'Load Project',
    buttonLabel: 'Load project',
    endpoint: '/rpc/list-folders',
    windowClass: 'path-window',
    render: (payload, node) => renderPathSelector(payload, node, 'load')
  },
  saveProject: {
    buttonLabel: 'Save project',
    action: saveProject
  },
  status: {
    title: 'Backend Status',
    buttonLabel: 'Open Status RPC',
    endpoint: '/rpc/status',
    render: renderStatus
  },
  metrics: {
    title: 'Demo Metrics',
    buttonLabel: 'Open Metrics RPC',
    endpoint: '/rpc/metrics',
    render: renderMetrics
  },
  picture: {
    title: 'Generated Picture',
    buttonLabel: 'Open Picture RPC',
    endpoint: '/rpc/picture',
    render: renderPicture
  },
  loadFile: {
    title: 'Code Editor',
    buttonLabel: 'Open Code Editor',
    endpoint: '/rpc/load-file',
    windowClass: 'editor-window',
    render: (payload) => renderEditorTabs(payload, '/rpc/load-file')
  },
  development: {
    title: 'Development',
    buttonLabel: 'Open Development',
    endpoint: '/rpc/get-opened-file-list',
    windowClass: 'development-window',
    render: renderDevelopment
  }
};

let nextOffset = 0;
let nextZIndex = 10;
let activeDevelopment = null;

createRpcButtons();
enableSplitter();

document.addEventListener('keydown', (event) => {
  if (event.key !== 'Escape') {
    return;
  }
  const windows = [...desktop.querySelectorAll('.internal-window')];
  const topWindow = windows.sort((a, b) => Number(b.style.zIndex || 0) - Number(a.style.zIndex || 0))[0];
  topWindow?._cleanup?.();
  topWindow?.remove();
});

function createRpcButtons() {
  launchButtons.replaceChildren();
  Object.entries(rpcConfig).forEach(([key, config]) => {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'rpc-launch-button';
    button.dataset.window = key;
    button.textContent = config.buttonLabel;
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

async function createWindow(key) {
  const config = rpcConfig[key];
  if (!config) {
    return;
  }

  const node = template.content.firstElementChild.cloneNode(true);
  const title = node.querySelector('h2');
  const content = node.querySelector('.window-content');
  const close = node.querySelector('.close-button');
  const maximize = node.querySelector('.maximize-button');

  if (config.windowClass) {
    node.classList.add(config.windowClass);
  }
  title.textContent = config.title;
  content.innerHTML = '<div class="loading">Loading RPC response...</div>';
  node.style.left = `${32 + nextOffset}px`;
  node.style.top = `${32 + nextOffset}px`;
  node.style.zIndex = String(nextZIndex++);
  nextOffset = (nextOffset + 28) % 140;

  close.addEventListener('click', () => {
    node._cleanup?.();
    node.remove();
  });
  maximize.addEventListener('click', () => toggleMaximized(node, maximize));
  node.addEventListener('pointerdown', () => {
    node.style.zIndex = String(nextZIndex++);
  });
  makeDraggable(node);
  desktop.appendChild(node);

  try {
    connection.textContent = `Calling ${config.endpoint}`;
    const payload = await callRpc(config.endpoint);
    const rendered = config.render(payload, node);
    if (rendered.cleanup) {
      node._cleanup = rendered.cleanup;
    }
    content.replaceChildren(rendered.element || rendered);
    rendered.afterAttach?.();
    connection.textContent = 'Ready';
  } catch (error) {
    content.replaceChildren(renderError(error));
    connection.textContent = 'RPC error';
  }
}

function renderStatus(payload) {
  const root = document.createElement('div');
  root.className = 'stack';
  root.append(
    field('State', payload.state),
    field('Uptime', `${payload.uptimeSeconds} seconds`),
    listField('Services', payload.services || [])
  );
  return root;
}

function renderMetrics(payload) {
  const root = document.createElement('div');
  root.className = 'stack';
  const chart = document.createElement('div');
  chart.className = 'bar-chart';

  const values = payload.latencyMs || [];
  const max = Math.max(...values, 1);
  values.forEach((value) => {
    const bar = document.createElement('span');
    bar.style.height = `${Math.max(12, (value / max) * 100)}%`;
    bar.title = `${value} ms`;
    chart.appendChild(bar);
  });

  root.append(
    field('Requests Today', payload.requestsToday),
    field('Active Users', payload.activeUsers),
    chart
  );
  return root;
}

function renderPicture(payload) {
  const root = document.createElement('div');
  root.className = 'stack';
  const img = document.createElement('img');
  img.className = 'rpc-image';
  img.src = payload.image;
  img.alt = payload.title || 'RPC generated image';
  root.appendChild(img);
  root.appendChild(field('MIME', payload.mime));
  return root;
}

function renderEditorTabs(payload, endpoint) {
  const tabs = new EditorTabs({
    endpoint,
    fetchJson: callRpc,
    onStatus: (message) => {
      connection.textContent = message;
    },
    onOpenFileRequest: (tabs) => openFileWindow(tabs)
  });
  tabs.loadInitial(payload);
  return {
    element: tabs.element(),
    afterAttach: () => tabs.focus()
  };
}

function renderDevelopment(payload) {
  const development = new Development({
    fetchJson: callRpc,
    onStatus: (message) => {
      connection.textContent = message;
    },
    onOpenFileRequest: (tabs) => openFileWindow(tabs)
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
    onProjectCreated: (project) => {
      connection.textContent = `Project: ${project.path}`;
      if (mode === 'load') {
        activeDevelopment?.refreshTabs();
      }
    },
    onDone: () => {
      windowNode.remove();
    }
  });
  return selector.element();
}

async function saveProject() {
  try {
    connection.textContent = 'Checking editors';
    await activeDevelopment?.saveModifiedEditors();
    connection.textContent = 'Calling /rpc/save-project';
    const payload = await callRpc('/rpc/save-project');
    connection.textContent = `Saved: ${payload.project.path}`;
  } catch (error) {
    connection.textContent = error instanceof Error ? error.message : String(error);
  }
}

async function openFileWindow(tabs) {
  const node = template.content.firstElementChild.cloneNode(true);
  node.classList.add('path-window');
  node.querySelector('h2').textContent = 'Open File';
  node.querySelector('.close-button').addEventListener('click', () => node.remove());
  const maximize = node.querySelector('.maximize-button');
  maximize.addEventListener('click', () => toggleMaximized(node, maximize));
  node.addEventListener('pointerdown', () => {
    node.style.zIndex = String(nextZIndex++);
  });
  makeDraggable(node);
  node.style.left = `${32 + nextOffset}px`;
  node.style.top = `${32 + nextOffset}px`;
  node.style.zIndex = String(nextZIndex++);
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

function field(label, value) {
  const row = document.createElement('div');
  row.className = 'field-row';
  const name = document.createElement('span');
  name.textContent = label;
  const data = document.createElement('strong');
  data.textContent = String(value);
  row.append(name, data);
  return row;
}

function listField(label, values) {
  const row = document.createElement('div');
  row.className = 'field-row vertical';
  const name = document.createElement('span');
  name.textContent = label;
  const list = document.createElement('ul');
  values.forEach((value) => {
    const item = document.createElement('li');
    item.textContent = value;
    list.appendChild(item);
  });
  row.append(name, list);
  return row;
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
    dragging = false;
  });
}

function toggleMaximized(windowNode, button) {
  const maximized = windowNode.classList.toggle('maximized');
  button.textContent = maximized ? '<>' : '[]';
  button.title = maximized ? 'Restore' : 'Maximize';
  button.setAttribute('aria-label', maximized ? 'Restore window' : 'Maximize window');

  if (!maximized) {
    windowNode.style.left = `${32 + nextOffset}px`;
    windowNode.style.top = `${32 + nextOffset}px`;
    nextOffset = (nextOffset + 28) % 140;
  }

  windowNode.style.zIndex = String(nextZIndex++);
}

function enableSplitter() {
  let dragging = false;

  splitter.addEventListener('pointerdown', (event) => {
    dragging = true;
    splitter.setPointerCapture(event.pointerId);
    document.body.classList.add('resizing-layout');
  });

  splitter.addEventListener('pointermove', (event) => {
    if (!dragging) {
      return;
    }
    const minLeft = 120;
    const maxLeft = Math.max(minLeft, window.innerWidth - 360);
    const width = Math.min(maxLeft, Math.max(minLeft, event.clientX));
    document.documentElement.style.setProperty('--sidebar-width', `${width}px`);
  });

  splitter.addEventListener('pointerup', () => {
    dragging = false;
    document.body.classList.remove('resizing-layout');
  });
}
