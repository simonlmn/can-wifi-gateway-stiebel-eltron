
export class DefinitionsPage {
    #client

    constructor(client) {
        this.#client = client;
    }

    get label() {
        return 'Definitions';
    }

    async enter(view) {
        view.h1('Datapoint definitions');
        const table = view.table();

        try {
            const response = await this.#client.get('/definitions');
            const data = await response.json();
            table.clear();
            table.addRow().addHeaders(['ID', 'Name', 'Unit', 'Access mode', 'Source']);
            for (const definition of data) {
                table.addRow().addColumns([definition.id, definition.name, definition.unit, definition.accessMode, definition.source]);
            }
        } catch (err) {
            view.p(err, { class: 'notice' });
        }
    }

    async leave() {
    }
}
