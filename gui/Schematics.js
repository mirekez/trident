const SVG_NS = 'http://www.w3.org/2000/svg';

export class Schematics {
  constructor(options = {}) {
    this.payload = options.payload || {};
    this.onStatus = options.onStatus || (() => {});
    this.onLoadStage = options.onLoadStage || null;
    this.activeStage = options.stage || this.payload.stage || 'Compilation';
    this.activeModule = options.module || this.payload.module || '';
    this.root = document.createElement('section');
    this.root.className = 'schematics';

    this.header = document.createElement('div');
    this.header.className = 'schematics-header';

    this.tabs = document.createElement('div');
    this.tabs.className = 'schematics-tabs';

    this.title = document.createElement('div');
    this.title.className = 'schematics-title';

    this.topButton = document.createElement('button');
    this.topButton.type = 'button';
    this.topButton.className = 'schematics-top-button';
    this.topButton.textContent = 'Top';
    this.topButton.title = 'Show top schematic';
    this.topButton.addEventListener('click', () => {
      this.openModule('');
    });

    this.summary = document.createElement('div');
    this.summary.className = 'schematics-summary';

    this.notice = document.createElement('div');
    this.notice.className = 'schematics-notice';
    this.notice.hidden = true;

    this.header.append(this.tabs, this.topButton, this.title, this.summary);

    this.canvas = document.createElement('div');
    this.canvas.className = 'schematics-canvas';

    this.root.append(this.header, this.notice, this.canvas);
    this.render();
  }

  element() {
    return this.root;
  }

  setDesign(payload) {
    this.payload = payload || {};
    this.render();
  }

  async selectStage(stage) {
    if (!this.onLoadStage) {
      return;
    }
    const stageChanged = stage !== this.activeStage;
    this.activeStage = stage;
    if (stageChanged) {
      this.activeModule = '';
    }
    const targetModule = this.activeModule;
    this.renderTabs();
    this.renderTopButton();
    this.canvas.replaceChildren(loadingNode(`Loading ${stage} schematics...`));
    this.onStatus(`Schematics: loading ${stage}`);
    try {
      this.setDesign(await this.onLoadStage(stage, targetModule));
      this.onStatus(`Schematics: ${stage} loaded`);
    } catch (error) {
      this.canvas.replaceChildren(errorNode(error));
      this.onStatus(`Schematics: ${stage} failed`);
    }
  }

  async openModule(moduleName) {
    if (!this.onLoadStage) {
      return;
    }
    const targetModule = moduleName || '';
    if (targetModule === this.activeModule) {
      return;
    }
    this.activeModule = targetModule;
    this.renderTopButton();
    const label = targetModule ? `${this.activeStage} / ${targetModule}` : `${this.activeStage} top`;
    this.canvas.replaceChildren(loadingNode(`Loading ${label}...`));
    this.onStatus(`Schematics: loading ${label}`);
    try {
      this.setDesign(await this.onLoadStage(this.activeStage, targetModule));
      this.onStatus(`Schematics: ${label} loaded`);
    } catch (error) {
      this.canvas.replaceChildren(errorNode(error));
      this.onStatus(`Schematics: ${label} failed`);
    }
  }

  render() {
    const layout = normalizeAllocatedLayout(this.payload) || layoutDesign(normalizeDesign(this.payload));
    const netCount = layout.traces ? new Set(layout.traces.map((trace) => trace.net)).size : layout.connections.length;
    const portCount = layout.modules.reduce((count, module) => count + module.inputs.length + module.outputs.length, 0);
    const memberCount = layout.modules.filter((module) => module.id.startsWith('__member__')).length;
    this.renderTabs();
    this.renderTopButton();
    this.title.textContent = this.activeModule ? `${layout.name || 'RTL Design'} / ${this.activeModule}` : (layout.name || 'RTL Design');
    this.summary.textContent = layout.leaf
      ? `leaf module, ${memberCount} members, ${portCount} ports`
      : `${layout.modules.length} modules, ${netCount} nets`;
    this.notice.hidden = !layout.leaf;
    this.notice.textContent = layout.leaf
      ? `${this.activeModule || layout.name} is open. It has no child module instances in the ${this.activeStage} hierarchy; its data members and ports are shown below.`
      : '';
    this.canvas.replaceChildren(renderSchematicView(layout, {
      onOpenModule: (moduleName) => {
        this.openModule(moduleName);
      },
      onModuleProbe: (module) => {
        const target = module.moduleName ? `drill target ${module.moduleName}` : 'leaf/non-drillable';
        this.onStatus(`Schematics: clicked ${module.title || module.id} (${target})`);
      }
    }));
    this.onStatus(`Schematics: ${this.summary.textContent}`);
  }

