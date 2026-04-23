# Crypto++ Class Mappings from launcher.exe Static RE

> Date: 2026-04-23  
> Binary: launcher.exe  
> Crypto++ version in tree: 8.9.0 (reference only; launcher was built ~2003–2005)  
> Status: living document — update when Ghidra findings change

This doc maps recovered OOAnalyzer / Ghidra class names to their original Crypto++ equivalents.
The launcher was statically linked against an older Crypto++ (likely 5.1–5.2).  Where the modern
8.9.0 tree has renamed or redesigned a class, the old name is noted.

---

## 1. RNG / RandomPool family

### 1.1 Outer wrapper — `CryptoInitHelper_0x4b42bc`

| Field | Address / VTable | Role |
|-------|-----------------|------|
| `this+0x00` | `0x4b695c` (`0x4bacbc` alternative) | Launcher-specific wrapper vtable |
| `this+0x04` | `0x4b68a8` | **Crypto++ RNG** (see §1.2) |
| `this+0x08` | `0x4b41e0` | **Crypto++ BufferedTransformation** (see §1.3) |

The object at `0x4b42bc` is **not** a pure Crypto++ class.  It is a launcher-owned wrapper that
embeds a Crypto++ RNG at offset `+4` and a `BufferedTransformation` at offset `+8`.

**Constructor** (`0x4686e0`):  
- Calls `CLTReferenceCountedBase_0x4b42b0` ctor at `+0`  
- Calls second `CLTReferenceCountedBase` ctor at `+4`  
- Assigns temporary vtables `0x4b9fa0` → `0x4bace0` → `0x4b41e0` during multi-stage init  
- Final vtables become `0x4b695c` (outer), `0x4b68a8` (RNG), `0x4b41e0` (BT)  
- Allocates buffer of caller-provided size (always `0x180` = 384 in observed call sites)  
- Allocates second buffer of size `0x40`  

**Call sites**  
- `0x4429d9` — margin bootstrap static init (`g_CryptoInitHelper_0x4f7bf4`)  
- `0x44557a` — `AuthBootstrap680ChildBase_ctor` field `+0x54`  
- `0x44d27a` — global auth helper (`DAT_004f80b8`)

---

### 1.2 RNG subobject — `0x4b68a8` → `CryptoPP::RandomPool` (old / `OldRandomPool`)

**Confidence: HIGH**

The vtable at `0x4b68a8` is a 32-entry table.  Key slots decompiled:

| VTable slot | Function address | Likely Crypto++ method |
|-------------|-----------------|------------------------|
| `+0x00` | `0x442950` | Destructor thunk |
| `+0x0c` | `0x468050` | `CanIncorporateEntropy()` (returns 1) |
| `+0x14` | `0x468c30` | `GenerateBlock(byte *out, size_t len)` |
| `+0x18` | `0x468640` | `GenerateIntoBufferedTransformation(...)` / `FillBytes` helper |
| `+0x1c` | `0x68d30` | `GenerateByte()` (read from internal pool, refill when exhausted) |
| `+0x20` | `0x68cb0` | `GenerateIntoBufferedTransformation` (full BufferedTransformation pump) |

**Why `RandomPool` (pre-5.5)?**

1. **Pool size** — constructor arg is always `0x180` (384).  This matches the default pool size of
   pre-5.5 `RandomPool`.
2. **Two-buffer layout** — `+0x14` (384-byte pool) and `+0x20` (64-byte key).  The old
   `RandomPool` maintained a `pool` and a `key`/`state` block.
3. **`InitializeCryptoState` (`0x468dc0`)** — creates a `CryptoPP_MicrosoftCryptoProvider`,
   allocates a temp buffer sized to the seed param (always `0x20` at call sites), seeds the RNG,
   then zero-frees the temp buffer.  This is the old `RandomPool::IncorporateEntropy` path.
4. **`RefillBufferedOutput` (`0x68dc0` family)** — the launcher helper calls into the RNG to
   refill a 384-byte output buffer from the keyed state, exactly matching the old `RandomPool`
   stir-and-output design.

In Crypto++ 8.9.0 the equivalent class is `OldRandomPool` (`randpool.h`).

---

### 1.3 BufferedTransformation subobject — `0x4b41e0` → `CryptoPP::BufferedTransformation`

**Confidence: MEDIUM-HIGH**

