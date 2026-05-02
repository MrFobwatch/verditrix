# Verditrix

A Pebble watchface with an animated X-shaped door that opens to reveal a complication, then closes after a hold period. Inspired by the Omnitrix aesthetic.

## Features

- Animated X-half door that opens and closes with eased timing
- Time displayed on the door faces while closed
- Complications: date, battery, weather — tap to scroll between them
- Iris-close exit animation when the door closes
- Sequence state machine: blink-in → hold → warning blink → close
- Tap to open; subsequent taps scroll to the next complication and reset the hold timer
- Rotating bezel nodes that advance with each complication scroll
- Weather via phone GPS + [Open-Meteo](https://open-meteo.com/) (no API key required)

## Platform Support

| Platform | Model | Display |
|----------|-------|---------|
| **emery** | Pebble Time 2 | 200×228 rectangular — rect border + L-brackets |
| **gabbro** | Pebble Round 2 | 260×260 round — circle border + arc decorations |

## Building

Requires the [Pebble SDK](https://developer.rebble.io/developer.pebble.com/sdk/index.html).

```bash
pebble build
```

### Run in emulator

```bash
# Rectangular (Pebble Time 2)
pebble install --emulator emery

# Round (Pebble Round 2)
pebble install --emulator gabbro
```

### Install on device

```bash
pebble install --cloudpebble
```

## License

MIT © 2026 Kevin
