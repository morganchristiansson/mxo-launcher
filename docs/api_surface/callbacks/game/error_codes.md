# Login Error and Event Codes

## Overview

This document catalogs all error and event codes used in the login system, extracted from `launcher.exe`.

---

## LTLO Constants - Login Observer Events/Errors

**Category**: Login Observer notifications  
**Base**: 0x00

| Constant | Value | Description |
|----------|-------|-------------|
| `:LTLO_INPROGRESS` | 0 (0x0) | Login operation in progress |
| `LTLO_NOAUTHSERVERADDR` | 1 (0x1) | No authentication server address available |
| `LTLO_COULDNTRESOLVEADDR` | 2 (0x2) | Could not resolve server address |
| `LTLO_SERVERREQUESTTIMEDOUT` | 3 (0x3) | Server request timed out |
| `LTLO_UNEXPECTEDAUTHMSG` | 4 (0x4) | Unexpected authentication message received |
| `LTLO_UNEXPECTEDMARGINMSG` | 5 (0x5) | Unexpected margin message received |
| `LTLO_NOTLOGGEDIN` | 6 (0x6) | Client not logged in |
| `LTLO_ALREADYLOGGEDIN` | 7 (0x7) | Client already logged in |
| `LTLO_DECRYPTIONFAILURE` | 8 (0x8) | Decryption failed |
| `LTLO_CHARACTERNOTREADY` | 9 (0x9) | Character not ready |
| (missing) | 10 (0xa) | *Reserved/undefined* |
| `LTLO_CERTVERIFYFAILED` | 11 (0xb) | Certificate verification failed |
| `LTLO_CLIENTHASHFAILED` | 12 (0xc) | Client hash verification failed |
| `LTLO_VERSIONREADFAILED` | 13 (0xd) | Version read failed |
| `LTLO_NOMXOSUB` | 14 (0xe) | No Matrix Online subscription |
| `LTLO_NEXT_PASSCODE` | 15 (0xf) | Next passcode required |
| `LTLO_NEW_PIN_REQUESTED` | 16 (0x10) | New PIN requested |
| `LTLO_NEW_PIN_REJECTED` | 17 (0x11) | New PIN rejected |
| `LTLO_NEW_PIN_ACCEPTED` | 18 (0x12) | New PIN accepted |

---

## LTAUTH Constants - Authentication Errors

**Category**: Authentication errors  
**Base**: 0x00

| Constant | Value | Description |
|----------|-------|-------------|
| `LTAUTH_INVALIDCERTIFICATE` | 0 (0x0) | Invalid certificate |
| `LTAUTH_EXPIREDCERTIFICATE` | 1 (0x1) | Expired certificate |
| `LTAUTH_LOGINTYPENOTACCEPTED` | 2 (0x2) | Login type not accepted |
| `LTAUTH_ALREADYCONNECTED` | 3 (0x3) | Already connected |
| `LTAUTH_AUTHKEYSIGINVALID` | 4 (0x4) | Authentication key signature invalid |

---

## LTMS Constants - Mediator Server Errors

**Category**: Login mediator server errors  
**Base**: 0x00

