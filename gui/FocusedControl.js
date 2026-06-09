export class FocusedControl {
  constructor(options = {}) {
    this.parentControl = options.parentControl || null;
    this.root = null;
    this.selfFocused = false;
    this.focused = false;
    this.focusedChildren = new Set();
  }

  setParentControl(parentControl) {
    this.parentControl = parentControl;
  }

  attachFocusRoot(root) {
    this.root = root;
    root.addEventListener('focusin', () => this.setSelfFocused(true));
    root.addEventListener('focusout', () => {
      setTimeout(() => {
        if (!root.contains(document.activeElement)) {
          this.setSelfFocused(false);
        }
      }, 0);
    });
  }

  setSelfFocused(focused) {
    this.selfFocused = focused;
    this.recomputeFocused();
  }

  childFocusChanged(child, focused) {
    if (focused) {
      this.focusedChildren.add(child);
    } else {
      this.focusedChildren.delete(child);
    }
    this.recomputeFocused();
  }

  focusedChild() {
    return this.focusedChildren.values().next().value || null;
  }

  recomputeFocused() {
    const nextFocused = this.selfFocused || this.focusedChildren.size > 0;
    if (nextFocused === this.focused) {
      return;
    }

    this.focused = nextFocused;
    this.root?.classList.toggle('focused-control', this.focused);
    this.onFocusChanged(this.focused);
    this.parentControl?.childFocusChanged(this, this.focused);
  }

  onFocusChanged() {
  }
}
