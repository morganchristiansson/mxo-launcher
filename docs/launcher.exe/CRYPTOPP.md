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

### 2.2 Decryptor hierarchy overview

**Confidence: HIGH for family identification; MEDIUM-HIGH for exact MI layering**

The auth-bootstrap decryptor path is best understood as an MSVC multiple-inheritance realization of:

- `CryptoPP::RSAES<OAEP<SHA1>>::Decryptor`
- containing / exposing an embedded `CryptoPP::InvertibleRSAFunction`
- with multiple adjustor/vbptr subobjects produced by old MSVC codegen

Important constructors / states:

| Address | Best current interpretation |
|---------|-----------------------------|
| `0x442b70` | `CryptoPP::RSAES_OAEP_SHA_Decryptor`-compatible constructor state |
| `0x442e20` | intermediate MI construction state for the same decryptor family |
| `0x443220` | launcher-visible complete-object constructor used by `0x443340` |
| `0x465d70` | key-material import / CRT derivation into embedded RSA private-key member |

The important correction vs older notes is that `0x443220` is **not** a simple launcher-local ctor.
It is the complete-object ctor that finishes the MI object and then calls `0x465d70` to load the RSA
key components.

---

### 2.3 `0x443220` complete-object ctor

**Confidence: HIGH**

Function: `CMarginConnectionBootstrapPrepStateA0_ctor` in current Ghidra output.

Observed sequence:
1. If `param_4 != 0`, seed temporary vbptr / secondary-vftable state
2. Call intermediate constructor at `0x442e20`
3. Rewrite vfptr / adjustor-vtable slots for the final complete-object state
4. Call `0x465d70` with `(param_1, param_2, param_3)`
5. Return `this`

The 4th argument is therefore **construction-state plumbing** rather than semantic launcher data.
That is why the source-side unused `param_4` warning is real but the parameter still belongs in the
signature for fidelity.

---

### 2.4 Decryptor leaf — `0x4b69b4` = `CryptoPP::RSAES_OAEP_SHA_Decryptor`-compatible

**Confidence: HIGH**

VTable: `0x4b69b4` (16 entries, 64 bytes). Constructor: `0x442b70`.

This class family is strongly identified as **old Crypto++ `RSAES<OAEP<SHA1>>::Decryptor`**.

**Key evidence:**

1. **`PerformRSADecryption` (`0x468130`)**
   - imports ciphertext into a Crypto++ integer
   - uses RNG/blinding context
   - invokes trapdoor inverse / RSA private op
   - exports result bytes
   - hands them to OAEP unpadding-style postprocessing

   This is exactly the shape expected from Crypto++ `TF_DecryptorBase::Decrypt`-style code.

2. **`MaxUnpaddedLength` (`0x464b80`)**
   - returns `k - 0x29` when `k > 0x29`
   - `0x29 == 2*20 + 1`
   - that is the OAEP overhead for **SHA-1**

3. **`OAEP_Pad` (`0x467640`)** and **`OAEP_Unpad` (`0x467780`)**
   - both use `0x14` byte hash lengths
   - both instantiate SHA1 hash context machinery
   - both run MGF1 masking / unmasking steps
   - behavior matches Crypto++ OAEP code very closely

4. **Key member semantics at `0x465d70`**
   - imports `n`, `e`, `d`
   - derives / stores `p`, `q`, `dp`, `dq`, `u`
   - this matches `CryptoPP::InvertibleRSAFunction`

5. **Encryptor counterpart already exists in source**
   - launcher source already uses `CryptoPP::RSAES_OAEP_SHA_Encryptor` in the bootstrap
     encryptor path, making the matching decryptor identification especially compelling

---

### 2.5 Encryptor counterpart — `0x4b75e4` = `CryptoPP::RSAES_OAEP_SHA_Encryptor`-compatible

**Confidence: HIGH for the exact scheme; MEDIUM-HIGH for the precise old-template spelling**

The raw `0x08` reply-public-key worker rebuilt at child `+0xa8` is now best read as the launcher's
**public-key encryptor half** of the same OAEP/SHA1 RSA family:

- ctor `0x447120`
- intermediate ctor-state vtable `0x004b74a8`
- final leaf vtable `0x004b75e4`
- output-sizing helper `0x468ea0`
- chunked encrypt loop `0x468f00`
- per-chunk encrypt virtual `0x468280`

Best current modern Crypto++ equivalent:

- `CryptoPP::RSAES_OAEP_SHA_Encryptor`
- more explicitly: `PK_FinalTemplate<TF_EncryptorImpl<TF_CryptoSchemeOptions<RSAES<OAEP<SHA1> >, RSA, OAEP<SHA1>>>>`

Why this mapping is now strong:

1. **Scheme match from call sites**
   - `0x4474f0` uses child `+0xa8` only on the outbound raw `0x08 / AS_AuthRequest` path
   - `0x468ea0` computes the encrypted output length from RSA modulus bytes and plaintext chunk
     bytes, matching `TF_EncryptorBase::FixedCiphertextLength()` / `FixedMaxPlaintextLength()`-style
     behavior
   - `0x468f00` then loops over plaintext chunks and calls worker vtable `+0x1c` once per chunk,
     exactly what we expect from an OAEP/RSA encryptor wrapper over a public key

2. **Key material shape matches `RSA::PublicKey` / `RSAFunction`**
   - ctor `0x447120` deep-copies only **two** Crypto++ integer objects:
     - destination `this+0x14` from ctor arg2
     - destination `this+0x28` from ctor arg3
   - that is exactly the public-key pair `(n, e)` shape of `CryptoPP::RSAFunction`
   - no private-key fields (`d/p/q/dp/dq/u`) are present here, so this is not the decryptor or
     `InvertibleRSAFunction` family

3. **Constructor shape matches `PK_FinalTemplate<...Encryptor...>` multiple inheritance**
   - `0x447120` performs staged retabling at several offsets (`+0x00/+0x08/+0x4c/+0x50/+0x54/+0x58`)
   - it first installs the 11-slot ctor-state vtable `0x004b74a8`, then rewrites the primary leaf
     vtable to `0x004b75e4`
   - both tables share the hot encrypt-side entries (`+0x1c = 0x468280`, `+0x20 = 0x4382c0`), which
     is exactly what we would expect from an intermediate-to-final retable within the same Crypto++
     encryptor family rather than from two unrelated launcher-local helper classes
   - this is the smell expected from old MSVC codegen for Crypto++'s layered template hierarchy:
     `PK_FinalTemplate` → `TF_EncryptorImpl` → `TF_EncryptorBase` with embedded key material and
     secondary interface slices

4. **Explicit adjustor thunk evidence**
   - `0x446d80` does:
     - call `~cls_0x4b74a8((this-0x50), flag)`
   - `0x4471f0` does:
     - `sub ecx, 0x50`
     - `jmp 0x447880`
   - those are classic non-primary-base `this` adjustor thunks and strongly support the view that
     the worker object is not a flat launcher-local struct but a real Crypto++ MI object family

5. **The public-key-only ctor arguments line up with Crypto++ constructor sugar**
   - modern Crypto++ exposes `PK_FinalTemplate(const T1&, const T2&)` which forwards to
     `AccessKey().Initialize(v1, v2)`
   - the older launcher build appears to inline that into direct bigint deep-copy work in
     `0x447120`, but the semantic surface is the same: construct an RSA OAEP encryptor from `(n, e)`

Practical static-RE consequence:
- child `+0xa8` should no longer be treated as just a generic "reply-public-key worker"
- the strongest current class-equivalent reading is **RSAES OAEP SHA-1 encryptor over an embedded
  RSA public key**, with old-MSVC multiple-inheritance thunks still visible in the layout

### 2.5.1 Exact public-key base class under the `0x4b659c` / `0x4b6680` family

**Confidence: HIGH**

The recovered ctor at `0x4420f0` now appears to be the exact Crypto++ public-key base class
`CryptoPP::RSAFunction` rather than a launcher-local RSA helper.

Key evidence:

- `0x4420f0` constructs **two** `CryptoPP_Int_0x4ba50c` objects at:
  - `this+0x08` → modulus `n`
  - `this+0x1c` → public exponent `e`
- the ctor installs the final vtable `0x004b6680`
- the surrounding layout is a classic RSA public-key family with MI/thunk scaffolding
- `0x42250` then builds the larger derived family by adding the private-key bigint slices, which
  matches `CryptoPP::InvertibleRSAFunction`
- in modern Crypto++ source terms, the public-key alias is `CryptoPP::RSA::PublicKey`, but the
  exact underlying class is `CryptoPP::RSAFunction`

So the launcher-recovered `AuthBootstrap680RsaPublicKeyPairSubobject0cSketch` was only a temporary
layout mirror. The source has since been pruned to use **direct `CryptoPP::RSA::PublicKey`** for
that family, and the Crypto++ class it corresponds to is `RSAFunction`.

