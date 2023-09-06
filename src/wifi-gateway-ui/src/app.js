import { HttpClient } from './client.js';
import { ContainerView } from './view.js'
import { Navigation } from './navigation.js';
import { DefinitionsPage } from './pages/definitions.js';
import { StatusPage } from './pages/status.js';
import { ConfigPage } from './pages/config.js';
import { DataPage } from './pages/data.js';
import { SystemPage } from './pages/system.js';

// For development environments, the UI is loaded from a local dev server and not the gateway itself, so we need to obtain the gateway address.
// We store that in the browsers local storage so it does not need to be entered with every reload.
// For production environments, the gatewayAddress is always empty and therefore it uses the same address as the gateway automatically.
let gatewayAddress = localStorage.getItem('gatewayAddress') ?? '';
if (!gatewayAddress && document.location.hostname === "127.0.0.1") {
    gatewayAddress = `http://${prompt("Enter gateway address:")}`;
    localStorage.setItem('gatewayAddress', gatewayAddress);
}

const client = new HttpClient(`${gatewayAddress}/api`, 5000);

let navView = new ContainerView(document.querySelector('nav'));
let pageView = new ContainerView(document.querySelector('main'));

let navigation = new Navigation(navView, pageView);
navigation.add('status', new StatusPage(client));
navigation.add('data', new DataPage(client));
navigation.add('config', new ConfigPage(client));
navigation.add('definitions', new DefinitionsPage(client));
navigation.add('system', new SystemPage(client));

window.addEventListener('popstate', async () => {
    await navigation.navigateTo(document.location.hash);
});

if (document.location.hash) {
    await navigation.navigateTo(document.location.hash);
} else {
    await navigation.navigateTo('status');
}

