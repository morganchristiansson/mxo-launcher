# Crypto++ Class Mappings from launcher.exe Static RE

> Date: 2026-04-23  
> Binary: launcher.exe  
> Crypto++ version in tree: 8.9.0 (reference only; launcher was built ~2003–2005)  
> Status: living document — update when Ghidra findings change

This doc maps recovered OOAnalyzer / Ghidra class names to their original Crypto++ equivalents.
The launcher was statically linked against an older Crypto++ (likely 5.1–5.2).  Where the modern
8.9.0 tree has renamed or redesigned a class, the old name is noted.

---

## 1. Implemented RNG / RandomPool mappings

These RNG/helper classes are now treated as implemented/proven in source. Keep this section factual
and address-oriented.

#### `0x004b42bc` / `0x004b695c` = launcher-owned CryptoInitHelper wrapper

| Address | Mapped name |
|---|---|
| `0x4686e0` | wrapper construct with pool size `0x180` |
| `0x4429d9` | global crypto-helper init call site |
| `0x44557a` | auth-bootstrap child `+0x54` helper init call site |
| `0x44d27a` | global auth helper init call site |

#### `0x004b68a8` = `CryptoPP::RandomPool` / modern `CryptoPP::OldRandomPool`

| Address | Mapped name |
|---|---|
| `0x442950` | dtor thunk |
| `0x468050` | `CanIncorporateEntropy()` |
| `0x468c30` | `GenerateBlock(byte*, size_t)` |
| `0x468640` | buffered `GenerateBlock` helper feeding auth-bootstrap child `+0x84..+0x93` |
| `0x468d30` | `GenerateByte()`-like helper |
| `0x468cb0` | buffered-transformation pump helper |
| `0x468dc0` | `IncorporateEntropy(...)` / seed helper |

#### `0x004b41e0` = `CryptoPP::BufferedTransformation`

| Address | Mapped name |
|---|---|
| `0x004b41e0` | buffered-transformation interface vtable |
| `0x004372c0` | first visible vtable entry in current listing |

### 1.4 Transient / alternative vtables

| Address | Role | Confidence |
|---------|------|------------|
| `0x4b9fa0` | 45-entry temp vtable set during RNG ctor | Medium — intermediate base before MI resolution |
| `0x4bace0` | 8-entry vtable also placed at `this+4` during ctor | Medium — `RandomNumberGenerator` pure-interface slice |
| `0x4b3e18` | Tiny 2-entry vtable placed at `this+8` during ctor | Low — launcher-specific stub; not a Crypto++ class |

---

## 2. RSA / OAEP Decryptor family

### 2.1 Root base — `0x4b42b0` = older `CryptoPP::Algorithm`

**Confidence: HIGH**

The old name we had in source, `CLTReferenceCountedBase_0x4b42b0`, is misleading.
This vtable is much better explained as an **older Crypto++ `Algorithm` base**.

VTable: `0x4b42b0` (3 entries).

| Slot | Address | Best match |
|------|---------|------------|
| `+0x00` | `0x41cda0` | deleting destructor |
| `+0x04` | `0x437b50` | default `Clone()`-style stub |
| `+0x08` | `0x41d880` | `AlgorithmName()` returning `"unknown"` |

Strongest evidence:
- `0x41d880` zeroes a string and writes `"unknown"`
- modern Crypto++ still has `Algorithm::AlgorithmName() const { return "unknown"; }`
- the launcher build predates the later `AlgorithmProvider()` virtual, so a 3-slot layout fits
  an **older Crypto++ release**

This finding matters because it explains why so many of the OAEP / SHA1 / RNG objects share the
same tiny root vtable even when they are not launcher-specific helper classes.

---

### 2.2 Decryptor family

