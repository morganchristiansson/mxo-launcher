# SetMasterDatabase Verification

## Date: 2025-03-11
## Status: VERIFIED

---

## Verification Summary

### **launcher.exe does NOT call client.dll!SetMasterDatabase**

---

## Verification Method

### 1. Search for SetMasterDatabase string references
```bash
r2 -qc 's 0x40a400; pd 200' launcher.exe | grep -i "5a0f\|SetMaster"
```
**Result**: No references found

### 2. Search for push of string address
```bash
r2 -qc '/x 680f5a0c00; pd 30' launcher.exe
```
**Result**: No push instructions found for 0x4c5a0f

### 3. Search for xrefs to string
```bash
r2 -qc 'axt 0x4c5a0f' launcher.exe
```
**Result**: No code references to SetMasterDatabase string

---

## What launcher.exe Actually Does

| Function | Direction | Status |
|----------|-----------|--------|
| SetMasterDatabase | **EXPORTED** by launcher | Called BY client.dll |
| InitClientDLL | Calls client.dll | ✅ Present in code |
| RunClientDLL | Calls client.dll | ✅ Present in code |
| TermClientDLL | Calls client.dll | ✅ Present in code |
| ErrorClientDLL | Calls client.dll | ✅ Present in code |

---

## Correct Architecture

```
launcher.exe                      client.dll
    │                                │
    ├─── exports SetMasterDatabase ──┤
    │           (RECEIVES)           │
    │                                │
    ├─── calls ──> InitClientDLL ────┤
    │           (passes argc/argv)   │
    │                                │
    │              InitClientDLL:
    │              GetProcAddress(hLauncher, "SetMasterDatabase")
    │              Call launcher.SetMasterDatabase(clientDB)
    │                   │              │
    │                   │              │
    │                   <────── returns
    │                                │
    │              (setup complete)
    │                                │
    ├─── calls ──> RunClientDLL ─────┤
                   (game loop runs)
```

---

## Our Mistake

We were calling:
```c
client.dll!SetMasterDatabase(NULL)  // WRONG
```

The real launcher **NEVER calls this**. 

Instead:
1. launcher waits for `InitClientDLL` to callback to `launcher.SetMasterDatabase`
2. client.dll discovers SetMasterDatabase via GetProcAddress
3. client.dll calls it with its own internal database
4. launcher receives this and stores callback pointers

---

## Conclusion

**Passing NULL to client.dll!SetMasterDatabase might work but is not the correct architecture.**

The proper reimplementation must:
1. Export SetMasterDatabase from our launcher
2. Let client.dll discover and call it during InitClientDLL
3. Store the callback pointers properly
4. Then InitClientDLL will complete successfully

---

**Status**: Directionality verified - SetMasterDatabase is launcher EXPORT, not import
