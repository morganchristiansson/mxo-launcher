# API Surface - Extracted Strings Analysis

## Critical Discovery Evidence

### Client.dll References Found

```
client.dll
ErrorClientDLL
TermClientDLL
RunClientDLL
InitClientDLL
```

### Client Communication Functions

```
SendPlayerToLoadingArea
```

### TCP Connection State (API Functions)

```
LTTCP_ALREADYCONNECTED
LTTCP_NOTCONNECTED
LTMS_ALREADYCONNECTED
LTMS_INCOMPATIBLECLIENTVERSION
LTMS_UNKNOWNSESSION
LTMS_SESSIONESTABLISHINPROGRESS
LTMS_NODATABASECONNECTION
LTMS_SOESESSIONVALIDATIONFAILED
LTMS_SOESESSIONCONSUMEFAILED
LTMS_SOESESSIONSTARTPLAYFAILED
```

### Session Management API

```
-session
-qlsession
CERT_NewSessionKey
CERT_ConnectReply
CERT_ConnectRequest
MS_GetClientIPReply
MS_GetClientIPRequest
MS_RefreshSessionKeyReply
MS_RefreshSessionKeyRequest
MS_EstablishUDPSessionReply
MS_ConnectReply
MS_ConnectChallengeResponse
MS_ConnectChallenge
```

### Authentication API

```
ILTLoginMediator.Default
LTAUTH_LOGINTYPENOTACCEPTED
LTAUTH_ALREADYCONNECTED
LTAS_INCOMPATIBLECLIENTVERSION
LTAS_PROXYALREADYCONNECTED
AS_PSSetTransSessionPenaltyReply
AS_PSSetTransSessionPenaltyRequest
AS_SetTransSessionPenaltyRequest
AS_ProxyConnectReply
AS_ProxyConnectRequest
```

### Error Handling API

```
LT_INVALIDPOINTER
ErrorClientDLL
LTLO_CLIENTHASHFAILED
```

## API Function Categories

### 1. Client.dll Lifecycle

| Function | Purpose |
|----------|---------|
| InitClientDLL | Initialize client library |
| RunClientDLL | Start client execution |
| TermClientDLL | Terminate client |
| ErrorClientDLL | Client error handling |

### 2. TCP Connection API

| Function | Purpose |
|----------|---------|
| LTTCP_AlreadyConnected | Connection state check |
| LTTCP_NotConnected | Disconnection state |
| CERT_ConnectRequest | Connect request |
| CERT_ConnectReply | Connect reply |

### 3. Session Management API

| Function | Purpose |
|----------|---------|
| MS_GetClientIPRequest | Get client IP |
| MS_RefreshSessionKey | Refresh session key |
| MS_EstablishUDPSession | Establish UDP session |
| CERT_NewSessionKey | Create session key |

### 4. Authentication API

| Function | Purpose |
|----------|---------|
| ILTLoginMediator.Default | Login mediator |
| AS_ProxyConnectRequest | Proxy connect request |
| AS_PSSetTransSessionPenalty | Session penalty |

### 5. Error Handling API

| Function | Purpose |
|----------|---------|
| LT_INVALIDPOINTER | Invalid pointer error |
| LTLO_CLIENTHASHFAILED | Client hash failed |
| LTAUTH_ALREADYCONNECTED | Already connected error |

## Function Pointer Evidence

```
_PatchCtl_SetNotifyFunction@4
_PatchCtl_PatchCheck
_PatchCtl_DisableDiagnostics
_PatchCtl_EnableDiagnostics
_PatchCtl_GetDllVersion
_PatchCtl_GetResultAsString
_PatchCtl_RecoverDirectory
_PatchCtl_PatchDirectory
```

## Data Structure Evidence

```
LTMS_ALREADYCONNECTED
LTMS_INCOMPATIBLECLIENTVERSION
LTMS_UNKNOWNSESSION
LTMS_SESSIONESTABLISHINPROGRESS
LTMS_NODATABASECONNECTION
```

## API Pattern Analysis

### Request/Reply Pattern
```
CERT_ConnectRequest → CERT_ConnectReply
MS_GetClientIPRequest → MS_GetClientIPReply
AS_ProxyConnectRequest → AS_ProxyConnectReply
```

### State Machine Pattern
```
LTTCP_NotConnected → LTTCP_AlreadyConnected
LTMS_SessionEstablishInProgress → LTMS_AlreadyConnected
```

### Callback Pattern
```
_PatchCtl_SetNotifyFunction@4
CLTEvilBlockingLoginObserver::OnLoginEvent()
CLTEvilBlockingLoginObserver::OnLoginError()
```

## API Surface Characteristics

### 1. Large Function Set
- Dozens of client.dll functions
- TCP connection management
- Session handling
- Authentication
- Error handling

### 2. Function Pointers
- Callback mechanisms
- Notification functions
- Patch control functions

### 3. Data Structures
- Session objects
- Connection state
- Authentication tokens
- Error codes

### 4. Message Passing
- Request/Reply pairs
- Event notifications
- State transitions

## Next Steps

### HIGH PRIORITY
1. **Extract ALL function exports** - Even if not in symbol table
2. **Find function pointer tables** - Look for arrays of function pointers
3. **Map API usage** - Trace how client.dll uses launcher API
4. **Document protocol flow** - Map request/reply sequences

### MEDIUM PRIORITY
5. **Analyze callback mechanisms** - Find callback registration
6. **Trace session management** - Follow session lifecycle
7. **Document TCP handling** - Find socket management code

### LOW PRIORITY
8. **Create API reference** - Document all functions
9. **Build call graph** - Map function relationships
10. **Optimize API interface** - Identify improvements

## Notes

- API surface is intentionally large
- Client.dll depends on launcher for all network communication
- Protocol handling is centralized in launcher
- Function pointers suggest dynamic dispatch
- Session management is complex