| VTable / Function | Crypto++ class / method |
|---|---|
| `0x004b69b4` | `CryptoPP::RSAES_OAEP_SHA_Decryptor` |
| `0x442b70` | ctor-state decryptor constructor |
| `0x442e20` | intermediate ctor-state decryptor constructor |
| `0x443220` | complete-object decryptor constructor |
| `0x465d70` | load embedded `CryptoPP::InvertibleRSAFunction` / `RSA::PrivateKey` material |
| `0x468130` | decrypt / trapdoor inverse path |
| `0x464b80` | query max unpadded plaintext length |
| `0x467640` | OAEP/SHA1 pad helper |
| `0x467780` | OAEP/SHA1 unpad helper |

### 2.3 Encryptor counterpart — `0x4b75e4`

| VTable / Function | Crypto++ class / method |
|---|---|
| `0x004b74a8` | ctor-state `CryptoPP::RSAES_OAEP_SHA_Encryptor` family vtable |
| `0x004b75e4` | `CryptoPP::RSAES_OAEP_SHA_Encryptor` |
| `0x447120` | construct-from-public-key |
| `0x468ea0` | query encrypted output length |
| `0x468f00` | encrypt into packet builder / chunk loop |
| `0x468280` | encrypt one plaintext chunk |
| `0x4382c0` | create inner `CryptoPP::PK_DefaultEncryptionFilter` |
| `0x447880` | dtor / leaf teardown |
| `0x4471f0` | adjustor thunk to leaf dtor (`this -= 0x50`) |
| `0x446d80` | adjustor thunk to ctor-state dtor (`this -= 0x50`) |

### 2.3.1 Public/private RSA key core

| VTable / Function | Crypto++ class / method |
|---|---|
| `0x004b659c / 0x004b6680` | `CryptoPP::RSAFunction` (`CryptoPP::RSA::PublicKey`) |
| `0x4420f0` | construct public-key core from modulus/exponent |
| `0x004b672c` | `CryptoPP::InvertibleRSAFunction` (`CryptoPP::RSA::PrivateKey`) |
| `0x442250` | construct private-key core |
| `0x004b6778` | launcher margin/auth wrapper over RSA private-key core |

### 2.5.3 Auth pubkey validator family

Once source uses the concrete Crypto++ leaf types directly, the useful documentation here is the
address-to-class / address-to-method mapping rather than the noisy ctor-state OOAnalyzer labels.

#### `0x004b7580` = `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier`

| Address | Mapped name |
|---|---|
| `0x447020` | construct-from-reply-public-key |
| `0x447260` | lazy fallback-key verifier builder |
| `0x468f80` | verify embedded auth reply public key against `pubkey.dat` signer |
| `0x44aec0` | verify auth reply copy shadow with verifier |

#### `0x004b6fe8`

| Address | Mapped name |
|---|---|
| `0x4458c0` | verifier-family ctor-state base constructor before final leaf install |

#### `0x004b6e40`

| Address | Mapped name |
|---|---|
| `0x444cc0` | verifier-family earlier ctor-state base constructor |
| `0x445a10` | verifier-family earlier ctor-state base destructor |

#### `0x004b7440` = verifier-family secondary `CryptoPP::PK_Verifier` interface slice

| Address | Mapped name |
|---|---|
| `0x445960` | adjustor thunk into verifier interface method |
| `0x445970` | adjustor thunk into verifier interface method |
| `0x445980` | adjustor thunk into verifier interface method |
| `0x445990` | adjustor thunk into verifier interface method |
| `0x4459a0` | adjustor thunk into verifier interface method |
| `0x4459b0` | adjustor thunk into verifier interface method |

#### `0x004b6c44`

Not a standalone semantic Crypto++ leaf. Treat it as old-MSVC multiple-inheritance construction
plumbing used by the verifier family ctors/dtors rather than as a recoverable public class name.

Practical auth-bootstrap mapping used in source:

- child `+0xa4` = `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier *` fallback validator for raw `0x07`
- child `+0xac` = `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier *` rebuilt from the reply public key

Implementation note:

- source now routes both verifier call sites through direct Crypto++ leaf usage via
  `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier::VerifyMessage(...)`
