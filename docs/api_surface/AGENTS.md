# API Surface Agent

## Agent Role: API_ANALYST

**Primary Responsibility**: API Surface Extraction and Documentation

---

## Current Status

- **Priority**: 1 (CRITICAL)
- **Progress**: 80%
- **Binaries Analyzed**: launcher.exe, client.dll
- **Callbacks Documented**: 13/39+ (33%)

---

## Quick Start

```bash
# Create full disassembly (do this first)
objdump -d ../../launcher.exe > /tmp/launcher_disasm.txt
objdump -d ../../client.dll > /tmp/client_disasm.txt

# Find strings with addresses
strings -t x ../../client.dll | grep "pattern"

# Search disassembly
grep -B10 -A10 "pattern" /tmp/launcher_disasm.txt
```

---

## Essential Commands (Use These)

### 1. Full Disassembly to File

**MOST USEFUL** - Create searchable disassembly:

```bash
# Disassemble entire binary to file
objdump -d ../../launcher.exe > /tmp/launcher_disasm.txt
objdump -d ../../client.dll > /tmp/client_disasm.txt

# Check file size
wc -l /tmp/launcher_disasm.txt
```

### 2. String Searches

Find diagnostic strings and error messages:

```bash
# Find strings with addresses (critical for validation)
strings -t x ../../client.dll | grep "error message"

# Find all strings matching pattern
strings ../../client.dll | grep -i "callback\|packet\|caps"

# Find specific string and get address
strings -t x ../../client.dll | grep "One or more of the caps"
# Output: 93f630 One or more of the caps bits...
```

### 3. Memory Dumps

Dump VTables and data structures:

```bash
# Dump VTable entries (show raw hex)
objdump -s --start-address=0x4a9988 --stop-address=0x4a9a00 ../../launcher.exe

# Dump specific range
objdump -s --start-address=0x4a94a0 --stop-address=0x4a94a8 ../../launcher.exe
```

### 4. Disassembly Search

Search within disassembly file:

```bash
# Search with context (before/after lines)
grep -B10 -A10 "pattern" /tmp/launcher_disasm.txt

# Find specific address
grep -B20 -A20 "401090:" /tmp/launcher_disasm.txt

# Find all calls to vtable offset
grep "ff 52 64\|ff 50 64" /tmp/client_disasm.txt

# Count occurrences
grep -c "ff 52 64" /tmp/client_disasm.txt
```

### 5. VTable Call Analysis

Find and analyze vtable function calls:

```bash
# Find most commonly called vtable offsets
grep -o "call.*\*0x[0-9a-f]*(" /tmp/client_disasm.txt | \
  sed 's/call.*\*//' | tr -d '(' | sort | uniq -c | sort -rn | head -20

# Find all calls to specific vtable offset
grep "ff 52 64\|ff 50 64" /tmp/client_disasm.txt | head -20

# Get context for call sites
grep -B15 "ff 52 64" /tmp/client_disasm.txt | head -50
```

### 6. Parameter Analysis

Determine function parameters by examining calls:

```bash
# Find push instructions before a call
grep -B15 "6200caed:" /tmp/client_disasm.txt | grep "push"

# Count parameters pushed
grep -B20 "call.*0x64" /tmp/client_disasm.txt | grep -E "push|mov.*%ecx"
```

---

## Validation Workflows

### Workflow 1: Validate Callback Implementation

**Goal**: Determine if a callback is real or a stub

```bash
# Step 1: Find VTable entry
objdump -s --start-address=0x4a9988 --stop-address=0x4a9b00 ../../launcher.exe

# Step 2: Get function address from VTable
# (Parse hex dump to extract function pointer)

# Step 3: Find function implementation
grep -A30 "function_address:" /tmp/launcher_disasm.txt

# Step 4: Check if stub (returns error code)
# Look for: jmp *0x... (jump to error code)
# Or: mov $0x...,%eax; ret (return error code)

# Step 5: Verify in client.dll
grep "call.*\*0x64(" /tmp/client_disasm.txt | wc -l
# If 0 calls, function not used

# Step 6: Check if return value is used
grep -A5 "call.*\*0x64(" /tmp/client_disasm.txt | grep -E "test|cmp|je|jne"
# If no test/cmp, return value is ignored
```

