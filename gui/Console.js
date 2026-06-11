import { FocusedControl } from './FocusedControl.js';

const STREAM_ENDPOINT = '/rpc/bash-console-stream';
const INPUT_ENDPOINT = '/rpc/bash-console-input';
const STOP_ENDPOINT = '/rpc/bash-console-stop';

export class Console extends FocusedControl {
  constructor(options = {}) {
    super({ parentControl: options.parentControl });
    this.fetchJson = options.fetchJson || defaultFetchJson;
    this.onStatus = options.onStatus || (() => {});
    this.reader = null;
    this.abort = new AbortController();
    this.controlBuffer = '';
    this.rows = 32;
    this.cols = 120;
    this.screen = Array.from({ length: this.rows }, () => blankLine(this.cols));
    this.cursorRow = 0;
    this.cursorCol = 0;
    this.savedCursor = { row: 0, col: 0 };
    this.handleDocumentClick = () => this.hideContextMenu();
    this.handleDocumentKeydown = (event) => {
      if (event.key === 'Escape') {
        this.hideContextMenu();
      }
    };

    this.root = document.createElement('section');
    this.root.className = 'console interactive-console';
    this.attachFocusRoot(this.root);

    this.output = document.createElement('pre');
    this.output.className = 'console-output';
    this.output.tabIndex = 0;
    this.output.setAttribute('aria-label', 'Interactive terminal');
    this.outputText = document.createTextNode('');
    this.output.appendChild(this.outputText);

    this.menu = this.createContextMenu();
    this.root.appendChild(this.menu);

    this.root.appendChild(this.output);

    this.output.addEventListener('keydown', (event) => this.handleKeydown(event));
    this.output.addEventListener('paste', (event) => this.handlePaste(event));
    this.output.addEventListener('contextmenu', (event) => this.showContextMenu(event));
    this.root.addEventListener('click', (event) => {
      this.hideContextMenu();
      if (!event.target.closest('.console-output')) {
        this.focus();
      }
    });
    document.addEventListener('click', this.handleDocumentClick);
    document.addEventListener('keydown', this.handleDocumentKeydown);

    this.startStream();
  }

  element() {
    return this.root;
  }

  focus() {
    this.output.focus({ preventScroll: true });
  }

  onFocusChanged() {
    this.renderScreen();
  }

  destroy() {
    document.removeEventListener('click', this.handleDocumentClick);
    document.removeEventListener('keydown', this.handleDocumentKeydown);
    this.abort.abort();
    this.fetchJson(STOP_ENDPOINT).catch(() => {});
  }

  async startStream() {
    this.onStatus(`Streaming ${STREAM_ENDPOINT}`);
    try {
      const response = await fetch(STREAM_ENDPOINT, {
        method: 'POST',
        signal: this.abort.signal
      });
      if (!response.ok || !response.body) {
        throw new Error(`Console stream failed with HTTP ${response.status}`);
      }

      this.onStatus('Console attached');
      this.reader = response.body.getReader();
      const decoder = new TextDecoder();
      while (true) {
        const { value, done } = await this.reader.read();
        if (done) {
          break;
        }
        this.append(decoder.decode(value, { stream: true }));
      }
      this.onStatus('Console closed');
    } catch (error) {
      if (error.name !== 'AbortError') {
        this.append(`\n${error instanceof Error ? error.message : String(error)}\n`);
        this.onStatus('Console error');
      }
    }
  }