  renderTabs() {
    const stages = ['Test', 'Compilation', 'Synthesis'];
    this.tabs.replaceChildren(...stages.map((stage) => {
      const button = document.createElement('button');
      button.type = 'button';
      button.className = `schematics-tab${stage === this.activeStage ? ' active' : ''}`;
      button.textContent = stage;
      button.addEventListener('click', () => {
        this.selectStage(stage);
      });
      return button;
    }));
  }

  renderTopButton() {
    this.topButton.hidden = !this.activeModule;
  }
}

function loadingNode(message) {
  const node = document.createElement('div');
  node.className = 'schematics-message';
  node.textContent = message;
  return node;
}

function errorNode(error) {
  const node = document.createElement('pre');
  node.className = 'schematics-message error';
  node.textContent = error?.message || String(error || 'Schematics load failed');
  return node;
}

function normalizeAllocatedLayout(payload) {
  if (!payload || !Array.isArray(payload.modules) || !Array.isArray(payload.traces)) {
    return null;
  }
  const width = numberOr(payload.width, 900);
  const height = numberOr(payload.height, 560);
  const modules = payload.modules.map((module, index) => {
    const ports = Array.isArray(module.ports) ? module.ports.map((port) => ({
      name: String(port.name || ''),
      direction: String(port.direction || 'input') === 'output' ? 'output' : 'input',
      width: port.width === undefined ? '' : String(port.width),
      x: numberOr(port.x, 0),
      y: numberOr(port.y, 0)
    })).filter((port) => port.name) : [];
    return {
      id: cleanId(module.id || `module_${index + 1}`),
      title: String(module.title || module.id || `module_${index + 1}`),
      moduleName: module.moduleName ? String(module.moduleName) : '',
      inputs: ports.filter((port) => port.direction === 'input'),
      outputs: ports.filter((port) => port.direction === 'output'),
      x: numberOr(module.x, 0),
      y: numberOr(module.y, 0),
      width: numberOr(module.width, 190),
      height: numberOr(module.height, 90),
      portPositions: new Map(ports.map((port) => [port.name, {
        x: port.x,
        y: port.y,
        direction: port.direction
      }]))
    };
  });
  return {
    name: String(payload.name || 'RTL Design'),
    leaf: payload.leaf === true,
    width,
    height,
    modules,
    traces: payload.traces.map(normalizeTrace).filter((trace) => trace.points.length >= 2)
  };
}

function normalizeTrace(trace) {
  return {
    net: String(trace.net || ''),
    points: Array.isArray(trace.points)
      ? trace.points.map((point) => ({ x: numberOr(point.x, 0), y: numberOr(point.y, 0) }))
      : [],
    label: trace.label ? { x: numberOr(trace.label.x, 0), y: numberOr(trace.label.y, 0) } : null,
    to: trace.to || null
  };
}

function numberOr(value, fallback) {
  const number = Number(value);
  return Number.isFinite(number) ? number : fallback;
}

function normalizeDesign(payload) {
  const modules = Array.isArray(payload.modules) ? payload.modules : [];
  const connections = Array.isArray(payload.connections) ? payload.connections : [];
  return {
    name: typeof payload.name === 'string' ? payload.name : 'RTL Design',
    leaf: payload.leaf === true,
    modules: modules.map((module, index) => normalizeModule(module, index)),
    connections: connections.map((connection, index) => normalizeConnection(connection, index))
  };
}

function normalizeModule(module, index) {
  const id = cleanId(module.id || module.name || `module_${index + 1}`);
  const ports = Array.isArray(module.ports) ? module.ports : [];
  return {
    id,
    title: String(module.title || module.name || id),
    ports: ports.map((port, portIndex) => ({
      name: String(port.name || `port_${portIndex + 1}`),
      direction: String(port.direction || 'input').toLowerCase() === 'output' ? 'output' : 'input',
      width: port.width === undefined ? '' : String(port.width)
    }))
  };
}

