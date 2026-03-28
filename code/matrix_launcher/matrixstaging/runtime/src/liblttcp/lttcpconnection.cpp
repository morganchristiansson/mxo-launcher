#include "lttcpconnection.h"

#include "ltthreadperclienttcpengine.h"

#include <winsock2.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "spdlog/spdlog.h"

namespace mxo::liblttcp {

namespace {

static constexpr uint32_t kInvalidSocketHandle = 0xffffffffu;
static void* g_ParsedPacketWorkItemVtable[2] = {nullptr, nullptr};
static void* g_BaseConnectionQueueContextVtable[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
static CLTTCPReadOperationFragmentVTable g_ReadOperationFragmentSourceVtable = {};

// UNANCHORED: source-owned endpoint-key comparison helper for the current connection wrapper.
static bool EndpointKeysDiffer(const LTTCPEndpointKey& lhs, const LTTCPEndpointKey& rhs) {
    return lhs.family != rhs.family ||
        lhs.portNetworkOrder != rhs.portNetworkOrder ||
        lhs.ipv4NetworkOrder != rhs.ipv4NetworkOrder ||
        lhs.reserved0 != rhs.reserved0 ||
        lhs.reserved1 != rhs.reserved1;
}

// UNANCHORED: source-owned shim for the explicit fragment `+0x04` virtual at the start of
// `CLTTCPConnection::OnReceive` / `CVariableLengthPrefixedTCPStreamParser::Parse`.
// Current best static read from the assembly:
// - this is a no-arg AddRef / retain on the fragment object itself
// - the apparent stack values around the call belong to the immediately following parser / append
//   call, not to the fragment virtual
static void ReadOperationFragment_AddRef(CLTTCPReadOperationFragmentScaffold* fragment) {
    if (!fragment || !fragment->vtable || !fragment->vtable->addRef) {
        return;
    }

    fragment->vtable->addRef(fragment);
}

// UNANCHORED: source-owned release shim for the read-operation fragment object passed to
// `CLTTCPConnection::OnReceive` / `CLTTCPConnection::OnClose`.
// Current best static read of the concrete `CLTTCPReadOperation` family:
// - decrements interlocked refcount at `+0x04`
// - zero-count path dispatches vtable `+0x0c`, which then reaches the deleting-dtor-style slot `+0x00`
static void ReadOperationFragment_Release(CLTTCPReadOperationFragmentScaffold* fragment) {
    if (!fragment || !fragment->vtable || !fragment->vtable->release) {
        return;
    }

    fragment->vtable->release(fragment);
}

// UNANCHORED: source-owned queue-context release bridge for current non-byte-faithful C++ objects.
static uint32_t __thiscall BaseConnectionQueueContext_ReleaseScaffold(
    CBaseConnection_QueueContextScaffold* /*self*/) {
    return 1u;
}

// UNANCHORED: source-owned queue-context completion bridge for current non-byte-faithful C++ objects.
static uint32_t __thiscall BaseConnectionQueueContext_OnOperationCompletedScaffold(
    CBaseConnection_QueueContextScaffold* self,
    void* workItem) {
    return (self && self->owner) ? self->owner->OnOperationCompleted(workItem) : 0u;
}

static void EnsureBaseConnectionQueueContextVtableInitialized() {
    if (!g_BaseConnectionQueueContextVtable[1]) {
        g_BaseConnectionQueueContextVtable[1] =
            reinterpret_cast<void*>(BaseConnectionQueueContext_ReleaseScaffold);
        g_BaseConnectionQueueContextVtable[4] =
            reinterpret_cast<void*>(BaseConnectionQueueContext_OnOperationCompletedScaffold);
    }
}

// anchor: launcher.exe:0x42fd50 / vtable 0x004b2300 +0x00
static void* __thiscall ReadOperationFragmentSource_DeletingDtorScaffold(
    CLTTCPReadOperationFragmentScaffold* self,
    uint8_t deleteFlag) {
    if (self && (deleteFlag & 1u) != 0u) {
        std::free(self);
    }
    return nullptr;
}

// anchor: launcher.exe:0x42f850 / vtable 0x004b2300 +0x04
static void __thiscall ReadOperationFragmentSource_AddRefScaffold(
    CLTTCPReadOperationFragmentScaffold* self) {
    if (!self) {
        return;
    }
    (void)InterlockedIncrement(reinterpret_cast<volatile LONG*>(&self->referenceCount));
}

// anchor: launcher.exe:0x42f860 / vtable 0x004b2300 +0x08
static void __thiscall ReadOperationFragmentSource_ReleaseScaffold(
    CLTTCPReadOperationFragmentScaffold* self) {
    if (!self) {
        return;
    }

    const LONG remaining =
        InterlockedDecrement(reinterpret_cast<volatile LONG*>(&self->referenceCount));
    if (remaining == 0 && self->vtable && self->vtable->deleteIfNonNull) {
        self->vtable->deleteIfNonNull(self);
    }
}

// anchor: launcher.exe:0x004199b0 / vtable 0x004b2300 +0x0c
static void __thiscall ReadOperationFragmentSource_DeleteIfNonNullScaffold(
    CLTTCPReadOperationFragmentScaffold* self) {
    if (!self || !self->vtable || !self->vtable->deletingDtor) {
        return;
    }

    (void)self->vtable->deletingDtor(self, 1u);
}

// anchor: launcher.exe:0x42f880 / vtable 0x004b2300 +0x10
static void __thiscall ReadOperationFragmentSource_ResetRefCountScaffold(
    CLTTCPReadOperationFragmentScaffold* self) {
    if (!self) {
        return;
    }
    (void)InterlockedExchange(reinterpret_cast<volatile LONG*>(&self->referenceCount), 0);
}

// anchor: launcher.exe:0x42f890 / vtable 0x004b2300 +0x14
static void __thiscall ReadOperationFragmentSource_SetRefCountFromPtrScaffold(
    CLTTCPReadOperationFragmentScaffold* self,
    const long* value) {
    if (!self || !value) {
        return;
    }
    (void)InterlockedExchange(
        reinterpret_cast<volatile LONG*>(&self->referenceCount),
        static_cast<LONG>(*value));
}

// anchor: launcher.exe:0x42fe50 TCP receive-path `CLTTCPReadOperation` allocation/setup
static CLTTCPReadOperationFragmentScaffold* AllocateReadOperationFragmentSourceScaffold() {
    constexpr size_t kPayloadCapacity = 0x1000u;
    constexpr size_t kAllocationSize = offsetof(CLTTCPReadOperationFragmentScaffold, bytes0C) + kPayloadCapacity;
    if (!g_ReadOperationFragmentSourceVtable.addRef) {
        g_ReadOperationFragmentSourceVtable.deletingDtor =
            &ReadOperationFragmentSource_DeletingDtorScaffold;
        g_ReadOperationFragmentSourceVtable.addRef = &ReadOperationFragmentSource_AddRefScaffold;
        g_ReadOperationFragmentSourceVtable.release = &ReadOperationFragmentSource_ReleaseScaffold;
        g_ReadOperationFragmentSourceVtable.deleteIfNonNull =
            &ReadOperationFragmentSource_DeleteIfNonNullScaffold;
        g_ReadOperationFragmentSourceVtable.resetRefCount =
            &ReadOperationFragmentSource_ResetRefCountScaffold;
        g_ReadOperationFragmentSourceVtable.setRefCountFromPtr =
            &ReadOperationFragmentSource_SetRefCountFromPtrScaffold;
    }

    CLTTCPReadOperationFragmentScaffold* fragment =
        static_cast<CLTTCPReadOperationFragmentScaffold*>(std::calloc(1, kAllocationSize));
    if (!fragment) {
        return nullptr;
    }

    fragment->vtable = &g_ReadOperationFragmentSourceVtable;
    fragment->referenceCount = 0;
    fragment->byteCount = 0u;
    return fragment;
}

// anchor: launcher.exe:0x452350
static void ReadOperationFragmentSource_SetByteCountScaffold(
    CLTTCPReadOperationFragmentScaffold* fragment,
    uint32_t byteCount) {
    if (!fragment) {
        return;
    }
    fragment->byteCount = std::min<uint32_t>(byteCount, 0x1000u);
}

// UNANCHORED: source-owned payload-base helper for the recovered read-operation fragment family.
static uint8_t* ReadOperationFragment_PayloadBegin(CLTTCPReadOperationFragmentScaffold* fragment) {
    return fragment ? fragment->bytes0C : nullptr;
}

// UNANCHORED: source-owned payload-end helper for the recovered read-operation fragment family.
static const uint8_t* ReadOperationFragment_PayloadEnd(
    const CLTTCPReadOperationFragmentScaffold* fragment) {
    return fragment ? (fragment->bytes0C + fragment->byteCount) : nullptr;
}

// UNANCHORED: source-owned remaining-byte helper for the recovered read-operation fragment family.
static uint32_t ReadOperationFragment_BytesRemainingFromCursor(
    const CLTTCPReadOperationFragmentScaffold* fragment,
    const uint8_t* cursor) {
    if (!fragment || !cursor) {
        return 0u;
    }

    const uint8_t* begin = fragment->bytes0C;
    const uint8_t* end = ReadOperationFragment_PayloadEnd(fragment);
    if (cursor < begin || cursor > end) {
        return 0u;
    }

    return static_cast<uint32_t>(end - cursor);
}

// UNANCHORED: source-owned parser current-fragment ref helper mirroring parser `+0x04` ownership.
static void Parser_AssignCurrentCursorFragmentScaffold(
    CVariableLengthPrefixedTCPStreamParserScaffold* parser,
    CLTTCPReadOperationFragmentScaffold* fragment) {
    if (!parser || parser->currentCursorFragment04 == fragment) {
        return;
    }

    ReadOperationFragment_Release(parser->currentCursorFragment04);
    parser->currentCursorFragment04 = fragment;
    if (fragment) {
        ReadOperationFragment_AddRef(fragment);
    }
}

// UNANCHORED: source-owned release helper for the parsed-packet work-item scaffold.
static uint32_t __thiscall ParsedPacketWorkItem_ReleaseScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* self) {
    if (!self) {
        return 1u;
    }

    if (self->firstRetainedFragment10) {
        ReadOperationFragment_Release(self->firstRetainedFragment10);
        self->firstRetainedFragment10 = nullptr;
    }

    if (self->retainedFragmentListOwner14) {
        CParsedPacketWorkItem_RetainedFragmentNodeScaffold* sentinel =
            self->retainedFragmentListOwner14->sentinel;
        if (sentinel) {
            CParsedPacketWorkItem_RetainedFragmentNodeScaffold* node = sentinel->next;
            while (node && node != sentinel) {
                CParsedPacketWorkItem_RetainedFragmentNodeScaffold* next = node->next;
                ReadOperationFragment_Release(node->retainedFragment08);
                std::free(node);
                node = next;
            }
            std::free(sentinel);
        }
        std::free(self->retainedFragmentListOwner14);
        self->retainedFragmentListOwner14 = nullptr;
    }

    std::free(self);
    return 1u;
}

// UNANCHORED: source-owned vtable init helper for queued parsed-packet work items.
static void EnsureParsedPacketWorkItemVtableInitialized() {
    if (!g_ParsedPacketWorkItemVtable[1]) {
        g_ParsedPacketWorkItemVtable[1] =
            reinterpret_cast<void*>(ParsedPacketWorkItem_ReleaseScaffold);
    }
}

// Recovered original source-file anchor for the parser helper family reflected below:
// - `\matrixstaging\runtime\src\libltmessaging\variablelengthprefixedtcpstreamparser.cpp`
// Current source still keeps the parser helper scaffolds beside `CLTTCPConnection` because the
// parser object is not yet split into its own source-owned runtime type. Direct one-to-one parser
// helper mirrors should still carry launcher.exe anchors even while this temporary file placement
// remains.

// UNANCHORED: source-owned allocator for the recovered `0x2c` parsed-packet work-item family.
static CLTTCPConnection_ParsedPacketWorkItemScaffold* AllocateParsedPacketWorkItemScaffold() {
    EnsureParsedPacketWorkItemVtableInitialized();
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem =
        static_cast<CLTTCPConnection_ParsedPacketWorkItemScaffold*>(
            std::calloc(1, sizeof(CLTTCPConnection_ParsedPacketWorkItemScaffold)));
    if (!workItem) {
        return nullptr;
    }

    workItem->vtable = g_ParsedPacketWorkItemVtable;
    workItem->workType = 3u;
    workItem->directFragmentTraversalPhase18 = 1u;
    return workItem;
}

// UNANCHORED: source-owned list-owner allocator for additional retained fragments.
static bool ParsedPacketWorkItem_EnsureAdditionalFragmentListScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    if (!workItem) {
        return false;
    }
    if (workItem->retainedFragmentListOwner14 && workItem->retainedFragmentListOwner14->sentinel) {
        return true;
    }

