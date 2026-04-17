#pragma once

#include "../liblttcp/lttcpconnection.h"

#include <cstdint>

namespace mxo::liblttcp {

// anchor: launcher.exe:0x434fa0
// The second argument is any one-pointer fragment slot (e.g. another ref handle or the stack
// parameter slot for `Parse(readOperationFragment, ...)`), not only a concrete handle object.
CLTTCPReadOperation* CLTTCPReadOperationRefHandle_AssignRetained(
    CLTTCPReadOperationRefHandle* handle,
    CLTTCPReadOperation* const* newRetainedFragmentSlot);
// anchor: launcher.exe:0x4350c0
CLTTCPReadOperationRefHandle* CParsedPacketWorkItem_BeginFragmentTraversal(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem,
    CLTTCPReadOperationRefHandle* outFragmentRef);
// anchor: launcher.exe:0x435510
CLTTCPReadOperationRefHandle* CParsedPacketWorkItem_GetNextFragment(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem,
    CLTTCPReadOperationRefHandle* outFragmentRef);
// anchor: launcher.exe:0x4355c0
CLTTCPReadOperationRefHandle* CParsedPacketWorkItem_GetTailFragment(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem,
    CLTTCPReadOperationRefHandle* outFragmentRef);
// anchor: launcher.exe:0x434f80
uint8_t* CParsedPacketWorkItem_GetCurrentCursor(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem);
// anchor: launcher.exe:0x434f60
uint32_t CParsedPacketWorkItem_GetAssembledByteCount(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem);

// UNANCHORED: source-owned release entry installed in the local parsed-packet work-item vtable.
uint32_t __thiscall CParsedPacketWorkItem_ReleaseScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* self);

// VTable `0x004baf84` - `CVariableLengthPrefixedTCPStreamParser`
// Current source mirrors the original object as an actual dedicated class instead of leaving the
// parser-owned state buried inside `CLTTCPConnection`.
class CVariableLengthPrefixedTCPStreamParser {
public:
    // anchor: launcher.exe:0x469b20
    CVariableLengthPrefixedTCPStreamParser();
    // anchor: launcher.exe:0x469ef0
    virtual ~CVariableLengthPrefixedTCPStreamParser();

    // anchor: launcher.exe:0x469bf0 / vtable 0x004baf88
    virtual uint32_t Parse(
        CLTTCPReadOperation* readOperationFragment,
        CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08** outCompletedPacketWorkItem);
    // anchor: launcher.exe:0x4415c0 / vtable 0x004baf8c
    // Exact original name/role still unsettled; keep the row explicit without inventing behavior.
    virtual void OnCompletedPacketScaffold();
    // anchor: launcher.exe:0x4725c0 / vtable 0x004baf90
    virtual void ResetAfterPacket();
    // anchor: launcher.exe:0x469b40 / vtable 0x004baf94
    virtual CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* AllocatePacketBuffer();

    CVariableLengthPrefixedTCPStreamParser(const CVariableLengthPrefixedTCPStreamParser&) = delete;
    CVariableLengthPrefixedTCPStreamParser& operator=(const CVariableLengthPrefixedTCPStreamParser&) = delete;

    // Recovered parser prefix fields proved on the current active seam:
    // - `+0x04` = retained fragment-handle helper currently containing cursor `+0x08`
    // - `+0x08` = next unread buffered byte pointer
    // - `+0x0c` = unread buffered byte count across retained fragments
    // - `+0x10` = byte-count state incremented by `0x472660`, zeroed by `0x4725c0`
    // - `+0x14` = current parser-owned `CParsedPacketWorkItem`
    CLTTCPReadOperationRefHandle currentCursorFragmentRef04{};
    uint8_t* currentCursor08 = nullptr;
    uint32_t unreadBufferedByteCount0C = 0u;
    uint32_t advancedBufferedByteCount10 = 0u;
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* currentPacketWorkItem14 = nullptr;

private:
    // anchor: launcher.exe:0x472560
    void InitState();
    // anchor: launcher.exe:0x472580
    void ClearState();
};

}  // namespace mxo::liblttcp