### 2.5.2 Derived private-key family under `0x4b672c`

**Confidence: HIGH**

The constructor at `0x442250` extends the RSA public-key base into the private-key family and
matches **`CryptoPP::InvertibleRSAFunction`**.

Key evidence:

- `0x442250` first runs the `0x4b659c` public-key ctor path, then installs additional vtables and
  helper slices on top of it
- the constructor emits six more `CryptoPP_Int_0x4ba50c` initializations at:
  - `field_0x3c`
  - `field_0x50`
  - `field_0x64`
  - `field_0x78`
  - `field_0x8c`
  - `field_0xa0`
- those extra bigint members line up with the private RSA components stored by
  `CryptoPP::InvertibleRSAFunction` / `RSA::PrivateKey`
- the vtable family at `0x004b672c` is therefore the derived private-key RSA branch, not a new
  unrelated launcher-local crypto type

Practical family picture:
- `0x004b659c / 0x004b6680` = `CryptoPP::RSAFunction` (`RSA::PublicKey`)
- `0x004b672c` = `CryptoPP::InvertibleRSAFunction` (`RSA::PrivateKey`)
- the later `0x004b6778` margin/auth wrapper sits on top of that RSA private-key core

### 2.6 Source implementation status

This replacement has now been done in source.

Current source direction:

- uses **`CryptoPP::RSAES_OAEP_SHA_Decryptor` directly** for the actual decryptor subobject
- uses **`CryptoPP::RSA::PrivateKey` directly** for the loaded bootstrap key state
- uses **direct `CryptoPP::RSA::PublicKey` + on-demand `CryptoPP::RSAES_OAEP_SHA_Encryptor`** for
  the recovered raw `0x08` worker family
- uses **direct `CryptoPP::RSA::PublicKey` + verifier-side PKCS#1 v1.5 / MD5 checks** for the
  recovered `+0xa4/+0xac` validator family
- uses **`CryptoPP::Integer` directly** for big-int semantics
- uses **`CryptoPP::OldRandomPool` directly** for the recovered RNG helper family
- keeps small launcher wrappers only for the preserved launcher entrypoints / boundaries:
  - `0x443220`
  - `0x437810`
  - `0x468130`
  - `0x465d70`
- child-side raw `0x14` integer objects are preserved only where the launcher child layout still
  really contains them before the margin-prep seam

This is the preferred fidelity tradeoff:
1. keep launcher.exe constructor / helper boundaries visible
2. stop pretending identified Crypto++ classes are launcher-local bespoke classes
3. preserve comments where old MSVC MI / adjustor-thunk behavior cannot be expressed 1:1 in modern source
4. keep canonical docs and Ghidra naming aligned with the Crypto++ identification

### 2.6 Integer / big-int family — `0x4ba50c`

**Confidence: HIGH**

`CBootstrapBigInt_0x4ba50c` is very likely an old **`CryptoPP::Integer`** family object.

This is not just because RSA key state stores several `0x14`-byte big-int subobjects. The class
behavior itself matches old Crypto++ integer machinery:

- default ctor `0x45d000` initializes a zero-valued digit array
- copy ctor `0x45d090` normalizes/copies digit capacity and digit words
- word ctor `0x45d340` constructs from a scalar value
- byte-reader ctor `0x45f940` imports from a source object
- raw-byte ctor `0x461ee0` imports from raw bytes + length + flags
- vtable `0x4ba50c` has import/export methods matching `ASN1Object`-style integer I/O

Important vtable slots:

| Slot | Address | Best interpretation |
|------|---------|---------------------|
| `+0x00` | `0x441610/0x45d040` | destructor / deleting destructor |
| `+0x04` | `0x45e030` | import / BER-like decode path |
| `+0x08` | `0x45fca0` | export / DER-like encode path |
| `+0x0c` | `0x4413a0` | wrapper forwarding one encode form to another |

Best current interpretation:
- **static-RE**: this is an actual Crypto++ integer-family class, most likely old `CryptoPP::Integer`
- **source**: keep it only as a recovered **raw boundary object** where launcher.exe really passes
  the exact `0x14`-byte shape around at bootstrap seams

Current source direction after the messaging-layer cleanup pass:
- internal semantic work now uses **direct `CryptoPP::Integer`**
- the old source-side stand-in **`CBootstrapBigInt_0x4ba50c` has been removed**
- preserved child-layout/raw-copy code still keeps the launcher-owned `0x14` object bytes where
  the original auth-bootstrap child really stores adjacent `0xb0/0xc4/0xd8` integer objects