**Indicators of STUB**:
- Function returns error code (0x80000678, etc.)
- Client.dll ignores return value
- No test/cmp instructions after call
- Diagnostic string exists but never used

### Workflow 2: Verify Function Name

**Goal**: Check if documented name matches actual behavior

```bash
# Step 1: Find all call sites
grep "ff 52 64\|ff 50 64" /tmp/client_disasm.txt > /tmp/callsites.txt

# Step 2: For each call site, count parameters
grep -B15 "6200caed:" /tmp/client_disasm.txt | grep "push" | wc -l

# Step 3: Check multiple call sites
for addr in 6200caed 62010e11 62019793; do
  echo "=== $addr ==="
  grep -B15 "${addr}:" /tmp/client_disasm.txt | grep "push"
done

# Step 4: If parameter counts vary, name is WRONG
# A real function has fixed signature
```

**Indicators of WRONG NAME**:
- Parameter count varies between calls (0, 2, 3 params)
- Call pattern doesn't match documented signature
- No validation/error handling after calls

### Workflow 3: Find Diagnostic Strings

**Goal**: Locate error messages and map to error codes

```bash
# Step 1: Find string
strings -t x ../../client.dll | grep "error message"
# Output: 93f630 One or more of the caps bits...

# Step 2: Convert to full address
# String at offset 0x93f630 -> address 0x6293f630

# Step 3: Find references to string address
grep "6293f630" /tmp/client_disasm.txt

# Step 4: Analyze error code mapping
# Look for: mov $0x88760064,%eax
# This means error code 0x88760064 maps to this string
```

### Workflow 4: Analyze VTable Structure

**Goal**: Map VTable entries to functions

```bash
# Step 1: Dump VTable
objdump -s --start-address=0x4a9988 --stop-address=0x4a9b00 ../../launcher.exe

# Step 2: Parse entries (each 4 bytes)
# 4a9988: 00 10 40 00  -> 0x00401000 (vtable[0])
# 4a998c: a0 12 40 00  -> 0x004012a0 (vtable[1])
# ...

# Step 3: Calculate slot for offset
python3 -c "
offset = 0x64
index = offset // 4
print(f'Offset 0x{offset:02x} = vtable[{index}]')
"

# Step 4: Find function at entry
grep -A20 "401000:" /tmp/launcher_disasm.txt
```

---

## Binary Information

### launcher.exe
- **Path**: `../../launcher.exe`
- **Size**: 5,267,456 bytes
- **Format**: PE32 executable (GUI) Intel 80386
- **Sections**: 6 sections
- **Entry Point**: 0x0048be94

### Key Addresses (launcher.exe)
| Address | Purpose |
|---------|---------|
| 0x004a9988 | Primary VTable |
| 0x004143f0 | SetMasterDatabase export |
| 0x00401000 | .text section start |
| 0x004a9000 | .rdata section start (VTables) |

### client.dll
- **Path**: `../../client.dll`
- **Size**: 11 MB
- **Format**: PE32 DLL (GUI) Intel 80386
- **Sections**: 6 sections

### Key Addresses (client.dll)
| Address | Purpose |
|---------|---------|
| 0x62001000 | .text section start |
| 0x6293f630 | Diagnostic string (caps validation) |
| 0x629f14a0 | Master Database pointer |

---

## Analysis Patterns

### Pattern: Identifying Stubs

Real vs. Stub function:

**Real Function**:
```assembly
401090: push   %ebp
401091: mov    %esp,%ebp
401093: mov    0xc(%ebp),%eax    ; Use parameters
401096: cmp    $0x1,%eax         ; Actual logic
...
4010dd: ret    $0xc              ; Clean stack
```

**Stub Function**:
```assembly
48b784: jmp    *0x4a94a0         ; Jump to error
# OR
48b784: mov    $0x80000678,%eax  ; Return error code
48b78b: ret
```

### Pattern: Parameter Counting

Count pushes before call:

```assembly
# 0 parameters:
6200caeb: mov    (%ecx),%edx
6200caed: call   *0x64(%edx)      ; <- call

# 2 parameters:
62010de2: push   $0x1             ; param 1
62010e10: push   %eax             ; param 2
62010e11: call   *0x64(%edx)      ; <- call

# 3 parameters:
6201978a: push   $0x1             ; param 1
6201978c: push   %ebx             ; param 2
62019790: push   %eax             ; param 3
62019793: call   *0x64(%edx)      ; <- call
```

### Pattern: Return Value Usage

Check if return value is used:

**Return value USED**:
```assembly
call   *0x64(%edx)
test   %eax,%eax        ; Check return value
je     error_handler    ; Conditional jump
```

**Return value IGNORED**:
```assembly
call   *0x64(%edx)
mov    0x188(%esi),%al  ; Use different register
test   %al,%al          ; Unrelated check
```

### Pattern: Fabrication Markers

Signs of stub/placeholder:

1. **Error code encodes vtable offset**:
   - Error: 0x88760064
   - Lower byte: 0x64 = vtable offset
   - Not a coincidence - artificially constructed

2. **Diagnostic string exists but unused**:
   - String in binary
   - No code path displays it
   - Prepared for feature never implemented

3. **Inconsistent parameters**:
   - Function called with 0, 2, 3 params
   - Real function has fixed signature
   - Suggests generic/multi-purpose slot

---

## Common Tasks

### Task: Document a New Callback

```bash
# 1. Find VTable entry
objdump -s --start-address=0x4a9988 --stop-address=0x4a9b00 ../../launcher.exe

# 2. Calculate slot
python3 -c "print(f'vtable[{0x64 // 4}]')"

# 3. Get function address
# Parse from hex dump

# 4. Analyze implementation
grep -A50 "address:" /tmp/launcher_disasm.txt

# 5. Check if stub
# Look for error code returns

# 6. Find call sites in client.dll
grep "call.*\*0xoffset" /tmp/client_disasm.txt

# 7. Verify parameters
grep -B20 "call.*\*0xoffset" /tmp/client_disasm.txt | grep "push"

# 8. Create documentation
cp callbacks/TEMPLATE.md callbacks/category/CallbackName.md
# Fill in details
```

### Task: Find All VTable Calls

```bash
# Count calls per vtable offset
grep -o "call.*\*0x[0-9a-f]*(" /tmp/client_disasm.txt | \
  sed 's/call.*\*//' | tr -d '(' | sort | uniq -c | sort -rn

# Output example:
#   4223 0x8     <- vtable[2] most called
#   2998 0x18    <- vtable[6]
#    231 0x64    <- vtable[25]
```

### Task: Find Related Strings

```bash
# Search both binaries
strings ../../launcher.exe | grep -i "keyword"
strings ../../client.dll | grep -i "keyword"

# With addresses
strings -t x ../../client.dll | grep -i "keyword"
```

---

## Troubleshooting

### Problem: Can't find string reference

```bash
# String might be in different section
objdump -s ../../client.dll | grep "string bytes"

# Or search entire disassembly
grep -i "string" /tmp/client_disasm.txt
```

### Problem: VTable address not in .rdata

```bash
# Check all sections
objdump -h ../../launcher.exe

# VTable might be in .data (runtime constructed)
objdump -s -j .data ../../launcher.exe | grep -A5 "address"
```

### Problem: Function too large to analyze

```bash
# Get just the prologue (first 20 instructions)
grep -A20 "address:" /tmp/launcher_disasm.txt | head -25

# Focus on stack setup and parameter access
grep "ebp" /tmp/launcher_disasm.txt | head -10
```

---

## Key Findings

### API Architecture
- **Single Export**: SetMasterDatabase is the only exported function
- **VTable-based**: 117 vtables with 5,145 function pointers
- **Runtime Discovery**: All API functions discovered at runtime
- **Bidirectional**: Both launcher→client and client→launcher calls

### Stub Detection
- **VTable slots exist** but implementations return error codes
- **Diagnostic strings prepared** but never displayed
- **Client.dll calls stubs** but ignores return values
- **Error codes encode vtable offsets** (fabrication marker)

