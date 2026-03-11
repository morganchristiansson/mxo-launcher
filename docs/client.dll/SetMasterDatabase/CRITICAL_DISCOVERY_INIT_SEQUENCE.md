# Critical Discovery: SetMasterDatabase Not Called by InitClientDLL

## Date: 2025-03-10 14:30

## The Mystery

After extensive testing with file logging, we confirmed:

**SetMasterDatabase is NOT called during InitClientDLL**

### Test Results

```
InitClientDLL returned: 0
SetMasterDatabase was NOT called
```

This means one of the following:

## Theory 1: SetMasterDatabase is called AFTER InitClientDLL

Maybe the sequence is:
```
1. LoadLibraryA("client.dll")
2. InitClientDLL()  ← Returns success
3. SetMasterDatabase()  ← Called separately?
4. RunClientDLL()
```

## Theory 2: SetMasterDatabase must be called BY US

Maybe launcher.exe calls SetMasterDatabase after InitClientDLL?
But client.dll exports SetMasterDatabase, so this seems wrong.

## Theory 3: SetMasterDatabase is called during RunClientDLL

Maybe RunClientDLL calls our exported SetMasterDatabase?

## Theory 4: Wrong parameters prevent SetMasterDatabase call

Maybe InitClientDLL is supposed to call SetMasterDatabase but our parameters are wrong so it doesn't?

## What We Know

### Facts
1. ✅ launcher.exe EXPORTS SetMasterDatabase (ordinal 1)
2. ✅ client.dll IMPORTS SetMasterDatabase (seen in relay trace)
3. ✅ InitClientDLL returns 0 (success) with our parameters
4. ❌ SetMasterDatabase is NOT called during InitClientDLL
5. ❓ SetMasterDatabase might be called during RunClientDLL?

### Parameters We're Passing to InitClientDLL

```c
InitClientDLL(
    nullptr,            // param1: unknown
    nullptr,            // param2: unknown
    hClient,            // param3: client.dll handle
    nullptr,            // param4: unknown
    &g_OurMasterDB,     // param5: our master database
    0,                  // param6: version/build info
    0,                  // param7: flags
    nullptr             // param8: unknown
);
```

## Next Experiments

### Test 1: Call SetMasterDatabase OURSELVES before InitClientDLL

```c
// Get SetMasterDatabase from client.dll
auto setMasterDB = GetProcAddress(hClient, "SetMasterDatabase");

// Call it ourselves
setMasterDB(&g_OurMasterDB);

// Then call InitClientDLL
InitClientDLL(...);
```

### Test 2: Check if SetMasterDatabase is called during RunClientDLL

Add more logging around RunClientDLL to see if SetMasterDatabase gets called then.

### Test 3: Study original launcher.exe more carefully

Maybe we're missing a step between loading client.dll and calling InitClientDLL.

## Code to Test

### launcher_test_theory1.cpp

Test calling SetMasterDatabase ourselves BEFORE InitClientDLL.

### launcher_test_theory2.cpp

Test if SetMasterDatabase is called during RunClientDLL with more logging.

## Conclusion

The initialization sequence is still not fully understood. We have successfully:
- ✅ Loaded all dependencies
- ✅ Loaded client.dll
- ✅ Called InitClientDLL successfully (returns 0)
- ❌ NOT gotten SetMasterDatabase to be called

The next step is to test different initialization orders and parameter combinations.
