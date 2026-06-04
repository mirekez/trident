const desktop = document.querySelector('#desktop');
const launchButtons = document.querySelector('#launch-buttons');
const template = document.querySelector('#window-template');
const connection = document.querySelector('#connection');

const rpcConfig = {
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
  }
};

let nextOffset = 0;
let nextZIndex = 10;

createRpcButtons();

document.addEventListener('keydown', (event) => {
  if (event.key !== 'Escape') {
    return;
  }
  const windows = [...desktop.querySelectorAll('.internal-window')];
  const topWindow = windows.sort((a, b) => Number(b.style.zIndex || 0) - Number(a.style.zIndex || 0))[0];
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
    button.addEventListener('click', () => createWindow(key));
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

  title.textContent = config.title;
  content.innerHTML = '<div class="loading">Loading RPC response...</div>';
  node.style.left = `${32 + nextOffset}px`;
  node.style.top = `${32 + nextOffset}px`;
  node.style.zIndex = String(nextZIndex++);
  nextOffset = (nextOffset + 28) % 140;

  close.addEventListener('click', () => node.remove());
  node.addEventListener('pointerdown', () => {
    node.style.zIndex = String(nextZIndex++);
  });
  makeDraggable(node);
  desktop.appendChild(node);

  try {
    connection.textContent = `Calling ${config.endpoint}`;
    const response = await fetch(config.endpoint, { method: 'POST' });
    if (!response.ok) {
      throw new Error(`RPC failed with HTTP ${response.status}`);
    }
    const payload = await response.json();
    content.replaceChildren(config.render(payload));
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