### Parameter Analysis
- **Count pushes before call** to determine parameter count
- **Varying parameters** = wrong function name or generic slot
- **Fixed signature** = real callback with correct name

---

## Tips for Success

### Do's
- ✅ Create full disassembly files first
- ✅ Use `strings -t x` to get addresses
- ✅ Count parameters at multiple call sites
- ✅ Check if return value is used
- ✅ Look for fabrication markers
- ✅ Validate against both binaries

### Don'ts
- ❌ Trust documentation without disassembly verification
- ❌ Assume fixed parameter count without checking
- ❌ Ignore return value usage patterns
- ❌ Assume VTable slot = real implementation
- ❌ Skip stub detection workflow

---

## Confidence Levels

When documenting, always state confidence:

- **High**: Verified with disassembly, multiple call sites analyzed
- **Medium**: Found in binary, but not fully validated
- **Low**: Based on pattern/documentation only
- **Unknown**: Function exists but purpose unclear

---

---

## Validation Guide

### Overview

**Purpose**: Ensure documentation matches actual binary implementation

**Critical Finding**: 75% of game callback documentation was fabricated (6 out of 8 files)

**Root Cause**: Documentation created without binary validation, based on pattern assumptions

---

### Validation Workflow

#### Step 1: Initial String Search

**Goal**: Verify the function/class/callback exists in the binary

```bash
# Search for exact function name
strings ../../launcher.exe | grep -i "FunctionName"
strings ../../client.dll | grep -i "FunctionName"

# Search for related terms
strings ../../launcher.exe | grep -i "keyword"
strings ../../client.dll | grep -E "(On|Callback|Event).*keyword"
```

**Decision Matrix**:
- ✅ **Found**: Proceed to Step 2
- ❌ **Not Found**: Function does not exist → **FABRICATED**
  - Create `FunctionName_validation.md` with evidence
  - Update `FunctionName.md` with DEPRECATED warning
  - Stop validation

**Example** (OnPlayerJoin):
```bash
$ strings launcher.exe | grep -i "OnPlayerJoin"
(no results)  # ❌ FABRICATED
```

#### Step 2: Find Function Implementation

**Goal**: Locate the actual code for the function

```bash
# Method A: Search for diagnostic strings
strings -t x ../../launcher.exe | grep "error message"
# Get address, then find references in disassembly

# Method B: If class method, search for class::method pattern
strings -t x ../../launcher.exe | grep "ClassName::MethodName"
# Example: "CLTEvilBlockingLoginObserver::OnLoginEvent"

# Method C: Find by VTable slot (if callback)
# 1. Dump VTable
objdump -s --start-address=0x4a9988 --stop-address=0x4a9b00 ../../launcher.exe

# 2. Parse function pointer from VTable entry
# 3. Search for function address in disassembly
grep -A50 "address:" /tmp/launcher_disasm.txt
```

**Decision Matrix**:
- ✅ **Found Implementation**: Proceed to Step 3
- ❌ **Not Found**: Check if stub or doesn't exist
  - Look for error code returns
  - Check multiple binaries
  - May be fabricated

#### Step 3: Analyze Function Signature

**Goal**: Verify documented parameters and return type

```bash
# Analyze function prologue
grep -A30 "function_address:" /tmp/launcher_disasm.txt

# Look for:
# - Calling convention (thiscall, cdecl, stdcall)
# - Parameter access (ebp+8, ebp+c, etc.)
# - Return value (eax before ret)

# Example thiscall pattern:
# push %ebp
# mov  %esp,%ebp
# mov  0x8(%ebp),%eax   ; First parameter
# mov  0xc(%ebp),%edx   ; Second parameter
# ...
# ret  $0x8             ; Clean 8 bytes (2 params)
```

**Parameter Counting**:
```bash
# Count parameters at multiple call sites
for addr in addr1 addr2 addr3; do
  echo "=== $addr ==="
  grep -B15 "${addr}:" /tmp/client_disasm.txt | grep "push" | wc -l
done

# If counts vary, documentation is WRONG
```

