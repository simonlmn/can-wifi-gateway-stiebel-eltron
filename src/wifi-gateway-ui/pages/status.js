
export class StatusPage {
    #client

    constructor(client) {
        this.#client = client;
    }

    get label() {
        return 'Status';
    }

    enter(view) {
        view.h1('Node Status');
        this.statusList = view.dl();
        this.statusState = view.p();
        this.toggleStatusRefresh(true);

        view.h1('Logs');
        this.logs = view.pre();
        this.logRefreshToggle = view.checkbox(view.label('Auto-refresh'), { checked: true }, (checked) => this.toggleLogRefresh(checked));
        this.logsState = view.p();      
        this.toggleLogRefresh(true);
    }

    leave() {
        this.toggleStatusRefresh(false);
        this.toggleLogRefresh(false);
    }

    toggleStatusRefresh(enable) {
        clearTimeout(this.statusTimeout);
        
        if (enable) {
            this.#refreshStatus();
        }
    }

    toggleLogRefresh(enable) {
        clearTimeout(this.logsTimeout);
        
        if (enable) {
            this.#refreshLogs();
        }
    }

    #refreshStatus() {
        this.statusState.attribute('class', null);
        this.statusState.content(`<small>Loading...</small>`);
        this.#client.get('/node/status')
            .then(async (response) => {
                const data = await response.json();
                this.statusList.clear();
                this.statusList.add('Chip ID', data.chipId);
                this.statusList.add('CPU frequency', `${data.cpuFreq} MHz`);
                this.statusList.add('Reset reason', data.resetReason);
                this.statusList.add('Sketch MD5', data.sketchMD5);
                this.statusList.add('Uptime', `${data.millis} ms (epoch ${data.epoch})`);
                this.statusList.add('Supply voltage', `${data.chipVcc} V`);
                this.statusList.add('Free heap', `${data.freeHeap} B`);
                this.statusList.add('Heap fragmentation', data.heapFragmentation);
                this.statusList.add('Max. free block size', `${data.maxFreeBlockSize} B`);
                this.statusList.add('WiFI RSSI', data.wifiRssi);
                this.statusList.add('IP address', data.ip);
                this.statusState.attribute('class', null);
                this.statusState.content(`<small>Updated on ${new Date().toISOString()}.</small>`);
                this.statusTimeout = setTimeout(() => this.#refreshStatus(), 5000);
            })
            .catch((err) => {
                this.statusState.attribute('class', 'notice');
                this.statusState.content(`${err}`);
                this.statusTimeout = setTimeout(() => this.#refreshStatus(), 5000);
            });
    }

    #refreshLogs() {
        this.logsState.attribute('class', null);
        this.logsState.content(`<small>Loading...</small>`);
        this.#client.get('/node/logs')
            .then(async (response) => {
                const data = await response.text();
                this.logs.clear();
                this.logs.content(data);
                this.logs.scrollToBottom();
                this.logsState.attribute('class', null);
                this.logsState.content(`<small>Updated on ${new Date().toISOString()}.</small>`);
                this.logsTimeout = setTimeout(() => this.#refreshLogs(), 5000);
            })
            .catch((err) => {
                this.logsState.attribute('class', 'notice');
                this.logsState.content(`${err}`);
                this.logsTimeout = setTimeout(() => this.#refreshLogs(), 5000);
            });
    }
}
