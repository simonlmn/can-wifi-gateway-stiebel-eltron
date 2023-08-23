
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

    content(content) {
        this._element.innerHTML = content;
    }

    addView(view) {
        this._element.appendChild(view._element);
        return view;
    }

    scrollToBottom() {
        this._element.scrollTo(0, this._element.scrollHeight);
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
        const view = this.addView(new ContainerView(attributes));
        view.h2(title);
        return view;
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
        this._element.onchange = (e) => callback(e.target.checked, this);

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
}

export class NumberView extends View {
    constructor(label, attributes, callback) {
        super(createElement('input', attributes));
        this._element.type = 'number';
        this._element.onchange = (e) => callback(e.target.value, this);

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
}

export class SelectView extends View {
    #options

    constructor(label, options, attributes, callback) {
        super(createElement('select', attributes));
        this._element.onchange = (e) => callback(e.target.value, this);
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
        this.#options[option].selected = true;
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
            this.#options[optionValue] = e;
        }
    }
}

export class ButtonView extends View {
    constructor(text, attributes, callback) {
        super(createElement('button', attributes, text));
        this._element.type = 'button';
        this._element.onclick = () => callback(this);
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