- we no longer keep a handwritten EMSA-PKCS1-v1_5 compare path for this launcher verifier family

### 2.5.4 FileSink family used by auth pubkey.dat recording

| Address | Mapped name |
|---|---|
| `0x004b77f8` | `CryptoPP::FileSink` family / configured output sink |
| `0x447b50` | `CryptoPP::FileSink` ctor/config-init path |
| `0x447dd0` | launcher auth helper that serializes a reply-public-key record into the sink |

Source now uses direct `CryptoPP::FileSink("pubkey.dat", true)`.

### 2.6 Source implementation status

Current source uses direct Crypto++ classes for the identified launcher crypto families:

- `CryptoPP::RSAES_OAEP_SHA_Decryptor`
- `CryptoPP::RSA::PrivateKey`
- `CryptoPP::RSAES_OAEP_SHA_Encryptor`
- `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier`
- `CryptoPP::Integer`
- `CryptoPP::OldRandomPool`
- `CryptoPP::FileSink`

Canonical docs should therefore prefer address/vtable to Crypto++ class and method mappings over
long evidence logs once source is using the concrete library classes directly.

### 2.6 Integer / big-int family — `0x4ba50c`

| VTable / Function | Crypto++ class / method |
|---|---|
| `0x004ba50c` | `CryptoPP::Integer` |
| `0x45d000` | `CryptoPP::Integer::Integer()` |
| `0x45d090` | `CryptoPP::Integer::Integer(const CryptoPP::Integer&)` |
| `0x45d340` | `CryptoPP::Integer::Integer(word32)` |
| `0x45e030` | `CryptoPP::Integer::BERDecode(CryptoPP::BufferedTransformation&)` |
| `0x45f940` | `CryptoPP::Integer::Decode(CryptoPP::BufferedTransformation&, size_t, CryptoPP::Signedness)` |
| `0x45fca0` | `CryptoPP::Integer::DEREncode(CryptoPP::BufferedTransformation&) const` |
| `0x461ee0` | `CryptoPP::Integer::Integer(const CryptoPP::byte*, size_t, CryptoPP::Signedness)` |

---

## 2.6 Are these classes actually Crypto++?

Yes for several of them — and this should be stated explicitly.

### Actually Crypto++ or best treated as actual Crypto++

| Address / recovered name | Best interpretation | Status |
|--------------------------|---------------------|--------|
| `0x4b42b0` / `cls_0x4b42b0` | `CryptoPP::Algorithm` | **Actually Crypto++** |
| `0x4b68a8` | old `CryptoPP::RandomPool` family (`CryptoPP::OldRandomPool` in modern tree) | **Actually Crypto++** |
| `0x4b41e0` | `CryptoPP::BufferedTransformation` interface slice | **Actually Crypto++ interface/base** |
| `0x4bace0` | `CryptoPP::RandomNumberGenerator` interface slice | **Actually Crypto++ interface/base** |
| `0x4ba50c` | old `CryptoPP::Integer` family object | **Actually Crypto++** |
| `0x4b659c` / `cls_0x4b659c` | `CryptoPP::InvertibleRSAFunction`-equivalent key state | **Actually Crypto++ semantics** |
| `0x4b69b4` | `CryptoPP::RSAES_OAEP_SHA_Decryptor` | **Actually Crypto++** |
| `0x4ba258` | SHA1 hash context/base | **Very likely actual Crypto++ SHA1-family code** |
| `0x4ba7d8` | SHA1 init wrapper / specialization layer | **Very likely actual Crypto++ SHA1-family code** |
| `0x4baed8` | old Microsoft crypto provider / OS entropy helper class | **Likely actual Crypto++ helper class** |

### Not actually a pure Crypto++ class

