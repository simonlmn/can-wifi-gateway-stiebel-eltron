
function mapToCondition(value) {
    if (value < 15) {
        return 'excellent';
    } else if (value < 30) {
        return 'good';
    } else if (value < 50) {
        return 'fair';
    } else {
        return 'poor';
    }
}

export class StatusPage {
    #client

    constructor(client) {
        this.#client = client;
    }

    get label() {
        return 'Status';
    }

    async enter(view) {
        view.h1('Node Status');
        this.statusList = view.dl();
        this.statusState = view.p();
        this.autoStatusRefresh(true);

        view.h1('Logs');
        this.logs = view.pre();
        this.logRefreshToggle = view.checkbox(view.label('Auto-refresh'), { checked: true }, (checked) => this.autoLogRefresh(checked));
        this.logsState = view.p();      
        this.autoLogRefresh(true);
    }

    async leave() {
        this.autoStatusRefresh(false);
        this.autoLogRefresh(false);
    }

    autoStatusRefresh(enable) {
        clearTimeout(this.statusTimeout);
        
        if (enable) {
            this.#refreshStatus()
                .finally(() => {
                    clearTimeout(this.statusTimeout);
                    this.statusTimeout = setTimeout(() => this.autoStatusRefresh(true), 5000);
                });
        }
    }

    autoLogRefresh(enable) {
        clearTimeout(this.logsTimeout);
        
        if (enable) {
            this.#refreshLogs()
                .finally(() => {
                    clearTimeout(this.logsTimeout);
                    this.logsTimeout = setTimeout(() => this.autoLogRefresh(true), 5000);
                });
        }
    }

    async #refreshStatus() {
        this.statusState.attribute('class', null);
        this.statusState.content(`<small>Loading...</small>`);
        try {
            const response = await this.#client.get('/node/status');
            const data = await response.json();
            this.statusList.clear();
            this.statusList.add('Chip ID', data.chipId);
            this.statusList.add('CPU frequency', `${data.cpuFreq} MHz`);
            this.statusList.add('Reset reason', data.resetReason);
            this.statusList.add('Sketch MD5', data.sketchMD5);
            this.statusList.add('Uptime', `${data.millis} ms (epoch ${data.epoch})`);
            this.statusList.add('Supply voltage', `${data.chipVcc} V`);
            this.statusList.add('Free heap', `${data.freeHeap} B`);
            this.statusList.add('Max. free block size', `${data.maxFreeBlockSize} B`);
            this.statusList.add('Heap fragmentation', `<progress value="${data.heapFragmentation}" max="100" class="condition-${mapToCondition(data.heapFragmentation)}">${data.heapFragmentation}%</progress>`);
            this.statusList.add('WiFI RSSI', data.wifiRssi);
            this.statusList.add('IP address', data.ip);
            this.statusState.attribute('class', null);
            this.statusState.content(`<small>Updated on ${new Date().toISOString()}.</small>`);
        } catch (err) {
            this.statusState.attribute('class', 'notice');
            this.statusState.content(`${err}`);
        }
    }

    async #refreshLogs() {
        this.logsState.attribute('class', null);
        this.logsState.content(`<small>Loading...</small>`);
        try {
            const response = await this.#client.get('/node/logs');
            const data = await response.text();
            this.logs.clear();
            this.logs.content(data);
            this.logs.scrollToBottom();
            this.logsState.attribute('class', null);
            this.logsState.content(`<small>Updated on ${new Date().toISOString()}.</small>`);
        } catch (err) {
            this.logsState.attribute('class', 'notice');
            this.logsState.content(`${err}`);
        }
    }
}