    CParsedPacketWorkItem_RetainedFragmentListOwnerScaffold* owner =
        static_cast<CParsedPacketWorkItem_RetainedFragmentListOwnerScaffold*>(
            std::calloc(1, sizeof(CParsedPacketWorkItem_RetainedFragmentListOwnerScaffold)));
    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* sentinel =
        static_cast<CParsedPacketWorkItem_RetainedFragmentNodeScaffold*>(
            std::calloc(1, sizeof(CParsedPacketWorkItem_RetainedFragmentNodeScaffold)));
    if (!owner || !sentinel) {
        std::free(sentinel);
        std::free(owner);
        return false;
    }

    sentinel->next = sentinel;
    sentinel->prev = sentinel;
    owner->sentinel = sentinel;
    workItem->retainedFragmentListOwner14 = owner;
    return true;
}

// UNANCHORED: source-owned retained-fragment append helper for the recovered parsed-packet family.
static bool ParsedPacketWorkItem_AppendFragmentScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
    CLTTCPReadOperationFragmentScaffold* fragment) {
    if (!workItem || !fragment) {
        return false;
    }

    if (workItem->retainedFragmentCount0C == 0u) {
        if (workItem->firstRetainedFragment10 != fragment) {
            ReadOperationFragment_Release(workItem->firstRetainedFragment10);
            workItem->firstRetainedFragment10 = fragment;
            ReadOperationFragment_AddRef(fragment);
        }
        workItem->retainedFragmentCount0C = 1u;
        return true;
    }

    if (!ParsedPacketWorkItem_EnsureAdditionalFragmentListScaffold(workItem)) {
        return false;
    }

    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* node =
        static_cast<CParsedPacketWorkItem_RetainedFragmentNodeScaffold*>(
            std::calloc(1, sizeof(CParsedPacketWorkItem_RetainedFragmentNodeScaffold)));
    if (!node) {
        return false;
    }

    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* sentinel =
        workItem->retainedFragmentListOwner14->sentinel;
    node->retainedFragment08 = fragment;
    ReadOperationFragment_AddRef(fragment);
    node->next = sentinel;
    node->prev = sentinel->prev;
    sentinel->prev->next = node;
    sentinel->prev = node;
    ++workItem->retainedFragmentCount0C;
    return true;
}

