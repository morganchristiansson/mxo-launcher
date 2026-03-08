# SetMasterDatabase Callbacks Documentation

## Directory Structure

```
callbacks/
├── README.md              (this file)
├── TEMPLATE.md            (blank template for new callbacks)
├── TEMPLATE_GUIDE.md      (how to use the template)
├── CALLBACK_INDEX.md      (master index of all callbacks)
├── registration/          (VTable registration functions)
│   ├── RegisterCallback.md
│   ├── SetEventHandler.md
│   └── RegisterCallback2.md
├── lifecycle/            (Lifecycle event callbacks)
│   ├── OnInitialize.md
│   ├── OnShutdown.md
│   ├── OnError.md
│   └── OnException.md
├── network/              (Network event callbacks)
│   ├── OnConnect.md
│   ├── OnDisconnect.md
│   ├── OnPacket.md
│   └── OnDistributeMonitor.md
├── game/                 (Game event callbacks)
│   ├── OnPlayerJoin.md
│   ├── OnWorldUpdate.md
│   └── OnLogin.md
├── ui/                   (UI event callbacks)
│   ├── OnInput.md
│   └── OnFocus.md
└── monitor/              (System monitor callbacks)
    └── OnMonitorEvent.md
```

## How to Document a New Callback

1. **Read the guide**: [TEMPLATE_GUIDE.md](TEMPLATE_GUIDE.md)
2. **Copy the template**: `cp callbacks/TEMPLATE.md callbacks/[category]/[CallbackName].md`
3. **Fill in sections**: Follow the guide's instructions
4. **Update the index**: Add entry to [CALLBACK_INDEX.md](CALLBACK_INDEX.md)
5. **Check existing examples**: See documented callbacks for reference

## Callback Overview

- **Total Callbacks**: 50-100+
- **Categories**: 5 main categories
- **Registration**: Via VTable functions (indices 4, 23, 24)

## Quick Reference

### Registration Functions
- **RegisterCallback** (vtable[4], 0x10) - Initial registration
- **SetEventHandler** (vtable[23], 0x5C) - Event handler registration  
- **RegisterCallback2** (vtable[24], 0x60) - Alternative registration

### Storage Offsets
- **0x20**: Callback pointer 1
- **0x24**: Callback pointer 2
- **0x28**: Callback pointer 3
- **0x34**: Callback user data

## Documentation Status

| Category | Total | Documented | Status |
|----------|-------|------------|--------|
| Registration | 3 | 0 | ⏳ Pending |
| Lifecycle | 5 | 0 | ⏳ Pending |
| Network | 12 | 0 | ⏳ Pending |
| Game | 10 | 0 | ⏳ Pending |
| UI | 5 | 0 | ⏳ Pending |
| Monitor | 4 | 0 | ⏳ Pending |
| **Total** | **39** | **0** | ⏳ **In Progress** |

