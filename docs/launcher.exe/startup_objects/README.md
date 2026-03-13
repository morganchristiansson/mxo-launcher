# launcher.exe startup-owned objects

This folder documents launcher-owned globals and heap objects that are part of the original startup path before `client.dll!InitClientDLL` and `RunClientDLL`.

## Canonical objects

- `0x4d2c58_ILTLoginMediator_Default.md`
  - runtime interface pointer used by nopatch setup, client startup, and teardown

- `0x4d2c58_RESOLUTION_MECHANISM.md`
  - how the launcher resolves and fills the `ILTLoginMediator.Default` pointer slot through registry state

- `0x4d6304_network_engine.md`
  - heap-allocated launcher object built at startup and passed both to the mediator and to `InitClientDLL`

- `0x4d3368_CLauncher.md`
  - global main launcher object whose methods drive the original startup path and supply arg7 fields

- `0x4d3d54_INTERFACE_REGISTRY.md`
  - global registry object used to resolve named interface binders into live pointer slots

- `0x4d3d54_SERVICE_NODE_4ADD34.md`
  - resolver/service node class used by the registry list at `+0x18`

## Why this matters

The current custom launcher failures are best explained by missing launcher-owned objects and registrations here, not by a need to patch `client.dll` memory.

For launcher-owned auth flow work that uses the standalone auth probe as a fast reference harness, see:
- `../auth/README.md`

## Key relationship

Static analysis currently supports this chain:

1. global constructor infrastructure prepares a runtime interface pointer at `0x4d2c58`
2. startup builds a heap object and stores it at `0x4d6304`
3. launcher registers / hands `0x4d6304` to the interface from `0x4d2c58`
4. launcher passes both values into `client.dll!InitClientDLL`
5. launcher later tears both down in the mirrored cleanup path