// UNANCHORED: source-owned tail-fragment helper for the recovered parsed-packet family.
static CLTTCPReadOperationFragmentScaffold* ParsedPacketWorkItem_GetTailFragmentScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    if (!workItem || workItem->retainedFragmentCount0C == 0u) {
        return nullptr;
    }
    if (workItem->retainedFragmentCount0C == 1u || !workItem->retainedFragmentListOwner14 ||
        !workItem->retainedFragmentListOwner14->sentinel) {
        return workItem->firstRetainedFragment10;
    }

    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* tailNode =
        workItem->retainedFragmentListOwner14->sentinel->prev;
    if (!tailNode || tailNode == workItem->retainedFragmentListOwner14->sentinel) {
        return workItem->firstRetainedFragment10;
    }
    return tailNode->retainedFragment08;
}

// anchor: launcher.exe:0x4355c0
// Narrow source-owned helper matching the temp-ref return shape used by
// `CVariableLengthPrefixedTCPStreamParser_ResetAfterPacket`.
static CLTTCPReadOperationFragmentScaffold* ParsedPacketWorkItem_GetTailFragmentTempRefScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    CLTTCPReadOperationFragmentScaffold* tailFragment =
        ParsedPacketWorkItem_GetTailFragmentScaffold(workItem);
    if (tailFragment) {
        ReadOperationFragment_AddRef(tailFragment);
    }
    return tailFragment;
}

// UNANCHORED: source-owned allocator guard for parser `+0x14` current work-item state.
static bool Parser_EnsureCurrentWorkItemScaffold(
    CVariableLengthPrefixedTCPStreamParserScaffold* parser) {
    if (!parser) {
        return false;
    }
    if (parser->currentPacketWorkItem14) {
        return true;
    }

    parser->currentPacketWorkItem14 = AllocateParsedPacketWorkItemScaffold();
    return parser->currentPacketWorkItem14 != nullptr;
}

// UNANCHORED: source-owned first-fragment helper for the recovered parsed-packet family.
static CLTTCPReadOperationFragmentScaffold* ParsedPacketWorkItem_GetFirstFragmentScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    return (workItem && workItem->retainedFragmentCount0C != 0u)
        ? workItem->firstRetainedFragment10
        : nullptr;
}

// UNANCHORED: source-owned next-fragment helper for the recovered parsed-packet family.
static CLTTCPReadOperationFragmentScaffold* ParsedPacketWorkItem_GetNextFragmentAfterScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
    const CLTTCPReadOperationFragmentScaffold* fragment) {
    if (!workItem || !fragment || workItem->retainedFragmentCount0C == 0u) {
        return nullptr;
    }

    if (fragment == ParsedPacketWorkItem_GetFirstFragmentScaffold(workItem)) {
        if (!workItem->retainedFragmentListOwner14 ||
            !workItem->retainedFragmentListOwner14->sentinel) {
            return nullptr;
        }

        CParsedPacketWorkItem_RetainedFragmentNodeScaffold* firstNode =
            workItem->retainedFragmentListOwner14->sentinel->next;
        return (firstNode && firstNode != workItem->retainedFragmentListOwner14->sentinel)
            ? firstNode->retainedFragment08
            : nullptr;
    }

    if (!workItem->retainedFragmentListOwner14 ||
        !workItem->retainedFragmentListOwner14->sentinel) {
        return nullptr;
    }

    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* sentinel =
        workItem->retainedFragmentListOwner14->sentinel;
    for (CParsedPacketWorkItem_RetainedFragmentNodeScaffold* node = sentinel->next;
         node && node != sentinel;
         node = node->next) {
        if (node->retainedFragment08 != fragment) {
            continue;
        }

        CParsedPacketWorkItem_RetainedFragmentNodeScaffold* nextNode = node->next;
        return (nextNode && nextNode != sentinel) ? nextNode->retainedFragment08 : nullptr;
    }
    return nullptr;
}

// UNANCHORED: source-owned rebase helper for the parser-owned work item after prefix consumption.
// Static RE of `0x4490c0` now confirms the consumer begins from
// `CParsedPacketWorkItem_BeginFragmentTraversal -> firstRetainedFragment10`, so when prefix consume
// advances the parser cursor into a later retained fragment we must also drop the fully consumed
// leading fragments from the current work item rather than teaching the consumer to search for a
// later cursor fragment.
static bool ParsedPacketWorkItem_RebaseFirstFragmentToScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
    CLTTCPReadOperationFragmentScaffold* firstFragmentToKeep) {
    if (!workItem || !firstFragmentToKeep || workItem->retainedFragmentCount0C == 0u ||
        workItem->firstRetainedFragment10 == firstFragmentToKeep) {
        return true;
    }

    std::vector<CLTTCPReadOperationFragmentScaffold*> keptFragments;
    for (CLTTCPReadOperationFragmentScaffold* fragment = firstFragmentToKeep;
         fragment != nullptr;
         fragment = ParsedPacketWorkItem_GetNextFragmentAfterScaffold(workItem, fragment)) {
        ReadOperationFragment_AddRef(fragment);
        keptFragments.push_back(fragment);
    }
    if (keptFragments.empty()) {
        return false;
    }

    if (workItem->firstRetainedFragment10) {
        ReadOperationFragment_Release(workItem->firstRetainedFragment10);
        workItem->firstRetainedFragment10 = nullptr;
    }

    if (workItem->retainedFragmentListOwner14) {
        CParsedPacketWorkItem_RetainedFragmentNodeScaffold* sentinel =
            workItem->retainedFragmentListOwner14->sentinel;
        if (sentinel) {
            CParsedPacketWorkItem_RetainedFragmentNodeScaffold* node = sentinel->next;
            while (node && node != sentinel) {
                CParsedPacketWorkItem_RetainedFragmentNodeScaffold* next = node->next;
                ReadOperationFragment_Release(node->retainedFragment08);
                std::free(node);
                node = next;
            }
            std::free(sentinel);
        }
        std::free(workItem->retainedFragmentListOwner14);
        workItem->retainedFragmentListOwner14 = nullptr;
    }

    workItem->retainedFragmentCount0C = 0u;
    workItem->directFragmentTraversalPhase18 = 1u;
    workItem->fragmentTraversalIndex1C = 0u;
    workItem->fragmentTraversalNode20 = nullptr;

    for (CLTTCPReadOperationFragmentScaffold* fragment : keptFragments) {
        const bool appended = ParsedPacketWorkItem_AppendFragmentScaffold(workItem, fragment);
        ReadOperationFragment_Release(fragment);
        if (!appended) {
            return false;
        }
    }
    return true;
}