function normalizeConnection(connection, index) {
  const targets = connection.to || connection.targets || connection.sinks || [];
  return {
    net: String(connection.net || connection.name || `net_${index + 1}`),
    from: normalizeEndpoint(connection.from || connection.source || {}),
    to: (Array.isArray(targets) ? targets : [targets]).map(normalizeEndpoint).filter((endpoint) => endpoint.module && endpoint.port)
  };
}

function normalizeEndpoint(endpoint) {
  return {
    module: cleanId(endpoint.module || endpoint.moduleId || ''),
    port: String(endpoint.port || '')
  };
}

function cleanId(value) {
  return String(value).trim().replace(/\s+/g, '_');
}

function layoutDesign(design) {
  const columns = assignColumns(design.modules, design.connections);
  const modules = design.modules.map((module, index) => createModuleBox(module, columns.get(module.id) || 0, index));
  const grouped = groupByColumn(modules);

  const columnGap = 280;
  const rowGap = 84;
  const margin = 42;
  grouped.forEach((columnModules, column) => {
    let y = margin;
    const maxWidth = Math.max(...columnModules.map((module) => module.width), 180);
    columnModules.forEach((module) => {
      module.x = margin + column * columnGap;
      module.y = y;
      module.width = Math.max(module.width, maxWidth);
      y += module.height + rowGap;
      assignPortPositions(module);
    });
  });

  const moduleBounds = modules.reduce((bounds, module) => ({
    right: Math.max(bounds.right, module.x + module.width),
    bottom: Math.max(bounds.bottom, module.y + module.height)
  }), { right: 0, bottom: 0 });

  return {
    name: design.name,
    leaf: design.leaf,
    modules,
    connections: design.connections,
    width: Math.max(900, moduleBounds.right + margin),
    height: Math.max(560, moduleBounds.bottom + margin)
  };
}

function assignColumns(modules, connections) {
  const columns = new Map(modules.map((module) => [module.id, 0]));
  const moduleIds = new Set(modules.map((module) => module.id));
  const order = new Map(modules.map((module, index) => [module.id, index]));

  for (let pass = 0; pass < modules.length; pass += 1) {
    let changed = false;
    connections.forEach((connection) => {
      const source = connection.from?.module;
      if (!moduleIds.has(source)) {
        return;
      }
      connection.to.forEach((target) => {
        if (!moduleIds.has(target.module)) {
          return;
        }
        if ((order.get(source) || 0) >= (order.get(target.module) || 0)) {
          return;
        }
        const nextColumn = Math.max(columns.get(target.module) || 0, (columns.get(source) || 0) + 1);
        if (nextColumn !== columns.get(target.module)) {
          columns.set(target.module, nextColumn);
          changed = true;
        }
      });
    });
    if (!changed) {
      break;
    }
  }
  return columns;
}

function groupByColumn(modules) {
  const grouped = [];
  modules.forEach((module) => {
    if (!grouped[module.column]) {
      grouped[module.column] = [];
    }
    grouped[module.column].push(module);
  });
  return grouped.filter(Boolean);
}

function createModuleBox(module, column, index) {
  const inputs = module.ports.filter((port) => port.direction === 'input');
  const outputs = module.ports.filter((port) => port.direction === 'output');
  const longestPort = module.ports.reduce((length, port) => Math.max(length, port.name.length + String(port.width || '').length), 0);
  const rows = Math.max(inputs.length, outputs.length, 2);
  const titleWidth = module.title.length * 8 + 44;
  const portWidth = Math.max(160, longestPort * 7 + 86);
  return {
    ...module,
    index,
    column,
    inputs,
    outputs,
    x: 0,
    y: 0,
    width: Math.max(190, titleWidth, portWidth),
    height: 54 + rows * 24,
    portPositions: new Map()
  };
}

function assignPortPositions(module) {
  const headerHeight = 34;
  const rowHeight = 24;
  module.inputs.forEach((port, index) => {
    module.portPositions.set(port.name, {
      x: module.x,
      y: module.y + headerHeight + 16 + index * rowHeight,
      direction: 'input'
    });
  });
  module.outputs.forEach((port, index) => {
    module.portPositions.set(port.name, {
      x: module.x + module.width,
      y: module.y + headerHeight + 16 + index * rowHeight,
      direction: 'output'
    });
  });
}