**Calling Convention Detection**:
- **thiscall**: `this` in ECX, callee cleans stack
- **cdecl**: Caller cleans stack (`add $N,%esp` after call)
- **stdcall**: Callee cleans stack (`ret $N`)

#### Step 4: Verify Architecture Pattern

**Goal**: Confirm the documented architecture matches reality

```bash
# Check for C callback pattern
# Look for: function pointer stored in structure
grep "mov.*function_ptr" /tmp/launcher_disasm.txt

# Check for C++ observer pattern
# Look for: class::method strings
strings ../../launcher.exe | grep "::.*Event\|::.*Callback"

# Check for vtable-based
# Look for: calls through vtable
grep "call.*\*.*%edx\|call.*\*.*%ecx" /tmp/launcher_disasm.txt
```

**Common Architectures**:
1. **C Callback**: Function pointer stored in structure, called indirectly
2. **C++ Observer**: Virtual method on observer class
3. **VTable-based**: Function called through object's vtable

#### Step 5: Find Related Diagnostic Strings

**Goal**: Locate error messages, debug strings, or logging

```bash
# Search for diagnostic strings
strings -t x ../../launcher.exe | grep -i "function\|event\|error"

# Find references to strings in code
grep "string_address" /tmp/launcher_disasm.txt

# Check if strings are actually used
# Look for push instructions before logging calls
```

**String Usage Check**:
```bash
# Find string
strings -t x ../../client.dll | grep "error message"
# Output: 93f630 One or more of the caps bits...

# Convert to full address (add base)
# 0x93f630 -> 0x6293f630

# Find references
grep "6293f630" /tmp/client_disasm.txt
# If no references, string is unused → likely stub
```

#### Step 6: Check Cross-References

**Goal**: Verify function is actually used

```bash
# Find all call sites
grep "call.*function_address" /tmp/client_disasm.txt

# Count usage
grep -c "call.*function_address" /tmp/client_disasm.txt

# If 0 calls, function is not used (may be stub or internal)
```

#### Step 7: Create Validation Report

**Goal**: Document findings comprehensively

**Template** (`FunctionName_validation.md`):

```markdown
# FunctionName Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
**Analysis Date**: YYYY-MM-DD

---

## Critical Findings

### 1. **[STATUS]** - [Summary]

[Description of what was found]

**Evidence**:
\`\`\`bash
$ strings launcher.exe | grep -i "FunctionName"
[output]
\`\`\`

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Function Existence | FunctionName callback | [Result] | ✅/❌ |
| Function Type | C callback | [Result] | ✅/❌ |
| Parameters | [Type] | [Result] | ✅/❌ |
| Return Value | [Type] | [Result] | ✅/❌ |

**Overall Status**: ✅/❌ **[VERDICT]**

---

## Correct Implementation

[If found, document actual implementation]

\`\`\`cpp
// Actual function signature
void ClassName::MethodName(int param);
\`\`\`

---

**Validation Status**: ✅/❌ **[VERDICT]**
```

---

### Red Flags: Signs of Fabrication

#### 🚩 Red Flag 1: No String Evidence

```bash
$ strings launcher.exe | grep -i "OnPlayerJoin"
(no results)
```

**Meaning**: Function name never appears in binary → **FABRICATED**

#### 🚩 Red Flag 2: "Inferred" Confidence

Documentation states:
- "Medium confidence - inferred from game event patterns"
- "Inferred from similar callbacks"
- "Pattern matching suggests"

**Meaning**: No binary validation was performed → **LIKELY FABRICATED**

#### 🚩 Red Flag 3: Identical Template Pattern

Multiple callbacks have:
- Same parameter count
- Same structure size
- Same registration pattern
- Same "ProcessEvent vtable index 6, offset 0x18"

**Meaning**: Template filling without validation → **LIKELY FABRICATED**

#### 🚩 Red Flag 4: Vague Diagnostic Strings

Documentation lists:
- "Player %d joined session %d" - Inferred
- "Reason: %d" - Inferred
- "Exact addresses to be confirmed"

**Meaning**: Strings don't exist in binary → **FABRICATED**

