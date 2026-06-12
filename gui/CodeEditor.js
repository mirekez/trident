import { FocusedControl } from './FocusedControl.js';

const KEYWORDS = new Set([
  'alignas', 'alignof', 'always_comb', 'and', 'asm', 'auto', 'begin', 'bool',
  'break', 'case', 'catch', 'char', 'class', 'const', 'constexpr', 'continue',
  'default', 'delete', 'do', 'double', 'else', 'end', 'endmodule', 'enum',
  'explicit', 'export', 'extern', 'false', 'float', 'for', 'friend', 'if',
  'import', 'inline', 'input', 'int', 'logic', 'module', 'mutable', 'namespace',
  'new', 'noexcept', 'not', 'nullptr', 'operator', 'or', 'output', 'override',
  'parameter', 'private', 'protected', 'public', 'register', 'return', 'signed',
  'sizeof', 'static', 'struct', 'switch', 'template', 'this', 'throw', 'true',
  'try', 'typedef', 'typename', 'uint32_t', 'union', 'unique', 'unsigned',
  'using', 'virtual', 'void', 'volatile', 'while'
]);

const TYPES = new Set([
  'int8_t', 'int16_t', 'int32_t', 'int64_t', 'uint8_t', 'uint16_t', 'uint32_t',
  'uint64_t', 'size_t', 'std', 'string', 'array', 'vector'
]);

export class CodeEditor extends FocusedControl {
  constructor(options = {}) {
    super({ parentControl: options.parentControl });
    this.value = options.value || '';
    this.path = options.path || 'untitled';
    this.language = options.language || 'cpp';
    this.onChange = options.onChange || (() => {});
    this.pendingHighlight = 0;

    this.root = document.createElement('section');
    this.root.className = 'code-editor';
    this.attachFocusRoot(this.root);

    this.scroller = document.createElement('div');
    this.scroller.className = 'code-editor-scroller';

    this.highlightViewport = document.createElement('div');
    this.highlightViewport.className = 'code-editor-highlight-viewport';

    this.highlight = document.createElement('pre');
    this.highlight.className = 'code-editor-highlight';
    this.highlight.setAttribute('aria-hidden', 'true');

    this.textarea = document.createElement('textarea');
    this.textarea.className = 'code-editor-input';
    this.textarea.spellcheck = false;
    this.textarea.autocapitalize = 'off';
    this.textarea.autocomplete = 'off';
    this.textarea.value = this.value;
    this.textarea.setAttribute('aria-label', `${this.path} source editor`);

    this.highlightViewport.appendChild(this.highlight);
    this.scroller.append(this.highlightViewport, this.textarea);
    this.root.append(this.scroller);

    this.textarea.addEventListener('input', () => {
      this.value = this.textarea.value;
      this.onChange(this.value);
      this.scheduleHighlight();
    });
    this.textarea.addEventListener('scroll', () => this.syncScroll());
    this.textarea.addEventListener('copy', (event) => this.handleCopy(event));
    this.textarea.addEventListener('cut', (event) => this.handleCut(event));
    this.textarea.addEventListener('paste', (event) => this.handlePaste(event));
    this.textarea.addEventListener('keydown', (event) => this.handleKeydown(event));

    this.renderHighlight();
  }

  element() {
    return this.root;
  }

  focus() {
    this.textarea.focus();
  }

  scheduleHighlight() {
    if (this.pendingHighlight) {
      return;
    }
    this.pendingHighlight = requestAnimationFrame(() => {
      this.pendingHighlight = 0;
      this.renderHighlight();
    });
  }

  renderHighlight() {
    this.highlight.innerHTML = highlightCode(this.textarea.value);
    this.syncScroll();
  }

  syncScroll() {
    this.highlight.style.minHeight = `${this.textarea.scrollHeight}px`;
    this.highlight.style.minWidth = `${this.textarea.scrollWidth}px`;
    this.highlight.style.transform = `translate(${-this.textarea.scrollLeft}px, ${-this.textarea.scrollTop}px)`;
  }