// UNANCHORED: source-owned cursor normalization helper for the recovered parser prefix.
static bool Parser_NormalizeCursorFragmentScaffold(
    CVariableLengthPrefixedTCPStreamParserScaffold* parser) {
    if (!parser) {
        return false;
    }
    if (parser->unreadBufferedByteCount0C == 0u) {
        return true;
    }
    if (!parser->currentPacketWorkItem14 || !parser->currentCursorFragment04 || !parser->currentCursor08) {
        return false;
    }

    const uint32_t remainingInCurrentFragment =
        ReadOperationFragment_BytesRemainingFromCursor(
            parser->currentCursorFragment04,
            parser->currentCursor08);
    if (remainingInCurrentFragment != 0u) {
        return true;
    }

    CLTTCPReadOperationFragmentScaffold* nextFragment =
        ParsedPacketWorkItem_GetNextFragmentAfterScaffold(
            parser->currentPacketWorkItem14,
            parser->currentCursorFragment04);
    if (!nextFragment) {
        return false;
    }

    Parser_AssignCurrentCursorFragmentScaffold(parser, nextFragment);
    parser->currentCursor08 = ReadOperationFragment_PayloadBegin(nextFragment);
    return true;
}

// UNANCHORED: source-owned buffered-byte peek helper over retained parser fragments.
static bool Parser_PeekBufferedByteAtOffsetScaffold(
    CVariableLengthPrefixedTCPStreamParserScaffold* parser,
    uint32_t byteOffset,
    uint8_t* outByte) {
    if (!parser || !outByte || byteOffset >= parser->unreadBufferedByteCount0C) {
        return false;
    }
    if (!Parser_NormalizeCursorFragmentScaffold(parser)) {
        return false;
    }

    CLTTCPReadOperationFragmentScaffold* fragment = parser->currentCursorFragment04;
    const uint8_t* cursor = parser->currentCursor08;
    uint32_t bytesRemainingInFragment =
        ReadOperationFragment_BytesRemainingFromCursor(fragment, cursor);
    if (byteOffset < bytesRemainingInFragment) {
        *outByte = cursor[byteOffset];
        return true;
    }

    byteOffset -= bytesRemainingInFragment;
    fragment = ParsedPacketWorkItem_GetNextFragmentAfterScaffold(parser->currentPacketWorkItem14, fragment);
    while (fragment) {
        if (byteOffset < fragment->byteCount) {
            *outByte = fragment->bytes0C[byteOffset];
            return true;
        }
        byteOffset -= fragment->byteCount;
        fragment = ParsedPacketWorkItem_GetNextFragmentAfterScaffold(
            parser->currentPacketWorkItem14,
            fragment);
    }
    return false;
}

// anchor: launcher.exe:0x472660
// Narrow source-owned mirror of the active nonzero-flag cursor-advance path used by current
// `0x469bf0` xrefs.
static bool Parser_AdvanceBufferedCursorScaffold(
    CVariableLengthPrefixedTCPStreamParserScaffold* parser,
    uint32_t byteCountToConsume) {
    if (!parser || byteCountToConsume == 0u) {
        return true;
    }
    if (!parser->currentPacketWorkItem14 || parser->unreadBufferedByteCount0C < byteCountToConsume) {
        return false;
    }
    if (!Parser_NormalizeCursorFragmentScaffold(parser)) {
        return false;
    }

    CLTTCPReadOperationFragmentScaffold* currentFragment = parser->currentCursorFragment04;
    const uint8_t* currentCursor = parser->currentCursor08;
    const uint32_t bytesRemainingInCurrentFragment =
        ReadOperationFragment_BytesRemainingFromCursor(currentFragment, currentCursor);
    const uint32_t consumeInCurrentFragment = std::min(byteCountToConsume, bytesRemainingInCurrentFragment);
    uint32_t remainingToConsume = byteCountToConsume - consumeInCurrentFragment;

    CLTTCPReadOperationFragmentScaffold* resolvedFragment = currentFragment;
    const uint8_t* resolvedCursor = currentCursor + consumeInCurrentFragment;
    if (remainingToConsume != 0u) {
        resolvedFragment =
            ParsedPacketWorkItem_GetNextFragmentAfterScaffold(parser->currentPacketWorkItem14, currentFragment);
        while (resolvedFragment) {
            const uint32_t fragmentByteCount = resolvedFragment->byteCount;
            if (remainingToConsume <= fragmentByteCount) {
                resolvedCursor = ReadOperationFragment_PayloadBegin(resolvedFragment) + remainingToConsume;
                break;
            }
            remainingToConsume -= fragmentByteCount;
            resolvedFragment = ParsedPacketWorkItem_GetNextFragmentAfterScaffold(
                parser->currentPacketWorkItem14,
                resolvedFragment);
        }
        if (!resolvedFragment) {
            return false;
        }
    }

    const uint32_t unreadBufferedAfterAdvance = parser->unreadBufferedByteCount0C - byteCountToConsume;
    if (unreadBufferedAfterAdvance != 0u && resolvedFragment &&
        resolvedCursor == ReadOperationFragment_PayloadEnd(resolvedFragment)) {
        CLTTCPReadOperationFragmentScaffold* nextFragment =
            ParsedPacketWorkItem_GetNextFragmentAfterScaffold(
                parser->currentPacketWorkItem14,
                resolvedFragment);
        if (!nextFragment) {
            return false;
        }
        resolvedFragment = nextFragment;
        resolvedCursor = ReadOperationFragment_PayloadBegin(nextFragment);
    }

    parser->unreadBufferedByteCount0C = unreadBufferedAfterAdvance;
    parser->advancedBufferedByteCount10 += byteCountToConsume;
    if (unreadBufferedAfterAdvance == 0u) {
        parser->currentCursor08 = nullptr;
        return true;
    }

    Parser_AssignCurrentCursorFragmentScaffold(parser, resolvedFragment);
    parser->currentCursor08 = const_cast<uint8_t*>(resolvedCursor);
    return true;
}

