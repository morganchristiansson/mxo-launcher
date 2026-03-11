# launcher.exe startup-owned objects

This folder documents launcher-owned globals and heap objects that are part of the original startup path before `client.dll!InitClientDLL` and `RunClientDLL`.

## Canonical objects

- `0x4d2c58_ILTLoginMediator_Default.md`
  - runtime interface pointer used by nopatch setup, client startup, and teardown

- `0x4d6304_network_engine.md`
  - heap-allocated launcher object built at startup and passed both to the mediator and to `InitClientDLL`

## Why this matters

The current custom launcher failures are best explained by missing launcher-owned objects and registrations here, not by a need to patch `client.dll` memory.

## Key relationship

Static analysis currently supports this chain:

1. global constructor infrastructure prepares a runtime interface pointer at `0x4d2c58`
2. startup builds a heap object and stores it at `0x4d6304`
3. launcher registers / hands `0x4d6304` to the interface from `0x4d2c58`
4. launcher passes both values into `client.dll!InitClientDLL`
5. launcher later tears both down in the mirrored cleanup path
