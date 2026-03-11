# SetMasterDatabase

## Two functions share this name

### launcher.exe
- export: `SetMasterDatabase`
- address: `0x4143f0`

### client.dll
- export: `SetMasterDatabase`
- address: `0x6229d760`

They should be documented separately in our reasoning, even when kept in the same topic folder.

## Hard boundary from static analysis

In the launcher startup path currently traced (`0x40b739..0x40b7d5` and `0x40a4d0..0x40a6fb`), `launcher.exe` does **not** directly call `client.dll!SetMasterDatabase()`.

That means the following should **not** be documented as original-launcher behavior:

```c
client_SetMasterDatabase(NULL);
InitClientDLL(...);
RunClientDLL();
```

That sequence is test-harness behavior only.

## Current canonical understanding

1. `launcher.exe!SetMasterDatabase` is a real launcher export.
2. `client.dll!SetMasterDatabase` is also a real client export.
3. The original launcher startup path we have statically traced does not show a direct launcher-to-client call into `client.dll!SetMasterDatabase`.
4. Therefore the reimplementation should not be built around that direct call unless new binary evidence proves it.

## Reimplementation implication

The current problem is more likely that we have not yet reproduced the launcher-owned objects and call frame that surround `InitClientDLL`, rather than that we forgot to manually call `client.dll!SetMasterDatabase()`.

## Open question

Whether `client.dll` later discovers or uses `launcher.exe!SetMasterDatabase` through another path is still a separate question, but it must be answered from binary evidence, not promoted from harness behavior.
