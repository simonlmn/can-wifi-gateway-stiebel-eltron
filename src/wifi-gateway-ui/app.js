import { HttpClient } from './client.js';
import { ContainerView } from './view.js'
import { Navigation } from './navigation.js';
import { DefinitionsPage } from './pages/definitions.js';
import { StatusPage } from './pages/status.js';
import { ConfigPage } from './pages/config.js';

const client = new HttpClient('http://192.168.3.225/api', 5000);

let navView = new ContainerView(document.querySelector('nav'));
let pageView = new ContainerView(document.querySelector('main'));

let navigation = new Navigation(navView, pageView);
navigation.add('status', new StatusPage(client));
navigation.add('config', new ConfigPage(client));
navigation.add('definitions', new DefinitionsPage(client));

window.addEventListener('popstate', (event) => {
    navigation.navigateTo(document.location.hash);
});

if (document.location.hash) {
    navigation.navigateTo(document.location.hash);
} else {
    navigation.navigateTo('status');
}

