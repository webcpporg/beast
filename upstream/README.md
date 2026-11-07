# Shipped Beast protocol engine

Native upstream-equivalent types remain Boost.Beast types. On Wasm, protocol
algorithms use the same isolated Asio backend as WebCPP's timers, sockets and
Cobalt. Browser WebSocket/fetch and WASI host HTTP adapters remain distinct from
the guest protocol engine.

`pin.json` records the Boost 1.92 headers by SHA-256. `sync.py` relocates include
paths, namespaces and configuration macros into `detail/backend/`; it does not
apply protocol algorithm corrections. `generated.json` verifies the outputs.
Consumers use these already-shipped files with ordinary include paths. B2 does
not patch Boost or create an include-directory override.

```sh
python3 upstream/sync.py --boost-include /path/to/boost/include
python3 upstream/sync.py --boost-include /path/to/boost/include --check
```

This is a maintainer operation. Preserve original copyright and licence notices
and never edit generated files directly. The immutable original example corpus
is separate from the generated application counterparts.
