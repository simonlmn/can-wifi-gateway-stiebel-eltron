
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
    let config = {};
    for (let [category, values] of groupBy(data.split(';\n').filter(line => line.length > 0).map(line => line.split('=')).map(([path, value]) => path.split('.').concat(value)), ([category, name, value]) => category, ([category, name, value]) => [name, value])) {
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

    enter(view) {
        view.h1('Configuration');

        let fieldset = view.fieldset('Settings');
        fieldset.disable();

        let writeEnabled = fieldset.checkbox(fieldset.label('Enable write access'), { indeterminate: true }, (checked) => { });
        let dataAccessMode = fieldset.select(fieldset.label('Data capture mode'), ['None', 'Configured', 'Defined', 'Any'], {}, (value) => { });
        let displayAddress = fieldset.number(fieldset.label('Display address'), { min: 1, max: 4 }, (value) => { });
        let canMode = fieldset.select(fieldset.label('CAN mode'), ['Normal', 'ListenOnly'], {}, (value) => { });

        fieldset.button('Save', {}, () => {
            fieldset.disable();

            let config = {
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

            this.#client.put('/node/config', serializeConfig(config))
                .then(() => {
                    fieldset.enable();
                })
                .catch((err) => {
                    alert(err);
                    fieldset.enable();
                });
        });

        this.#client.get('/node/config')
            .then(async (response) => {
                let config = parseConfig(await response.text());
                dataAccessMode.selected = config.dta.mode;
                writeEnabled.checked = (config.dta.readOnly == "false");
                displayAddress.value = parseInt(config.sep.display) + 1;
                canMode.selected = config.can.mode;
                fieldset.enable();
            })
            .catch((err) => {
                alert(err);
            });
    }

    leave() {
    }
}