// anchor: launcher.exe:0x4725c0
// Narrow source-owned mirror of the post-emit reset helper reached from parser virtual `+0x0c`.
static void Parser_ResetAfterPacketScaffold(
    CVariableLengthPrefixedTCPStreamParserScaffold* parser) {
    if (!parser) {
        return;
    }

    CLTTCPConnection_ParsedPacketWorkItemScaffold* emittedWorkItem = parser->currentPacketWorkItem14;
    CLTTCPConnection_ParsedPacketWorkItemScaffold* replacementWorkItem =
        AllocateParsedPacketWorkItemScaffold();
    parser->currentPacketWorkItem14 = replacementWorkItem;
    Parser_AssignCurrentCursorFragmentScaffold(parser, nullptr);
    parser->advancedBufferedByteCount10 = 0u;

    if (parser->unreadBufferedByteCount0C == 0u || !replacementWorkItem || !emittedWorkItem) {
        return;
    }

    CLTTCPReadOperationFragmentScaffold* tailFragmentTempRef =
        ParsedPacketWorkItem_GetTailFragmentTempRefScaffold(emittedWorkItem);
    if (tailFragmentTempRef != parser->currentCursorFragment04) {
        Parser_AssignCurrentCursorFragmentScaffold(parser, tailFragmentTempRef);
    }
    ReadOperationFragment_Release(tailFragmentTempRef);

    CLTTCPReadOperationFragmentScaffold* parserCurrentCursorFragment =
        parser->currentCursorFragment04;
    if (parserCurrentCursorFragment) {
        ReadOperationFragment_AddRef(parserCurrentCursorFragment);
        const bool appended = ParsedPacketWorkItem_AppendFragmentScaffold(
            replacementWorkItem,
            parserCurrentCursorFragment);
        ReadOperationFragment_Release(parserCurrentCursorFragment);
        if (!appended) {
            return;
        }
    }

    replacementWorkItem->currentCursor24 = parser->currentCursor08;
}

}  // namespace

// ============================================================
// VTable 0x004b8034 - CLTTCPConnection (Base Class)
// High-confidence recovered wrapper entries:
// - 0x004b8040 -> 0x00449ca0 = Close wrapper into engine slot +0x1c
// - 0x004b8048 -> 0x00449d40 = OnReceive
// - 0x004b804c -> 0x00449fd0 = OnClose callback forwarder
// - 0x004b8050 -> 0x00449cd0 = Connect wrapper into engine slot +0x18
// - 0x004b8054 -> 0x00449d20 = SendBuffer wrapper into engine slot +0x20
// ============================================================

// UNANCHORED: source-owned narrow subset of the `0x44aad0` ctor family.
CLTTCPConnection::CLTTCPConnection()
    : CBaseConnection(LTTCPEngineConnectionState::kClosed),
      engine_(nullptr),
      ownerContext_(nullptr),
      socketHandle_(kInvalidSocketHandle),
      remoteEndpoint_(),
      remoteHostName_(),
      receivedBytes_(),
      parserScaffold_() {
    parserScaffold_.currentPacketWorkItem14 = AllocateParsedPacketWorkItemScaffold();
}

// UNANCHORED: source-owned narrow subset of the `0x44aad0` ctor family that also seeds the
// replacement-side owner-context scaffold.
CLTTCPConnection::CLTTCPConnection(void* ownerContext)
    : CBaseConnection(LTTCPEngineConnectionState::kClosed),
      engine_(nullptr),
      ownerContext_(ownerContext),
      socketHandle_(kInvalidSocketHandle),
      remoteEndpoint_(),
      remoteHostName_(),
      receivedBytes_(),
      parserScaffold_() {
    parserScaffold_.currentPacketWorkItem14 = AllocateParsedPacketWorkItemScaffold();
}

// anchor: launcher.exe:0x44ac40
CLTTCPConnection::~CLTTCPConnection() {
    Parser_AssignCurrentCursorFragmentScaffold(&parserScaffold_, nullptr);
    (void)ParsedPacketWorkItem_ReleaseScaffold(parserScaffold_.currentPacketWorkItem14);
    parserScaffold_.currentPacketWorkItem14 = nullptr;
}

// UNANCHORED: source-owned utility accessor over the recovered `+0x34` state field.
bool CBaseConnection::IsConnected() const {
    return static_cast<uint32_t>(state_) != static_cast<uint32_t>(LTTCPEngineConnectionState::kClosed);
}

// UNANCHORED: source-owned narrow mirror of the `0x44a9f0` base-ctor state write.
CBaseConnection::CBaseConnection(LTTCPEngineConnectionState initialState)
    : state_(initialState),
      queueContextScaffold_() {
    EnsureBaseConnectionQueueContextVtableInitialized();
    queueContextScaffold_.vtable = g_BaseConnectionQueueContextVtable;
    queueContextScaffold_.autoReleaseFlag = 0u;
    queueContextScaffold_.owner = this;
}

// UNANCHORED: source-owned compatibility wrapper over the recovered connection `+0x10` engine field.
void CLTTCPConnection::SetEngine(CLTThreadPerClientTCPEngine* engine) {
    engine_ = engine;
}

// UNANCHORED: source-owned compatibility accessor over the recovered connection `+0x10` engine field.
CLTThreadPerClientTCPEngine* CLTTCPConnection::Engine() const {
    return engine_;
}

// UNANCHORED: source-owned owner-context setter used by the current scaffolds.
void CLTTCPConnection::SetOwnerContext(void* ownerContext) {
    ownerContext_ = ownerContext;
}

// UNANCHORED: source-owned owner-context accessor used by the current scaffolds.
void* CLTTCPConnection::OwnerContext() const {
    return ownerContext_;
}

// UNANCHORED: source-owned socket-handle setter used by the current scaffolds.
void CLTTCPConnection::SetSocketHandle(uint32_t socketHandle) {
    socketHandle_ = socketHandle;
}

// UNANCHORED: source-owned socket-handle accessor used by the current scaffolds.
uint32_t CLTTCPConnection::SocketHandle() const {
    return socketHandle_;
}

// UNANCHORED: source-owned connection-state setter used by the current scaffolds.
void CLTTCPConnection::SetState(LTTCPEngineConnectionState state) {
    state_ = state;
}

// UNANCHORED: source-owned connection-state accessor used by the current scaffolds.
LTTCPEngineConnectionState CLTTCPConnection::State() const {
    return CBaseConnection::State();
}

// UNANCHORED: source-owned endpoint setter over the recovered connection `+0x24` copy.
void CLTTCPConnection::SetRemoteEndpoint(const LTTCPEndpointKey& endpoint) {
    remoteEndpoint_ = endpoint;
}

// UNANCHORED: source-owned endpoint accessor over the recovered connection `+0x24` copy.
const LTTCPEndpointKey& CLTTCPConnection::RemoteEndpoint() const {
    return remoteEndpoint_;
}

// UNANCHORED: source-owned hostname setter used by the current resolver scaffold.
void CLTTCPConnection::SetRemoteHostName(const char* hostName) {
    remoteHostName_ = hostName ? hostName : "";
}

// UNANCHORED: source-owned hostname accessor used by the current resolver scaffold.
const std::string& CLTTCPConnection::RemoteHostName() const {
    return remoteHostName_;
}

