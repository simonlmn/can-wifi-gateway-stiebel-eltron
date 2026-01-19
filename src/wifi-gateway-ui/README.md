# wifi-gateway-ui

Web-based UI for configuration and administration of the gateway.

## Software design and development constraints

The whole web-app is developed using vanialla JavaScript modules and the built-in DOM manipulation without using any third-party libraries or frameworks as it has to be able to be packed into a single HTML file that can be gzipped and embedded into the firmware's flash memory (aka PROGMEM).

The only exception regarding third-party libraries is the use of Simple.css (https://github.com/kevquirk/simple.css) as it is specifically designed to be a very lightweight but generally nice looking basic CSS framework. It is only about 10 kB (uncompressed) and is directly embedded into the main.html file as minified CSS.

Due to not using any libraries or frameworks, it is expected that the app might not run on all browsers the same or at all, as we don't use any kind of polyfills or similar. As this frontend is mainly used as a convenience feature in addition to using the gateway's API, this is acceptable.