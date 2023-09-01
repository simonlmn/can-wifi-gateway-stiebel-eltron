
function combineSignals(...signals) {
    const controller = new AbortController();

    for (const signal of signals) {
        if (signal) {
            signal.addEventListener('abort', () => { controller.abort(signal.reason) });
        }
    }

    return controller.signal;
}

export class HttpClient {
    #baseUrl
    #timeout

    constructor(baseUrl, timeout) {
        this.#baseUrl = baseUrl;
        this.#timeout = timeout;
    }

    async fetch(resource, options = {}) {
        const { timeout = this.#timeout } = options;

        const timeoutController = new AbortController();
        const timeoutId = setTimeout(() => timeoutController.abort(new DOMException(`Request timed out after ${timeout} ms.`, "TimeoutError")), timeout);

        try {
            const response = await fetch(`${this.#baseUrl}${resource}`, {
                ...options,
                signal: combineSignals(timeoutController.signal, options.signal)
            });

            if (!response.ok) {
                throw new Error(`HTTP error: ${response.status} ${response.statusText}.`)
            }

            return response;
        } finally {
            clearTimeout(timeoutId);
        }
    }

    async get(resource, options = {}) {
        return this.fetch(resource, { ...options, method: 'GET' });
    }

    async post(resource, body, options = {}) {
        return this.fetch(resource, { ...options, method: 'POST', body: body });
    }

    async put(resource, body, options = {}) {
        return this.fetch(resource, { ...options, method: 'PUT', body: body });
    }

    async delete(resource, options = {}) {
        return this.fetch(resource, { ...options, method: 'DELETE' });
    }
}