// UNANCHORED: source-owned nonblocking socket poll helper used by the launcher bridge scaffolds.
int CLTTCPConnection::PollReceiveNonBlocking() {
    if (socketHandle_ == kInvalidSocketHandle ||
        (state_ != LTTCPEngineConnectionState::kConnectActive &&
         state_ != LTTCPEngineConnectionState::kUdpMonitorActive)) {
        return 0;
    }

    SOCKET socket = static_cast<SOCKET>(socketHandle_);
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socket, &readSet);
    timeval timeout = {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    const int ready = select(0, &readSet, nullptr, nullptr, &timeout);
    if (ready <= 0 || !FD_ISSET(socket, &readSet)) {
        return 0;
    }

    u_long available = 0;
    if (ioctlsocket(socket, FIONREAD, &available) != 0) {
        return 0;
    }

    if (available == 0) {
        char probeByte = 0;
        const int peekResult = recv(socket, &probeByte, 1, MSG_PEEK);
        if (peekResult == 0) {
            spdlog::info(
                "CLTTCPConnection::PollReceiveNonBlocking peer closed socket=0x{:08x} remoteHost='{}'",
                socketHandle_,
                remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_);
            state_ = LTTCPEngineConnectionState::kClosed;
            closesocket(socket);
            socketHandle_ = kInvalidSocketHandle;
            return -1;
        }
        if (peekResult == SOCKET_ERROR) {
            const int wsaError = WSAGetLastError();
            if (wsaError == WSAEWOULDBLOCK) {
                return 0;
            }
            spdlog::warn(
                "CLTTCPConnection::PollReceiveNonBlocking recv(MSG_PEEK) failed socket=0x{:08x} remoteHost='{}' wsaError={} -> closing",
                socketHandle_,
                remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_,
                wsaError);
            state_ = LTTCPEngineConnectionState::kClosed;
            closesocket(socket);
            socketHandle_ = kInvalidSocketHandle;
            return -1;
        }
        available = 1;
    }

    const int toRead = static_cast<int>(std::min<u_long>(available, 4096));
    if (toRead <= 0) {
        return 0;
    }

    const size_t oldSize = receivedBytes_.size();
    receivedBytes_.resize(oldSize + static_cast<size_t>(toRead));
    const int received = recv(
        socket,
        reinterpret_cast<char*>(receivedBytes_.data() + oldSize),
        toRead,
        0);
    if (received <= 0) {
        receivedBytes_.resize(oldSize);
        if (received == 0) {
            spdlog::info(
                "CLTTCPConnection::PollReceiveNonBlocking recv returned EOF socket=0x{:08x} remoteHost='{}'",
                socketHandle_,
                remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_);
            state_ = LTTCPEngineConnectionState::kClosed;
            closesocket(socket);
            socketHandle_ = kInvalidSocketHandle;
            return -1;
        }

        const int wsaError = WSAGetLastError();
        if (wsaError == WSAEWOULDBLOCK) {
            return 0;
        }
        spdlog::warn(
            "CLTTCPConnection::PollReceiveNonBlocking recv failed socket=0x{:08x} remoteHost='{}' wsaError={} -> closing",
            socketHandle_,
            remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_,
            wsaError);
        state_ = LTTCPEngineConnectionState::kClosed;
        closesocket(socket);
        socketHandle_ = kInvalidSocketHandle;
        return -1;
    }

    receivedBytes_.resize(oldSize + static_cast<size_t>(received));
    return received;
}

// anchor: launcher.exe:0x42fe50 TCP receive subpath
int CLTTCPConnection::PollReceiveAndDeliverReadOperationFragmentsScaffold() {
    const int received = PollReceiveNonBlocking();
    if (received <= 0) {
        return received;
    }

    size_t consumedBytes = 0u;
    while (consumedBytes < receivedBytes_.size()) {
        const size_t chunkByteCount = std::min<size_t>(receivedBytes_.size() - consumedBytes, 0x1000u);
        CLTTCPReadOperationFragmentScaffold* readOperationFragment =
            AllocateReadOperationFragmentSourceScaffold();
        if (!readOperationFragment) {
            spdlog::warn(
                "CLTTCPConnection::PollReceiveAndDeliverReadOperationFragmentsScaffold failed fragment allocation this={} bufferedBytes={} remoteHost='{}'",
                fmt::ptr(this),
                static_cast<unsigned>(receivedBytes_.size() - consumedBytes),
                remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_);
            break;
        }

        std::memcpy(
            readOperationFragment->bytes0C,
            receivedBytes_.data() + consumedBytes,
            chunkByteCount);
        ReadOperationFragmentSource_SetByteCountScaffold(
            readOperationFragment,
            static_cast<uint32_t>(chunkByteCount));
        ReadOperationFragment_AddRef(readOperationFragment);
        OnReceive(readOperationFragment);
        ReadOperationFragment_Release(readOperationFragment);
        consumedBytes += chunkByteCount;
    }

    if (consumedBytes != 0u) {
        ConsumeReceivedBytesPrefix(consumedBytes);
    }
    return received;
}

// UNANCHORED: source-owned diagnostic accessor over the buffered receive bytes.
const std::vector<uint8_t>& CLTTCPConnection::ReceivedBytes() const {
    return receivedBytes_;
}

// UNANCHORED: source-owned buffered receive reset helper.
void CLTTCPConnection::ClearReceivedBytes() {
    receivedBytes_.clear();
}

// UNANCHORED: source-owned buffered receive prefix-consumption helper.
void CLTTCPConnection::ConsumeReceivedBytesPrefix(size_t byteCount) {
    if (byteCount == 0u) {
        return;
    }
    if (byteCount >= receivedBytes_.size()) {
        receivedBytes_.clear();
        return;
    }
    receivedBytes_.erase(receivedBytes_.begin(), receivedBytes_.begin() + static_cast<std::ptrdiff_t>(byteCount));
}

// anchor: launcher.exe:0x449ca0
uint32_t CLTTCPConnection::Close(bool graceful) {
    if (state_ == LTTCPEngineConnectionState::kClosed) {
        return 0u;
    }

    return engine_
        ? engine_->CloseConnectionScaffold(this, graceful)
        : CloseSocketTransportScaffold(graceful);
}

// anchor: launcher.exe:0x449cd0
uint32_t CLTTCPConnection::Connect(const LTTCPEndpointKey& endpoint) {
    if (EndpointKeysDiffer(remoteEndpoint_, endpoint)) {
        (void)Close(false);
        remoteEndpoint_ = endpoint;
    }

    return engine_ ? engine_->ConnectConnectionScaffold(this) : 0u;
}

// anchor: launcher.exe:0x449d20
uint32_t CLTTCPConnection::SendBuffer(const void* buffer, uint32_t byteCount, void* completionContext) {
    if (!buffer || byteCount == 0u) {
        return 0u;
    }

    return engine_
        ? engine_->SendBufferConnectionScaffold(this, buffer, byteCount, completionContext)
        : SendRawSocketBufferScaffold(buffer, byteCount, completionContext);
}

// anchor: launcher.exe:0x449fd0
void CLTTCPConnection::OnClose(
    CLTTCPReadOperationFragmentScaffold* readOperationFragment,
    void* /*opaqueArg08*/,
    void* /*opaqueArg0c*/) {
    ReadOperationFragment_Release(readOperationFragment);
}