| Address / recovered name | Best interpretation | Status |
|--------------------------|---------------------|--------|
| `0x4b42bc` / `CryptoInitHelper_0x4b42bc` | launcher-owned wrapper around Crypto++ subobjects | **Launcher wrapper** |
| `0x4b6a60` | MSVC MI construction-stage object for the decryptor family | **Construction scaffold** |
| `0x4b6ae0` | final complete-object construction state around the decryptor family | **Construction scaffold** |

### Heuristic note: MSVC MI construction is now a Crypto++ fingerprint in this binary

At this point, repeated MSVC multiple-inheritance construction patterns are not random noise for
this launcher. In practice they correlate strongly with Crypto++:

- vbptr writes
- temporary constructor-state vtables
- adjustor thunks
- complete-object ctor flags like `param_4 != 0`

So for this binary, **“we hit crazy MSVC MI construction” is now positive evidence toward a
Crypto++ identification**, especially when combined with method-level behavior like OAEP, SHA1,
RandomPool, or AlgorithmName=`"unknown"`.

### Documentation split note

`CRYPTOPP.md` is still manageable as the canonical overview. If it grows further, the best split is
likely per-vtable notes under `../../docs/launcher.exe/VTABLES/0x00*.md`, with `CRYPTOPP.md`
remaining the top-level mapping/index.

## 3. Summary table

| Launcher address / name | Crypto++ class (best match) | Confidence | Notes |
|------------------------|------------------------------|------------|-------|
| `0x4b42bc` wrapper (`0x4b695c` vtable) | *Launcher-specific* | **Certain** | Wraps RNG + BT subobjects |
| `0x4b42b0` vtable | `CryptoPP::Algorithm` | **High** | `AlgorithmName()` returns `"unknown"` |
| `0x4b68a8` vtable at `wrapper+4` | `CryptoPP::RandomPool` (old; `OldRandomPool` in 8.9.0) | **High** | Pool size 384, two-buffer design, MS CAPI seeding |
| `0x4b41e0` vtable at `wrapper+8` | `CryptoPP::BufferedTransformation` | **Medium-High** | Filter-pipeline interface for old RandomPool |
| `0x4bace0` transient vtable | `CryptoPP::RandomNumberGenerator` interface slice | **Medium** | Pure interface used during construction |
| `0x4ba50c` vtable | `CryptoPP::Integer` | **High** | import/export + ctor family match old integer machinery |
| `0x4b6778` vtable | `CryptoPP::InvertibleRSAFunction` / `TF_DecryptorBase` | **Medium-High** | vbptr + adjustor thunks match MI hierarchy |
| `0x4b69b4` vtable | `CryptoPP::RSAES<OAEP<SHA1>>::Decryptor` (`RSAES_OAEP_SHA_Decryptor`) | **Medium-High** | Inherits from `0x4b6778`, `PerformRSADecryption` matches `Decrypt` |
| `0x4ba258` | `CryptoPP::SHA1` base/hash context | **High** | SHA1 IV and algorithm layout match |
| `0x4ba7d8` | `CryptoPP::SHA1` init/specialization layer | **High** | wraps/initializes the SHA1 base context |
| `0x4baed8` | `CryptoPP::MicrosoftCryptoProvider`-like helper | **Medium** | used in RandomPool seeding path |
| `0x4ba110` | `CryptoPP::ByteQueue` | **High** | owns linked 0x18-byte ByteQueueNode allocations; `NodeSize` init + queue semantics match |
| `0x4b9c20` | old `CryptoPP::Filter` common base | **Medium-High** | shared initialize/flush/message-series plumbing reused by PK default filters |
| `0x4b4478` | `CryptoPP::PK_DefaultEncryptionFilter` | **High** | message-end path matches `m_plaintextQueue -> Encrypt -> output` |
| `0x4b4548` | `CryptoPP::PK_DefaultDecryptionFilter` | **High** | message-end path matches `m_ciphertextQueue -> Decrypt -> output` |
| `0x4b00b0` / `FeedbackSizeTransformAdapter_0x4b00b0` | `CryptoPP::CBC_Encryption`-compatible mode object | **Medium-High** | two block-sized SecByteBlock-like buffers; process row matches CBC encrypt semantics |
| `0x4b7500` / `FeedbackSizeTransformAdapterLarge_0x4b7500` | `CryptoPP::CBC_Decryption`-compatible mode object | **Medium-High** | inherits from `0x4b00b0`; adds third temp buffer; process row matches CBC decrypt semantics |
| `0x4b9fa0` | 45-entry temp vtable set during RNG ctor | **Medium** | Intermediate base before MI resolution; not in final object |
| `0x4b3e18` | Tiny 2-entry vtable placed at `this+8` during ctor | **Low** | Launcher-specific stub; no Crypto++ equivalent identified |
| `0x468130` `PerformRSADecryption` | `PK_Decryptor::Decrypt` / `TF_DecryptorBase::Decrypt` | **High** | Exact call shape and algorithm |

