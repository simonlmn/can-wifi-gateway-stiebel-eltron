
export function createElement(name, attributes, innerHTML) {
    let e = document.createElement(name);
    Object.assign(e, attributes);
    if (innerHTML) {
        e.innerHTML = innerHTML;
    }
    return e;
}

export class View {
    constructor(element) {
        this._element = element;
    }

    get id() {
        if (!this._element.id) {
            this._element.id = `_generated-${Math.floor(Math.random() * 2 ** 32)}`;
        }
        return this._element.id;
    }

    clear() {
        while (this._element.firstChild) {
            this._element.removeChild(this._element.firstChild);
        }
    }

    remove() {
        this._element.remove();
    }

    addElement(name, attributes, innerHTML) {
        if (innerHTML instanceof View) {
            const child = this._element.appendChild(createElement(name, attributes));
            child.appendChild(innerHTML._element);
            return child;
        } else {
            return this._element.appendChild(createElement(name, attributes, innerHTML));
        }
    }

    attribute(name, value) {
        if (value !== undefined && value !== null) {
            this._element.setAttribute(name, value);
        } else {
            this._element.removeAttribute(name);
        }
    }

    set content(content) {
        this._element.innerHTML = content;
    }

    get content() {
        return this._element.innerHTML;
    }

    set textContent(content) {
        this._element.innerText = content;
    }

    get textContent() {
        return this._element.innerText;
    }

    addView(view) {
        this._element.appendChild(view._element);
        return view;
    }

    scrollToBottom() {
        this._element.scrollTo(0, this._element.scrollHeight);
    }

    show() {
        this._element.style.display = '';
    }

    hide() {
        this._element.style.display = 'none';
    }
}

export class ContainerView extends View {
    constructor(element) {
        super(element);
    }

    h1(content, attributes) {
        return this.addView(new View(createElement('h1', attributes, content)));
    }

    h2(content, attributes) {
        return this.addView(new View(createElement('h2', attributes, content)));
    }

    link(content, attributes) {
        return this.addView(new View(createElement('a', attributes, content)));
    }

    p(content, attributes) {
        return this.addView(new View(createElement('p', attributes, content)));
    }

    dl(attributes) {
        return this.addView(new DefinitionListView(attributes));
    }

    table(attributes) {
        return this.addView(new TableView(attributes));
    }

    pre(content, attributes) {
        return this.addView(new View(createElement('pre', attributes, content)));
    }

    checkbox(label, attributes, callback) {
        return this.addView(new CheckboxView(label, attributes, callback));
    }

    select(label, options, attributes, callback) {
        return this.addView(new SelectView(label, options, attributes, callback));
    }

    number(label, attributes, callback) {
        return this.addView(new NumberView(label, attributes, callback));
    }

    text(label, attributes, callback) {
        return this.addView(new TextView(label, attributes, callback));
    }

    textarea(label, attributes, callback) {
        return this.addView(new TextAreaView(label, attributes, callback));
    }

    file(label, attributes, callback) {
        return this.addView(new FileView(label, attributes, callback));
    }

    label(labelText, forView, attributes) {
        return this.addView(new LabelView(labelText, forView, attributes));
    }

    button(text, attributes, callback) {
        return this.addView(new ButtonView(text, attributes, callback));
    }

    fieldset(legend, attributes) {
        return this.addView(new FieldsetView(legend, attributes));
    }

    section(title, attributes) {
        const view = this.addView(new ContainerView(createElement('section', attributes)));
        view.h2(title);
        return view;
    }

    block(attributes) {
        return this.addView(new ContainerView(createElement('div', attributes)));
    }

    progress(attributes) {
        return this.addView(new ProgressView(attributes));
    }

    modal() {
        return this.addView(new ModalView());
    }

    loader() {
        return this.addView(new LoaderView());
    }
}

export class ModalView extends ContainerView {
    constructor() {
        super(createElement('div', { className: 'modal' }));
    }
}

export class LoaderView extends View {
    constructor() {
        super(createElement('div', { className: 'loader centered' }));
    }
}

export class FieldsetView extends ContainerView {
    constructor(legend, attributes) {
        super(createElement('fieldset', attributes));
        if (legend) {
            this._element.appendChild(createElement('legend', {}, legend));
        }
    }

    disable() {
        this._element.disabled = true;
    }

    enable() {
        this._element.disabled = false;
    }

    validate() {
        return this._element.checkValidity();
    }
}

export class ProgressView extends View {
    constructor(attributes) {
        super(createElement('progress', attributes));
    }

    set value(value) {
        this._element.value = value;
    }

    get value() {
        return this._element.value;
    }

    set indeterminate(enable) {
        if (enable) {
            this._element.indeterminate = true;
        } else {
            this._element.indeterminate = false;
        }
    }

    get indeterminate() {
        return this._element.indeterminate;
    }
}

export class LabelView extends View {
    #forView

    constructor(labelText, forView, attributes) {
        super(createElement('label', attributes, labelText));
        this.forView = forView;
    }

    set forView(view) {
        if (view) {
            this._element.htmlFor = view.id;
        } else {
            this._element.htmlFor = '';
        }

        this.#forView = view;
    }

    get forView() {
        return this.#forView;
    }
}

