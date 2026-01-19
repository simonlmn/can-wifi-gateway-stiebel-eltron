
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
        this.filter = view.select(view.label('Filter:'), ['All', 'Undefined', 'Configured', 'Not Configured'], {}, async (selected) => {
            this.filter.disable();
            await this.#reload();
            this.filter.enable();
        });
        this.updatedSince = view.text(view.label('Updated since (ISO date/time, optional):'), { placeholder: '2024-01-01T00:00:00' }, async () => {
            this.updatedSince.disable();
            await this.#reload();
            this.updatedSince.enable();
        });
        this.numbersAsDecimals = view.checkbox(view.label('Show raw values as decimal'), { indeterminate: false }, async () => {
            this.numbersAsDecimals.disable();
            await this.#reload();
            this.numbersAsDecimals.enable();
        });
        this.table = view.table();
        this.state = view.p();
        this.filter.selected = 'All';
        this.autoRefresh(true);
    }

    async leave() {
        this.autoRefresh(false);
    }

    autoRefresh(enable) {
        clearTimeout(this.timeout);
        
        if (enable) {
            this.#reload()
                .finally(() => {
                    clearTimeout(this.timeout);
                    this.timeout = setTimeout(() => this.autoRefresh(true), 5000);
                });
        }
    }

    #getDataQueryParam() {
        const params = [];
        switch (this.filter.selected) {
            case 'Undefined':
                params.push('filter=undefined');
                break;
            case 'Configured':
                params.push('filter=configured');
                break;
            case 'Not Configured':
                params.push('filter=notConfigured');
                break;
            default:
                break;
        }

        const updatedSinceValue = this.updatedSince?.value?.trim();
        if (updatedSinceValue) {
            params.push(`updatedSince=${encodeURIComponent(updatedSinceValue)}`);
        }

        if (this.numbersAsDecimals?.checked) {
            params.push('numbersAsDecimals');
        }

        return params.length ? `?${params.join('&')}` : '';
    }

    async #reload() {
        this.state.attribute('class', null);
        this.state.content = `<small>Loading...</small>`;
        try {
            const response = await this.#client.get(`/data${this.#getDataQueryParam()}`);
            const data = await response.json();
            this.table.clear();

            if (data.actualItems > 0) {
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
                this.state.attribute('class', null);
                this.state.content = `<small>Updated on ${new Date().toISOString()}.</small>`;
            } else {
                this.state.attribute('class', 'notice');
                this.state.content = 'No data has been captured yet. You need to configure which datapoints shall be requested and/or captured.';
            }
        } catch (err) {
            this.state.attribute('class', 'notice');
            this.state.content = `${err}`;
        }
    }
}
