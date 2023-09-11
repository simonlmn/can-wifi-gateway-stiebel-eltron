
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

function parseAllConfig(data) {
    const config = {};
    for (const [category, values] of groupBy(data.split(';\n').filter(line => line.length > 0).map(line => line.split('=')).map(([path, value]) => path.split('.').concat(value)), ([category, name, value]) => category, ([category, name, value]) => [name, value])) {
        config[category] = Object.fromEntries(new Map(values));
    }

    return config;
}

function serializeAllConfig(config) {
    return Object.entries(config).flatMap(([category, params]) => Object.entries(params).map(([name, value]) => `${category}.${name}=${value};`)).join('\n');
}

function parseConfig(data) {
    const config = {};
    for (const [name, value] of data.split(';\n').filter(line => line.length > 0).map(line => line.split('='))) {
        config[name] = value;
    }

    return config;
}

function serializeConfig(config) {
    return Object.entries(config).flatMap(([name, value]) => `${name}=${value};`).join('\n');
}

const SourceTypes = [
    { value: 'SYS', label: 'System' },
    { value: 'HEA', label: 'Heating Circuit' },
    { value: 'X09', label: '? X09 ?' },
    { value: 'SEN', label: 'Sensor' },
    { value: 'X0A', label: '? X0A ?' },
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
        this.loadFileFieldset = this.fieldset('Load from file');
        this.loadFileInput = this.loadFileFieldset.file(this.loadFileFieldset.label('Configuration file'), {}, () => {});
        this.loadFileUpload = this.loadFileFieldset.button('Load', {}, async () => {
            const configFile = this.loadFileInput.files.item(0);
            if (!configFile) {
                return;
            }

            if (configFile.size > 2048) {
                alert('Config file too large.');
                return;
            }

            try {
                this.modalLoader.show();
                const config = await configFile.text();
                const matches = config.matchAll(/^#+ *(\w*)\s*$/gm);
                let iterator = matches.next();
                while (!iterator.done) {
                    const sectionName = iterator.value[1];
                    const sectionStartIndex = iterator.value.index + iterator.value[0].length + 1;
                    iterator = matches.next();
                    const sectionEndIndex = iterator.done ? config.length : iterator.value.index;
                    const sectionContent = config.substring(sectionStartIndex, sectionEndIndex).trim();

                    switch (sectionName) {
                        case 'Subscriptions':
                            await this.#client.post('/subscriptions', sectionContent);
                            break;
                        case 'Writable':
                            await this.#client.post('/writable', sectionContent);
                            break;
                        default:
                            break;
                    }
                }
            } catch (err) {
                alert(err);
            } finally {
                await this.#loadDataConfigs();
                this.#refreshTable();
                this.modalLoader.hide();
            }
        });
        this.addFieldset = this.fieldset('Add configuration');
        this.dataId = this.addFieldset.select(this.addFieldset.label('ID'), [], {}, () => this.#updateSourceAndMode());
        this.sourceType = this.addFieldset.select(this.addFieldset.label('Source Type'), SourceTypes);
        this.sourceAddress = this.addFieldset.number(this.addFieldset.label('Source Address'), { min: 0, max: 127 });
        this.mode = this.addFieldset.select(this.addFieldset.label('Mode'));
        this.add = this.addFieldset.button('Add', {}, () => this.#addDataConfig());
        this.addFieldset.disable();
        this.table = this.table();
        this.clearTable = this.button('Clear all', {}, async () => {
            if (confirm('Delete all data configurations?')) {
                try {
                    this.modalLoader.show();
                    for (const dataConfig of this.#dataConfigs.items.values()) {
                        if (dataConfig.subscribed) {
                            await this.#disableSubscription(dataConfig);
                        }
                        if (dataConfig.writable) {
                            await this.#disableWritable(dataConfig);
                        }
                    }
                    await this.#loadDataConfigs();
                    this.#refreshTable();
                } finally {
                    this.modalLoader.hide();
                }
            }
        });
        this.state = this.p();
        this.modalLoader = this.modal();
        this.modalLoader.loader();
        this.modalLoader.hide();
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

        this.sourceType.enable();
        if (sourceType != 'ANY') {
            this.sourceType.selected = sourceType;   
        }
        
        this.sourceAddress.enable();
        if (sourceAddress == '*') {
            this.sourceAddress.value = 1;
        } else {
            this.sourceAddress.value = parseInt(sourceAddress);
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
            this.state.content = `<small>Updated on ${new Date().toISOString()}.</small>`;
        } else {
            this.state.attribute('class', 'notice');
            this.state.content = 'No data has been configured yet.';
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

        await this.#showBaseSettings(view);
        await this.#showMqttSettings(view);
        
        this.dataConfig = view.addView(new DataConfigurationView(this.#client));
        this.dataConfig.show();
    }

    async leave() {
    }

    async #showBaseSettings(view) {
        const fieldset = view.fieldset('Base Settings');
        fieldset.disable();

        const displayAddress = fieldset.number(fieldset.label('Display address'), { required: true, min: 1, max: 4 }, (value) => { displayAddress.validate(); });
        const writeEnabled = fieldset.checkbox(fieldset.label('Enable write access'), { indeterminate: true }, (checked) => { writeEnabled.validate(); });
        const dataAccessMode = fieldset.select(fieldset.label('Data capture mode'), ['None', 'Configured', 'Defined', 'Any'], {}, (value) => { dataAccessMode.validate(); });
        const canMode = fieldset.select(fieldset.label('CAN mode'), ['Normal', 'ListenOnly', 'LoopBack'], {}, (value) => { canMode.validate(); });

        fieldset.button('Save', {}, async () => {
            if (!fieldset.validate()) {
                return;
            }

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
                await this.#client.put('/system/config', serializeAllConfig(config));
            } catch (err) {
                alert(err);
            }
            fieldset.enable();
        });

        try {
            const response = await this.#client.get('/system/config');
            const config = parseAllConfig(await response.text());
            dataAccessMode.selected = config.dta.mode;
            writeEnabled.checked = (config.dta.readOnly == "false");
            displayAddress.value = parseInt(config.sep.display) + 1;
            canMode.selected = config.can.mode;
        } catch (err) {
            alert(err);
        }
        fieldset.enable();
    }

    async #showMqttSettings(view) {
        const fieldset = view.fieldset('MQTT Settings');
        fieldset.disable();

        const enabled = fieldset.checkbox(fieldset.label('Enable MQTT protocol'), { required: true, indeterminate: true }, (value) => { enabled.validate(); });
        const brokerAddress = fieldset.text(fieldset.label('Broker IP'), { required: true, pattern: '([1-9]|[1-9][0-9]|1[1-9][0-9]|2[0-5][0-5])\.([0-9]|[1-9][0-9]|1[1-9][0-9]|2[0-5][0-5])\.([0-9]|[1-9][0-9]|1[1-9][0-9]|2[0-5][0-5])\.([1-9]|[1-9][0-9]|1[1-9][0-9]|2[0-5][0-3])', maxlength: 15 }, (checked) => { brokerAddress.validate(); });
        const brokerPort = fieldset.number(fieldset.label('Broker port'), { required: true, min: 1, max: 65535 }, (value) => { brokerPort.validate(); });
        const topic = fieldset.text(fieldset.label('Topic'), { required: true, maxlength: 31 }, (value) => { topic.validate(); });

        fieldset.button('Save', {}, async () => {
            if (!fieldset.validate()) {
                return;
            }

            fieldset.disable();

            const config = {
                enabled: enabled.checked ? 'true' : 'false',
                broker: brokerAddress.value,
                port: brokerPort.value,
                topic: topic.value
            };

            try {
                await this.#client.put('/system/config/mqc', serializeConfig(config));
            } catch (err) {
                alert(err);
            }
            fieldset.enable();
        });

        try {
            const response = await this.#client.get('/system/config/mqc');
            const config = parseConfig(await response.text());
            enabled.checked = config.enabled == 'true';
            brokerAddress.value = config.broker;
            brokerPort.value = parseInt(config.port);
            topic.value = config.topic;
        } catch (err) {
            alert(err);
        }
        fieldset.enable();
    }
}