export class CheckboxView extends View {
    constructor(label, attributes, callback) {
        super(createElement('input', attributes));
        this._element.type = 'checkbox';
        if (callback) {
            this._element.onchange = (e) => callback(e.target.value, this);
        }

        if (label) {
            label.forView = this;
        }
    }

    disable() {
        this._element.disabled = true;
    }

    enable() {
        this._element.disabled = false;
    }

    set checked(checked) {
        this._element.checked = checked;
    }

    get checked() {
        return this._element.checked
    }

    validate() {
        return this._element.checkValidity();
    }
}

export class NumberView extends View {
    constructor(label, attributes, callback) {
        super(createElement('input', attributes));
        this._element.type = 'number';
        if (callback) {
            this._element.onchange = (e) => callback(e.target.value, this);
        }

        if (label) {
            label.forView = this;
        }
    }

    disable() {
        this._element.disabled = true;
    }

    enable() {
        this._element.disabled = false;
    }

    set value(value) {
        this._element.value = value;
    }

    get value() {
        return this._element.value
    }

    validate() {
        return this._element.checkValidity();
    }
}

export class TextView extends View {
    constructor(label, attributes, callback) {
        super(createElement('input', attributes));
        this._element.type = 'text';
        if (callback) {
            this._element.onchange = (e) => callback(e.target.value, this);
        }

        if (label) {
            label.forView = this;
        }
    }

    disable() {
        this._element.disabled = true;
    }

    enable() {
        this._element.disabled = false;
    }

    set value(value) {
        this._element.value = value;
    }

    get value() {
        return this._element.value
    }

    validate() {
        return this._element.checkValidity();
    }
}

export class TextAreaView extends View {
  constructor(label, attributes, callback) {
      super(createElement('textarea', attributes));
      if (callback) {
          this._element.onchange = (e) => callback(e.target.value, this);
      }

      if (label) {
          label.forView = this;
      }
  }

  disable() {
      this._element.disabled = true;
  }

  enable() {
      this._element.disabled = false;
  }

  set value(value) {
      this._element.value = value;
  }

  get value() {
      return this._element.value
  }

  validate() {
      return this._element.checkValidity();
  }
}

export class FileView extends View {
    constructor(label, attributes, callback) {
        super(createElement('input', attributes));
        this._element.type = 'file';
        if (callback) {
            this._element.onchange = (e) => callback(e.target.value, this);
        }

        if (label) {
            label.forView = this;
        }
    }

    disable() {
        this._element.disabled = true;
    }

    enable() {
        this._element.disabled = false;
    }

    get files() {
        return this._element.files
    }

    validate() {
        return this._element.checkValidity();
    }
}

export class SelectView extends View {
    #options

    constructor(label, options, attributes, callback) {
        super(createElement('select', attributes));
        if (callback) {
            this._element.onchange = (e) => callback(e.target.value, this);
        }
        this.options = options;
        if (label) {
            label.forView = this;
        }
    }

    disable() {
        this._element.disabled = true;
    }

    enable() {
        this._element.disabled = false;
    }

    set selected(option) {
        if (this.#options.has(option)) {
            this.#options.get(option).selected = true;
        } else {
            this._element.selectedIndex = -1;
        }
    }

    get selected() {
        return this._element.value;
    }

    set options(options) {
        this.clear();
        this.#options = new Map();
        if (!options) {
            return;
        }

        for (let option of options) {
            let optionValue;
            let optionLabel;
            if (option instanceof Object) {
                optionLabel = option.label;
                optionValue = option.value;
            } else if (Array.isArray(option)) {
                [optionValue,optionLabel] = option;
            } else {
                optionValue = optionLabel = option;
            }
            let e = this.addElement('option', { value: optionValue }, optionLabel);
            this.#options.set(optionValue, e);
        }
    }

    validate() {
        return this._element.checkValidity();
    }
}

export class ButtonView extends View {
    constructor(text, attributes, callback) {
        super(createElement('button', attributes, text));
        this._element.type = 'button';
        if (callback) {
            this._element.onclick = () => {
                try {
                    this.disable();
                    callback(this);
                } finally {
                    this.enable();
                }
            }
        }
    }

    disable() {
        this._element.disabled = true;
    }

    enable() {
        this._element.disabled = false;
    }
}

export class DefinitionListView extends View {
    constructor(attributes) {
        super(createElement('dl', attributes));
    }

    add(dt, dd) {
        this.addElement('dt', {}, dt);
        this.addElement('dd', {}, dd);
    }
}

export class TableView extends View {
    constructor(attributes) {
        super(createElement('table', attributes));
    }

    addRow(attributes) {
        return this.addView(new RowView(attributes));
    }
}

export class RowView extends View {
    constructor(attributes) {
        super(createElement('tr', attributes));
    }

    addColumn(content, attributes) {
        this.addElement('td', attributes, content);
    }

    addColumns(contents, attributes) {
        for (let content of contents) {
            this.addElement('td', attributes, content);
        }
    }

    addHeader(content, attributes) {
        this.addElement('th', attributes, content);
    }

    addHeaders(contents, attributes) {
        for (let content of contents) {
            this.addElement('th', attributes, content);
        }
    }
}