- the `0x443340 -> 0x443220 -> 0x465d70` margin-bootstrap-prep path now converts those raw child
  objects once, then stays on direct `CryptoPP::Integer` / `CryptoPP::RSA::PrivateKey`
- post-reconstruction capacity introspection now computes the original rounded word-capacity from
  `CryptoPP::Integer` directly instead of materializing source-only fake big-int objects

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
| `0x4ba50c` / `CBootstrapBigInt_0x4ba50c` | old `CryptoPP::Integer` family object | **Actually Crypto++ or so close that source should treat it that way** |
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
| `0x4ba50c` vtable / `CBootstrapBigInt_0x4ba50c` | `CryptoPP::Integer` family object | **High** | import/export + ctor family match old integer machinery |
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

### 2.7 Verifier counterpart — `0x4b7580` = `CryptoPP::RSASSA_PKCS1v15_MD5_Verifier`-compatible

**Confidence: HIGH for the exact scheme; MEDIUM-HIGH for the precise old-template spelling**

The validator family used at child `+0xa4` and `+0xac` is now best read as the launcher's
**public-key verifier half** of the RSA PKCS#1 v1.5 / MD5 signature family:

- ctor `0x447020 = AuthBootstrap680ReplyAuthDataValidator_ConstructFromReplyPublicKey`
- lazy fallback-key builder `0x447260 = AuthBootstrap680_CreateLazyPubkeyDatState`
- final leaf vtable `0x004b7580`
- algorithm-name body `0x446f30 = AuthBootstrap680Validator_BuildRsaMd5Pkcs1NameBody`
- MD5 DigestInfo prefix getter `0x445410 = AuthBootstrap680Validator_GetMd5DigestInfoPrefixPair`

Best current modern Crypto++ equivalent:

- `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier`
- equivalently: `PK_FinalTemplate<TF_VerifierImpl<TF_SignatureSchemeOptions<RSASS<PKCS1v15, Weak1::MD5>, RSA, PKCS1v15_SignatureMessageEncodingMethod, Weak1::MD5>>>`

Why this mapping is now strong:

1. **Public-key-only constructor shape**
   - `0x447020` deep-copies only two `CryptoPP::Integer`-like objects:
     - destination `this+0x14` from ctor arg1
     - destination `this+0x28` from ctor arg2
   - just like the raw `0x08` encryptor family, this exactly matches an embedded RSA public key
     `(n, e)` rather than a private-key family

2. **Exact algorithm-name evidence**
   - vtable `+0x08` reaches `0x446f10`, which wraps `0x446f30`
   - `0x446f30` concatenates:
     - `"RSA"`
     - `"/"`
     - `"EMSA-PKCS1-v1_5"`
     - `"("`
     - `"MD5"`
     - `")"`
   - that gives the exact Crypto++-style scheme name:
     - `RSA/EMSA-PKCS1-v1_5(MD5)`
   - this is verifier-side naming, not encryptor-side naming

3. **Exact DigestInfo prefix evidence**
   - `0x445410` returns `(ptr=0x004baefc, len=0x12)`
   - the validator finalize path uses that pair as the fixed MD5 `DigestInfo` prefix before the
     caller digest bytes are compared against the RSA-decoded representative
   - that is the canonical PKCS#1 v1.5 signature verification shape for MD5

4. **Vtable behavior matches verifier-style helpers**
   - `+0x1c` allocates a temporary worker
   - `+0x20` loads signature bytes into that worker
   - `+0x28` finalizes and returns a boolean verification result
   - `+0x2c` is the wrapper that performs the whole verify flow on caller bytes + signature
   - those slots look like a launcher-visible decomposition of verifier-side work, not an encryptor
     or decryptor API

5. **Constructor-phase retabling and thunks again show old MSVC Crypto++ MI**
   - `0x447020` installs staged tables `0x004b73c8` -> `0x004b7450` -> final `0x004b7580`
   - it also installs a secondary adjustor thunk through `0x004b741c`
   - that is the same overall smell as the raw `0x08` encryptor family, but now on the verifier side

Practical static-RE consequence:
- child `+0xa4` and `+0xac` should now be treated as **Crypto++ verifier-family objects over an
  embedded RSA public key**, not just as launcher-local validation helpers
- the strongest current class-equivalent reading is **RSASSA PKCS#1 v1.5 MD5 verifier** with
  old-MSVC multiple-inheritance construction tables still visible

