
import { CheckboxView, ContainerView, createElement } from '../view.js';

export class SystemPage {
    #client

    constructor(client) {
        this.#client = client;
    }

    get label() {
        return 'System';
    }

    async enter(view) {
        const fieldset = view.fieldset('Control');
        fieldset.disable();

        fieldset.button('Restart', {}, async () => {
            fieldset.disable();
            try {
                await this.#client.post('/system/reset', '');
            } catch (err) {
                alert(err);
            }
            fieldset.enable();
        });

        fieldset.button('Factory Reset', {}, async () => {
            fieldset.disable();
            try {
                if (confirm('WARNING: this will clear all configuration including WiFi settings from the device! Are you sure?')) {
                    await this.#client.post('/system/factory-reset', '');
                }
            } catch (err) {
                alert(err);
            }
            fieldset.enable();
        });
        fieldset.enable();
    }

    async leave() {
    }
}
