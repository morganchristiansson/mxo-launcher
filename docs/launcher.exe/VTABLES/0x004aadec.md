# 0x004aadec - CLTLoginObserver_PassThrough

## Overview

`0x004aadec` is the tiny vtable for the pass-through login observer helper used by the launcher/login mediator path.

Current evidence:

- constructor/data xrefs point at this vtable from the mediator-side setup code
- slot `0x0041b8e0` logs the exact method string `CLTLoginObserver_PassThrough::OnLoginEvent()`
- slot `0x0041b990` logs the exact method string `CLTLoginObserver_PassThrough::OnLoginError()`
- both functions forward into a user callback stored on the helper object

This object is useful because it bridges internal mediator/login events into a simpler callback surface while preserving debug strings that help anchor static RE.

## VTable

| Slot | Offset | Entry Address | Current symbol | Notes |
|---|---:|---:|---|---|
| 1 | `+0x00` | `0x0041b8e0` | `CLTLoginObserver_PassThrough_OnLoginEvent` | logs event number + current server result, then forwards callback with errorFlag `0` |
| 2 | `+0x04` | `0x0041b990` | `CLTLoginObserver_PassThrough_OnLoginError` | logs error number, samples current server result through mediator `+0x178` / owner `+0x80`, then forwards callback with errorFlag `1` |

## Notes

The vtable is only two slots long.
Data immediately after it is not more virtual methods.

The callback forwarding shape currently reads as:

- event path -> callback `(0, eventNumber, 0, userData)`
- error path -> callback `(1, 0, errorNumber, userData)`

where `userData` comes from object offset `+0x0c`.

## Confidence

High for:

- class identity `CLTLoginObserver_PassThrough`
- slot names above
- the pass-through event/error forwarding role
