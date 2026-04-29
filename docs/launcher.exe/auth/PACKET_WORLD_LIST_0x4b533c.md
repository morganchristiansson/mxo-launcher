# Packet_WorldList_0x4b533c

Current source naming/shape correction for launcher.exe world-list packet/accessor object.

## Summary

`launcher.exe:0x4b533c` is modeled in source as `Packet_WorldList_0x4b533c`, a real `Packet_0x4af2a4` child.

This replaces the older nested `CLTLoginMediator::WorldDescriptorState_0x4b533c` shim because the static RE shows a packet-shaped class with inherited base fields, not a mediator-only helper shell.

## Anchors

- vtable: `launcher.exe:0x4b533c`
- incoming-message init helper: `launcher.exe:0x4399e0`
- temp-record copy helper: `launcher.exe:0x43c310`
- debug printer: `launcher.exe:0x43ded0`
- payload reset/builder helper: `launcher.exe:0x439b50`
- payload base getter: `launcher.exe:0x481760`
- owner world-descriptor table materialization: `launcher.exe:0x43f300`

## Current interpretation

The object is reused as a world-list/world-descriptor packet/accessor surface:

- builder/reset path at `0x439b50` writes payload opcode `0x35`
  - interpreted as `AS_GetWorldListRequest`
- related enclosing world-list reply path writes opcode `0x36`
  - interpreted as `AS_GetWorldListReply`
- the mediator stores an array of these packet/accessor objects at owner `+0xd84`

## Inherited `Packet_0x4af2a4` fields used

- `payloadPtr04`
- `messageRef08`
- `createRefParam0c`
- `payloadAlias10`

## Decoded semantic payload fields mirrored in source

Recovered from `0x43ded0` and owner-side readers:

- `payload + 0x01` = world id
- `payload + 0x03` = inline world name
- `payload + 0x17` = world status
- `payload + 0x18` = world type
- `payload + 0x19` = server version
- `payload + 0x1d` = server language
- `payload + 0x1e` = private flag
- `payload + 0x1f` = population level

## Source follow-up already applied

- removed the nested mediator-only class definition
- renamed the source type to `Packet_WorldList_0x4b533c`
- dropped the old unreferenced pseudo-vtable helper methods from the source model
- kept the real `Packet_0x4af2a4` inheritance prefix and slot overrides only