  handleKeydown(event) {
    if (event.key !== 'Tab') {
      return;
    }

    event.preventDefault();
    const start = this.textarea.selectionStart;
    const end = this.textarea.selectionEnd;
    const before = this.textarea.value.slice(0, start);
    const after = this.textarea.value.slice(end);
    this.textarea.value = `${before}    ${after}`;
    this.textarea.selectionStart = this.textarea.selectionEnd = start + 4;
    this.value = this.textarea.value;
    this.onChange(this.value);
    this.scheduleHighlight();
  }

  handleCopy(event) {
    const selected = this.selectedText();
    if (!selected) {
      return;
    }
    event.clipboardData?.setData('text/plain', selected);
    event.preventDefault();
  }

  handleCut(event) {
    const selected = this.selectedText();
    if (!selected) {
      return;
    }
    event.clipboardData?.setData('text/plain', selected);
    event.preventDefault();
    this.replaceSelection('');
  }

  handlePaste(event) {
    const text = event.clipboardData?.getData('text/plain');
    if (text === undefined) {
      return;
    }
    event.preventDefault();
    this.replaceSelection(text);
  }

  selectedText() {
    return this.textarea.value.slice(this.textarea.selectionStart, this.textarea.selectionEnd);
  }

  replaceSelection(text) {
    const start = this.textarea.selectionStart;
    const end = this.textarea.selectionEnd;
    const before = this.textarea.value.slice(0, start);
    const after = this.textarea.value.slice(end);
    this.textarea.value = `${before}${text}${after}`;
    const cursor = start + text.length;
    this.textarea.selectionStart = this.textarea.selectionEnd = cursor;
    this.value = this.textarea.value;
    this.onChange(this.value);
    this.scheduleHighlight();
    this.syncScroll();
  }

  getValue() {
    return this.textarea.value;
  }
}

function highlightCode(source) {
  let html = '';
  let i = 0;

  while (i < source.length) {
    const char = source[i];
    const next = source[i + 1];

    if (char === '/' && next === '/') {
      const end = source.indexOf('\n', i);
      const stop = end === -1 ? source.length : end;
      html += span('comment', source.slice(i, stop));
      i = stop;
      continue;
    }

    if (char === '/' && next === '*') {
      const end = source.indexOf('*/', i + 2);
      const stop = end === -1 ? source.length : end + 2;
      html += span('comment', source.slice(i, stop));
      i = stop;
      continue;
    }

    if (char === '"' || char === '\'' || char === '`') {
      const start = i;
      const quote = char;
      i += 1;
      while (i < source.length) {
        if (source[i] === '\\') {
          i += 2;
          continue;
        }
        if (source[i] === quote) {
          i += 1;
          break;
        }
        i += 1;
      }
      html += span('string', source.slice(start, i));
      continue;
    }

    if (char === '#') {
      const end = source.indexOf('\n', i);
      const stop = end === -1 ? source.length : end;
      html += span('preproc', source.slice(i, stop));
      i = stop;
      continue;
    }

    if (isDigit(char)) {
      const start = i;
      while (i < source.length && /[0-9a-fA-F_xXbBuUlL'.]/.test(source[i])) {
        i += 1;
      }
      html += span('number', source.slice(start, i));
      continue;
    }

    if (isIdentStart(char)) {
      const start = i;
      i += 1;
      while (i < source.length && isIdentPart(source[i])) {
        i += 1;
      }
      const word = source.slice(start, i);
      if (KEYWORDS.has(word)) {
        html += span('keyword', word);
      } else if (TYPES.has(word) || /^[A-Z][A-Za-z0-9_]*$/.test(word)) {
        html += span('type', word);
      } else {
        html += escapeHtml(word);
      }
      continue;
    }

    html += escapeHtml(char);
    i += 1;
  }

  return html.endsWith('\n') ? `${html} ` : html;
}

function span(kind, text) {
  return `<span class="tok-${kind}">${escapeHtml(text)}</span>`;
}

function escapeHtml(text) {
  return text
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;');
}

function isDigit(char) {
  return char >= '0' && char <= '9';
}

function isIdentStart(char) {
  return /[A-Za-z_]/.test(char);
}

function isIdentPart(char) {
  return /[A-Za-z0-9_]/.test(char);
}
