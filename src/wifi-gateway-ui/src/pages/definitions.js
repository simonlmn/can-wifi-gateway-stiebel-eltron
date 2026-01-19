
function formatDescription(description) {
  return description.replaceAll(/\^([-0123456789]+)/g, '<sup>$1</sup>');
}

export class DefinitionsPage {
    #client
    
    #codecs
    #converters
    #customConverters
    #definitions

    #definitionsView
    #addFormView
    #tableView
    #convertersView
    #codecsView
    #editingDefinitionId

    constructor(client) {
        this.#client = client;
        this.#editingDefinitionId = null;
    }

    get label() {
        return 'Definitions';
    }

    async enter(view) {
        await this.#loadCodecs();
        await this.#loadConverters();
        await this.#loadCustomConverters();
        await this.#loadDefinitions();
        
        this.#definitionsView = view.section('Definitions').block();
        this.#addFormView = this.#definitionsView.block();
        this.#tableView = this.#definitionsView.block();
        
        this.#createAddDefinitionForm();
        this.#viewDefinitions();

        this.#convertersView = view.section('Converters').block();
        this.#viewConverters();

        this.#codecsView = view.section('Codecs').block();
        this.#viewCodecs();
    }

    async leave() {
    }

    #createAddDefinitionForm() {
        this.#addFormView.clear();
        const addFieldset = this.#addFormView.fieldset('Add New Definition');
        
        const idInput = addFieldset.number(addFieldset.label('ID'), { required: true, min: 0 });
        const nameInput = addFieldset.text(addFieldset.label('Name'), { required: true, maxlength: 50 });
        const unitInput = addFieldset.text(addFieldset.label('Unit'), { maxlength: 20 });
        const accessSelect = addFieldset.select(addFieldset.label('Access'), ['None', 'Readable', 'Writable']);
        