This vtable sits at `this+8` inside the RNG subobject.  It is a ~24-entry table with slots
that look like `Put` / `Put2` / `ChannelPut2` / `MessageEnd` / `MessageSeriesEnd`.

Old Crypto++ `RandomPool` (pre-5.5) inherited from `BufferedTransformation` so that it could
be used directly in filter pipelines (`Pump`, `TransferTo`, etc.).  The vtable size and slot
positions line up with the `BufferedTransformation` interface layout in Crypto++.

**Cross-reference:** the auth bootstrap source already hard-codes this value:

```cpp
// authbootstrap680.cpp
helperVtable08 = 0x004b41e0u;   // ResetAuthBootstrap680Field54Helper
```

---

### 1.4 Transient / alternative vtables

| Address | Role | Confidence |
|---------|------|------------|
| `0x4b9fa0` | 45-entry temp vtable set during RNG ctor | Medium — intermediate base before MI resolution |
| `0x4bace0` | 8-entry vtable also placed at `this+4` during ctor | Medium — `RandomNumberGenerator` pure-interface slice |
| `0x4b3e18` | Tiny 2-entry vtable placed at `this+8` during ctor | Low — launcher-specific stub; not a Crypto++ class |

---

## 2. RSA / OAEP Decryptor family

### 2.1 Base crypto object — `CMarginConnectionAuthBootstrapCrypto_0x4b6778`

**Confidence: MEDIUM-HIGH**

VTable: `0x4b6778` (10 entries, 40 bytes).  Constructor: `0x442440`.

The object has:
- `vfptr_0x0` at `+0x00`
- `vbptr_0x4` at `+0x04` → points to `DAT_004b6e30` (virtual inheritance table)
- `rsaModulus` at `+0x08` (actually an embedded `Integer`-like object)
- `field_0x0c` / `field_0x10` / `mbr_0x14` — MSVC vtable-adjustor thunks (`0x4b6db4`, `0x4b6300`)

This layout is characteristic of Crypto++ trapdoor-function objects that use **virtual
multiple inheritance**:

```
TF_ObjectImplBase
  └─ TF_ObjectImpl<TF_DecryptorBase, SchemeOptions, PrivateKey>
       └─ PK_FinalTemplate<...>
            └─ RSAES<OAEP<SHA1>>::Decryptor
```

The `vbptr` at `+4` and the adjustor thunks (`0x4b6db4`) are required because
`RSAFunction` inherits from both `TrapdoorFunction` and `X509PublicKey` (which itself
inherits virtually from `PublicKey` / `ASN1Object`).

---

### 2.2 Decryptor leaf — `CMarginConnectionAuthBootstrapDecryptor_0x4b69b4`

**Confidence: MEDIUM-HIGH**

VTable: `0x4b69b4` (16 entries, 64 bytes).  Constructor: `0x442b70`.

This class **inherits** from `CMarginConnectionAuthBootstrapCrypto_0x4b6778`:

```
CryptoInitHelper_0x4b42bc (outer wrapper)
  └─ RNG subobject at +4

CMarginConnectionAuthBootstrapDecryptor_0x4b69b4
  └─ CMarginConnectionAuthBootstrapCrypto_0x4b6778
       └─ CryptoPP::RSAES<OAEP<SHA1>>::Decryptor   (most likely)
```

**Key evidence:**

1. **`PerformRSADecryption` (`0x468130`)** — the vtable slot called by margin bootstrap
   (`CBaseMarginConnection_HandleCode2ForBootstrap` at `0x4429b0`) to decrypt the server
   challenge blob.

   Decompiled shape:
   ```cpp
   void* PerformRSADecryption(
       void* outputBuffer,
       CryptoInitHelper_0x4b42bc* cryptoContext,   // RNG for blinding
       uint32_t keySizeBytes,
       byte* encryptedChallengeData);
   ```

   Internally it:
   - Queries the modulus bit count → byte count
   - Imports ciphertext into a big-int (`CryptoPP_Int_0x4ba50c`)
   - Calls `CalculateInverse` (slot `+0xc` on the trapdoor function) passing the RNG
   - Exports the result to a byte buffer
   - Compares / validates the output length

   This is **exactly** the `TF_DecryptorBase::Decrypt` implementation in Crypto++.