---

## 4. CBC mode family recovered under the auth-bootstrap “FeedbackSize” names

### 4.1 Current identification

Despite the recovered constructor/configure string `"FeedbackSize"`, the strongest static-RE match
for the two auth-bootstrap transform classes is **Crypto++ CBC mode**, not CFB mode and not a
launcher-local bespoke packet transform.

Current best mapping:

| Recovered class | Best Crypto++ match | Confidence |
|---|---|---|
| `FeedbackSizeTransformAdapter_0x4b00b0` | `CryptoPP::CBC_Encryption` | **Medium-High** |
| `FeedbackSizeTransformAdapterLarge_0x4b7500` | `CryptoPP::CBC_Decryption` | **Medium-High** |

Most likely these live inside an older Crypto++ holder/final-template family around Twofish, but the
**mode semantics** are already identifiable from slots + fields + process behavior even if the exact
old public template spelling is still unsettled.

### 4.2 Evidence from fields and destructors

`FeedbackSizeTransformAdapter_0x4b00b0::~cls_0x4b00b0` (`0x41e010`) frees **two** tracked buffers.
This matches the expected storage split for older Crypto++ CBC encrypt-side mode objects:
- `CipherModeBase::m_register`
- `BlockOrientedCipherModeBase::m_buffer`

`FeedbackSizeTransformAdapterLarge_0x4b7500::~cls_0x4b7500` (`0x446e40`) frees **three** tracked
buffers. That matches the extra temp buffer carried by old Crypto++ `CBC_Decryption`.

The resize rows are especially strong:
- `0x41d6e0` resizes **two** block-sized buffers
- `0x446d00` resizes **three** block-sized buffers

That is a close structural match for:
- `BlockOrientedCipherModeBase::ResizeBuffers()`
- `CBC_Decryption::ResizeBuffers()`

### 4.3 Evidence from process rows / signatures

The most important recovered process rows are:

| Address | Recovered class | Observed behavior | Best Crypto++ match |
|---|---|---|---|
| `0x44b2e0` | `FeedbackSizeTransformAdapter_0x4b00b0` | XOR input into live register, run block transform, copy register to output, advance block-by-block | `CryptoPP::CBC_Encryption::ProcessData` |
| `0x44b6c0` | `FeedbackSizeTransformAdapterLarge_0x4b7500` | save ciphertext block to temp, decrypt current block, XOR with previous register/IV, swap saved ciphertext into register | `CryptoPP::CBC_Decryption::ProcessData` |

This is stronger proof than the configure-time string because the slot behavior is characteristic of
CBC encrypt/decrypt and does **not** look like a simple launcher-local wrapper around
`CryptoPP::CBC_Mode<Twofish>`.

### 4.4 Why the `"FeedbackSize"` string does not overturn the CBC identification

Modern Crypto++ uses `Name::FeedbackSize()` most visibly with CFB mode configuration, so the string
initially suggested a CFB-family object.

However, the stronger evidence here is:
- actual block-processing slot behavior
- number and role of internal buffers
- destructor/free layout
- resize-buffer layout
- inheritance between the small/base and large/derived object

