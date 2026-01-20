
import { BlockView, View, TextView, AsyncButtonView, ButtonView, createElement } from '../view.js';

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
    #data
    #searchTimeout
    #editingKey
    #writeInProgress

    constructor(client) {
        this.#client = client;
        this.#data = null;
        this.#searchTimeout = null;
        this.#editingKey = null;
        this.#writeInProgress = false;
    }

    get label() {
        return 'Data';
    }

    async enter(view) {
        view.h1('Current data');
        
        const filterFieldset = view.fieldset('Search and Filter Options');
        this.filter = filterFieldset.select(filterFieldset.label('Filter:'), ['All', 'Undefined', 'Configured', 'Not Configured'], {}, async (selected) => {
            this.filter.disable();
            await this.#reload();
            this.filter.enable();
        });
        this.updatedSince = filterFieldset.text(filterFieldset.label('Updated since (ISO date/time):'), { placeholder: '2024-01-01T00:00:00' }, async () => {
            this.updatedSince.disable();
            await this.#reload();
            this.updatedSince.enable();
        });
        this.numbersAsDecimals = filterFieldset.checkbox(filterFieldset.label('Show raw values as decimal'), { indeterminate: false }, async () => {
            this.numbersAsDecimals.disable();
            await this.#reload();
            this.numbersAsDecimals.enable();
        });
        this.searchFilter = filterFieldset.text(filterFieldset.label('Search by ID or name:'), { placeholder: 'e.g., 42 or temperature', liveUpdate: true }, async () => {
            clearTimeout(this.#searchTimeout);
            this.#searchTimeout = setTimeout(() => {
                this.#refreshTable();
            }, 300);
        });
        
        this.table = view.table();
        this.state = view.p();
        this.filter.selected = 'All';
        this.autoRefresh(true);
    }

    async leave() {
        clearTimeout(this.#searchTimeout);
        this.autoRefresh(false);
    }

    autoRefresh(enable) {
        clearTimeout(this.timeout);
        
        if (enable && !this.#isBusy()) {
            this.#reload()
                .finally(() => {
                    clearTimeout(this.timeout);
                    this.timeout = setTimeout(() => this.autoRefresh(true), 5000);
                });
        } else if (enable && this.#isBusy()) {
            // Schedule retry after editing or write completes
            this.timeout = setTimeout(() => this.autoRefresh(true), 500);
        }
    }

    #isBusy() {
        return this.#editingKey !== null || this.#writeInProgress;
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
            this.#data = await response.json();
            this.#refreshTable();
        } catch (err) {
            this.state.attribute('class', 'notice');
            this.state.content = `${err}`;
        }
    }

    #refreshTable() {
        this.table.clear();

        if (!this.#data || this.#data.actualItems === 0) {
            this.state.attribute('class', 'notice');
            this.state.content = 'No data has been captured yet. You need to configure which datapoints shall be requested and/or captured.';
            return;
        }

        const searchTerm = this.searchFilter?.value?.trim().toLowerCase() || '';
        const datapoints = [];

        // Collect all datapoints for filtering
        for (const deviceType in this.#data.items) {
            for (const deviceAddress in this.#data.items[deviceType]) {
                for (const id in this.#data.items[deviceType][deviceAddress]) {
                    const datapoint = this.#data.items[deviceType][deviceAddress][id];
                    datapoints.push(datapoint);
                }
            }
        }

        // Filter datapoints based on search term
        let filteredDatapoints = datapoints;
        if (searchTerm) {
            filteredDatapoints = datapoints.filter(dp => {
                const idStr = dp.id?.toString().toLowerCase() || '';
                const nameStr = dp.name?.toLowerCase() || '';
                return idStr.includes(searchTerm) || nameStr.includes(searchTerm);
            });
        }

        if (filteredDatapoints.length > 0) {
            this.table.addRow().addHeaders([
                'Source',
                'ID',
                'Name',
                'Raw Value',
                'Value',
                'Unit',
                'Last Update',
                'Write Value'
            ]);
            for (const datapoint of filteredDatapoints) {
                const editKey = `${datapoint.source}#${datapoint.id}`;
                const isEditing = this.#editingKey === editKey;

                const writeCell = new BlockView();
                if (datapoint.writable) {
                    if (isEditing) {
                        const input = new TextView(null, {
                            placeholder: datapoint.value !== undefined ? `${datapoint.value}` : '',
                            size: 12,
                            value: datapoint.value !== undefined ? `${datapoint.value}` : ''
                        });
                        const status = new View(createElement('small'));
                        const writeButton = new AsyncButtonView('Write', {}, async () => {
                            const rawValue = (input.value ?? '').trim();
                            if (!rawValue) {
                                status.textContent = 'Enter a value to write.';
                                return;
                            }

                            this.#writeInProgress = true;
                            status.textContent = 'Sending...';
                            try {
                                const { deviceType, deviceAddress } = this.#parseSource(datapoint.source);
                                const parsedValue = this.#parseInputValue(rawValue);
                                await this.#client.put(`/data/${encodeURIComponent(deviceType)}/${encodeURIComponent(deviceAddress)}/${encodeURIComponent(datapoint.id)}`, JSON.stringify(parsedValue));
                                status.textContent = 'Write accepted.';
                                this.#editingKey = null;
                                await this.#reload();
                                this.autoRefresh(true);
                            } catch (err) {
                                status.textContent = `Write failed: ${err.message ?? err}`;
                            } finally {
                                this.#writeInProgress = false;
                            }
                        });

                        const cancelButton = new ButtonView('Cancel', {}, () => {
                            this.#editingKey = null;
                            this.autoRefresh(true);
                        });

                        writeCell.addView(input);
                        writeCell.addView(writeButton);
                        writeCell.addView(cancelButton);
                        writeCell.addView(status);
                    } else {
                        const editButton = new ButtonView('Edit', {}, () => {
                            this.#editingKey = editKey;
                            this.autoRefresh(false);
                            this.#refreshTable();
                        });
                        writeCell.addView(editButton);
                    }
                }

                const row = this.table.addRow();
                row.addColumns([
                    datapoint.source,
                    datapoint.id,
                    datapoint.name,
                    datapoint.rawValue,
                    formatValue(datapoint.value),
                    datapoint.unit,
                    datapoint.lastUpdate,
                    writeCell
                ]);
            }
            this.state.attribute('class', null);
            this.state.content = `<small>Showing ${filteredDatapoints.length} of ${datapoints.length} items. Updated on ${new Date().toISOString()}.</small>`;
        } else if (searchTerm) {
            this.state.attribute('class', 'notice');
            this.state.content = `No data matches the search filter "${searchTerm}".`;
        } else {
            this.state.attribute('class', 'notice');
            this.state.content = 'No data has been captured yet. You need to configure which datapoints shall be requested and/or captured.';
        }
    }

    #parseSource(source) {
        const parts = (source || '').split('/');
        if (parts.length !== 2) {
            throw new Error(`Invalid source '${source}'.`);
        }
        return { deviceType: parts[0], deviceAddress: parts[1] };
    }

    #parseInputValue(rawValue) {
        try {
            return JSON.parse(rawValue);
        } catch (_) {
            // Fallback: treat as plain string if it is not valid JSON
            return rawValue;
        }
    }
}
