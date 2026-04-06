#pragma once

#include <cstdint>

namespace mxo::liblttcp {

class CLTTCPReadOperationFragmentScaffold;
struct CLTTCPConnection_ParsedPacketWorkItemScaffold;

// anchor: launcher.exe:0x42f850 / vtable 0x004b2300 +0x04
void CLTTCPReadOperationFragment_AddRefScaffold(CLTTCPReadOperationFragmentScaffold* fragment);
// anchor: launcher.exe:0x42f860 / vtable 0x004b2300 +0x08
void CLTTCPReadOperationFragment_ReleaseScaffold(CLTTCPReadOperationFragmentScaffold* fragment);

// UNANCHORED: source-owned helpers over the recovered `CLTTCPReadOperation` fragment layout.
uint8_t* CLTTCPReadOperationFragment_PayloadBeginScaffold(
    CLTTCPReadOperationFragmentScaffold* fragment);
const uint8_t* CLTTCPReadOperationFragment_PayloadBeginScaffold(
    const CLTTCPReadOperationFragmentScaffold* fragment);
const uint8_t* CLTTCPReadOperationFragment_PayloadEndScaffold(
    const CLTTCPReadOperationFragmentScaffold* fragment);
uint32_t CLTTCPReadOperationFragment_BytesRemainingFromCursorScaffold(
    const CLTTCPReadOperationFragmentScaffold* fragment,
    const uint8_t* cursor);

// anchor: launcher.exe:0x4350c0
CLTTCPReadOperationFragmentScaffold* CParsedPacketWorkItem_BeginFragmentTraversalScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem);
// anchor: launcher.exe:0x435510
CLTTCPReadOperationFragmentScaffold* CParsedPacketWorkItem_GetNextFragmentScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem);
// anchor: launcher.exe:0x4355c0
CLTTCPReadOperationFragmentScaffold* CParsedPacketWorkItem_GetTailFragmentTempRefScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem);
// anchor: launcher.exe:0x434f80
uint8_t* CParsedPacketWorkItem_GetCurrentCursorScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem);
// anchor: launcher.exe:0x434f60
uint32_t CParsedPacketWorkItem_GetAssembledByteCountScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem);

// UNANCHORED: source-owned release entry installed in the local parsed-packet work-item vtable.
uint32_t __thiscall CParsedPacketWorkItem_ReleaseScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* self);

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
        CLTTCPReadOperationFragmentScaffold* readOperationFragment,
        CLTTCPConnection_ParsedPacketWorkItemScaffold** outCompletedPacketWorkItem);
    // anchor: launcher.exe:0x4415c0 / vtable 0x004baf8c
    // Exact original name/role still unsettled; keep the row explicit without inventing behavior.
    virtual void OnCompletedPacketScaffold();
    // anchor: launcher.exe:0x4725c0 / vtable 0x004baf90
    virtual void ResetAfterPacket();
    // anchor: launcher.exe:0x469b40 / vtable 0x004baf94
    virtual CLTTCPConnection_ParsedPacketWorkItemScaffold* AllocatePacketBuffer();

    CVariableLengthPrefixedTCPStreamParser(const CVariableLengthPrefixedTCPStreamParser&) = delete;
    CVariableLengthPrefixedTCPStreamParser& operator=(const CVariableLengthPrefixedTCPStreamParser&) = delete;

    // Recovered parser prefix fields proved on the current active seam:
    // - `+0x04` = retained fragment currently containing parser cursor `+0x08`
    // - `+0x08` = next unread buffered byte pointer
    // - `+0x0c` = unread buffered byte count across retained fragments
    // - `+0x10` = byte-count state incremented by `0x472660`, zeroed by `0x4725c0`
    // - `+0x14` = current parser-owned `CParsedPacketWorkItem`
    CLTTCPReadOperationFragmentScaffold* currentCursorFragment04 = nullptr;
    uint8_t* currentCursor08 = nullptr;
    uint32_t unreadBufferedByteCount0C = 0u;
    uint32_t advancedBufferedByteCount10 = 0u;
    CLTTCPConnection_ParsedPacketWorkItemScaffold* currentPacketWorkItem14 = nullptr;

private:
    // anchor: launcher.exe:0x472560
    void InitState();
    // anchor: launcher.exe:0x472580
    void ClearState();
};

}  // namespace mxo::liblttcp
