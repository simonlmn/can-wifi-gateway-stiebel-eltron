
class NotFoundPage {
    #id

    constructor(id) {
        this.#id = id;
    }

    get label() {
        return '';
    }

    enter(view) {
        view.p(`Page "${this.#id}" not found.`, { class: 'notice' });
    }

    leave() {
    }
}

export class Navigation {
    #navView
    #pageView

    constructor(navView, pageView) {
        this.pages = new Map();
        this.currentPage = null;
        this.#navView = navView;
        this.#pageView = pageView;
    }

    add(id, page) {
        this.pages.set(id, page);
        this.#navView.link(page.label, { href: `#${id}` });
    }

    navigateTo(id) {
        if (id.startsWith('#')) {
            id = id.substring(1);
        }
        document.location.hash = id;
        this.currentPage?.leave();
        this.#pageView.clear();
        this.currentPage = this.pages.has(id) ? this.pages.get(id) : new NotFoundPage(id);
        this.currentPage.enter(this.#pageView);
    }
}