  async handleKeydown(event) {
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'c' && hasSelectionInside(this.output)) {
      return;
    }
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'v') {
      event.preventDefault();
      await this.pasteFromClipboard();
      return;
    }
    if (event.ctrlKey && event.key.toLowerCase() === 'c') {
      event.preventDefault();
      await this.sendInput('\x03');
      return;
    }
    if (event.ctrlKey && event.key.toLowerCase() === 'd') {
      event.preventDefault();
      await this.sendInput('\x04');
      return;
    }
    if (event.ctrlKey && event.key.length === 1) {
      event.preventDefault();
      await this.sendInput(String.fromCharCode(event.key.toUpperCase().charCodeAt(0) - 64));
      return;
    }
    if (event.key === 'Enter') {
      event.preventDefault();
      await this.sendInput('\r');
      return;
    }
    if (event.key === 'Backspace') {
      event.preventDefault();
      await this.sendInput('\x7f');
      return;
    }
    if (event.key === 'Escape') {
      event.preventDefault();
      await this.sendInput('\x1b');
      return;
    }
    if (event.key === 'Tab') {
      event.preventDefault();
      await this.sendInput('\t');
      return;
    }
    if (event.key === 'ArrowLeft') {
      event.preventDefault();
      await this.sendInput('\x1b[D');
      return;
    }
    if (event.key === 'ArrowRight') {
      event.preventDefault();
      await this.sendInput('\x1b[C');
      return;
    }
    if (event.key === 'ArrowUp') {
      event.preventDefault();
      await this.sendInput('\x1b[A');
      return;
    }
    if (event.key === 'ArrowDown') {
      event.preventDefault();
      await this.sendInput('\x1b[B');
      return;
    }
    if (event.key === 'Home') {
      event.preventDefault();
      await this.sendInput('\x1b[H');
      return;
    }
    if (event.key === 'End') {
      event.preventDefault();
      await this.sendInput('\x1b[F');
      return;
    }
    if (event.key === 'Delete') {
      event.preventDefault();
      await this.sendInput('\x1b[3~');
      return;
    }
    if (event.key.length === 1 && !event.metaKey && !event.altKey) {
      event.preventDefault();
      await this.sendInput(event.key);
    }
  }

  async handlePaste(event) {
    event.preventDefault();
    const text = event.clipboardData?.getData('text/plain') || '';
    if (text) {
      await this.sendInput(text.replace(/\r?\n/g, '\r'));
    }
  }

  createContextMenu() {
    const menu = document.createElement('div');
    menu.className = 'console-context-menu';

    const copy = this.menuButton('Copy', async () => {
      await this.copySelection();
      this.hideContextMenu();
    });
    const paste = this.menuButton('Paste', async () => {
      await this.pasteFromClipboard();
      this.hideContextMenu();
      this.focus();
    });

    menu.append(copy, paste);
    return menu;
  }

  menuButton(label, onClick) {
    const button = document.createElement('button');
    button.type = 'button';
    button.textContent = label;
    button.addEventListener('click', (event) => {
      event.stopPropagation();
      onClick();
    });
    return button;
  }

  showContextMenu(event) {
    event.preventDefault();
    this.focus();
    const bounds = this.root.getBoundingClientRect();
    this.menu.style.left = `${event.clientX - bounds.left}px`;
    this.menu.style.top = `${event.clientY - bounds.top}px`;
    this.menu.classList.add('visible');
  }

  hideContextMenu() {
    this.menu.classList.remove('visible');
  }

  async copySelection() {
    const selection = window.getSelection();
    const text = hasSelectionInside(this.output) ? selection.toString() : this.outputText.data;
    if (!text) {
      return;
    }
    try {
      await navigator.clipboard.writeText(text);
    } catch {
      document.execCommand('copy');
    }
  }

  async pasteFromClipboard() {
    try {
      const text = await navigator.clipboard.readText();
      if (text) {
        await this.sendInput(text.replace(/\r?\n/g, '\r'));
      }
    } catch (error) {
      this.onStatus('Clipboard paste unavailable');
    }
  }

  async sendInput(input) {
    try {
      await this.fetchJson(INPUT_ENDPOINT, { input });
    } catch (error) {
      this.append(`\n${error instanceof Error ? error.message : String(error)}\n`);
      this.onStatus('Console input error');
    }
  }

  append(text) {
    if (!text) {
      return;
    }

    const shouldScroll = isSelectionOutside(this.output) &&
      this.output.scrollTop + this.output.clientHeight >= this.output.scrollHeight - 8;
    this.writeTerminalText(text);
    this.renderScreen();
    if (shouldScroll) {
      this.output.scrollTop = this.output.scrollHeight;
    }
  }

  writeTerminalText(text) {
    const combined = this.controlBuffer + text;
    this.controlBuffer = '';

    for (let i = 0; i < combined.length; i += 1) {
      const ch = combined[i];
      if (ch !== '\x1b') {
        this.writeChar(ch);
        continue;
      }

      const parsed = parseControl(combined, i);
      if (!parsed) {
        this.controlBuffer = combined.slice(i);
        break;
      }
      this.applyControl(parsed);
      i = parsed.end;
    }
  }

  writeChar(ch) {
    if (ch === '\r') {
      this.cursorCol = 0;
      return;
    }
    if (ch === '\b') {
      this.cursorCol = Math.max(0, this.cursorCol - 1);
      return;
    }
    if (ch === '\n') {
      this.cursorRow += 1;
      this.ensureCursor();
      return;
    }
    if (ch === '\t') {
      this.cursorCol = Math.min(this.cols - 1, this.cursorCol + (4 - (this.cursorCol % 4)));
      return;
    }
    if (ch < ' ') {
      return;
    }

    const line = this.screen[this.cursorRow].split('');
    line[this.cursorCol] = ch;
    this.screen[this.cursorRow] = line.join('');
    this.cursorCol += 1;
    if (this.cursorCol >= this.cols) {
      this.cursorCol = 0;
      this.cursorRow += 1;
      this.ensureCursor();
    }
  }

  applyControl(control) {
    if (control.type === 'esc') {
      if (control.code === '7') {
        this.savedCursor = { row: this.cursorRow, col: this.cursorCol };
      } else if (control.code === '8') {
        this.cursorRow = this.savedCursor.row;
        this.cursorCol = this.savedCursor.col;
      }
      return;
    }

    if (control.type !== 'csi') {
      return;
    }

    const command = control.command;
    const params = control.params;
    const n = (index, fallback) => {
      const value = Number(params[index]);
      return Number.isFinite(value) && value > 0 ? value : fallback;
    };

    if (command === 'm' || command === 'h' || command === 'l' || command === 'q' || command === 'r') {
      return;
    }
    if (command === 'A') this.cursorRow = Math.max(0, this.cursorRow - n(0, 1));
    if (command === 'B') this.cursorRow = Math.min(this.rows - 1, this.cursorRow + n(0, 1));
    if (command === 'C') this.cursorCol = Math.min(this.cols - 1, this.cursorCol + n(0, 1));
    if (command === 'D') this.cursorCol = Math.max(0, this.cursorCol - n(0, 1));
    if (command === 'G') this.cursorCol = Math.min(this.cols - 1, n(0, 1) - 1);
    if (command === 'H' || command === 'f') {
      this.cursorRow = Math.min(this.rows - 1, n(0, 1) - 1);
      this.cursorCol = Math.min(this.cols - 1, n(1, 1) - 1);
    }
    if (command === 's') {
      this.savedCursor = { row: this.cursorRow, col: this.cursorCol };
    }
    if (command === 'u') {
      this.cursorRow = this.savedCursor.row;
      this.cursorCol = this.savedCursor.col;
    }
    if (command === 'K') {
      this.clearLine(Number(params[0] || 0));
    }
    if (command === 'J') {
      this.clearScreen(Number(params[0] || 0));
    }
    if (command === 'P') {
      this.deleteChars(n(0, 1));
    }
    if (command === '@') {
      this.insertSpaces(n(0, 1));
    }
  }

  deleteChars(count) {
    const line = this.screen[this.cursorRow].split('');
    line.splice(this.cursorCol, count);
    while (line.length < this.cols) {
      line.push(' ');
    }
    this.screen[this.cursorRow] = line.slice(0, this.cols).join('');
  }

  insertSpaces(count) {
    const line = this.screen[this.cursorRow].split('');
    line.splice(this.cursorCol, 0, ...Array.from({ length: count }, () => ' '));
    this.screen[this.cursorRow] = line.slice(0, this.cols).join('');
  }

  clearLine(mode) {
    const line = this.screen[this.cursorRow].split('');
    const start = mode === 1 ? 0 : this.cursorCol;
    const end = mode === 0 ? this.cols : this.cursorCol + 1;
    for (let i = start; i < end; i += 1) {
      line[i] = ' ';
    }
    this.screen[this.cursorRow] = line.join('');
  }

  clearScreen(mode) {
    if (mode === 2 || mode === 3) {
      this.screen = Array.from({ length: this.rows }, () => blankLine(this.cols));
      this.cursorRow = 0;
      this.cursorCol = 0;
      return;
    }
    const start = mode === 1 ? 0 : this.cursorRow;
    const end = mode === 0 ? this.rows : this.cursorRow + 1;
    for (let row = start; row < end; row += 1) {
      this.screen[row] = blankLine(this.cols);
    }
  }

  ensureCursor() {
    while (this.cursorRow >= this.rows) {
      this.screen.shift();
      this.screen.push(blankLine(this.cols));
      this.cursorRow -= 1;
    }
  }

  renderScreen() {
    if (!isSelectionOutside(this.output)) {
      return;
    }
    const display = this.screen.map((line, row) => {
      if (!this.focused || row !== this.cursorRow) {
        return line;
      }
      const chars = line.split('');
      chars[this.cursorCol] = chars[this.cursorCol] === ' ' ? '█' : '▌';
      return chars.join('');
    });
    this.outputText.data = display.map((line) => line.trimEnd()).join('\n').replace(/\n+$/g, '');
  }
}

