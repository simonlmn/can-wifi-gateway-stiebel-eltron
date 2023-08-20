
export class DataPage {
    #client

    constructor(client) {
        this.#client = client;
    }

    get label() {
        return 'Data';
    }

    enter(view) {
        view.h1('Current data');
        let table = view.table();

        this.#client.get('/data')
            .then(async (response) => {
                const data = await response.json();
                table.clear();
                table.addRow().addHeaders(['Source', 'ID', 'Name', 'Raw Value', 'Value', 'Unit', 'Last Update']);
                for (let deviceType in data.items) {
                    for (let deviceAddress in data.items[deviceType]) {
                        for (let id in data.items[deviceType][deviceAddress]) {
                            let datapoint = data.items[deviceType][deviceAddress][id];
                            let value = datapoint.value;
                            if (value instanceof Object) {
                                if (Array.isArray(value)) {
                                    value = value.join(', ');
                                } else {
                                    value = Object.entries(value).flatMap(([name, value]) => `${name}=${value}`).join('<br>\n');
                                }
                            }
                            table.addRow().addColumns([datapoint.source, datapoint.id, datapoint.name, datapoint.rawValue, `${value}`, datapoint.unit, datapoint.lastUpdate]);
                        }
                    }
                }
            })
            .catch((err) => {
                view.p(err, { class: 'notice' });
            });
    }

    leave() {
    }
}