2. **Encryptor counterpart is already source-owned** — `authbootstrap680.cpp` uses
   `CryptoPP::RSAES_OAEP_SHA_Encryptor` for the `AuthBootstrap680Raw08PublicKeyWorker`
   path.  It is overwhelmingly likely the launcher uses the matching
   `RSAES_OAEP_SHA_Decryptor` on the margin-connection side.

3. **Constructor vtable dance** — the ctor walks through three vtable stages
   (`0x4b6500` → `0x4b6604` → `0x4b6778`) plus a parallel adjustor thunk chain.
   This matches Crypto++ template-instantiation object construction where each base
   class constructor sets its own vtable slice before the most-derived ctor overwrites
   them all.

---

## 3. Summary table

| Launcher address / name | Crypto++ class (best match) | Confidence | Notes |
|------------------------|------------------------------|------------|-------|
| `0x4b42bc` wrapper (`0x4b695c` vtable) | *Launcher-specific* | **Certain** | Wraps RNG + BT subobjects |
| `0x4b68a8` vtable at `wrapper+4` | `CryptoPP::RandomPool` (old; `OldRandomPool` in 8.9.0) | **High** | Pool size 384, two-buffer design, MS CAPI seeding |
| `0x4b41e0` vtable at `wrapper+8` | `CryptoPP::BufferedTransformation` | **Medium-High** | Filter-pipeline interface for old RandomPool |
| `0x4bace0` transient vtable | `CryptoPP::RandomNumberGenerator` interface slice | **Medium** | Pure interface used during construction |
| `0x4b6778` vtable | `CryptoPP::InvertibleRSAFunction` / `TF_DecryptorBase` | **Medium-High** | vbptr + adjustor thunks match MI hierarchy |
| `0x4b69b4` vtable | `CryptoPP::RSAES<OAEP<SHA1>>::Decryptor` (`RSAES_OAEP_SHA_Decryptor`) | **Medium-High** | Inherits from `0x4b6778`, `PerformRSADecryption` matches `Decrypt` |
| `0x4b9fa0` | 45-entry temp vtable set during RNG ctor | **Medium** | Intermediate base before MI resolution; not in final object |
| `0x4b3e18` | Tiny 2-entry vtable placed at `this+8` during ctor | **Low** | Launcher-specific stub; no Crypto++ equivalent identified |
| `0x468130` `PerformRSADecryption` | `PK_Decryptor::Decrypt` / `TF_DecryptorBase::Decrypt` | **High** | Exact call shape and algorithm |

---

## 4. Hash / SHA1 family

### 4.1 Base hash context — `cls_0x4ba258` → `CryptoPP::SHA1` (or compatible)

**Confidence: HIGH**

VTable: `0x4ba258` (21 entries, 84 bytes).  Constructor: `0x455f70`.

This class provides a SHA1-compatible hash interface but is wrapped with launcher
base classes. The vtable uses Crypto++ `Algorithm` base class layout (proved by "unknown"
string at vtable slot).

**Class layout (36 bytes):**
| Offset | Field | Description |
|--------|-------|-------------|
| +0x00 | `cls_0x4b42b0` | Launcher base (CLTReferenceCountedBase) |
| +0x08 | `mbr_0x8` | Algorithm ID (1 for SHA1) |
| +0x0c | `mbr_0xc` | Hash state buffer pointer |
| +0x14 | `mbr_0x14` | Output size (words) |
| +0x18 | `mbr_0x18` | Output digest buffer |
| +0x1c | `mbr_0x1c` | Byte count (low) |
| +0x20 | `mbr_0x20` | Byte count (high) |

**Key evidence:**

1. **`HashContext_InitSHA1` (`0x46ea60`)** — sets SHA-1 IV constants exactly:
   ```
   0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0
   ```
   These are the official SHA1 initial value constants.

2. **"unknown" string proof** — vtable entry at offset +0x10 points to `0x41d880` which
   returns "unknown", matching Crypto++ `Algorithm::AlgorithmName()` default. This proves inheritance
   from Crypto++ `Algorithm` base class.

3. **Interface matches CryptoPP** — Update (vtable slot +0x04) and Final (implied)
   methods have identical signatures to `HashTransformation::Update` / `HashTransformation::Final`.

**Callers (10+ sites):**
- `0x43d417` — `cls_0x4b5670::ctor`
- `0x43d8a8` — `GenerateClientChunkHashes`
- `0x60b77` — `cls_0x4ba7d8::ctor`
- Many more in auth bootstrap and client code

---

