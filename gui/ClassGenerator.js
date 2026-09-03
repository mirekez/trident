const FIELD_TYPES = [
  'int',
  'uint',
  'logic',
  'struct',
  'int array',
  'uint array',
  'logic array',
  'struct array',
  'interface'
];

const IDENTIFIER_RE = /^[A-Za-z_][A-Za-z0-9_]*$/;
const CPP_KEYWORDS = new Set([
  'alignas', 'alignof', 'and', 'asm', 'auto', 'bool', 'break', 'case', 'catch',
  'char', 'class', 'const', 'constexpr', 'continue', 'decltype', 'default',
  'delete', 'do', 'double', 'else', 'enum', 'explicit', 'export', 'extern',
  'false', 'float', 'for', 'friend', 'goto', 'if', 'inline', 'int', 'long',
  'namespace', 'new', 'noexcept', 'not', 'operator', 'private', 'protected',
  'public', 'register', 'return', 'short', 'signed', 'sizeof', 'static',
  'struct', 'switch', 'template', 'this', 'throw', 'true', 'try', 'typedef',
  'typename', 'union', 'unsigned', 'using', 'virtual', 'void', 'volatile',
  'while'
]);
const HIDDEN_PORT_NAMES = new Set(['clk_in', 'reset_in']);

export class ClassGenerator {
  constructor({ payload, fetchJson, onStatus, onGenerated }) {
    this.fetchJson = fetchJson;
    this.onStatus = onStatus || (() => {});
    this.onGenerated = onGenerated || (() => {});
    this.state = this.normalizePayload(payload);
    this.root = null;
    this.message = null;
  }

  element() {
    this.root = document.createElement('div');
    this.root.className = 'class-generator';
    this.render();
    return this.root;
  }

  normalizePayload(payload = {}) {
    return {
      className: payload.className || 'NewModule',
      ports: Array.isArray(payload.ports) ? payload.ports.map((field) => this.normalizeField(field)) : [],
      members: Array.isArray(payload.members) ? payload.members.map((field) => this.normalizeField(field, true)) : [],
      generateTestHarness: Boolean(payload.generateTestHarness),
      generateVerilatorTestHarness: Boolean(payload.generateVerilatorTestHarness),
      generateNegedgeWork: Boolean(payload.generateNegedgeWork)
    };
  }

  normalizeField(field = {}, member = false) {
    return {
      name: field.name || '',
      type: FIELD_TYPES.includes(field.type) ? field.type : 'logic',
      width: Number.isFinite(Number(field.width)) ? Math.max(1, Number(field.width)) : 32,
      size: Number.isFinite(Number(field.size)) ? Math.max(1, Number(field.size)) : 4,
      packedArray: Boolean(field.packedArray),
      isRegister: member ? Boolean(field.isRegister) : false,
      combinational: member ? Boolean(field.combinational) : false
    };
  }

  render() {
    this.root.replaceChildren(
      this.renderHeader(),
      this.renderClassName(),
      this.renderFieldSection('Ports List', 'ports', false),
      this.renderFieldSection('Members', 'members', true),
      this.renderOptions(),
      this.renderActions()
    );
  }

  renderHeader() {
    const header = document.createElement('div');
    header.className = 'class-generator-header';
    header.textContent = 'cpphdl module class';
    return header;
  }

  renderClassName() {
    const row = document.createElement('label');
    row.className = 'class-generator-class-row';
    const label = document.createElement('span');
    label.textContent = 'Class Name';
    const input = document.createElement('input');
    input.type = 'text';
    input.value = this.state.className;
    input.addEventListener('input', () => {
      this.state.className = input.value.trim();
      input.classList.toggle('invalid', !isClassName(this.state.className));
    });
    input.classList.toggle('invalid', !isClassName(this.state.className));
    row.append(label, input);
    return row;
  }