| Constant | Value | Description |
|----------|-------|-------------|
| `LTMS_ALREADYCONNECTED` | 0 (0x0) | Already connected to mediator |
| `LTMS_NONAMECLAIMED` | 1 (0x1) | No character name claimed |
| `LTMS_NOCHARACTERLOADED` | 2 (0x2) | No character loaded |
| `LTMS_INVALIDSECRET` | 3 (0x3) | Invalid secret/token |
| `LTMS_NAMEALREADYINUSE` | 4 (0x4) | Character name already in use |
| (missing) | 5-6 | *Reserved/undefined* |
| `LTMS_INCOMPATIBLECLIENTVERSION` | 7 (0x7) | Incompatible client version |
| (missing) | 8 | *Reserved/undefined* |
| `LTMS_INVALIDCHARACTERNAME` | 9 (0x9) | Invalid character name |
| `LTMS_CHARACTERNOTFOUND` | 10 (0xa) | Character not found |
| `LTMS_UNKNOWNSESSION` | 11 (0xb) | Unknown session |
| `LTMS_INVALIDREALNAME` | 12 (0xc) | Invalid real name |
| `LTMS_CLUSTERNOTREADY` | 13 (0xd) | Cluster not ready |
| `LTMS_OFFENSIVEREALNAME` | 14 (0xe) | Offensive real name detected |
| `LTMS_RESERVEDREALNAME` | 15 (0xf) | Reserved real name |
| `LTMS_CLUSTERFULL` | 16 (0x10) | Cluster is full |
| `LTMS_CHECKINGPOPULATION` | 17 (0x11) | Checking population |
| `LTMS_TRYANOTHERSERVER` | 18 (0x12) | Try another server |
| `LTMS_MARGINCHARACTERINUSE` | 19 (0x13) | Margin character in use |
| `LTMS_FAILOVERUNEXPECTED` | 20 (0x14) | Unexpected failover |
| `LTMS_FAILOVERINPROGRESS` | 21 (0x15) | Failover in progress |
| `LTMS_PROTOCOLCHECKSUMMISMATCH` | 22 (0x16) | Protocol checksum mismatch |
| (missing) | 23 (0x17) | *Reserved/undefined* |
| `LTMS_GOBFILEGUIDMISMATCH` | 24 (0x18) | GOB file GUID mismatch |
| `LTMS_SESSIONESTABLISHINPROGRESS` | 25 (0x19) | Session establishment in progress |
| `LTMS_ADMINSONLY` | 26 (0x1a) | Admins only |
| `LTMS_INCORRECTHASHRESULT` | 27 (0x1b) | Incorrect hash result |
| `LTMS_ACCOUNTINUSE` | 28 (0x1c) | Account in use |
| `LTMS_NOTCHALLENGED` | 29 (0x1d) | Not challenged |
| `LTMS_CHALLENGED` | 30 (0x1e) | Challenged |
| `LTMS_NODATABASECONNECTION` | 31 (0x1f) | No database connection |
| `LTMS_INVALIDTRAIT` | 32 (0x20) | Invalid trait |
| `LTMS_INVALIDSTARTINGINVENTORYDATA` | 33 (0x21) | Invalid starting inventory data |
| `LTMS_INVALIDRSIDATA` | 34 (0x22) | Invalid RSI data |
| `LTMS_AUTHSERVERCHARACTERDELETEFAILED` | 35 (0x23) | Auth server character delete failed |
| `LTMS_DELETEINPROGRESS` | 36 (0x24) | Delete in progress |
| `LTMS_OBJECTCHARACTERINUSE` | 37 (0x25) | Object character in use |
| `LTMS_LOADINPROGRESS` | 38 (0x26) | Load in progress |
| `LTMS_OBJECTCACHEINUSE` | 39 (0x27) | Object cache in use |
| `LTMS_SOESESSIONVALIDATIONFAILED` | 40 (0x28) | SOE session validation failed |
| `LTMS_SOESESSIONCONSUMEFAILED` | 41 (0x29) | SOE session consume failed |
| `LTMS_SOESESSIONSTARTPLAYFAILED` | 42 (0x2a) | SOE session start play failed |

---

## Result Constants

| Constant | Context |
|----------|---------|
| `RESULT_USAGE_LIMIT_DENIED_LOGIN` | Usage limit denied for login |

---

## Usage in Code

### Error Checking Pattern

```cpp
void CLTLoginObserver_PassThrough::OnLoginError(int errorNumber) {
    switch (errorNumber) {
        case 0x1: // LTLO_NOAUTHSERVERADDR
            Log("No auth server address available");
            break;
        case 0x8: // LTLO_DECRYPTIONFAILURE
            Log("Decryption failed");
            break;
        // ... etc
    }
}
```

### Event Number Ranges

- **LTLO (Login Observer)**: 0x00 - 0x12 (0-18)
- **LTAUTH (Authentication)**: 0x00 - 0x04 (0-4)
- **LTMS (Mediator Server)**: 0x00 - 0x2A (0-42)

---

## Notes

1. Error numbers are passed as `int` parameters to observer methods
2. Each error category (LTLO/LTAUTH/LTMS) has its own numbering space
3. Some numbers are skipped/reserved in the LTMS range
4. The `:` prefix on `:LTLO_INPROGRESS` distinguishes it as a status rather than error
5. Error strings are stored in the binary at addresses shown in validation reports

---

## Binary Evidence

All error codes and strings were extracted from:
- **Binary**: `../../launcher.exe`
- **String addresses**: 0xab000 - 0xad2ac range
- **Code addresses**: Various locations calling error registration functions
- **Pattern**: `push <error_string>; push <error_code>; push <flags>; push <category>; call <registration>`

For detailed disassembly evidence, see the validation reports.
