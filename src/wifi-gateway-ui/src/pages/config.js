
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
    { value: 'X04', label: '? X04 ?' },
    { value: 'X05', label: '? X05 ?' },
    { value: 'HEA', label: 'Heating Circuit' },
    { value: 'X07', label: '? X07 ?' },
    { value: 'SEN', label: 'Sensor' },
    { value: 'X09', label: '? X09 ?' },
    { value: 'X0A', label: '? X0A ?' },
    { value: 'X0B', label: '? X0B ?' },
    { value: 'X0C', label: '? X0C ?' },
    { value: 'DIS', label: 'Display' },
    { value: 'X0E', label: '? X0E ?' },
    { value: 'X0F', label: '? X0F ?' }
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

            if (configFile.size > 4096) {
                alert('Config file too large.');
                return;
            }

            try {
                this.modalLoader.show();
                const config = await configFile.text();
                await this.#client.post('/data/config', config);
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
                    for (const dataConfig of this.#dataConfigs) {
                        await this.#deleteDataConfig(dataConfig);
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
            this.#definitions = await response.json();
        } catch (err) {
            alert(err);
        }
    }

    async #loadDataConfigs() {
        try {
            const response = await this.#client.get('/data/config');
            this.#dataConfigs = await response.json();
        } catch (err) {
            alert(err);
        }
    }

    #refreshAddIds() {
        const options = Object.entries(this.#definitions)
            .filter(([id, definition]) => definition.access != 'None')
            .map(([id, definition]) => ({ value: id, label: `[${id}] ${definition.name}`}));
        this.dataId.options = options;
        this.dataId.selected = options[0].value;
        this.#updateSourceAndMode();
        this.addFieldset.enable();
    }
  
    #updateSourceAndMode() {
        const definition = this.#definitions[this.dataId.selected];

        this.sourceType.enable();
        this.sourceAddress.enable();

        if (definition.access == 'Readable') {
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
        await this.#updateDataConfig({
          valueId: Number.parseInt(this.dataId.selected),
          source: `${this.sourceType.selected}/${this.sourceAddress.value}`,
          subscribed: this.mode.selected == 'Subscribe' || this.mode.selected == 'Both',
          writable: this.mode.selected == 'Writable' || this.mode.selected == 'Both'
        });
        this.addFieldset.enable();
    }

    async #updateDataConfig(dataConfig) {
      try {
          const [sourceType, sourceAddress] = dataConfig.source.split('/');
          await this.#client.put(`/data/config/${sourceType}/${sourceAddress}/${dataConfig.valueId}`, JSON.stringify(dataConfig));
          await this.#loadDataConfigs();
          this.#refreshTable();
          this.#refreshAddIds();
      } catch (err) {
          alert(err);
      }
    }

    async #deleteDataConfig(dataConfig) {
      try {
          const [sourceType, sourceAddress] = dataConfig.source.split('/');
          await this.#client.delete(`/data/config/${sourceType}/${sourceAddress}/${dataConfig.valueId}`);
          await this.#loadDataConfigs();
          this.#refreshTable();
          this.#refreshAddIds();
      } catch (err) {
          alert(err);
      } finally {
          this.addFieldset.enable();
      }
    }

    #refreshTable() {
        this.table.clear();

        if (this.#dataConfigs.length > 0) {
            this.table.addRow().addHeaders([
                'Source',
                'ID',
                'Name',
                'Unit',
                'Subscribed',
                'Writable'
            ]);
            for (const dataConfig of this.#dataConfigs) {
                const definition = this.#definitions[dataConfig.valueId.toString()]
                this.table.addRow().addColumns([
                    dataConfig.source,
                    dataConfig.valueId,
                    definition?.name,
                    definition?.unit,
                    new CheckboxView(null, { checked: dataConfig.subscribed }, async (checked, checkbox) => {
                        checkbox.disable();
                        dataConfig.subscribed = checked;
                        await this.#updateDataConfig(dataConfig);
                        checkbox.enable();
                        if (!dataConfig.subscribed && !dataConfig.writable) {
                            await this.#loadDataConfigs();
                            this.#refreshTable();
                        }
                    }),
                    new CheckboxView(null, { checked: dataConfig.writable, disabled: definition?.access == 'Readable' }, async (checked, checkbox) => {
                        checkbox.disable();
                        dataConfig.writable = checked;
                        await this.#updateDataConfig(dataConfig);
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

        const deviceType = fieldset.select(fieldset.label('Device Type'), SourceTypes, {}, (value) => { deviceType.validate(); });
        const deviceAddress = fieldset.number(fieldset.label('Device Address'), { required: true, min: 0, max: 127 }, (value) => { deviceAddress.validate(); });
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
                    readOnly: writeEnabled.checked ? 'false' : 'true',
                    deviceId: `${deviceType.selected}/${deviceAddress.value}`
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
            writeEnabled.checked = (config.dta.readOnly == 'false');
            deviceType.selected = config.dta.deviceId.split('/')[0];
            deviceAddress.value = config.dta.deviceId.split('/')[1];
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
        const topic = fieldset.text(fieldset.label('Base Topic'), { required: true, maxlength: 31 }, (value) => { topic.validate(); });

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