async function defaultFetchJson(endpoint, body = {}) {
  const response = await fetch(endpoint, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
  if (!response.ok) {
    throw new Error(`RPC failed with HTTP ${response.status}`);
  }
  return response.json();
}

function isSelectionOutside(node) {
  const selection = window.getSelection();
  if (!selection || selection.rangeCount === 0 || selection.isCollapsed) {
    return true;
  }
  const range = selection.getRangeAt(0);
  return !node.contains(range.commonAncestorContainer);
}

function hasSelectionInside(node) {
  return !isSelectionOutside(node);
}

function blankLine(cols) {
  return ' '.repeat(cols);
}

function parseControl(text, start) {
  const next = text[start + 1];
  if (next === undefined) {
    return null;
  }

  if (next === '[') {
    for (let i = start + 2; i < text.length; i += 1) {
      const code = text.charCodeAt(i);
      if (code >= 0x40 && code <= 0x7e) {
        const raw = text.slice(start + 2, i);
        return {
          type: 'csi',
          params: raw.replace(/^\?/, '').split(';').filter(Boolean),
          command: text[i],
          end: i
        };
      }
    }
    return null;
  }

  if (next === ']') {
    for (let i = start + 2; i < text.length; i += 1) {
      if (text[i] === '\x07') {
        return { type: 'osc', end: i };
      }
      if (text[i] === '\x1b' && text[i + 1] === '\\') {
        return { type: 'osc', end: i + 1 };
      }
    }
    return null;
  }

  if (next === 'P' || next === '^' || next === '_' || next === 'X') {
    for (let i = start + 2; i < text.length; i += 1) {
      if (text[i] === '\x1b' && text[i + 1] === '\\') {
        return { type: 'string', end: i + 1 };
      }
    }
    return null;
  }

  return { type: 'esc', code: next, end: start + 1 };
}
