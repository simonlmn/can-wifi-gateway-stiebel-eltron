# APIs

## Interfaces

### HTTP

The HTTP API is always enabled and listens on port 80. Due to device constraints, only plain HTTP is supported (instead of HTTPS) and therefore also no authentication.

#### GET /api/data

Returns the [DataEntryCollection](#DataEntryCollection) with all currently known/configured data entrys with the most recent data (if any) received from the heatpump.

See [example-data/data.json](example-data/data.json) for an example of an actual response from this endpoint.

#### GET|PUT /api/data/{device-type}/{device-address}/{value-id}

##### GET
Returns a specific [DataEntry](#DataEntry).

Responds with:
 * 200 if successful.
 * 404 if the entry was not found.

##### PUT
Request to write a new value to a _writable_ data entry. The value must be in the entry specific format and unit (e.g. `12.3 °C`, `true` or `20 min`) according to its definition.

Responds with:
 * 200 if the request would be processed successfully and the `validateOnly` query parameter was set.
 * 202 if the request was processed successfully. Since the value is written asynchronously to the device, the actual value change happens later or not at all if the device does not allow or process the request for some reason.
 * 400 if the request is invalid, e.g. the entry is not writable or the provided new value and/or unit does not match the definition.
 * 404 if the entry was not found.

#### GET|POST /api/subscriptions

#### DELETE /api/subscriptions/{device-type}/{device-address}/{value-id}

#### GET|POST /api/writable

#### DELETE /api/writable/{device-type}/{device-address}/{value-id}

#### GET /api/definitions

#### GET /api/devices

#### GET /api/system/status

#### GET /api/system/logs

#### GET|PUT /api/system/log-level

#### PUT /api/system/log-level/{category}

#### GET|PUT /api/system/config

#### GET|PUT /api/system/config/{category}

#### POST /api/system/stop

#### POST /api/system/reset

#### POST /api/system/factory-reset

### MQTT

The MQTT client is not enabled by default, but can be enabled through configuration.

When configured, it will publish all updates to data entries to an entry specific topic, using the [DataEntry](#DataEntry) schema. All topics share a common base topic which can be changed through configuration.

At the moment, no authentication or encryption is supported.

## JSON Structures

### DataEntry

| Property         | Data Type | Description                                                                                               |
| :--------------- | :-------: | :-------------------------------------------------------------------------------------------------------- |
| id               | number    | The <value-id> defining what this value means (e.g. 12).                                                  |
| name             | string    | A (mostly) human readable name for the value ID (e.g. "outside temperature").                             |
| accessMode       | string    | Possible access mode (e.g. "Readable", see `enum ValueAccessMode` for all possible values).               |
| unit             | string    | Unit of measurement, if applicable (e.g. "°C" for temperatures, see `enum Unit` for all possible values). |
| rawValue         | string    | Raw 16-bit value in hexadecimal notation as read/written on the underlying protocol (e.g. "0xFFF1").      |
| value            | _dynamic_ | Interpreted value (e.g. -1.5). Interpretation and data type depend on the value definition.               |
| lastUpdate       | string    | Local date and time when the value was last received.                                                     |
| source           | string    | Device type and address where this value comes from.                                                      |
| subscribed       | boolean   | Indication if this value is subscribed, i.e. if it is actively monitored for changes.                     |
| writable         | boolean   | Indication if this value is configured for write access (only possible with a matching `accessMode`).     |

Example:
```
{
    "id": 12,
    "name": "outside temperature",
    "accessMode": "Readable",
    "unit": "°C",
    "rawValue": "0xFFF1",
    "value": -1.5,
    "lastUpdate": "2024-01-11T20:12:15.219",
    "source": "SYS/0",
    "subscribed": true,
    "writable": false
}
```

### DataEntryCollection

| Property         | Data Type                                       | Description                                                                                                 |
| :--------------- | :---------------------------------------------: | :---------------------------------------------------------------------------------------------------------- |
| retrievedOn      | string                                          | Local date and time when this response was generated.                                                       |
| totalItems       | number                                          | Total number of stored data items.                                                                          |
| actualItems      | number                                          | Number of data items included in this response (may be less than totalItems due to filtering).              |
| items            | Map<string, Map<string, Map<string, DataEntry>>> | Object with data items, grouped into sub objects by "device type", "device address" and finally "value id". |

```
{
    "retrievedOn": "2024-01-11T20:12:15.673",
    "totalItems": 5,
    "items": {
        "SYS": {
            "0": {
                "12": {
                    "id": 12,
                    "name": "outside temperature",
                    "accessMode": "Readable",
                    "unit": "°C",
                    "rawValue": "0xFFF1",
                    "value": -1.5,
                    "lastUpdate": "2024-01-11T20:12:15.219",
                    "source": "SYS/0",
                    "subscribed": true,
                    "writable": false
                }
            }
        }
    },
    "actualItems": 1
}
```