### 4.2 SHA1 wrapper — `cls_0x4ba7d8` → `CryptoPP::SHA1::Init`

**Confidence: HIGH**

VTable: `0x4ba7d8`. Constructor: `0x460b70`.

This class extends `cls_0x4ba258` and provides SHA1-specific initialization.
Calls `HashContext_InitSHA1` to set the SHA-1 IV in the state buffer.

---

## 5. Crypto++ "unknown" fingerprint — Algorithm base class proof

### 5.1 Evidence: `0x41d880` = Crypto++ `Algorithm::AlgorithmName()`

The function at `0x41d880` returns the string "unknown":

```cpp
void AuthBootstrap680Field54Helper_ResetUnknownString(undefined4 param_1, undefined4 *param_2)
{
    *param_2 = 0;
    param_2[1] = 0;
    param_2[2] = 0;
    StringReallocateContent(param_2, "unknown", "");
    return param_2;
}
```

This is **direct copy** of Crypto++ `cryptlib.h:624`:

```cpp
virtual std::string AlgorithmName() const {return "unknown";}
```

### 5.2 VTables using "unknown" as fallback

**58 vtable entries** across the launcher point to `0x41d880` as their `AlgorithmName()`
method — proving these classes inherit from Crypto++ `Algorithm`:

| VTable Address | Offset→"unknown" | Notes |
|---------------|------------------|-------|
| `0x4b41e0` | +0x10 | BufferedTransformation |
| `0x4b6778` | +0x0c | RSA crypto object |
| `0x4b68a8` | +0x10 | RandomPool |
| `0x4b42bc` | +0x10 | CryptoInitHelper outer |
| `0x4b5670` | +0x10 | Hash wrapper |
| ... (53 more) | Various | Various Crypto++ objects |

The Crypto++ vtable layout (MSVC x86) places `AlgorithmName()` at offset +0x04.
The launcher classes use launcher-owned base classes but inherit Crypto++ virtual-method layout.

---

## 6. Open questions / negative results

- **Exact Crypto++ version** — the launcher was likely built against 5.1.x or 5.2.x.
  The `OldRandomPool` in 8.9.0 is a faithful reproduction, but slot-level vtable offsets
  may differ by a few entries.

- **`0x4b41e0` slot-by-slot** — we have not decompiled every entry in the
  `BufferedTransformation` vtable to confirm exact method names (`Put2`, `ChannelPut2`,
  etc.).  The size and rough layout match; individual slots remain unverified.

- **Encryptor decryptor asymmetry** — the source side already owns the encryptor
  (`RSAES_OAEP_SHA_Encryptor`).  The decryptor mapping is inferred from inheritance,
  `PerformRSADecryption` shape, and the encryptor counterpart.  We have not yet
  decompiled the decryptor vtable slot-by-slot to prove every method.

- **`CryptoPP_MicrosoftCryptoProvider_0x4baed8`** — the class used inside
  `InitializeCryptoState` is believed to be `CryptoPP::MicrosoftCryptoProvider` (or the
  internal `Win32CryptoProvider` that existed in old Crypto++).  Its exact 8.9.0
  equivalent may be `AutoSeededRandomPool` or `OS_GenerateRandomBlock`, but the old
  direct CAPI wrapper is gone.

---

## 5. Source anchors

| Source file | Anchor / comment |
|-------------|-----------------|
| `matrixstaging/runtime/src/libltmessaging/crypto_init_helper.h` | `0x4f7bf4` global layout, `0x4b695c`/`0x4b68a8`/`0x4b41e0` vtable pointers |
| `matrixstaging/runtime/src/libltmessaging/crypto_init_helper.cpp` | `EnsureCryptoContextInitialized` replicates `0x4429b0` static init block |
| `matrixstaging/game/src/libltclientlogin/authbootstrap680.cpp` | `0x4b695c`/`0x4b68a8`/`0x4b41e0` hard-coded in `ResetAuthBootstrap680Field54Helper` |
| `matrixstaging/runtime/src/libltcrypto/auth_internal.h` | `RSAES_OAEP_SHA_Encryptor` source-owned encryptor path |

---

## 6. How to update this doc

1. Run Ghidra decompile on any unmapped slot function.
2. Compare its shape to `third_party/cryptopp890/*.h`.
3. Update the mapping table above with the new address → name.
4. If confidence changes (higher or lower), move the row and update the note.
5. If a negative result disproves a mapping, record it explicitly in §4.
