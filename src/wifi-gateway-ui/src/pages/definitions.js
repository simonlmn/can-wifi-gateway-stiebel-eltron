
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
    #convertersView
    #codecsView

    constructor(client) {
        this.#client = client;
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
        this.#viewDefinitions();

        this.#convertersView = view.section('Converters').block();
        this.#viewConverters();

        this.#codecsView = view.section('Codecs').block();
        this.#viewCodecs();
    }

    async leave() {
    }

    #editDefinitions() {
        this.#definitionsView.clear();
        const textarea = this.#definitionsView.textarea(null, { rows: 25 });
        textarea.value = JSON.stringify(this.#definitions, null, 2);
        this.#definitionsView.button('Save', {}, async (button) => {
            button.disable();
            if (await this.#updateDefinitions(textarea.value)) {
                this.#viewDefinitions();
                return;
            }
            button.enable();
        });
        this.#definitionsView.button('Discard', {}, () => this.#viewDefinitions());
    }

    #viewDefinitions() {
        this.#definitionsView.clear();
        this.#definitionsView.button('Edit', {}, () => this.#editDefinitions());
        const table =this.#definitionsView.table();
        
        table.addRow().addHeaders(['ID', 'Name', 'Unit', 'Access', 'Converter', 'Codec']);
        for (const [id, definition] of Object.entries(this.#definitions)) {
            const codec = this.#codecs.get(definition.codec);
            const converter = this.#converters.get(definition.converter);
            table.addRow().addColumns([id, definition.name, definition.unit, definition.access, formatDescription(converter.description), codec.description]);
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