  renderFieldSection(title, key, member) {
    const section = document.createElement('section');
    section.className = 'class-generator-section';

    const toolbar = document.createElement('div');
    toolbar.className = 'class-generator-section-title';
    const titleNode = document.createElement('strong');
    titleNode.textContent = title;
    const add = document.createElement('button');
    add.type = 'button';
    add.textContent = member ? 'Add member' : 'Add port';
    add.addEventListener('click', () => {
      this.state[key].push(this.normalizeField({
        name: member ? 'value' : 'value_in',
        type: 'logic',
        width: 1,
        size: 4
      }, member));
      this.render();
    });
    toolbar.append(titleNode, add);

    const table = document.createElement('table');
    table.className = `class-generator-table ${member ? 'members' : 'ports'}`;
    const head = document.createElement('thead');
    head.innerHTML = member
      ? '<tr><th>Name</th><th>Type</th><th>Register</th><th>Comb</th><th>Width</th><th>Array size</th><th>Packed</th><th></th></tr>'
      : '<tr><th>Name</th><th>Type</th><th>Width</th><th>Array size</th><th>Packed</th><th></th></tr>';
    const body = document.createElement('tbody');
    this.state[key].forEach((field, index) => {
      body.append(this.renderFieldRow(field, key, index, member));
    });
    table.append(head, body);
    section.append(toolbar, table);
    return section;
  }

  renderFieldRow(field, key, index, member) {
    const row = document.createElement('tr');
    const arrayType = isArrayType(field.type);

    const nameCell = document.createElement('td');
    const name = document.createElement('input');
    name.type = 'text';
    name.value = field.name;
    name.addEventListener('input', () => {
      field.name = name.value.trim();
      name.classList.toggle('invalid', !this.validFieldName(field, member));
    });
    name.classList.toggle('invalid', !this.validFieldName(field, member));
    nameCell.append(name);
    row.append(nameCell);

    const typeCell = document.createElement('td');
    const type = document.createElement('select');
    FIELD_TYPES.forEach((fieldType) => {
      const option = document.createElement('option');
      option.value = fieldType;
      option.textContent = fieldType;
      option.selected = field.type === fieldType;
      type.append(option);
    });
    type.addEventListener('change', () => {
      field.type = type.value;
      if (!isArrayType(field.type)) {
        field.packedArray = false;
      }
      this.render();
    });
    typeCell.append(type);
    row.append(typeCell);

    if (member) {
      row.append(this.checkboxCell(field, 'isRegister', () => {
        if (field.isRegister) {
          field.combinational = false;
        }
        this.render();
      }));
      row.append(this.checkboxCell(field, 'combinational', () => {
        if (field.combinational) {
          field.isRegister = false;
        }
        this.render();
      }));
    }

    row.append(this.numberCell(field, 'width', !usesWidth(field.type)));
    row.append(this.numberCell(field, 'size', !arrayType));
    row.append(this.checkboxCell(field, 'packedArray', () => {}, !arrayType));

    const actionCell = document.createElement('td');
    const remove = document.createElement('button');
    remove.type = 'button';
    remove.className = 'class-generator-row-button';
    remove.textContent = 'X';
    remove.title = 'Remove';
    remove.addEventListener('click', () => {
      this.state[key].splice(index, 1);
      this.render();
    });
    actionCell.append(remove);
    row.append(actionCell);
    return row;
  }

  checkboxCell(field, key, afterChange = () => {}, disabled = false) {
    const cell = document.createElement('td');
    const input = document.createElement('input');
    input.type = 'checkbox';
    input.checked = Boolean(field[key]);
    input.disabled = disabled;
    input.addEventListener('change', () => {
      field[key] = input.checked;
      afterChange();
    });
    cell.append(input);
    return cell;
  }

  numberCell(field, key, disabled) {
    const cell = document.createElement('td');
    const input = document.createElement('input');
    input.type = 'number';
    input.min = '1';
    input.value = String(field[key]);
    input.disabled = disabled;
    input.addEventListener('input', () => {
      field[key] = Math.max(1, Number(input.value) || 1);
    });
    cell.append(input);
    return cell;
  }