## 2.8 Raw `0x08` helper stack closure: `ByteQueue` + `PK_Default*Filter`

### 2.8.1 `0x4ba110` = `CryptoPP::ByteQueue`

**Confidence: HIGH**

The object previously tracked as a ByteQueueNode-like helper is better identified as the full
**`CryptoPP::ByteQueue`** object itself. The linked 0x18-byte heap chunks are the internal
`ByteQueueNode` allocations owned by that queue.

Key evidence:
- ctor `0x454f10`
  - stores a caller-supplied node size
  - allocates an initial 0x18-byte node record plus node buffer
  - sets both head and tail to that first node
- destroy helper `0x455400` and dtor `0x455560`
  - walk the linked list, zero/free each node buffer, then free each node object
- reset path `0x455470`
  - destroys all nodes and recreates a fresh one-head/one-tail queue
- isolated init `0x455520`
  - reads the **`"NodeSize"`** parameter and resets the queue
- `0x454a70`
  - sums `(tail - head)` over nodes and adds the lazy tail length, matching `CurrentSize()`
- `0x454ff0`
  - appends bytes to the tail node and allocates a new node when needed, matching `Put2()`

Static RE still identifies the launcher member as an embedded **`CryptoPP::ByteQueue`** rather
than a standalone node helper class.

Source now takes the more direct representation:
- the old sketch-only `AuthBootstrap680Raw08PerChunkNodeBufferHelper1cSketch` name is retired
- the embedded queue member is modeled directly as `CryptoPP::ByteQueue`
- the prior 0x1c queue-boundary offset/size asserts were pruned because source no longer depends on
  reproducing the exact launcher ABI/layout for this third-party class member

The important fidelity point remains semantic and boundary-oriented:
- the linked 0x18-byte allocations are internal ByteQueueNode-like records owned by the queue
- no remaining auth-bootstrap source logic models the embedded object as a single node
- launcher anchors stay attached to the surrounding recovered filter boundary

### 2.8.2 `0x4b9c20` = old `CryptoPP::Filter` common base

**Confidence: MEDIUM-HIGH**

Ctor `0x453570` stores one attachment-like constructor argument, installs the
`CryptoPP::BufferedTransformation` secondary slice, and leaves the object in the exact role we
expect from the common **Crypto++ `Filter` attachment base** reused by the default public-key
filters.

Why this is the right level:
- the object has **no ByteQueue of its own**
- the vtable methods at `0x453640 / 0x453690 / 0x453710` behave like shared
  initialize/flush/message-series plumbing
- both `0x4b4478` and `0x4b4548` construct this base first, exactly like old
  `PK_DefaultEncryptionFilter` / `PK_DefaultDecryptionFilter` would through
  `Unflushable<Filter>`

So `0x4b9c20` is best described as the **old Crypto++ Filter-family common base**, not a
launcher-specific helper shell.

### 2.8.3 `0x4b4478` = `CryptoPP::PK_DefaultEncryptionFilter`

**Confidence: HIGH**

Ctor `0x438120`:
- constructs the `0x4b9c20` common filter base
- constructs `0x4ba110` at `this+0x14` with node size `0x100`
- installs final vtable `0x004b4478`

The message-end path at `0x438320` matches modern Crypto++
`PK_DefaultEncryptionFilter::Put2` in `cryptlib.cpp`:
- accumulate plaintext in the embedded `ByteQueue`
- on `messageEnd`, compute plaintext length from that queue
- ask the owner encryptor for ciphertext output size
- drain the queue into a temporary buffer
- run encryptor-side RSA/OAEP encryption
- emit the ciphertext through the filter output path

That is a direct semantic match for **`PK_DefaultEncryptionFilter`**.

### 2.8.4 `0x4b4548` = `CryptoPP::PK_DefaultDecryptionFilter`

**Confidence: HIGH**

The sibling vtable at `0x004b4548` differs mainly in the final leaf slot and the message-end path
at `0x438430`.

That routine matches modern Crypto++ **`PK_DefaultDecryptionFilter::Put2`**:
- accumulate ciphertext bytes in the embedded `ByteQueue`
- on `messageEnd`, compute ciphertext length from the queue
- ask the owner decryptor for maximum plaintext length
- drain the queue into a temporary ciphertext buffer
- call the owner decryptor
- store the returned decode result (`isValidCoding`, `messageLength`-style fields)
- emit only the decoded plaintext length on success

So `0x4b4548` is best read as **`PK_DefaultDecryptionFilter`**, i.e. the decrypt-side sibling of
`0x4b4478`.

