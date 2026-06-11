export class Log {
  constructor() {
    this.root = document.createElement('div');
    this.root.className = 'development-log';

    this.output = document.createElement('pre');
    this.output.className = 'development-log-output';
    this.output.textContent = '';

    this.root.appendChild(this.output);
  }

  element() {
    return this.root;
  }

  setText(text) {
    this.output.textContent = text || '';
    this.scrollToBottom();
  }

  appendText(text) {
    this.output.textContent += text || '';
    this.scrollToBottom();
  }

  clear() {
    this.setText('');
  }

  text() {
    return this.output.textContent;
  }

  scrollToBottom() {
    requestAnimationFrame(() => {
      this.output.scrollTop = this.output.scrollHeight;
    });
  }
}