  renderOptions() {
    const options = document.createElement('section');
    options.className = 'class-generator-options';
    options.append(
      this.optionCheckbox('Generate test harness', 'generateTestHarness'),
      this.optionCheckbox('Generate verilator test harness', 'generateVerilatorTestHarness'),
      this.optionCheckbox('Generate negedge clock work function', 'generateNegedgeWork')
    );
    return options;
  }

  optionCheckbox(labelText, key) {
    const label = document.createElement('label');
    const input = document.createElement('input');
    input.type = 'checkbox';
    input.checked = Boolean(this.state[key]);
    input.addEventListener('change', () => {
      this.state[key] = input.checked;
    });
    const text = document.createElement('span');
    text.textContent = labelText;
    label.append(input, text);
    return label;
  }

  renderActions() {
    const actions = document.createElement('div');
    actions.className = 'class-generator-actions';
    this.message = document.createElement('div');
    this.message.className = 'class-generator-message';
    const generate = document.createElement('button');
    generate.type = 'button';
    generate.textContent = 'Generate';
    generate.addEventListener('click', async () => {
      await this.generate(false);
    });
    actions.append(this.message, generate);
    return actions;
  }

  validFieldName(field, member) {
    if (!isIdentifier(field.name)) {
      return false;
    }
    if (!member) {
      return !HIDDEN_PORT_NAMES.has(field.name) && (field.name.endsWith('_in') || field.name.endsWith('_out'));
    }
    return !field.combinational || field.name.endsWith('_comb');
  }

  validate() {
    if (!isClassName(this.state.className)) {
      return 'Class name must be a valid C++ class identifier';
    }
    for (const port of this.state.ports) {
      if (!this.validFieldName(port, false)) {
        return 'Each port name must be valid, must end with _in or _out, and cannot be hidden clk_in/reset_in';
      }
    }
    for (const member of this.state.members) {
      if (!this.validFieldName(member, true)) {
        return 'Each member name must be valid; combinational members must end with _comb';
      }
      if (member.isRegister && member.combinational) {
        return 'A member cannot be both register and combinational';
      }
    }
    return '';
  }

  payload(overwrite) {
    return {
      ...this.state,
      ports: this.state.ports.map((field) => this.normalizeField(field)),
      members: this.state.members.map((field) => this.normalizeField(field, true)),
      overwrite
    };
  }

  async generate(overwrite) {
    const error = this.validate();
    if (error) {
      this.setMessage(error, true);
      return;
    }
    try {
      this.setMessage('Generating...');
      this.onStatus('Calling /rpc/generate-class');
      const payload = await this.fetchJson('/rpc/generate-class', this.payload(overwrite));
      this.setMessage(`Generated ${payload.path}`);
      this.onStatus(`Generated: ${payload.path}`);
      this.onGenerated(payload);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      if (!overwrite && message === 'file_exists') {
        if (window.confirm(`${this.state.className}.h already exists. Overwrite it?`)) {
          await this.generate(true);
        } else {
          this.setMessage('Generation cancelled');
        }
        return;
      }
      this.setMessage(message, true);
      this.onStatus(message);
    }
  }

  setMessage(text, error = false) {
    if (!this.message) {
      return;
    }
    this.message.textContent = text;
    this.message.classList.toggle('error', error);
  }
}

function isIdentifier(value) {
  return IDENTIFIER_RE.test(value) && !CPP_KEYWORDS.has(value);
}

function isClassName(value) {
  return isIdentifier(value) && !value.startsWith('_');
}

function isArrayType(type) {
  return type.includes('array');
}

function usesWidth(type) {
  return type === 'int' || type === 'uint' || type === 'logic' ||
    type === 'int array' || type === 'uint array' || type === 'logic array';
}