So the safest interpretation is:
- `"FeedbackSize"` comes from shared older Crypto++ parameter/configuration plumbing visible in the
  launcher build
- but the concrete recovered transform objects used by the auth bootstrap path are CBC-compatible
  mode objects

### 4.5 Inheritance / object-family note

Ghidra-side recovery now has:
- `FeedbackSizeTransformAdapter_0x4b00b0`
- `FeedbackSizeTransformAdapterLarge_0x4b7500`
- with inheritance between them

This fits the CBC identification above, but adjustor-thunk evidence still suggests old Crypto++
multiple-inheritance / holder/subobject complexity. Treat the CBC mapping as **semantic class
identification**, not proof that the launcher’s exact original source used a simple two-class
hand-written hierarchy.

### 4.6 Source implementation status / replacement consequence

The current source has now moved to the preferred fidelity tradeoff for this family:
- keep the launcher-facing wrapper/boundary names
  - `FeedbackSizeTransformAdapter_ConstructSmall` (`0x41df60`)
  - `FeedbackSizeTransformAdapter_ConstructLarge` (`0x446d90`)
  - `FeedbackSizeTransformAdapter_TransformBuffer` (`0x44b570`)
- back those wrappers with the **real Crypto++ CBC Twofish classes**
  - small -> `CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption`
  - large -> `CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption`
- reinitialize the CBC object from the stored key/IV for each transform call so the configured-IV
  call pattern matches the launcher usage already recovered in auth bootstrap and margin paths

What still remains true:
- the original launcher object family almost certainly had older Crypto++ holder / MI / internal
  buffer semantics that are richer than a modern minimal wrapper
- the Ghidra-side inheritance notes (`0x4b00b0` base, `0x4b7500` derived) should still be preserved
  in comments/docs where object layout matters
- if future static-RE closes more of the old Crypto++ internal holder layers, source can be refined
  again without changing the launcher-facing method boundaries

---

## 5. Hash / SHA1 family

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

## 6. Crypto++ "unknown" fingerprint — Algorithm base class proof

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

### 2.7 Implemented verifier / filter / accumulator mappings

These auth-bootstrap Crypto++ classes are now treated as implemented/proven in source. Keep this
section factual and address-oriented.

#### `0x004b7580` = `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier`

| Address | Mapped name |
|---|---|
| `0x447020` | verifier construct-from-public-key |
| `0x447260` | lazy fallback-key verifier builder |
| `0x446f30` | `AlgorithmName()` body for `RSA/EMSA-PKCS1-v1_5(MD5)` |
| `0x445410` | MD5 `DigestInfo` prefix getter |
| `0x4472f0` | temporary accumulator create |
| `0x468520` | load signature into temporary accumulator |
| `0x467ee0` | finalize/verify on temporary accumulator |
| `0x467f70` | fill temporary-accumulator result pair |
| `0x4474c0` | dtor / leaf teardown |
| `0x4470e0` | adjustor thunk to leaf dtor (`this -= 0x50`) |
| `0x446d70` | adjustor thunk to related verifier-family dtor (`this -= 0x50`) |

#### `0x004b7668` = `CryptoPP::PK_MessageAccumulatorImpl<MD5>`-like leaf

| Address | Mapped name |
|---|---|
| `0x4472f0` | complete-object create/finalize |
| `0x447340` | `Update()` |
| `0x447380` | `AccessHash()` |

#### `0x004b76b0` = `CryptoPP::PK_MessageAccumulatorBase` ctor-state vtable

| Address | Mapped name |
|---|---|
| `0x447390` | base construct |
| `0x447340` | `Update()` |
| `0x447380` | `AccessHash()` slot on finalized leaf family |

#### `0x004ba110` = `CryptoPP::ByteQueue`