// anchor: launcher.exe:0x449d40
void CLTTCPConnection::OnReceive(void* readOperationFragment) {
    // Current best static read of `0x449d40`:
    // - the explicit arg is a refcounted `CLTTCPReadOperation`-family buffer fragment
    // - the early fragment `+0x04` call is a no-arg AddRef / retain on that fragment only
    //   - the stack values visible around that call belong to the immediately following parser
    //     call, not to the fragment virtual itself
    // - connection `+0x6c` is a `CVariableLengthPrefixedTCPStreamParser` family object
    //   - current recovered source-side parser prefix now includes:
    //     - `+0x04` retained current-cursor fragment
    //     - `+0x08` next unread buffered byte pointer
    //     - `+0x0c` unread buffered byte count
    //     - `+0x10` provisional advanced-byte-count state
    //     - `+0x14` current parser-owned work item
    // - first parser handoff is `Parse(fragment, &completedPacketWorkItem)`
    // - later drain handoffs are `Parse(nullptr, &completedPacketWorkItem)`
    // - successful emits then hand off exactly `(engine+0x10, completedPacketWorkItem, this, false)`
    //   through `0x436820`; this path does not use queue34 and does not branch on enqueue success
    // - parser-emitted `completedPacketWorkItem` is the same `0x2c` / vtable-`0x4b3e08`
    //   `CParsedPacketWorkItem` family built by `0x435db0 -> 0x435090`
    // - that object family is both:
    //   - the parser-owned assembly state while bytes are still buffered, and
    //   - the completed packet object once `Parse(...)` returns `0`
    // - after each successful emit, `ResetAfterPacket` allocates a fresh replacement work item and
    //   may carry the tail fragment / cursor forward when unread stream bytes remain buffered
    // - the final fragment `+0x08` here releases only the outer OnReceive-held fragment reference
    CLTTCPReadOperationFragmentScaffold* fragment =
        static_cast<CLTTCPReadOperationFragmentScaffold*>(readOperationFragment);
    CLTTCPConnection_ParsedPacketWorkItemScaffold* completedPacketWorkItem = nullptr;

    ReadOperationFragment_AddRef(fragment);
    uint32_t parseResult = ParseReadOperationFragmentScaffold(fragment, &completedPacketWorkItem);
    while (parseResult == 0u) {
        EnqueueCompletedPacketWorkItemScaffold(completedPacketWorkItem);
        completedPacketWorkItem = nullptr;
        parseResult = ParseReadOperationFragmentScaffold(nullptr, &completedPacketWorkItem);
    }

    if (static_cast<int32_t>(parseResult) > 0 && parseResult != 0x7000000u) {
        // Current source scaffolds still do not reconstruct the original `+0x24` endpoint-copy
        // logging helper family used here before close. Keep the control-flow shape faithful first.
        (void)Close(false);
    }

    OnClose(fragment);
}

// UNANCHORED: low-level socket close helper used beneath the anchored Close wrapper.
uint32_t CLTTCPConnection::CloseSocketTransportScaffold(bool /*graceful*/) {
    if (state_ != LTTCPEngineConnectionState::kConnectActive &&
        state_ != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0u;
    }

    state_ = LTTCPEngineConnectionState::kClosing;
    if (socketHandle_ != kInvalidSocketHandle) {
        closesocket(static_cast<SOCKET>(socketHandle_));
    }
    socketHandle_ = kInvalidSocketHandle;
    return 1u;
}

// UNANCHORED: low-level raw-socket send helper used beneath the anchored SendBuffer wrapper.
uint32_t CLTTCPConnection::SendRawSocketBufferScaffold(
    const void* buffer,
    uint32_t byteCount,
    void* /*completionContext*/) {
    if (!buffer || byteCount == 0u) {
        return 0u;
    }

    if (state_ != LTTCPEngineConnectionState::kConnectActive &&
        state_ != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0u;
    }

    if (socketHandle_ == kInvalidSocketHandle) {
        return 0u;
    }

    const int sent = send(
        static_cast<SOCKET>(socketHandle_),
        static_cast<const char*>(buffer),
        static_cast<int>(byteCount),
        0);
    return (sent == static_cast<int>(byteCount)) ? 1u : 0u;
}