function renderSchematicView(layout, options = {}) {
  const view = document.createElement('div');
  view.className = 'schematics-view';
  view.style.width = `${layout.width}px`;
  view.style.height = `${layout.height}px`;
  view.append(renderSvg(layout, options));

  layout.modules.forEach((module) => {
    const overlay = document.createElement('button');
    overlay.type = 'button';
    overlay.className = `schematics-module-overlay${module.moduleName ? ' drillable' : ' leaf'}`;
    overlay.title = module.moduleName
      ? `Open ${module.moduleName} internals`
      : `${module.title || module.id} has no drill target`;
    overlay.setAttribute('aria-label', overlay.title);
    overlay.style.left = `${module.x}px`;
    overlay.style.top = `${module.y}px`;
    overlay.style.width = `${module.width}px`;
    overlay.style.height = `${module.height}px`;
    const openModule = (event) => {
      event.preventDefault();
      event.stopPropagation();
      options.onModuleProbe?.(module);
      console.info('Schematics module double-click', {
        id: module.id,
        title: module.title,
        moduleName: module.moduleName
      });
      if (!module.moduleName) {
        window.alert(`${module.title || module.id} is a leaf/non-drillable module.\nNo moduleName was returned by the backend.`);
        return;
      }
      options.onOpenModule?.(module.moduleName);
    };
    overlay.addEventListener('click', (event) => {
      options.onModuleProbe?.(module);
      console.info('Schematics module click', {
        id: module.id,
        title: module.title,
        moduleName: module.moduleName,
        detail: event.detail
      });
      if (event.detail >= 2) {
        openModule(event);
      }
    });
    overlay.addEventListener('dblclick', openModule);
    overlay.addEventListener('keydown', (event) => {
      if (event.key === 'Enter') {
        openModule(event);
      }
    });
    view.append(overlay);
  });

  return view;
}

function renderSvg(layout, options = {}) {
  const svg = svgEl('svg', {
    class: 'schematics-svg',
    viewBox: `0 0 ${layout.width} ${layout.height}`,
    width: layout.width,
    height: layout.height,
    role: 'img',
    'aria-label': `${layout.name} schematics`
  });

  svg.append(svgEl('rect', {
    x: 0,
    y: 0,
    width: layout.width,
    height: layout.height,
    class: 'schematics-background'
  }));

  const modulesById = new Map(layout.modules.map((module) => [module.id, module]));
  if (layout.traces) {
    renderTraces(svg, layout.traces);
  } else {
    renderConnections(svg, layout.connections, modulesById);
  }
  layout.modules.forEach((module) => renderModule(svg, module, options));
  return svg;
}

function renderTraces(svg, traces) {
  const labeledNets = new Set();
  traces.forEach((trace) => {
    svg.append(svgEl('path', {
      d: pathToString(trace.points),
      class: 'schematics-net'
    }));
    const endPoint = trace.points[trace.points.length - 1];
    svg.append(svgEl('circle', {
      cx: endPoint.x,
      cy: endPoint.y,
      r: 3,
      class: 'schematics-net-dot'
    }));
    if (trace.net && trace.label && !labeledNets.has(trace.net)) {
      labeledNets.add(trace.net);
      svg.append(svgText(trace.net, trace.label.x, trace.label.y, 'schematics-net-label'));
    }
  });
}

function renderConnections(svg, connections, modulesById) {
  connections.forEach((connection, netIndex) => {
    const source = modulesById.get(connection.from.module);
    const sourcePoint = source?.portPositions.get(connection.from.port);
    if (!sourcePoint) {
      return;
    }

    connection.to.forEach((target, targetIndex) => {
      const targetModule = modulesById.get(target.module);
      const targetPoint = targetModule?.portPositions.get(target.port);
      if (!targetPoint) {
        return;
      }
      const pathPoints = routeConnection(sourcePoint, targetPoint, netIndex + targetIndex);
      svg.append(svgEl('path', {
        d: pathToString(pathPoints),
        class: 'schematics-net'
      }));
      svg.append(svgEl('circle', {
        cx: targetPoint.x,
        cy: targetPoint.y,
        r: 3,
        class: 'schematics-net-dot'
      }));
      if (targetIndex === 0) {
        const labelPoint = pathPoints[Math.min(2, pathPoints.length - 1)];
        svg.append(svgText(connection.net, labelPoint.x + 8, labelPoint.y - 6, 'schematics-net-label'));
      }
    });
  });
}

