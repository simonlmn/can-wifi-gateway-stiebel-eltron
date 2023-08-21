
import { CheckboxView, ButtonView } from '../view.js';

function groupBy(list, keyGetter, valueGetter) {
    const map = new Map();
    list.forEach((item) => {
        const key = keyGetter(item);
        const collection = map.get(key);
        if (!collection) {
            map.set(key, [valueGetter(item)]);
        } else {
            collection.push(valueGetter(item));
        }
    });
    return map;
}

function parseConfig(data) {
    const config = {};
    for (const [category, values] of groupBy(data.split(';\n').filter(line => line.length > 0).map(line => line.split('=')).map(([path, value]) => path.split('.').concat(value)), ([category, name, value]) => category, ([category, name, value]) => [name, value])) {
        config[category] = Object.fromEntries(new Map(values));
    }

    return config;
}

function serializeConfig(config) {
    return Object.entries(config).flatMap(([category, params]) => Object.entries(params).map(([name, value]) => `${category}.${name}=${value};`)).join('\n');
}

export class ConfigPage {
    #client

    constructor(client) {
        this.#client = client;
    }

    get label() {
        return 'Configuration';
    }

    async enter(view) {
        view.h1('Configuration');

        await this.#showSettings(view);
        //await this.#showSubscriptions(view);
    }

    async leave() {
    }

    async #showSettings(view) {
        const fieldset = view.fieldset('Settings');
        fieldset.disable();

        const writeEnabled = fieldset.checkbox(fieldset.label('Enable write access'), { indeterminate: true }, (checked) => { });
        const dataAccessMode = fieldset.select(fieldset.label('Data capture mode'), ['None', 'Configured', 'Defined', 'Any'], {}, (value) => { });
        const displayAddress = fieldset.number(fieldset.label('Display address'), { min: 1, max: 4 }, (value) => { });
        const canMode = fieldset.select(fieldset.label('CAN mode'), ['Normal', 'ListenOnly'], {}, (value) => { });

        fieldset.button('Save', {}, async () => {
            fieldset.disable();

            const config = {
                dta: {
                    mode: dataAccessMode.selected,
                    readOnly: writeEnabled.checked ? 'false' : 'true'
                },
                sep: {
                    display: displayAddress.value - 1
                },
                can: {
                    mode: canMode.selected
                }
            };

            try {
                await this.#client.put('/node/config', serializeConfig(config));
            } catch (err) {
                alert(err);
            }
            fieldset.enable();
        });

        try {
            const response = await this.#client.get('/node/config');
            const config = parseConfig(await response.text());
            dataAccessMode.selected = config.dta.mode;
            writeEnabled.checked = (config.dta.readOnly == "false");
            displayAddress.value = parseInt(config.sep.display) + 1;
            canMode.selected = config.can.mode;
        } catch (err) {
            alert(err);
        }
        fieldset.enable();
    }

    async #showSubscriptions(view) {
        const fieldset = view.fieldset('Data configuration');
        fieldset.disable();

        const table = fieldset.table();
        const add = fieldset.button('Add', {}, () => {
            // TODO POST to /subscriptions and refresh table
        });
        const state = fieldset.p();
        
        state.attribute('class', null);
        state.content(`<small>Loading...</small>`);
        try {
            const response = await this.#client.get('/data?onlyConfigured');
            const data = await response.json();
            table.clear();

            if (data.totalItems > 0) {
                table.addRow().addHeaders([
                    'Source',
                    'ID',
                    'Name',
                    'Unit',
                    'Subscribed',
                    'Writable'
                ]);
                for (const deviceType in data.items) {
                    for (const deviceAddress in data.items[deviceType]) {
                        for (const id in data.items[deviceType][deviceAddress]) {
                            const datapoint = data.items[deviceType][deviceAddress][id];
                            table.addRow().addColumns([
                                datapoint.source,
                                datapoint.id,
                                datapoint.name,
                                datapoint.unit,
                                new CheckboxView(null, { checked: datapoint.subscribed }, (checked) => {
                                    // TODO POST/DELETE to /subscriptions/...
                                }),
                                new CheckboxView(null, { checked: datapoint.writable }, (checked) => {
                                    // TODO POST/DELETE to /writable/...
                                })
                            ]);
                        }
                    }
                }
            } else {
                state.attribute('class', 'notice');
                state.content('No data has been configured yet.');
            }
        } catch (err) {
            state.attribute('class', 'notice');
            state.content(`${err}`);
        }

        fieldset.enable();
    }
}