// UNANCHORED: source-owned mirror of the connection `+0x6c` parser call shape seen in `0x449d40`.
uint32_t CLTTCPConnection::ParseReadOperationFragmentScaffold(
    CLTTCPReadOperationFragmentScaffold* readOperationFragment,
    CLTTCPConnection_ParsedPacketWorkItemScaffold** outCompletedPacketWorkItem) {
    // Current best static read of `0x449d40` / `0x469bf0`:
    // - connection `+0x6c` is a `CVariableLengthPrefixedTCPStreamParser` family object
    // - original callee is `CVariableLengthPrefixedTCPStreamParser::Parse` at `0x469bf0`
    // - current recovered parser prefix at connection `+0x6c` is:
    //   - `+0x04` retained fragment currently containing parser cursor `+0x08`
    //   - `+0x08` next unread buffered byte pointer
    //   - `+0x0c` total unread buffered byte count across retained fragments
    //   - `+0x10` provisional byte-count accumulator advanced by `0x472660`
    //   - `+0x14` current parser-owned `CParsedPacketWorkItem`
    // - first receive pass reaches it as `Parse(readOperationFragment, &completedPacketWorkItem)`
    //   immediately after a no-arg fragment AddRef in `0x449d40`
    // - later drain passes reach it as `Parse(nullptr, &completedPacketWorkItem)` until the parser
    //   stops yielding complete packets
    // - parser slot `+0x10` / `0x469b40` allocates the current `CParsedPacketWorkItem` object via
    //   `0x435db0 -> 0x435090`, i.e. the same `0x2c` / vtable-`0x4b3e08` work-item family already
    //   seen in queue producer xrefs
    // - that current work item starts as parser-owned assembly state and becomes the emitted
    //   completed packet object when `Parse(...)` returns `0`
    // - current best static read of `0x469bf0` also narrows the emit contract further:
    //   - `Parse(...) == 0` writes `*outCompletedPacketWorkItem = parser+0x14` before
    //     `ResetAfterPacket`
    //   - no evidence currently supports an intentional `Parse(...) == 0` /
    //     `*outCompletedPacketWorkItem == nullptr`
    //     result on this receive path; null work items belong to later lifecycle/shutdown producers
    // - `0x4725c0` / `ResetAfterPacket` then allocates a fresh replacement object and, when unread
    //   bytes remain in the old tail fragment, carries that tail fragment plus the new cursor into
    //   the replacement work item
    // - current source now implements the smallest evidence-backed live subset of that contract:
    //   - fragment retain/append into parser-owned work-item state
    //   - 1-byte / 2-byte prefix decode with original `0`, `0x7000000`, `0x700000b` returns
    //   - active `0x472660`-style cursor advance across retained fragments on the nonzero flag path
    //     used by `0x469bf0`
    //   - queueable parsed-packet work-item emits even when the framed packet body crosses the
    //     retained-fragment boundary inside the current parser work item
    //   - post-emit reset/carry-over now also follows the narrower `0x4725c0` temp-ref ordering
    //     around tail-fragment handoff into the replacement work item
    // - the remaining local gap is now narrower:
    //   - exact temp AddRef/Release ordering inside `0x472660`
    //   - the dormant zero-flag branch shape in that helper, which is not used by current parse xrefs
    if (outCompletedPacketWorkItem) {
        *outCompletedPacketWorkItem = nullptr;
    }

    if (!Parser_EnsureCurrentWorkItemScaffold(&parserScaffold_)) {
        return 1u;
    }

    CLTTCPConnection_ParsedPacketWorkItemScaffold* currentWorkItem =
        parserScaffold_.currentPacketWorkItem14;
    if (readOperationFragment && readOperationFragment->byteCount != 0u) {
        ReadOperationFragment_AddRef(readOperationFragment);
        const bool appended =
            ParsedPacketWorkItem_AppendFragmentScaffold(currentWorkItem, readOperationFragment);
        if (parserScaffold_.currentCursorFragment04 == nullptr) {
            Parser_AssignCurrentCursorFragmentScaffold(&parserScaffold_, readOperationFragment);
        }
        if (parserScaffold_.unreadBufferedByteCount0C == 0u) {
            parserScaffold_.currentCursor08 = ReadOperationFragment_PayloadBegin(readOperationFragment);
        }
        parserScaffold_.unreadBufferedByteCount0C += readOperationFragment->byteCount;
        ReadOperationFragment_Release(readOperationFragment);
        if (!appended) {
            return 1u;
        }
    }

    if (parserScaffold_.unreadBufferedByteCount0C == 0u) {
        return 0x7000000u;
    }
    if (!Parser_NormalizeCursorFragmentScaffold(&parserScaffold_)) {
        spdlog::debug(
            "CLTTCPConnection::ParseReadOperationFragmentScaffold inconsistent buffered cursor this={} unreadBuffered=0x{:08x} assembledByteCount=0x{:08x} retainedFragmentCount={} currentCursor={} currentFragment={}",
            fmt::ptr(this),
            static_cast<unsigned>(parserScaffold_.unreadBufferedByteCount0C),
            currentWorkItem ? static_cast<unsigned>(currentWorkItem->assembledByteCount28) : 0u,
            currentWorkItem ? static_cast<unsigned>(currentWorkItem->retainedFragmentCount0C) : 0u,
            fmt::ptr(parserScaffold_.currentCursor08),
            fmt::ptr(parserScaffold_.currentCursorFragment04));
        return 0x7000000u;
    }

    uint32_t packetBodyByteCount = currentWorkItem->assembledByteCount28;
    if (packetBodyByteCount == 0u) {
        if (parserScaffold_.unreadBufferedByteCount0C < 2u) {
            return 0x7000000u;
        }

        uint32_t prefixByteCount = 1u;
        uint8_t firstPrefixByte = 0u;
        uint8_t secondPrefixByte = 0u;
        if (!Parser_PeekBufferedByteAtOffsetScaffold(&parserScaffold_, 0u, &firstPrefixByte)) {
            return 0x7000000u;
        }

        packetBodyByteCount = static_cast<uint32_t>(firstPrefixByte);
        if ((firstPrefixByte & 0x80u) != 0u) {
            if (!Parser_PeekBufferedByteAtOffsetScaffold(&parserScaffold_, 1u, &secondPrefixByte)) {
                return 0x7000000u;
            }

            packetBodyByteCount =
                (static_cast<uint32_t>(firstPrefixByte & 0x7fu) << 8u) |
                static_cast<uint32_t>(secondPrefixByte);
            if (packetBodyByteCount < 0x80u) {
                return 0x700000bu;
            }
            prefixByteCount = 2u;
        }

        if (packetBodyByteCount > 0x1000u) {
            return 0x700000bu;
        }

        currentWorkItem->assembledByteCount28 = packetBodyByteCount;
        if (!Parser_AdvanceBufferedCursorScaffold(&parserScaffold_, prefixByteCount)) {
            spdlog::debug(
                "CLTTCPConnection::ParseReadOperationFragmentScaffold cursor advance failed during prefix consume this={} unreadBuffered=0x{:08x} packetBodyByteCount=0x{:08x} retainedFragmentCount={}",
                fmt::ptr(this),
                static_cast<unsigned>(parserScaffold_.unreadBufferedByteCount0C),
                static_cast<unsigned>(packetBodyByteCount),
                static_cast<unsigned>(currentWorkItem->retainedFragmentCount0C));
            return 0x7000000u;
        }
        if (parserScaffold_.currentCursorFragment04 &&
            !ParsedPacketWorkItem_RebaseFirstFragmentToScaffold(
                currentWorkItem,
                parserScaffold_.currentCursorFragment04)) {
            spdlog::debug(
                "CLTTCPConnection::ParseReadOperationFragmentScaffold failed to rebase current work-item first fragment after prefix consume this={} retainedFragmentCount={} currentCursorFragment={} currentCursor={}",
                fmt::ptr(this),
                static_cast<unsigned>(currentWorkItem->retainedFragmentCount0C),
                fmt::ptr(parserScaffold_.currentCursorFragment04),
                fmt::ptr(parserScaffold_.currentCursor08));
            return 1u;
        }
    }

    if (packetBodyByteCount <= parserScaffold_.unreadBufferedByteCount0C) {
        if (!Parser_NormalizeCursorFragmentScaffold(&parserScaffold_)) {
            return 0x7000000u;
        }

        currentWorkItem->currentCursor24 = parserScaffold_.currentCursor08;
        if (!Parser_AdvanceBufferedCursorScaffold(&parserScaffold_, packetBodyByteCount)) {
            spdlog::debug(
                "CLTTCPConnection::ParseReadOperationFragmentScaffold cursor advance failed during packet-body consume this={} unreadBuffered=0x{:08x} packetBodyByteCount=0x{:08x} retainedFragmentCount={}",
                fmt::ptr(this),
                static_cast<unsigned>(parserScaffold_.unreadBufferedByteCount0C),
                static_cast<unsigned>(packetBodyByteCount),
                static_cast<unsigned>(currentWorkItem->retainedFragmentCount0C));
            return 0x7000000u;
        }
        if (outCompletedPacketWorkItem) {
            *outCompletedPacketWorkItem = currentWorkItem;
        }
        Parser_ResetAfterPacketScaffold(&parserScaffold_);
        return 0u;
    }

    return 0x7000000u;
}

// UNANCHORED: source-owned mirror of the exact `0x449d8a` enqueue handoff.
void CLTTCPConnection::EnqueueCompletedPacketWorkItemScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    // Current best static read of `0x449d40` / `0x469bf0`:
    // - the queue handoff is exactly `(engine+0x10, completedPacketWorkItem, this, false)`
    // - this receive path therefore always targets queue0C through `0x436820`
    // - original caller-side lifetime does not depend on enqueue success because `0x436820`
    //   returns `void`; once we reach this seam the completed parsed-packet work item is
    //   queue-owned / consumer-owned rather than connection-owned
    if (!Engine()) {
        return;
    }

    Engine()->EnqueueCompletedOperationFromConnectionScaffold(
        workItem,
        static_cast<CLTTCPConnection*>(this),
        "CLTTCPConnection::OnReceive");
}

}  // namespace mxo::liblttcp