### 2.8.5 Practical auth-bootstrap consequence

The raw `0x08` helper stack is no longer an open identification problem:
- `0x4ba110` = `ByteQueue`
- `0x4b9c20` = old `Filter` common base
- `0x4b4478` = `PK_DefaultEncryptionFilter`
- `0x4b4548` = `PK_DefaultDecryptionFilter`

This means the auth-bootstrap source/docs should treat that stack as **old Crypto++ default PK
filter plumbing** surrounding the already-identified RSAES-OAEP encryptor / decryptor families,
rather than as a launcher-owned bespoke helper family.

## 6.2 Validator temporary worker closure: `PK_MessageAccumulatorBase` / `PK_MessageAccumulatorImpl<MD5>`

**Confidence: MEDIUM-HIGH**

The worker allocated from validator slot `+0x1c` is now better understood as a real Crypto++
verifier-accumulator family object rather than as a launcher-only scratch shell.

Best current read:
- `0x447390` = base-ish accumulator ctor
- `0x4472f0` = complete-object factory / leaf finisher
- semantic family = **old `CryptoPP::PK_MessageAccumulatorBase` with a derived
  `PK_MessageAccumulatorImpl<MD5>`-like leaf**

### 6.2.1 Construction shape

`0x4472f0 = AuthBootstrap680ReplyAuthDataValidator_CreateTemporaryWorker`:
- allocates `0x84` bytes
- calls `0x447390 = AuthBootstrap680ValidatorTemporaryWorker_Construct`
- constructs hash object at `this+0x60` via `0x43d410`
- then rewrites the primary vfptr to final vtable `0x004b7668`

`0x447390` itself:
- calls the root/base ctor
- installs ctor-state vtable `0x004b76b0`
- zeroes worker fields
- default-constructs two adjacent `CryptoPP::Integer`-family subobjects at `+0x34` and `+0x48`
- seeds byte `+0x5c = 1`

This is a **mild staged-retable smell**, but not the strong old-MSVC multi-inheritance /
adjustor-thunk pattern seen on the outer RSA verifier/encryptor families.

### 6.2.2 `0x447340` strongly matches `PK_MessageAccumulatorBase::Update`

`0x447340 = AuthBootstrap680ValidatorTemporaryWorker_UpdateDigestMaybeClearReadyFlag`:
- calls worker vtable `+0x44`
- receives the hash object at `this+0x60`
- calls that hash object's update slot with caller bytes
- updates byte `this+0x5c` so it stays `1` only if it was already `1` and `length == 0`, else clears

That matches modern Crypto++ `PK_MessageAccumulatorBase::Update()` almost exactly:

```cpp
AccessHash().Update(input, length);
m_empty = m_empty && length == 0;
```

So the strongest current field interpretation is:
- `worker +0x5c` = **`m_empty`-style accumulator flag**
- worker vtable `+0x44` = **`AccessHash()`-style accessor**

### 6.2.3 `0x447380` strongly matches `AccessHash()`

`0x447380 = AuthBootstrap680ValidatorTemporaryWorker_GetDigestObject60` simply returns:
- `this + 0x60`

That is exactly the shape expected from a derived
`PK_MessageAccumulatorImpl<HASH>::AccessHash()` implementation where the hash object is embedded in
place.

### 6.2.4 Embedded hash object is MD5-family

The object constructed at worker `+0x60` through `0x43d410` is the recovered MD5-family object used
by this validator path. In the live verifier flow:
- outer validator naming proves `RSA/EMSA-PKCS1-v1_5(MD5)`
- slot `+0x3c / 0x445410` returns the fixed MD5 `DigestInfo` prefix
- the worker-local hash reached through `+0x44` feeds the expected PKCS#1 v1.5 MD5 comparison path

So the leaf object finalized at `0x4472f0` is best described as an
**MD5-backed Crypto++ verification accumulator**.

### 6.2.5 Practical source/doc consequence

The old source-side `AuthBootstrap680ValidatorTemporaryWorker84Sketch` should no longer be described
as merely a launcher-owned temporary worker. The strongest current interpretation is:
- **base semantics:** `CryptoPP::PK_MessageAccumulatorBase`
- **leaf semantics:** `CryptoPP::PK_MessageAccumulatorImpl<MD5>` (old-version equivalent)
- **usage context:** temporary accumulator created by the
  `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier` family

The remaining launcher-owned pieces are the outer orchestration around allocation, signature loading,
and final compare wiring — not the accumulator semantics themselves.

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