| Address | Mapped name |
|---|---|
| `0x454f10` | ctor |
| `0x455400` | destroy nodes |
| `0x455470` | reset queue |
| `0x455520` | isolated initialize |
| `0x455560` | dtor |
| `0x454a70` | `CurrentSize()` |
| `0x454ff0` | `Put2()` |

#### `0x004b4478` = `CryptoPP::PK_DefaultEncryptionFilter`

| Address | Mapped name |
|---|---|
| `0x438120` | ctor |
| `0x438180` | dtor |
| `0x438320` | `Put2()` |

#### `0x004b4548` = `CryptoPP::PK_DefaultDecryptionFilter`

| Address | Mapped name |
|---|---|
| `0x438210` | dtor |
| `0x438430` | `Put2()` |

#### `0x004b75e4` = `CryptoPP::RSAES_OAEP_SHA_Encryptor`

| Address | Mapped name |
|---|---|
| `0x447120` | construct-from-public-key |
| `0x468ea0` | query encrypted output length |
| `0x468f00` | encrypt into packet builder / chunk loop |
| `0x468280` | encrypt one plaintext chunk |
| `0x4382c0` | create inner `PK_DefaultEncryptionFilter` |

## 7. Open questions / negative results

- The auth-reply parse accessor/object sketches remain launcher-owned packet shells.

- **Exact Crypto++ version** — the launcher was likely built against 5.1.x or 5.2.x.
  The `OldRandomPool` in 8.9.0 is a faithful reproduction, but slot-level vtable offsets
  may differ by a few entries.

- **`0x4b41e0` slot-by-slot** — we have not decompiled every entry in the
  `BufferedTransformation` vtable to confirm exact method names (`Put2`, `ChannelPut2`,
  etc.).  The size and rough layout match; individual slots remain unverified.

- **Encryptor/decryptor asymmetry remains at the subobject level** — we now have a strong exact
  scheme match for both sides (`RSAES_OAEP_SHA_Encryptor` at child `+0xa8`, decryptor-compatible
  `RSAES_OAEP_SHA_Decryptor` family elsewhere), but we have not yet mapped every secondary
  sub-vtable in the old MSVC MI layout slot-by-slot. The top-level class IDs are strong; the full
  inheritance graph is still being tightened.

- **`CryptoPP_MicrosoftCryptoProvider_0x4baed8`** — the class used inside
  `InitializeCryptoState` is believed to be `CryptoPP::MicrosoftCryptoProvider` (or the
  internal `Win32CryptoProvider` that existed in old Crypto++).  Its exact 8.9.0
  equivalent may be `AutoSeededRandomPool` or `OS_GenerateRandomBlock`, but the old
  direct CAPI wrapper is gone.

---

## 8. Source anchors

| Source file | Anchor / comment |
|-------------|-----------------|
| `matrixstaging/runtime/src/libltmessaging/crypto_init_helper.h` | `0x4f7bf4` global layout, `0x4b695c`/`0x4b68a8`/`0x4b41e0` vtable pointers |
| `matrixstaging/runtime/src/libltmessaging/crypto_init_helper.cpp` | `EnsureCryptoContextInitialized` replicates `0x4429b0` static init block |
| `matrixstaging/game/src/libltclientlogin/authbootstrap680.cpp` | `0x4b695c`/`0x4b68a8`/`0x4b41e0` hard-coded in `ResetAuthBootstrap680Field54Helper`; `0x447120/0x468ea0/0x468f00` evidence now also anchors the raw `0x08` `RSAES_OAEP_SHA_Encryptor` family |
| `matrixstaging/runtime/src/libltcrypto/auth_internal.h` | `RSAES_OAEP_SHA_Encryptor` source-owned encryptor path |

---

## 9. How to update this doc

1. Run Ghidra decompile on any unmapped slot function.
2. Compare its shape to `third_party/cryptopp890/*.h`.
3. Update the mapping table above with the new address → name.
4. If confidence changes (higher or lower), move the row and update the note.
5. If a negative result disproves a mapping, record it explicitly in §4.
