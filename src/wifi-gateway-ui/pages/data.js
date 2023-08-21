
function formatValue(value) {
    if (value instanceof Object) {
        if (Array.isArray(value)) {
            return value.join(', ');
        } else {
            return Object.entries(value).flatMap(([name, value]) => `${name}=${value}`).join('<br>\n');
        }
    } else {
        return `${value}`;
    }
}

export class DataPage {
    #client

    constructor(client) {
        this.#client = client;
    }

    get label() {
        return 'Data';
    }

    async enter(view) {
        view.h1('Current data');
        this.table = view.table();
        this.state = view.p();
        
        this.state.attribute('class', null);
        this.state.content(`<small>Loading...</small>`);
        try {
            const response = await this.#client.get('/data');
            const data = await response.json();
            this.table.clear();

            if (data.totalItems > 0) {
                this.table.addRow().addHeaders([
                    'Source',
                    'ID',
                    'Name',
                    'Raw Value',
                    'Value',
                    'Unit',
                    'Last Update'
                ]);
                for (const deviceType in data.items) {
                    for (const deviceAddress in data.items[deviceType]) {
                        for (const id in data.items[deviceType][deviceAddress]) {
                            const datapoint = data.items[deviceType][deviceAddress][id];
                            this.table.addRow().addColumns([
                                datapoint.source,
                                datapoint.id,
                                datapoint.name,
                                datapoint.rawValue,
                                formatValue(datapoint.value),
                                datapoint.unit,
                                datapoint.lastUpdate
                            ]);
                        }
                    }
                }
            } else {
                this.state.attribute('class', 'notice');
                this.state.content('No data has been captured yet. You need to configure which datapoints shall be requested and/or captured.');
            }
        } catch (err) {
            this.state.attribute('class', 'notice');
            this.state.content(`${err}`);
        }
    }

    async leave() {
    }
}
