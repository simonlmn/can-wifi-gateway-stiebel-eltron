
export class DefinitionsPage {
    #client

    constructor(client) {
        this.#client = client;
    }

    get label() {
        return 'Definitions';
    }

    enter(view) {
        view.h1('Datapoint definitions');
        let table = view.table();

        this.#client.get('/definitions')
            .then(async (response) => {
                const data = await response.json();
                table.clear();
                table.addRow().addHeaders(['ID', 'Name', 'Unit', 'Access mode', 'Source']);
                for (let definition of data) {
                    table.addRow().addColumns([definition.id, definition.name, definition.unit, definition.accessMode, definition.source]);
                }
            })
            .catch((err) => {
                view.p(err, { class: 'notice' });
            });
    }

    leave() {
    }
}
