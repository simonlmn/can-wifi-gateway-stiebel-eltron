
import { CheckboxView, ContainerView, createElement } from '../view.js';

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

const SourceTypes = [
    { value: 'SYS', label: 'System' },
    { value: 'HEA', label: 'Heating Circuit' },
    { value: 'SEN', label: 'Sensor' },
    { value: 'DIS', label: 'Display' }
];

class DataConfigurationView extends ContainerView {
    #client
    #definitions
    #dataConfigs

    constructor(client) {
        super(createElement('section'));
        this.#client = client;
        this.h2('Data configuration');
        this.addFieldset = this.fieldset('Add configuration');
        this.dataId = this.addFieldset.select(this.addFieldset.label('ID'), [], {}, () => this.#updateSourceAndMode());
        this.sourceType = this.addFieldset.select(this.addFieldset.label('Source Type'));
        this.sourceAddress = this.addFieldset.number(this.addFieldset.label('Source Address'), { min: 0, max: 127 });
        this.mode = this.addFieldset.select(this.addFieldset.label('Mode'));
        this.add = this.addFieldset.button('Add', {}, () => this.#addDataConfig());
        this.addFieldset.disable();
        this.table = this.table();
        this.state = this.p();
    }

    async show() {
        await this.#loadDefinitions();
        await this.#loadDataConfigs();
        this.#refreshTable();
        this.#refreshAddIds();
    }

    async hide() {
    }

    async #loadDefinitions() {
        try {
            const response = await this.#client.get('/definitions');
            const data = await response.json();
            this.#definitions = new Map();
            for (const definition of data) {
                this.#definitions.set(definition.id, definition);// definition.id, definition.name, definition.unit, definition.accessMode, definition.source
            }
        } catch (err) {
            alert(err);
        }
    }

    async #loadDataConfigs() {
        try {
            const response = await this.#client.get('/data?onlyConfigured');
            const data = await response.json();
            const items = [];
            for (const deviceType in data.items) {
                for (const deviceAddress in data.items[deviceType]) {
                    for (const id in data.items[deviceType][deviceAddress]) {
                        items.push(data.items[deviceType][deviceAddress][id]);
                    }
                }
            }
            data.items = items;
            this.#dataConfigs = data;
        } catch (err) {
            alert(err);
        }
    }

    #refreshAddIds() {
        const options = Array.from(this.#definitions.values()).map(definition => ({ value: definition.id, label: `[${definition.id}] ${definition.name}`}));
        this.dataId.options = options;
        this.dataId.selected = options[0].value;
        this.#updateSourceAndMode();
        this.addFieldset.enable();
    }
  
    #updateSourceAndMode() {
        const definition = this.#definitions.get(parseInt(this.dataId.selected));
        const [sourceType, sourceAddress] = definition.source.split('/');

        if (sourceType == 'ANY') {
            this.sourceType.options = SourceTypes;
            this.sourceType.enable();
        } else {
            this.sourceType.options = SourceTypes.filter(type => type.value == sourceType);
            this.sourceType.disable();
        }
        
        if (sourceAddress == '*') {
            this.sourceAddress.value = 1;
            this.sourceAddress.enable();
        } else {
            this.sourceAddress.value = parseInt(sourceAddress);
            this.sourceAddress.disable();
        }

