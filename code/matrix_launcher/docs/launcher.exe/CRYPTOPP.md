# launcher.exe Crypto++ mapping notes

## Summary

The launcher's auth-bootstrap RSA/OAEP decryptor is not a launcher-specific crypto class hierarchy.
It is a **launcher-wrapped / MI-adjusted Crypto++ RSAES<OAEP<SHA1>>::Decryptor family object**.

The strongest class identification from the current fidelity pass is:

- **`0x4b42b0` = `CryptoPP::Algorithm`-compatible base** from an older Crypto++ version
- **`0x4b69b4` = `CryptoPP::RSAES_OAEP_SHA_Decryptor`-compatible subobject**
- **`0x443220` = complete-object constructor that finishes the launcher wrapper and then loads RSA key material into the embedded Crypto++ private key member**

## Why `0x4b42b0` is CryptoPP::Algorithm

### Evidence 1: `0x41d880` returns `"unknown"`

The vtable at `0x4b42b0` has a slot at `+0x08` that Ghidra decompiles as a function writing the string `"unknown"`:

- launcher.exe `0x41d880`
- current source name: `AuthBootstrap680Field54Helper_ResetUnknownString`

Behavior:
- zeroes a string object
- calls `StringReallocateContent(..., "unknown", "")`

This is a very strong match for the old Crypto++ default implementation of:

- `CryptoPP::Algorithm::AlgorithmName() const { return "unknown"; }`

In current Crypto++ headers (`third_party/cryptopp890/cryptlib.h`) the modern class is:

- `class Algorithm : public Clonable`
- `virtual std::string AlgorithmName() const { return "unknown"; }`

The launcher build is old enough that it likely predates the later `AlgorithmProvider()` virtual,
so a 3-slot vtable is plausible:

- deleting destructor
- `Clone()`-style slot
- `AlgorithmName()`

### Evidence 2: slot count matches old pre-AlgorithmProvider Crypto++

Current Crypto++ adds `AlgorithmProvider()`, but the launcher's `0x4b42b0` vtable is only 3 entries.
That fits an **older Crypto++ `Algorithm` layout** better than a launcher-local base class.

### Evidence 3: this base appears under known Crypto++ objects

The OAEP/SHA1 padding helpers and hash-context objects in the same family decompile with nested
subobjects rooted through the `0x4b42b0` base, which is exactly where a generic Crypto++
`Algorithm` base would show up.

## Why the decryptor family is RSAES<OAEP<SHA1>>::Decryptor

## Evidence 1: `0x464b80` matches OAEP max-unpadded-length math exactly

Launcher function:
- `0x464b80`
- current Ghidra name: `MaxUnpaddedLength`

Observed logic:
- if `(paddedBits >> 3) > 0x29`
- return `(paddedBits >> 3) - 0x29`
- else return `0`

`0x29 == 41 == 2*20 + 1`, which is the OAEP overhead for **SHA-1**.
That matches Crypto++ `OAEP_Base::MaxUnpaddedLength()`.

## Evidence 2: `0x467640` is OAEP pad with SHA-1 / MGF1 constants

Launcher function:
- `0x467640`
- current Ghidra name: `OAEP_Pad`

Observed behavior:
- uses `0x14` byte hash size repeatedly
- builds SHA-1 hash context
- applies `OAEP_MGF1_Mask(...)` twice
- performs standard OAEP seed/DB masking flow

This is a direct behavioral match for **OAEP with SHA-1 and MGF1**.

## Evidence 3: `0x467780` is OAEP unpad with SHA-1 / MGF1 constants

Launcher function:
- `0x467780`
- current Ghidra name: `OAEP_Unpad`

Observed behavior:
- checks OAEP block length constraints
- unmasks seed and DB using MGF1
- validates the leading SHA-1 hash value (`0x14` bytes)
- scans for the `0x01` separator
- returns `{success, messageLength}` style output

Again, this is standard **OAEP/SHA-1 unpadding**.

## Evidence 4: constructor naming in Ghidra already points at Crypto++

Ghidra currently identifies launcher constructor `0x442b70` as belonging to:

- `OOAnalyzer::CryptoPP_RSAES_OAEP_SHA_Decryptor_0x4b69b4`

That naming is not proof by itself, but it is aligned with the behavioral evidence above.

## Evidence 5: embedded key member layout matches CryptoPP::InvertibleRSAFunction

The object at `field_0xc` is initialized by `0x465d70` from three bootstrap bigints.
The stored values map cleanly to RSA private-key state:

- modulus `n`
- public exponent `e`
- private exponent `d`
- prime1 `p`
- prime2 `q`
- CRT exponent1 `dp`
- CRT exponent2 `dq`
- CRT inverse `u`

That is the same semantic state held by Crypto++:

- `CryptoPP::InvertibleRSAFunction`

So the decryptor object looks exactly like a Crypto++ OAEP decryptor object containing an embedded
RSA private key object.

## Interpreting `0x443220`

`0x443220` is **not** just a tiny launcher-local constructor.
It is the complete-object ctor that:

1. walks an MSVC multiple-inheritance construction sequence
2. seeds vbptr / secondary-vftable state when `param_4 != 0`
3. constructs the Crypto++ decryptor-family base object(s)
4. calls `0x465d70` to load the RSA bootstrap key into the embedded key member

Important nearby constructors:

- `0x442b70` = Crypto++ decryptor-family ctor (`RSAES_OAEP_SHA_Decryptor`-compatible)
- `0x442e20` = intermediate complete/subobject ctor state
- `0x443220` = final launcher-visible complete-object ctor used by `0x443340`

## Best current mapping

### Confirmed / high confidence

- `0x4b42b0` -> **`CryptoPP::Algorithm`** (older layout)
- `0x464b80` -> **`CryptoPP::OAEP_Base::MaxUnpaddedLength`** behavior
- `0x467640` -> **OAEP pad using SHA-1 + MGF1**
- `0x467780` -> **OAEP unpad using SHA-1 + MGF1**
- `0x4b69b4` family -> **`CryptoPP::RSAES_OAEP_SHA_Decryptor`-compatible**

### Strong but still worth preserving as “compatible” wording

- `0x443220` / `0x4b6ae0` -> launcher wrapper / complete-object realization of the Crypto++
  decryptor hierarchy, with extra MSVC adjustor-vtable plumbing that does not map 1:1 to a
  single pretty source-level class declaration.

## Source implications

For source fidelity work in `messageconnection.*`:

- treat the auth-bootstrap crypto object as a **Crypto++ OAEP/SHA-1 RSA decryptor family object**
- treat the `param_4` complete-object-ctor flag as **MSVC construction-state plumbing**, not
  semantic launcher data
- treat `field_0xc` as the embedded RSA private key material / prep-state member
- avoid overfitting launcher-local names where Crypto++ behavior is directly identifiable

## Next fidelity targets

To tighten this further in Ghidra/source later:

1. map `0x442b70`, `0x442e20`, and `0x443220` to a cleaner reconstructed MI hierarchy
2. identify whether the intermediate `0x4b6a60` / `0x4b6ae0` layers correspond to specific
   old-Crypto++ implementation templates versus launcher-added wrappers
3. rename the `0x465d70` prep-state member fields in Ghidra/source to explicit RSA names
   (`modulus`, `publicExponent`, `privateExponent`, `prime1`, `prime2`, `dp`, `dq`, `u`)