#### 🚩 Red Flag 5: Wrong Architecture

Documentation describes C callback:
```c
int OnEvent(EventStruct* event, void* userData);
```

Reality is C++ observer:
```cpp
class Observer {
    void OnEvent(int eventNumber);
};
```

**Meaning**: Misunderstood architecture → **WRONG PARADIGM**

---

### Validation Checklist

Before marking documentation as complete:

#### Existence Validation
- [ ] Function/class name found in strings
- [ ] Diagnostic strings found
- [ ] Implementation located in disassembly
- [ ] Cross-references found in code

#### Signature Validation
- [ ] Parameter count verified at multiple call sites
- [ ] Parameter types inferred from usage
- [ ] Return type verified
- [ ] Calling convention identified

#### Architecture Validation
- [ ] Documented architecture matches reality
- [ ] Correct pattern (C callback vs C++ observer)
- [ ] Registration/access pattern verified
- [ ] VTable entries correct (if applicable)

#### Usage Validation
- [ ] Function actually used in code
- [ ] Return value checked/used
- [ ] Error handling present
- [ ] Diagnostic strings used

#### Documentation Quality
- [ ] Binary addresses provided
- [ ] Assembly examples included
- [ ] Confidence level stated
- [ ] Validation date recorded

---

### Common Validation Scenarios

#### Scenario 1: Function Doesn't Exist

```bash
# Search for function
$ strings launcher.exe | grep -i "OnPlayerJoin"
(no results)

# Search for related terms
$ strings launcher.exe | grep -i "player.*join\|join.*player"
(no results)

# Search entire disassembly
$ grep -i "join" /tmp/launcher_disasm.txt
(no relevant results)
```

**Conclusion**: ❌ **FABRICATED**

**Action**:
1. Create validation report
2. Deprecate documentation
3. Document what actually exists (if anything)

#### Scenario 2: Wrong Architecture

```bash
# Documented as C callback
# Search for C++ class pattern
$ strings launcher.exe | grep "::OnEvent"
CLTEvilBlockingLoginObserver::OnLoginEvent(): Got event we're waiting for!
CLTLoginObserver_PassThrough::OnLoginEvent(): Event# %d

# Found C++ observer pattern instead
```

**Conclusion**: ❌ **WRONG ARCHITECTURE**

**Action**:
1. Identify actual architecture
2. Rewrite documentation with correct pattern
3. Update all related callbacks

#### Scenario 3: Correct but Incomplete

```bash
# Function exists
$ strings launcher.exe | grep -i "OnLoginEvent"
CLTEvilBlockingLoginObserver::OnLoginEvent(): Event# %d

# Implementation found
$ grep -A30 "41b520:" /tmp/launcher_disasm.txt
# Shows thiscall, takes int parameter

# But documented as taking structure
```

**Conclusion**: ⚠️ **PARTIALLY CORRECT**

**Action**:
1. Verify parameter types
2. Update signature
3. Provide assembly evidence

#### Scenario 4: Stub Implementation

```bash
# Function exists in vtable
$ objdump -s --start-address=0x4a9988 --stop-address=0x4a9b00 launcher.exe

# But implementation returns error
$ grep -A10 "401090:" /tmp/launcher_disasm.txt
401090: jmp *0x4a94a0    ; Jump to error code
```

**Conclusion**: ⚠️ **STUB**

**Action**:
1. Document as stub
2. Note diagnostic strings exist
3. Explain error code significance

---

### Validation Report Examples

#### Example 1: Fabricated Function

See `callbacks/game/OnPlayerJoin_validation.md`

**Key Points**:
- No string evidence
- No implementation found
- Similar pattern to other fabricated callbacks
- Documented what actually exists (admin commands)

#### Example 2: Wrong Architecture

See `callbacks/game/OnLoginEvent_validation.md`

**Key Points**:
- Function exists but wrong paradigm
- Documented as C callback, actual is C++ observer
- Wrong parameters (structure vs int)
- Wrong return type (int vs void)
- Complete rewrite required

#### Example 3: Correct Documentation

See `callbacks/game/OnLoginEvent.md` (corrected version)