        const converterOptions = Array.from(this.#converters.values()).map(c => ({ value: c.key, label: `${c.key} - ${c.description}` }));
        const converterSelect = addFieldset.select(addFieldset.label('Converter'), converterOptions);
        
        const codecOptions = Array.from(this.#codecs.values()).map(c => ({ value: c.key, label: `${c.key} - ${c.description}` }));
        const codecSelect = addFieldset.select(addFieldset.label('Codec'), codecOptions);
        
        addFieldset.button('Add', {}, async (button) => {
            if (!idInput.value || !nameInput.value) {
                alert('ID and Name are required');
                return;
            }
            
            if (this.#definitions[idInput.value]) {
                alert(`Definition with ID ${idInput.value} already exists`);
                return;
            }
            
            button.disable();
            const newDefinition = {
                name: nameInput.value,
                unit: unitInput.value || '',
                access: accessSelect.selected,
                converter: converterSelect.selected,
                codec: codecSelect.selected
            };
            
            if (await this.#updateSingleDefinition(idInput.value, newDefinition)) {
                idInput.value = '';
                nameInput.value = '';
                unitInput.value = '';
            }
            button.enable();
        });
    }

    #editDefinitions() {
        this.#tableView.clear();
        const textarea = this.#tableView.textarea(null, { rows: 25 });
        textarea.value = JSON.stringify(this.#definitions, null, 2);
        this.#tableView.button('Save', {}, async (button) => {
            button.disable();
            if (await this.#updateDefinitions(textarea.value)) {
                this.#viewDefinitions();
                return;
            }
            button.enable();
        });
        this.#tableView.button('Discard', {}, () => {
            this.#viewDefinitions();
        });
    }

    #editSingleDefinition(id) {
        this.#editingDefinitionId = id;
        this.#viewDefinitions();
    }

    #viewDefinitions() {
        this.#tableView.clear();
        this.#tableView.button('Bulk Edit (JSON)', {}, () => this.#editDefinitions());
        const table = this.#tableView.table();
        
        table.addRow().addHeaders(['ID', 'Name', 'Unit', 'Access', 'Converter', 'Codec', 'Actions']);
        for (const [id, definition] of Object.entries(this.#definitions)) {
            const codec = this.#codecs.get(definition.codec);
            const converter = this.#converters.get(definition.converter);
            
            if (this.#editingDefinitionId === id) {
                // Show edit form inline
                const row = table.addRow();
                row.addColumn(id);
                
                const nameInput = row.addElement('input');
                nameInput.type = 'text';
                nameInput.value = definition.name;
                
                const unitInput = row.addElement('input');
                unitInput.type = 'text';
                unitInput.value = definition.unit;
                
                const accessSelect = row.addElement('select');
                ['None', 'Readable', 'Writable'].forEach(opt => {
                    const option = document.createElement('option');
                    option.value = opt;
                    option.textContent = opt;
                    option.selected = opt === definition.access;
                    accessSelect.appendChild(option);
                });
                
                const converterSelect = row.addElement('select');
                Array.from(this.#converters.values()).forEach(c => {
                    const option = document.createElement('option');
                    option.value = c.key;
                    option.textContent = c.key;
                    option.selected = c.key === definition.converter;
                    converterSelect.appendChild(option);
                });
                
                const codecSelect = row.addElement('select');
                Array.from(this.#codecs.values()).forEach(c => {
                    const option = document.createElement('option');
                    option.value = c.key;
                    option.textContent = c.key;
                    option.selected = c.key === definition.codec;
                    codecSelect.appendChild(option);
                });
                
                const actionsCell = row.addElement('td');
                const saveBtn = document.createElement('button');
                saveBtn.textContent = 'Save';
                saveBtn.onclick = async () => {
                    saveBtn.disabled = true;
                    const updatedDefinition = {
                        name: nameInput.value,
                        unit: unitInput.value,
                        access: accessSelect.value,
                        converter: converterSelect.value,
                        codec: codecSelect.value
                    };
                    if (await this.#updateSingleDefinition(id, updatedDefinition)) {
                        this.#editingDefinitionId = null;
                    }
                    saveBtn.disabled = false;
                };
                actionsCell.appendChild(saveBtn);
                
                const cancelBtn = document.createElement('button');
                cancelBtn.textContent = 'Cancel';
                cancelBtn.onclick = () => {
                    this.#editingDefinitionId = null;
                    this.#viewDefinitions();
                };
                actionsCell.appendChild(cancelBtn);
            } else {
                // Show normal row
                const row = table.addRow();
                row.addColumns([
                    id,
                    definition.name,
                    definition.unit,
                    definition.access,
                    formatDescription(converter.description),
                    codec.description
                ]);
                
                const actionsCell = row.addElement('td');
                const editBtn = document.createElement('button');
                editBtn.textContent = 'Edit';
                editBtn.onclick = () => this.#editSingleDefinition(id);
                actionsCell.appendChild(editBtn);
                
                const deleteBtn = document.createElement('button');
                deleteBtn.textContent = 'Delete';
                deleteBtn.onclick = async () => {
                    if (confirm(`Delete definition ${id} (${definition.name})?`)) {
                        deleteBtn.disabled = true;
                        await this.#deleteSingleDefinition(id);
                    }
                };
                actionsCell.appendChild(deleteBtn);
            }
        }
    }

    #editConverters() {
        this.#convertersView.clear();
        const textarea = this.#convertersView.textarea(null, { rows: 25 });
        textarea.value = JSON.stringify(this.#customConverters, null, 2);
        this.#convertersView.button('Save', {}, async (button) => {
            button.disable();
            if (await this.#updateConverters(textarea.value)) {
                this.#viewConverters();
                return;
            }
            button.enable();
        });
        this.#convertersView.button('Discard', {}, () => this.#viewConverters());
    }

    #viewConverters() {
        this.#convertersView.clear();
        this.#convertersView.button('Edit', {}, () => this.#editConverters());
        const table =this.#convertersView.table();
        
        table.addRow().addHeaders(['ID', 'Key', 'Description', 'Built-in']);
        for (const [_, converter] of this.#converters) {
            table.addRow().addColumns([converter.id.toString(), converter.key, formatDescription(converter.description), converter.builtIn ? 'Yes' : 'No']);
        }
    }

    #viewCodecs() {
        this.#codecsView.clear();
        const table =this.#codecsView.table();
        
        table.addRow().addHeaders(['Key', 'Description']);
        for (const [_, codec] of this.#codecs) {
            table.addRow().addColumns([codec.key, formatDescription(codec.description)]);
        }
    }

    async #loadCodecs() {
        try {
            const response = await this.#client.get('/codecs');
            this.#codecs = new Map();
            for (const codec of await response.json()) {
               this.#codecs.set(codec.key, codec);
            }
        } catch (err) {
            alert(err);
        }
    }

    async #loadConverters() {
        try {
            const response = await this.#client.get('/converters');
            this.#converters = new Map();
            for (const converter of await response.json()) {
               this.#converters.set(converter.key, converter);
            }
        } catch (err) {
            alert(err);
        }
    }

    async #loadCustomConverters() {
        try {
            const response = await this.#client.get('/converters/custom');
            this.#customConverters = await response.json();
        } catch (err) {
            alert(err);
        }
    }

    async #loadDefinitions() {
        try {
            const response = await this.#client.get('/definitions');
            this.#definitions = await response.json();
        } catch (err) {
            alert(err);
        }
    }

    async #updateDefinitions(definitionsJson) {
        try {
            const newDefinitions = JSON.parse(definitionsJson);
            for (const id of Object.keys(this.#definitions)) {
                if (newDefinitions[id] === undefined) {
                    await this.#client.delete(`/definitions/${id}`);
                }
            }
            for (const [id,definition] of Object.entries(newDefinitions)) {
                await this.#client.put(`/definitions/${id}`, JSON.stringify(definition));
            }
            await this.#loadDefinitions();
            return true;
        } catch (err) {
            alert(err);
            return false;
        }
    }

    async #updateSingleDefinition(id, definition) {
        try {
            await this.#client.put(`/definitions/${id}`, JSON.stringify(definition));
            await this.#loadDefinitions();
            this.#editingDefinitionId = null;
            this.#viewDefinitions();
            return true;
        } catch (err) {
            alert(err);
            return false;
        }
    }

    async #deleteSingleDefinition(id) {
        try {
            await this.#client.delete(`/definitions/${id}`);
            await this.#loadDefinitions();
            this.#editingDefinitionId = null;
            this.#viewDefinitions();
            return true;
        } catch (err) {
            alert(err);
            return false;
        }
    }

    async #updateConverters(convertersJson) {
        try {
            const newConverters = JSON.parse(convertersJson);
            for (const converter of this.#customConverters) {
                if (newConverters.find(c => c.id == converter.id) === undefined) {
                    await this.#client.delete(`/converters/custom/${converter.id}`);
                }
            }
            for (const converter of newConverters) {
                await this.#client.put(`/converters/custom/${converter.id}`, JSON.stringify(converter));
            }
            await this.#loadConverters();
            await this.#loadCustomConverters();
            await this.#loadDefinitions();
            return true;
        } catch (err) {
            alert(err);
            return false;
        }
    }
}
