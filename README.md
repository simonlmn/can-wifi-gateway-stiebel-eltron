# CAN/WiFi Gateway for Stiebel Eltron Heatpumps
Gateway to communicate with Stiebel Eltron heat pump controllers via an HTTP-based API. It communicates with the heatpump controller using the native, proprietary CAN protocol and provides the features and data through a more integration friendly HTTP API to the outside. Developed for deployment on an ESP8266 board with a separate CAN adapter/bridge.

For a schematic and PCB layout to build a device able to run this go to https://github.com/simonlmn/can-wifi-gateway-hardware.

## Similar projects and references

Of course, this is not the only or even the first attempt to reverse engineer the CAN bus protocol of Stiebel Eltron devices and to build a custom gateway to integrate these into various smart home solutions or similar things. What follows is a list in no particular order of websites, blog posts or forum discussions which in one way or another helped to build this solution:

 * http://juerg5524.ch/
 * https://www.haustechnikdialog.de/Forum/t/79101/Stiebel-Eltron-WPC10-ueber-CAN-Bus-auslesen-steuern
 * https://elkement.blog/2016/08/03/hacking-my-heat-pump-part-1-can-bus-testing-with-uvr1611/
 * http://messpunkt.org/blog/?p=7
 * https://github.com/Hunv/can2mqtt/tree/d9925cabcf278f3237240f8a44b852395e5d3c1e/can2mqtt_core/can2mqtt_core/Translator/StiebelEltron
 * https://community.home-assistant.io/t/configured-my-esphome-with-mcp2515-can-bus-for-stiebel-eltron-heating-pump/366053