**Key Points**:
- Correct C++ observer pattern
- Accurate method signatures
- Assembly evidence provided
- String references documented
- Binary addresses included

---

### Best Practices

#### Do's

✅ **Always validate against binary** before writing documentation

✅ **Use multiple search methods** (strings, disassembly, cross-references)

✅ **Check multiple call sites** to verify parameter count

✅ **Document actual implementation**, not assumptions

✅ **Include assembly evidence** in documentation

✅ **State confidence level** honestly

✅ **Create validation reports** for all findings

#### Don'ts

❌ **Assume patterns imply existence**

❌ **Trust "inferred" documentation** without verification

❌ **Fill templates** without binary analysis

❌ **Ignore missing string evidence**

❌ **Document what "should" exist** instead of what exists

❌ **Skip validation** for "obvious" callbacks

---

### Validation Tools

#### Essential Tools

```bash
# String extraction with addresses
strings -t x binary > strings.txt

# Full disassembly
objdump -d binary > disasm.txt

# Section dump (for VTables, data)
objdump -s --start-address=X --stop-address=Y binary

# Section headers
objdump -h binary

# Symbol table
objdump -t binary 2>/dev/null || nm binary 2>/dev/null
```

#### Analysis Pipeline

```bash
# 1. Create working files
strings -t x ../../launcher.exe > /tmp/launcher_strings.txt
strings -t x ../../client.dll > /tmp/client_strings.txt
objdump -d ../../launcher.exe > /tmp/launcher_disasm.txt
objdump -d ../../client.dll > /tmp/client_disasm.txt

# 2. Search strings first (fastest)
grep -i "keyword" /tmp/launcher_strings.txt
grep -i "keyword" /tmp/client_strings.txt

# 3. Search disassembly (slower)
grep -B10 -A10 "pattern" /tmp/launcher_disasm.txt

# 4. Cross-reference between binaries
grep "address" /tmp/launcher_strings.txt
grep "address" /tmp/launcher_disasm.txt
```

---

### Validation Metrics

Track these metrics to measure validation quality:

| Metric | Target | Measurement |
|--------|--------|-------------|
| String Evidence | 100% | % of callbacks with string references |
| Implementation Found | 100% | % of callbacks with implementation located |
| Parameter Verification | 100% | % of signatures verified at call sites |
| Architecture Correctness | 100% | % with correct paradigm documented |
| Assembly Evidence | 100% | % with disassembly examples provided |
| Fabrication Rate | 0% | % of fabricated documentation |

**Current Status** (2025-03-08):
- **Fabrication Rate**: 75% (6 out of 8 game callbacks were fabricated)
- **Architecture Errors**: 100% of remaining callbacks had wrong architecture
- **Validation Coverage**: 100% (all game callbacks now validated)

---

### Post-Validation Actions

#### If Fabricated

1. **Create validation report** with evidence
2. **Deprecate documentation** with warning
3. **Document what actually exists** (if anything)
4. **Update summary files** (VALIDATION_SUMMARY.md)
5. **Check related files** for same fabrication pattern

#### If Incorrect Architecture

1. **Identify actual architecture** (C callback vs C++ observer)
2. **Rewrite documentation** with correct paradigm
3. **Update all related callbacks** with same issue
4. **Provide assembly evidence** for new documentation
5. **Update summary files**

#### If Correct

1. **Mark as validated** with date
2. **Add validation metadata** (confidence, evidence)
3. **No changes needed** to documentation

---

## Quick Reference

```bash
# Create disassembly
objdump -d file > /tmp/disasm.txt

# Find strings with addresses
strings -t x file | grep "pattern"

# Dump memory range
objdump -s --start-address=X --stop-address=Y file

# Search disassembly
grep -B10 -A10 "pattern" /tmp/disasm.txt

# Count vtable calls
grep -o "call.*\*0x[0-9a-f]*(" /tmp/disasm.txt | sed 's/call.*\*//' | tr -d '(' | sort | uniq -c | sort -rn

# Analyze parameters
grep -B20 "call_address" /tmp/disasm.txt | grep "push"

# Validate function exists
strings file | grep -i "functionname"
```
