# Crypto++ identification notes

Date: 2026-05-01

## `launcher.exe:0x447020`

Strong current identification: `launcher.exe:0x447020` is the constructor/helper that materializes a
`CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier`-family object from a reply public key.

Best current recovered name:

- `AuthBootstrap680ReplyAuthDataValidator_ConstructFromReplyPublicKey`

But the important class identity is the Crypto++ side:

- verifier family: `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier`
- template expansion in Crypto++ headers:
  - `rsa.h`: `Weak::RSASSA_PKCS1v15_MD5_Verifier`
  - alias of `RSASS<PKCS1v15, Weak1::MD5>::Verifier`
  - which bottoms out in `PK_FinalTemplate<TF_VerifierImpl<...>>`

## Why this looks like Crypto++ and not a launcher-local class

### 1. Constructor shape matches a final trapdoor-function verifier

Ghidra decompile of `0x447020` shows:

- optional construction flag parameter
- repeated vftable rewrites during base/derived construction
- two deep copies of `CryptoPP::Integer` into object fields:
  - one at `this + 0x14`
  - one at `this + 0x28`

Those two copied integers match the recovered reply public key material:

- modulus
- public exponent

That is exactly what we would expect from a Crypto++ RSA public-key based verifier object.

### 2. `0x447260` builds the fallback pubkey validator from modulus + exponent `0x11`

`launcher.exe:0x447260`:

- allocates a `0x54`-byte object
- constructs a `CryptoPP::Integer(0x11)` exponent
- constructs a `CryptoPP::Integer` from the embedded `g_AuthBootstrap680PubkeyDatFallbackModulus100`
- feeds both into `0x447020`

That is the launcher's lazy `pubkey.dat` validator creation path.

### 3. The verifier is consumed as a verifier-family object at `0x468f80`

`launcher.exe:0x468f80`:

- exports a 128-byte modulus into a stack buffer
- fetches one public-exponent byte
- calls a virtual at `param_4->vtable + 0x2c`
- passes:
  - signed bytes length `0x81`
  - signature pointer
  - signature length `0x100`

This is verifier-family behavior, not encryptor behavior.

## Practical source consequence

For launcher RE purposes, treat the object rooted at child:

- `+0xa4` = lazy fallback verifier
- `+0xac` = reply-auth-data verifier

as Crypto++ verifier-family objects, with the strongest concrete label currently being:

- `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier`

Even if the exact inheritance ladder remains noisy in Ghidra, the public-key material, callsites,
and verify usage make the family identification strong.

## Nearby sibling

The sibling helper at `launcher.exe:0x447120` is the corresponding reply-public-key worker builder
for the raw `0x08` auth-request path. Current source models that as:

- `CryptoPP::RSAES_OAEP_SHA_Encryptor`

So the pair currently reads as:

- `0x447020` = MD5/PKCS#1 v1.5 RSA verifier family
- `0x447120` = OAEP/SHA RSA encryptor family
