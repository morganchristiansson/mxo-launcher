# r3d9.dll Crash Analysis

## Crash Location

**Address**: 0x75b81d31 (in r3d9.dll)  
**Instruction**: `int $0x29` (Windows fast fail)

## Root Cause

r3d9.dll performs a **processor feature check** during DllMain:

```asm
10071d23: push   $0x17              ; Feature ID = 23 (0x17)
10071d25: call   IsProcessorFeaturePresent
10071d2a: test   %eax,%eax
10071d2c: je     0x10071d33         ; Skip if feature present
10071d2e: push   $0x7               ; Fast fail code
10071d30: pop    %ecx
10071d31: int    $0x29              ; CRASH: Fast fail!
```

**Problem**: r3d9.dll checks if processor feature 0x17 (23) is present. If not, it triggers an immediate process termination via `int 0x29`.

## What is Feature 0x17?

Processor feature 23 in Windows API is likely one of:
- PF_XMMI64_INSTRUCTIONS_AVAILABLE (SSE2)
- PF_3DNOW_INSTRUCTIONS_AVAILABLE
- PF_SSE3_INSTRUCTIONS_AVAILABLE

Wine may not correctly report this feature as present, causing the check to fail.

## Solution

### Option A: Patch r3d9.dll

Skip the processor feature check:

```python
# Patch r3d9.dll at file offset 0x7112c
# Change: je 0x10071d33 (skip fast fail)
# To:     jmp 0x10071d33 (always skip)
# 
# At file offset 0x7112c (RVA 0x71d2c):
# 74 05 -> EB 05

with open('r3d9.dll', 'r+b') as f:
    f.seek(0x7112c)
    f.write(bytes([0xeb, 0x05]))  # jmp instead of je
```

### Option B: Force Feature Present in Wine

Set Wine to report all processor features as present (may not be possible without Wine patching).

## File Offsets

- **RVA**: 0x71d31 (crash location)
- **RVA**: 0x71d2c (conditional jump to patch)
- **File offset**: 0x7112c (where to patch)

## Test

After patching:
```bash
cd ~/MxO_7.6005
python3 << 'EOF'
with open('r3d9.dll', 'r+b') as f:
    f.seek(0x7112c)
    f.write(bytes([0xeb, 0x05]))
EOF
wine resurrections.exe
```

---
*Date: March 10, 2025*
*Discovery: Processor feature check causes fast fail*
