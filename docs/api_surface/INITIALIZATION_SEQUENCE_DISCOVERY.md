# CRITICAL DISCOVERY: SetMasterDatabase Direction

## Date: 2025-03-10 13:30

## The Revelation

After disassembling the original launcher.exe, I discovered:

**SetMasterDatabase is EXPORTED by launcher.exe (ordinal 1, address 0x004143f0)**

This means the flow is OPPOSITE of what we thought!

## Correct Initialization Sequence

```
1. launcher.exe loads client.dll via LoadLibraryA
2. launcher.exe calls GetProcAddress for:
   - InitClientDLL
   - RunClientDLL  
   - TermClientDLL
   - ErrorClientDLL
   
3. launcher.exe prepares parameters for InitClientDLL
4. launcher.exe calls InitClientDLL(parameters...)
   └─> Inside InitClientDLL:
       └─> client.dll calls launcher.exe!SetMasterDatabase(masterDB_ptr)
           to register the launcher's API functions
       
5. launcher.exe calls RunClientDLL()
6. Game runs
7. launcher.exe calls TermClientDLL() on exit
```

## Key Insight

**WE DON'T CALL SetMasterDatabase - client.dll calls OURS!**

We need to:
1. EXPORT SetMasterDatabase from our launcher
2. Have a global Master Database ready
3. When client.dll calls our SetMasterDatabase, store the pointer it provides
4. Use that pointer to call launcher functions via vtables

## InitClientDLL Parameters

From disassembly at 0x0040a55c - 0x0040a5a4:

InitClientDLL takes **8 parameters** (not 6!), loaded from these addresses:

```assembly
push edx              ; [esp+32] flags from 0x4d2c69
push eax              ; [esp+28] combined version/build from [ebx+0xac] and [ebx+0xa8]  
push eax              ; [esp+24] master database ptr from 0x4d2c58
push ecx              ; [esp+20] param from 0x4d6304
push edx              ; [esp+16] param from 0x4d2c4c
push eax              ; [esp+12] client.dll handle from 0x4d2c50
push ecx              ; [esp+8]  param from 0x4d2c60
push edx              ; [esp+4]  param from 0x4d2c5c
call InitClientDLL
add esp, 0x20        ; cleanup 8 parameters (32 bytes)
```

Parameter sources:
- 0x4d2c50: client.dll handle (HMODULE)
- 0x4d2c58: Master Database pointer
- 0x4d2c4c: Unknown parameter 1
- 0x4d2c5c: Unknown parameter 2
- 0x4d2c60: Unknown parameter 3
- 0x4d6304: Unknown parameter 4
- [ebx+0xac]: Version info (masked with 0xffffff)
- [ebx+0xa8]: Build info (shifted left by 24 bits)
- 0x4d2c69: Flags byte

## What We Need to Implement

### 1. Export SetMasterDatabase

```c
// Global storage for the master database client.dll provides
static void* g_ClientMasterDatabase = NULL;

// EXPORT this function - client.dll will call it!
extern "C" __declspec(dllexport) void __stdcall SetMasterDatabase(void* pMasterDatabase) {
    g_ClientMasterDatabase = pMasterDatabase;
    // Client.dll has registered its API with us
}
```

### 2. Call InitClientDLL with 8 parameters

```c
// Prepare parameters
HMODULE hClient = LoadLibraryA("client.dll");
void* masterDB = CreateOurMasterDatabase(); // Our API for client to call
// ... get other parameters ...

// Call InitClientDLL
typedef int (*InitClientDLL_t)(void*, void*, void*, void*, void*, void*, void*, void*);
InitClientDLL_t init = (InitClientDLL_t)GetProcAddress(hClient, "InitClientDLL");

int result = init(
    param1,  // from 0x4d2c5c
    param2,  // from 0x4d2c60
    hClient, // client.dll handle
    param3,  // from 0x4d2c4c
    masterDB,// our master database
    version, // combined version/build
    flags,   // from 0x4d2c69
    param4   // from 0x4d6304
);
```

### 3. Use .def file to export SetMasterDatabase

Create `launcher.def`:
```
EXPORTS
    SetMasterDatabase @1
```

Compile with:
```bash
i686-w64-mingw32-g++ -Wall -O2 -o launcher.exe launcher.cpp -Wl,--export-all-symbols
# Or better:
i686-w64-mingw32-g++ -Wall -O2 -o launcher.exe launcher.cpp -Wl,--def,launcher.def
```

## Next Steps

1. Create launcher that EXPORTS SetMasterDatabase
2. Prepare Master Database structure for client.dll to use
3. Call InitClientDLL with proper 8 parameters
4. Let client.dll call our SetMasterDatabase
5. Test the sequence

## Files

- Original launcher.exe analysis: disassembly at 0x0040a4d0
- SetMasterDatabase export: 0x004143f0
- InitClientDLL call: 0x0040a5a4