        if (definition.accessMode == 'Readable') {
            this.mode.options = ['Subscribe'];
            this.mode.selected = 'Subscribe';
            this.mode.disable();
        } else {
            this.mode.options = ['Subscribe', 'Writable', 'Both'];
            this.mode.selected = 'Writable';
            this.mode.enable();
        }
    }

    async #addDataConfig() {
        this.addFieldset.disable();
        const id = this.dataId.selected;
        const sourceType = this.sourceType.selected;
        const sourceAddress = this.sourceAddress.value;
        const mode = this.mode.selected;

        try {
            if (mode == 'Subscribe' || mode == 'Both') {
                await this.#client.post('/subscriptions', `${id}@${sourceType}/${sourceAddress}`);
            }
    
            if (mode == 'Writable' || mode == 'Both') {
                await this.#client.post('/writable', `${id}@${sourceType}/${sourceAddress}`);
            }

            await this.#loadDataConfigs();
            this.#refreshTable();
            this.#refreshAddIds();
        } catch (err) {
            alert(err);
        } finally {
            this.addFieldset.enable();
        }
    }

    async #enableSubscription(dataConfig) {
        try {
            await this.#client.post('/subscriptions', `${dataConfig.id}@${dataConfig.source}`);
            dataConfig.subscribed = true;
        } catch (err) {
            alert(err);
        }
    }

    async #disableSubscription(dataConfig) {
        try {
            await this.#client.delete(`/subscriptions/${dataConfig.source}/${dataConfig.id}`);
            dataConfig.subscribed = false;
        } catch (err) {
            alert(err);
        }
    }

    async #enableWritable(dataConfig) {
        try {
            await this.#client.post('/writable', `${dataConfig.id}@${dataConfig.source}`);
            dataConfig.writable = true;
        } catch (err) {
            alert(err);
        }
    }

    async #disableWritable(dataConfig) {
        try {
            await this.#client.delete(`/writable/${dataConfig.source}/${dataConfig.id}`);
            dataConfig.writable = false;
        } catch (err) {
            alert(err);
        }
    }

    #refreshTable() {
        this.table.clear();

        if (this.#dataConfigs.actualItems > 0) {
            this.table.addRow().addHeaders([
                'Source',
                'ID',
                'Name',
                'Unit',
                'Subscribed',
                'Writable'
            ]);
            for (const dataConfig of this.#dataConfigs.items.values()) {
                this.table.addRow().addColumns([
                    dataConfig.source,
                    dataConfig.id,
                    dataConfig.name,
                    dataConfig.unit,
                    new CheckboxView(null, { checked: dataConfig.subscribed }, async (checked, checkbox) => {
                        checkbox.disable();
                        if (checked) {
                            await this.#enableSubscription(dataConfig);
                        } else {
                            await this.#disableSubscription(dataConfig);
                        }
                        checkbox.enable();
                        if (!dataConfig.subscribed && !dataConfig.writable) {
                            await this.#loadDataConfigs();
                            this.#refreshTable();
                        }
                    }),
                    new CheckboxView(null, { checked: dataConfig.writable, disabled: dataConfig.accessMode == 'Readable' }, async (checked, checkbox) => {
                        checkbox.disable();
                        if (checked) {
                            await this.#enableWritable(dataConfig);
                        } else {
                            await this.#disableWritable(dataConfig);
                        }
                        checkbox.enable();
                        if (!dataConfig.subscribed && !dataConfig.writable) {
                            await this.#loadDataConfigs();
                            this.#refreshTable();
                        }
                    })
                ]);
            }
            this.state.attribute('class', null);
            this.state.content(`<small>Updated on ${new Date().toISOString()}.</small>`);
        } else {
            this.state.attribute('class', 'notice');
            this.state.content('No data has been configured yet.');
        }
    }
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
        
        this.dataConfig = view.addView(new DataConfigurationView(this.#client));
        this.dataConfig.show();
    }

    async leave() {
    }

    async #showSettings(view) {
        const fieldset = view.fieldset('Settings');
        fieldset.disable();

        const writeEnabled = fieldset.checkbox(fieldset.label('Enable write access'), { indeterminate: true }, (checked) => { });
        const dataAccessMode = fieldset.select(fieldset.label('Data capture mode'), ['None', 'Configured', 'Defined', 'Any'], {}, (value) => { });
        const displayAddress = fieldset.number(fieldset.label('Display address'), { min: 1, max: 4 }, (value) => { });
        const canMode = fieldset.select(fieldset.label('CAN mode'), ['Normal', 'ListenOnly', 'LoopBack'], {}, (value) => { });

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
                await this.#client.put('/system/config', serializeConfig(config));
            } catch (err) {
                alert(err);
            }
            fieldset.enable();
        });

        try {
            const response = await this.#client.get('/system/config');
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

    }
}