function routeConnection(sourcePoint, targetPoint, index) {
  if (targetPoint.x < sourcePoint.x) {
    const laneY = Math.max(sourcePoint.y, targetPoint.y) + 86 + index * 10;
    const sourceLaneX = sourcePoint.x + 22 + index * 4;
    const targetLaneX = targetPoint.x - 22 - index * 4;
    return [
      { x: sourcePoint.x, y: sourcePoint.y },
      { x: sourceLaneX, y: sourcePoint.y },
      { x: sourceLaneX, y: laneY },
      { x: targetLaneX, y: laneY },
      { x: targetLaneX, y: targetPoint.y },
      { x: targetPoint.x, y: targetPoint.y }
    ];
  }

  const laneOffset = ((index % 7) - 3) * 10;
  let midX = Math.round((sourcePoint.x + targetPoint.x) / 2) + laneOffset;
  if (targetPoint.x < sourcePoint.x + 36) {
    midX = Math.max(sourcePoint.x, targetPoint.x) + 86 + index * 8;
  }
  return [
    { x: sourcePoint.x, y: sourcePoint.y },
    { x: sourcePoint.x + 22, y: sourcePoint.y },
    { x: midX, y: sourcePoint.y },
    { x: midX, y: targetPoint.y },
    { x: targetPoint.x - 22, y: targetPoint.y },
    { x: targetPoint.x, y: targetPoint.y }
  ];
}

function pathToString(points) {
  return points.map((point, index) => `${index === 0 ? 'M' : 'L'} ${point.x} ${point.y}`).join(' ');
}

function renderModule(svg, module, options = {}) {
  const group = svgEl('g', {
    class: 'schematics-module-group'
  });
  if (module.moduleName) {
    group.dataset.moduleName = module.moduleName;
    group.setAttribute('tabindex', '0');
    group.setAttribute('role', 'button');
    group.setAttribute('aria-label', `Open ${module.moduleName} internals`);
    group.addEventListener('keydown', (event) => {
      if (event.key === 'Enter') {
        event.preventDefault();
        event.stopPropagation();
        options.onOpenModule?.(module.moduleName);
      }
    });
  }

  group.append(svgEl('rect', {
    x: module.x,
    y: module.y,
    width: module.width,
    height: module.height,
    rx: 4,
    class: 'schematics-module'
  }));
  group.append(svgEl('rect', {
    x: module.x,
    y: module.y,
    width: module.width,
    height: 34,
    rx: 4,
    class: 'schematics-module-titlebar'
  }));
  group.append(svgEl('rect', {
    x: module.x,
    y: module.y + 28,
    width: module.width,
    height: 8,
    class: 'schematics-module-titlebar-fill'
  }));
  group.append(svgText(module.title, module.x + module.width / 2, module.y + 22, 'schematics-module-title', 'middle'));

  module.inputs.forEach((port) => renderPort(group, module, port, 'input'));
  module.outputs.forEach((port) => renderPort(group, module, port, 'output'));

  svg.append(group);
}

function renderPort(svg, module, port, direction) {
  const point = module.portPositions.get(port.name);
  if (!point) {
    return;
  }
  const text = port.width ? `${port.name}[${port.width}]` : port.name;
  if (direction === 'input') {
    svg.append(svgEl('line', {
      x1: point.x,
      y1: point.y,
      x2: point.x + 10,
      y2: point.y,
      class: 'schematics-port-stub'
    }));
    svg.append(svgText(text, module.x + 14, point.y + 4, 'schematics-port-label', 'start'));
  } else {
    svg.append(svgEl('line', {
      x1: point.x - 10,
      y1: point.y,
      x2: point.x,
      y2: point.y,
      class: 'schematics-port-stub'
    }));
    svg.append(svgText(text, module.x + module.width - 14, point.y + 4, 'schematics-port-label', 'end'));
  }
}

function svgEl(name, attributes = {}) {
  const node = document.createElementNS(SVG_NS, name);
  Object.entries(attributes).forEach(([key, value]) => {
    node.setAttribute(key, String(value));
  });
  return node;
}

function svgText(text, x, y, className, anchor = 'start') {
  const node = svgEl('text', {
    x,
    y,
    class: className,
    'text-anchor': anchor
  });
  node.textContent = text;
  return node;
}
