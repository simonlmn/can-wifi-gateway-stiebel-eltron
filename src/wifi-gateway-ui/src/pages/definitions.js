
export class DefinitionsPage {
    #client
    #codecs
    #converters
    #definitions

    #table

    constructor(client) {
        this.#client = client;
    }

    get label() {
        return 'Definitions';
    }

    async enter(view) {
        view.h1('Datapoint definitions');
        this.#table = view.table();

        await this.#loadCodecs();
        await this.#loadConverters();
        await this.#loadDefinitions();
        this.#refreshTable();
    }

    async leave() {
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

    async #loadDefinitions() {
        try {
            const response = await this.#client.get('/definitions');
            this.#definitions = await response.json();
        } catch (err) {
            alert(err);
        }
    }

    #refreshTable() {
        this.#table.clear();
        this.#table.addRow().addHeaders(['ID', 'Name', 'Unit', 'Access', 'Converter', 'Codec']);
        for (const [id, definition] of Object.entries(this.#definitions)) {
            const codec = this.#codecs.get(definition.codec);
            const converter = this.#converters.get(definition.converter);
            this.#table.addRow().addColumns([id, definition.name, definition.unit, definition.access, converter.description.replaceAll(/\^([-0123456789]+)/g, '<sup>$1</sup>'), codec.description]);
        }
    }
}
