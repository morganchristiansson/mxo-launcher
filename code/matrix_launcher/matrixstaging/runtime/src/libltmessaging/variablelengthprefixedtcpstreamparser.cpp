// Recovered source-file anchors:
// - `\matrixstaging\runtime\src\libltmessaging\variablelengthprefixedtcpstreamparser.cpp`
// - `\matrixstaging\runtime\src\libltmessaging\messageconnection.cpp`
//
// Current split after the fragment-parser cleanup:
// - auth-side framing helpers stay here under `mxo::auth`
// - the receive-side `CVariableLengthPrefixedTCPStreamParser` / `CParsedPacketWorkItem` runtime
//   mirrors now also live here under `mxo::liblttcp`
// - `CLTTCPConnection::OnReceive` stays connection-focused and delegates parser-owned state
//   handling into this file instead of keeping that scaffold buried in `lttcpconnection.cpp`

#include "variablelengthprefixedtcpstreamparser.h"

#include "../libltcrypto/auth_crypto.h"
#include "../liblttcp/lttcpconnection.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "spdlog/spdlog.h"

namespace mxo::auth {

bool BuildVariableLengthPacket(
    const uint8_t* payload,
    size_t payloadSize,
    FrameMode mode,
    std::vector<uint8_t>* outPacketBytes,
    size_t* outHeaderByteCount) {
    if (!payload || !outPacketBytes) {
        return false;
    }

    size_t headerByteCount = 0u;
    if (mode == kFrameModeForceOneByte) {
        if (payloadSize > 0x7fu) {
            return false;
        }
        headerByteCount = 1u;
    } else {
        if (payloadSize > 0x7fffu) {
            return false;
        }
        headerByteCount = (mode == kFrameModeForceTwoByte || payloadSize >= 0x80u) ? 2u : 1u;
    }

    outPacketBytes->clear();
    outPacketBytes->reserve(headerByteCount + payloadSize);
    if (headerByteCount == 2u) {
        outPacketBytes->push_back(static_cast<uint8_t>(0x80u | ((payloadSize >> 8u) & 0x7fu)));
        outPacketBytes->push_back(static_cast<uint8_t>(payloadSize & 0xffu));
    } else {
        outPacketBytes->push_back(static_cast<uint8_t>(payloadSize));
    }
    outPacketBytes->insert(outPacketBytes->end(), payload, payload + payloadSize);
    if (outHeaderByteCount) {
        *outHeaderByteCount = headerByteCount;
    }
    return true;
}

}  // namespace mxo::auth

namespace mxo::liblttcp {

// anchor: launcher.exe:0x434fa0
CLTTCPReadOperation* CLTTCPReadOperationRefHandle_AssignRetained(
    CLTTCPReadOperationRefHandle* handle,
    CLTTCPReadOperation* const* newRetainedFragmentSlot) {
    if (!handle) {
        return nullptr;
    }

    CLTTCPReadOperation* const newRetainedFragment =
        newRetainedFragmentSlot ? *newRetainedFragmentSlot : nullptr;
    CLTTCPReadOperation* const oldRetainedFragment = handle->retainedFragment00;
    if (oldRetainedFragment != newRetainedFragment) {
        if (oldRetainedFragment) {
            oldRetainedFragment->Release();
        }
        handle->retainedFragment00 = newRetainedFragment;
        if (newRetainedFragment) {
            newRetainedFragment->AddRef();
        }
    }
    return handle->retainedFragment00;
}

namespace {

static void* g_ParsedPacketWorkItemVtable[2] = {nullptr, nullptr};

static void EnsureParsedPacketWorkItemVtableInitialized() {
    if (!g_ParsedPacketWorkItemVtable[1]) {
        g_ParsedPacketWorkItemVtable[1] =
            reinterpret_cast<void*>(CParsedPacketWorkItem_ReleaseScaffold);
    }
}

static void CParsedPacketWorkItem_ClearFragmentListScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem) {
    if (!workItem || !workItem->retainedFragmentListOwner14 ||
        !workItem->retainedFragmentListOwner14->sentinel) {
        return;
    }

    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* sentinel =
        workItem->retainedFragmentListOwner14->sentinel;
    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* node = sentinel->next;
    while (node && node != sentinel) {
        CParsedPacketWorkItem_RetainedFragmentNodeScaffold* next = node->next;
        if (node->retainedFragment08) { node->retainedFragment08->Release(); }
        std::free(node);
        node = next;
    }

    std::free(sentinel);
    std::free(workItem->retainedFragmentListOwner14);
    workItem->retainedFragmentListOwner14 = nullptr;
}

static void CParsedPacketWorkItem_ResetFragmentStateScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem) {
    if (!workItem) {
        return;
    }

    if (workItem->firstRetainedFragment10) { workItem->firstRetainedFragment10->Release(); }
    workItem->firstRetainedFragment10 = nullptr;
    CParsedPacketWorkItem_ClearFragmentListScaffold(workItem);
    workItem->retainedFragmentCount0C = 0u;
    workItem->directFragmentTraversalPhase18 = 1u;
    workItem->fragmentTraversalIndex1C = 0u;
    workItem->fragmentTraversalNode20 = nullptr;
    workItem->currentCursor24 = nullptr;
    workItem->assembledByteCount28 = 0u;
}

static CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* CParsedPacketWorkItem_AllocateScaffold() {
    EnsureParsedPacketWorkItemVtableInitialized();
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem =
        static_cast<CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08*>(
            std::calloc(1, sizeof(CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08)));
    if (!workItem) {
        return nullptr;
    }

    workItem->vtable = g_ParsedPacketWorkItemVtable;
    workItem->workType = 3u;
    workItem->directFragmentTraversalPhase18 = 1u;
    return workItem;
}

// UNANCHORED: source-owned list-owner allocator for additional retained fragments.
static bool CParsedPacketWorkItem_EnsureAdditionalFragmentListScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem) {
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

// anchor: launcher.exe:0x435e60
// Current source now restores the helper's exact trailing lifetime effect from static RE:
// - `if (param_1 != 0) param_1->Release();`
// So callers may hand this helper one temporary retained fragment ref and let the helper consume
// that temporary on return.
//
// Remaining source-owned differences are narrower than before:
// - the broader end-to-end worker/OnReceive/Parse/queue-consumer reconciliation is still being
//   tightened
// - this helper still keeps source-owned bool failure reporting and current list/count scaffolding
//   instead of claiming byte-for-byte structural parity yet
static bool CParsedPacketWorkItem_AppendFragmentScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem,
    CLTTCPReadOperation* fragment) {
    if (!workItem || !fragment) {
        return false;
    }

    bool appended = false;
    if (workItem->retainedFragmentCount0C == 0u) {
        workItem->retainedFragmentCount0C = 1u;
        if (workItem->firstRetainedFragment10 != fragment) {
            if (workItem->firstRetainedFragment10) { workItem->firstRetainedFragment10->Release(); }
            workItem->firstRetainedFragment10 = fragment;
            if (fragment) { fragment->AddRef(); }
        }
        appended = true;
    } else {
        if (!CParsedPacketWorkItem_EnsureAdditionalFragmentListScaffold(workItem)) {
            if (fragment) { fragment->Release(); }
            return false;
        }

        CParsedPacketWorkItem_RetainedFragmentNodeScaffold* node =
            static_cast<CParsedPacketWorkItem_RetainedFragmentNodeScaffold*>(
                std::calloc(1, sizeof(CParsedPacketWorkItem_RetainedFragmentNodeScaffold)));
        if (!node) {
            if (fragment) { fragment->Release(); }
            return false;
        }

        node->retainedFragment08 = fragment;
        if (fragment) { fragment->AddRef(); }
        CParsedPacketWorkItem_RetainedFragmentNodeScaffold* sentinel =
            workItem->retainedFragmentListOwner14->sentinel;
        node->next = sentinel;
        node->prev = sentinel->prev;
        sentinel->prev->next = node;
        sentinel->prev = node;
        ++workItem->retainedFragmentCount0C;
        appended = true;
    }

    if (fragment) { fragment->Release(); }
    return appended;
}

// UNANCHORED: source-owned work-item rebase helper used after prefix consume whenever the parser
// cursor has advanced into a later retained fragment.
static bool CParsedPacketWorkItem_RebaseFirstFragmentToScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem,
    CLTTCPReadOperation* firstFragmentToKeep) {
    if (!workItem || !firstFragmentToKeep || workItem->retainedFragmentCount0C == 0u ||
        workItem->firstRetainedFragment10 == firstFragmentToKeep) {
        return true;
    }

    std::vector<CLTTCPReadOperation*> retainedFragmentsToKeep;
    CLTTCPReadOperationRefHandle fragmentRef{};
    CLTTCPReadOperation* fragment =
        CParsedPacketWorkItem_BeginFragmentTraversal(workItem, &fragmentRef)->retainedFragment00;
    bool keepFragment = false;
    while (fragment) {
        if (fragment == firstFragmentToKeep) {
            keepFragment = true;
        }
        if (keepFragment) {
            fragment->AddRef();
            retainedFragmentsToKeep.push_back(fragment);
        }

        CLTTCPReadOperationRefHandle nextFragmentRef{};
        CLTTCPReadOperation* nextFragment =
            CParsedPacketWorkItem_GetNextFragment(workItem, &nextFragmentRef)->retainedFragment00;
        fragment->Release();
        fragment = nextFragment;
    }

    if (retainedFragmentsToKeep.empty()) {
        return false;
    }

    CParsedPacketWorkItem_ResetFragmentStateScaffold(workItem);
    for (size_t index = 0; index < retainedFragmentsToKeep.size(); ++index) {
        CLTTCPReadOperation* retainedFragmentTempRef = retainedFragmentsToKeep[index];
        if (!CParsedPacketWorkItem_AppendFragmentScaffold(workItem, retainedFragmentTempRef)) {
            for (size_t remaining = index + 1; remaining < retainedFragmentsToKeep.size(); ++remaining) {
                if (retainedFragmentsToKeep[remaining]) {
                    retainedFragmentsToKeep[remaining]->Release();
                }
            }
            return false;
        }
    }
    return true;
}

static bool CVariableLengthPrefixedTCPStreamParser_NormalizeCursorFragmentScaffold(
    CVariableLengthPrefixedTCPStreamParser* parser) {
    if (!parser) {
        return false;
    }
    if (parser->unreadBufferedByteCount0C == 0u) {
        return true;
    }
    if (!parser->currentPacketWorkItem14 ||
        !parser->currentCursorFragmentRef04.retainedFragment00 ||
        !parser->currentCursor08) {
        return false;
    }

    const uint8_t* const currentFragmentBegin =
        reinterpret_cast<const uint8_t*>(parser->currentCursorFragmentRef04.retainedFragment00 + 1);
    const uint8_t* const currentFragmentEnd =
        currentFragmentBegin + parser->currentCursorFragmentRef04.retainedFragment00->byteCount08;
    if (parser->currentCursor08 < currentFragmentBegin ||
        parser->currentCursor08 > currentFragmentEnd) {
        return false;
    }
    const uint32_t remainingInCurrentFragment =
        static_cast<uint32_t>(currentFragmentEnd - parser->currentCursor08);
    if (remainingInCurrentFragment != 0u) {
        return true;
    }

    CLTTCPReadOperationRefHandle fragmentRef{};
    CLTTCPReadOperation* fragment =
        CParsedPacketWorkItem_BeginFragmentTraversal(
            parser->currentPacketWorkItem14,
            &fragmentRef)->retainedFragment00;
    while (fragment) {
        CLTTCPReadOperationRefHandle nextFragmentRef{};
        CLTTCPReadOperation* nextFragment =
            CParsedPacketWorkItem_GetNextFragment(
                parser->currentPacketWorkItem14,
                &nextFragmentRef)->retainedFragment00;
        if (fragment == parser->currentCursorFragmentRef04.retainedFragment00) {
            fragment->Release();
            if (!nextFragment) {
                return false;
            }
            (void)CLTTCPReadOperationRefHandle_AssignRetained(
                &parser->currentCursorFragmentRef04,
                &nextFragment);
            parser->currentCursor08 =
                reinterpret_cast<uint8_t*>(parser->currentCursorFragmentRef04.retainedFragment00 + 1);
            nextFragment->Release();
            return true;
        }

        fragment->Release();
        fragment = nextFragment;
    }
    return false;
}

// anchor: launcher.exe:0x472660
// Current source mirrors the active launcher.exe callers from `0x469bf0`, which all pass
// low-byte-nonzero `cursorAdvanceFlags == 1`.
//
// The helper body itself also contains a low-byte-zero branch:
// - `test cl, cl` / `setz dl` feeds the exact-boundary math
// - but `ghidra_get_xrefs_to(0x472660)` currently shows only the three `0x469bf0` callers, and
//   each of those pushes `1`
// So the zero-flag shape below stays only a conservative structural translation of the recovered
// arithmetic/traversal split, not a separately proven launcher.exe-active behavior.
static bool CVariableLengthPrefixedTCPStreamParser_AdvanceBufferedCursorScaffold(
    CVariableLengthPrefixedTCPStreamParser* parser,
    uint32_t byteCountToConsume,
    uint32_t cursorAdvanceFlags) {
    if (!parser || byteCountToConsume == 0u) {
        return true;
    }
    if (!parser->currentPacketWorkItem14 ||
        !parser->currentCursorFragmentRef04.retainedFragment00 ||
        !parser->currentCursor08 ||
        parser->unreadBufferedByteCount0C < byteCountToConsume) {
        return false;
    }

    const uint32_t boundaryBias = ((cursorAdvanceFlags & 0xffu) == 0u) ? 1u : 0u;
    const uint8_t* const currentFragmentBegin =
        reinterpret_cast<const uint8_t*>(parser->currentCursorFragmentRef04.retainedFragment00 + 1);
    const uint8_t* const currentFragmentEnd =
        currentFragmentBegin + parser->currentCursorFragmentRef04.retainedFragment00->byteCount08;
    if (parser->currentCursor08 < currentFragmentBegin ||
        parser->currentCursor08 > currentFragmentEnd) {
        return false;
    }
    const uint32_t bytesRemainingInCurrentFragment =
        static_cast<uint32_t>(currentFragmentEnd - parser->currentCursor08);
    if (bytesRemainingInCurrentFragment < boundaryBias) {
        return false;
    }

    const uint32_t maxConsumeFromCurrentFragment = bytesRemainingInCurrentFragment - boundaryBias;
    const uint32_t consumeFromCurrentFragment =
        std::min<uint32_t>(byteCountToConsume, maxConsumeFromCurrentFragment);
    uint32_t remainingToConsume = byteCountToConsume - consumeFromCurrentFragment;
    uint8_t* newCursorBase = nullptr;
    uint32_t cursorByteOffsetWithinFragment = 0u;

    if (remainingToConsume == 0u) {
        if (((cursorAdvanceFlags & 0xffu) == 0u) ||
            consumeFromCurrentFragment != maxConsumeFromCurrentFragment ||
            parser->unreadBufferedByteCount0C <= byteCountToConsume) {
            newCursorBase = parser->currentCursor08;
            cursorByteOffsetWithinFragment = byteCountToConsume;
        } else {
            CLTTCPReadOperationRefHandle traversedFragmentRef{};
            CLTTCPReadOperation* traversedFragment =
                CParsedPacketWorkItem_BeginFragmentTraversal(
                    parser->currentPacketWorkItem14,
                    &traversedFragmentRef)->retainedFragment00;
            while (traversedFragment) {
                CLTTCPReadOperationRefHandle nextFragmentRef{};
                CLTTCPReadOperation* nextFragment =
                    CParsedPacketWorkItem_GetNextFragment(
                        parser->currentPacketWorkItem14,
                        &nextFragmentRef)->retainedFragment00;
                if (traversedFragment == parser->currentCursorFragmentRef04.retainedFragment00) {
                    traversedFragment->Release();
                    if (!nextFragment) {
                        return false;
                    }
                    (void)CLTTCPReadOperationRefHandle_AssignRetained(
                        &parser->currentCursorFragmentRef04,
                        &nextFragment);
                    newCursorBase =
                        reinterpret_cast<uint8_t*>(parser->currentCursorFragmentRef04.retainedFragment00 + 1);
                    cursorByteOffsetWithinFragment = 0u;
                    nextFragment->Release();
                    break;
                }
                traversedFragment->Release();
                traversedFragment = nextFragment;
            }
            if (!newCursorBase) {
                return false;
            }
        }
    } else {
        CLTTCPReadOperationRefHandle traversedFragmentRef{};
        CLTTCPReadOperation* traversedFragment =
            CParsedPacketWorkItem_BeginFragmentTraversal(
                parser->currentPacketWorkItem14,
                &traversedFragmentRef)->retainedFragment00;
        bool passedCurrentCursorFragment = false;
        while (traversedFragment) {
            if (!passedCurrentCursorFragment) {
                if (traversedFragment == parser->currentCursorFragmentRef04.retainedFragment00) {
                    passedCurrentCursorFragment = true;
                }
            } else {
                if (remainingToConsume <= traversedFragment->byteCount08) {
                    if (traversedFragment != parser->currentCursorFragmentRef04.retainedFragment00) {
                        (void)CLTTCPReadOperationRefHandle_AssignRetained(
                            &parser->currentCursorFragmentRef04,
                            &traversedFragment);
                    }
                    newCursorBase =
                        reinterpret_cast<uint8_t*>(parser->currentCursorFragmentRef04.retainedFragment00 + 1);
                    cursorByteOffsetWithinFragment = remainingToConsume - boundaryBias;
                    traversedFragment->Release();
                    traversedFragment = nullptr;
                    break;
                }
                remainingToConsume -= traversedFragment->byteCount08;
            }

            CLTTCPReadOperationRefHandle nextFragmentRef{};
            CLTTCPReadOperation* nextFragment =
                CParsedPacketWorkItem_GetNextFragment(
                    parser->currentPacketWorkItem14,
                    &nextFragmentRef)->retainedFragment00;
            traversedFragment->Release();
            traversedFragment = nextFragment;
        }
        if (traversedFragment) {
            traversedFragment->Release();
        }
        if (!newCursorBase) {
            return false;
        }
    }

    parser->unreadBufferedByteCount0C -= byteCountToConsume;
    parser->advancedBufferedByteCount10 += byteCountToConsume;
    if (parser->unreadBufferedByteCount0C == 0u) {
        parser->currentCursor08 = nullptr;
        return true;
    }

    parser->currentCursor08 = newCursorBase + cursorByteOffsetWithinFragment;
    return true;
}

static CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08*
CVariableLengthPrefixedTCPStreamParser_AllocatePacketBufferScaffold() {
    return CParsedPacketWorkItem_AllocateScaffold();
}

}  // namespace

uint32_t __thiscall CParsedPacketWorkItem_ReleaseScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* self) {
    if (!self) {
        return 1u;
    }

    CParsedPacketWorkItem_ResetFragmentStateScaffold(self);
    std::free(self);
    return 1u;
}

// anchor: launcher.exe:0x4350c0
CLTTCPReadOperationRefHandle* CParsedPacketWorkItem_BeginFragmentTraversal(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem,
    CLTTCPReadOperationRefHandle* outFragmentRef) {
    if (!outFragmentRef) {
        return nullptr;
    }
    outFragmentRef->retainedFragment00 = nullptr;
    if (!workItem || workItem->retainedFragmentCount0C == 0u) {
        return outFragmentRef;
    }

    workItem->directFragmentTraversalPhase18 = 1u;
    workItem->fragmentTraversalIndex1C = 0u;
    workItem->fragmentTraversalNode20 = nullptr;
    CLTTCPReadOperation* fragment = workItem->firstRetainedFragment10;
    outFragmentRef->retainedFragment00 = fragment;
    if (fragment) {
        fragment->AddRef();
    }
    return outFragmentRef;
}

// anchor: launcher.exe:0x435510
CLTTCPReadOperationRefHandle* CParsedPacketWorkItem_GetNextFragment(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem,
    CLTTCPReadOperationRefHandle* outFragmentRef) {
    if (!outFragmentRef) {
        return nullptr;
    }
    outFragmentRef->retainedFragment00 = nullptr;
    if (!workItem) {
        return outFragmentRef;
    }

    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* traversalNode = nullptr;
    if (workItem->directFragmentTraversalPhase18 == 0u) {
        traversalNode = workItem->fragmentTraversalNode20;
    } else {
        const uint32_t traversalIndex = workItem->fragmentTraversalIndex1C + 1u;
        workItem->fragmentTraversalIndex1C = traversalIndex;
        if (traversalIndex == 0u) {
            CLTTCPReadOperation* fragment = workItem->firstRetainedFragment10;
            outFragmentRef->retainedFragment00 = fragment;
            if (fragment) {
                fragment->AddRef();
            }
            return outFragmentRef;
        }
        if (!workItem->retainedFragmentListOwner14 || !workItem->retainedFragmentListOwner14->sentinel) {
            return outFragmentRef;
        }
        workItem->directFragmentTraversalPhase18 = 0u;
        traversalNode = workItem->retainedFragmentListOwner14->sentinel;
    }

    if (!traversalNode || !workItem->retainedFragmentListOwner14 ||
        !workItem->retainedFragmentListOwner14->sentinel) {
        return outFragmentRef;
    }

    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* nextNode = traversalNode->next;
    workItem->fragmentTraversalNode20 = nextNode;
    if (nextNode == workItem->retainedFragmentListOwner14->sentinel) {
        return outFragmentRef;
    }

    CLTTCPReadOperation* fragment = nextNode->retainedFragment08;
    outFragmentRef->retainedFragment00 = fragment;
    if (fragment) {
        fragment->AddRef();
    }
    return outFragmentRef;
}

// anchor: launcher.exe:0x4355c0
CLTTCPReadOperationRefHandle* CParsedPacketWorkItem_GetTailFragment(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem,
    CLTTCPReadOperationRefHandle* outFragmentRef) {
    if (!outFragmentRef) {
        return nullptr;
    }
    outFragmentRef->retainedFragment00 = nullptr;
    if (!workItem || workItem->retainedFragmentCount0C == 0u) {
        return outFragmentRef;
    }

    CLTTCPReadOperation* tailFragment = nullptr;
    if (!workItem->retainedFragmentListOwner14 || !workItem->retainedFragmentListOwner14->sentinel) {
        tailFragment = workItem->firstRetainedFragment10;
    } else {
        CParsedPacketWorkItem_RetainedFragmentNodeScaffold* tailNode =
            workItem->retainedFragmentListOwner14->sentinel->prev;
        tailFragment =
            (!tailNode || tailNode == workItem->retainedFragmentListOwner14->sentinel)
            ? workItem->firstRetainedFragment10
            : tailNode->retainedFragment08;
    }

    outFragmentRef->retainedFragment00 = tailFragment;
    if (tailFragment) {
        tailFragment->AddRef();
    }
    return outFragmentRef;
}

// anchor: launcher.exe:0x434f80
uint8_t* CParsedPacketWorkItem_GetCurrentCursor(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem) {
    return workItem ? workItem->currentCursor24 : nullptr;
}

// anchor: launcher.exe:0x434f60
uint32_t CParsedPacketWorkItem_GetAssembledByteCount(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem) {
    return workItem ? workItem->assembledByteCount28 : 0u;
}

// anchor: launcher.exe:0x469b20
CVariableLengthPrefixedTCPStreamParser::CVariableLengthPrefixedTCPStreamParser() {
    InitState();
    ResetAfterPacket();
}

// anchor: launcher.exe:0x469ef0
CVariableLengthPrefixedTCPStreamParser::~CVariableLengthPrefixedTCPStreamParser() {
    if (currentPacketWorkItem14) {
        CParsedPacketWorkItem_ReleaseScaffold(currentPacketWorkItem14);
        currentPacketWorkItem14 = nullptr;
    }
    ClearState();
}

// anchor: launcher.exe:0x4415c0 / vtable 0x004baf8c
void CVariableLengthPrefixedTCPStreamParser::OnCompletedPacketScaffold() {
    // Exact original helper role is still unsettled in the current receive-focused pass.
}

// anchor: launcher.exe:0x469b40 / vtable 0x004baf94
CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08*
CVariableLengthPrefixedTCPStreamParser::AllocatePacketBuffer() {
    return CVariableLengthPrefixedTCPStreamParser_AllocatePacketBufferScaffold();
}

// anchor: launcher.exe:0x472560
void CVariableLengthPrefixedTCPStreamParser::InitState() {
    // Important exact write set from `0x472560`:
    // - `+0x04 = 0`
    // - `+0x08 = 0`
    // - `+0x0c = 0`
    // - `+0x14 = 0`
    // The helper does not explicitly touch `+0x10`; ctor `0x469b20` reaches
    // `ResetAfterPacket` immediately afterward, which is where the active path clears `+0x10`.
    currentCursorFragmentRef04.retainedFragment00 = nullptr;
    currentCursor08 = nullptr;
    unreadBufferedByteCount0C = 0u;
    currentPacketWorkItem14 = nullptr;
}

// anchor: launcher.exe:0x472580
void CVariableLengthPrefixedTCPStreamParser::ClearState() {
    if (currentPacketWorkItem14) {
        CParsedPacketWorkItem_ReleaseScaffold(currentPacketWorkItem14);
        currentPacketWorkItem14 = nullptr;
    }
    CLTTCPReadOperation* clearedCursorFragment = nullptr;
    (void)CLTTCPReadOperationRefHandle_AssignRetained(
        &currentCursorFragmentRef04,
        &clearedCursorFragment);
}

// anchor: launcher.exe:0x4725c0 / vtable 0x004baf90
void CVariableLengthPrefixedTCPStreamParser::ResetAfterPacket() {
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* emittedWorkItem = currentPacketWorkItem14;
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* replacementWorkItem = AllocatePacketBuffer();
    currentPacketWorkItem14 = replacementWorkItem;
    CLTTCPReadOperation* clearedCursorFragment = nullptr;
    (void)CLTTCPReadOperationRefHandle_AssignRetained(
        &currentCursorFragmentRef04,
        &clearedCursorFragment);
    advancedBufferedByteCount10 = 0u;

    if (unreadBufferedByteCount0C == 0u || !replacementWorkItem || !emittedWorkItem) {
        return;
    }

    CLTTCPReadOperationRefHandle tailFragmentTempRef{};
    CLTTCPReadOperation* const tailFragment =
        CParsedPacketWorkItem_GetTailFragment(
            emittedWorkItem,
            &tailFragmentTempRef)->retainedFragment00;
    if (tailFragment != currentCursorFragmentRef04.retainedFragment00) {
        (void)CLTTCPReadOperationRefHandle_AssignRetained(
            &currentCursorFragmentRef04,
            &tailFragment);
    }
    if (tailFragment) {
        tailFragment->Release();
    }
    CLTTCPReadOperation* parserCurrentCursorFragment = currentCursorFragmentRef04.retainedFragment00;
    if (parserCurrentCursorFragment) {
        // `0x4725c0` takes one transient ref on parser `+0x04` immediately before `0x435e60`.
        // The helper now mirrors the original trailing `Release(param_1)` again, so this extra
        // carry-over temp is consumed inside AppendFragment rather than by a source-only caller
        // cleanup step.
        if (parserCurrentCursorFragment) { parserCurrentCursorFragment->AddRef(); }
        const bool appended = CParsedPacketWorkItem_AppendFragmentScaffold(
            replacementWorkItem,
            parserCurrentCursorFragment);
        if (!appended) {
            return;
        }
    }

    replacementWorkItem->currentCursor24 = currentCursor08;
}

// anchor: launcher.exe:0x469bf0 / vtable 0x004baf88
uint32_t CVariableLengthPrefixedTCPStreamParser::Parse(
    CLTTCPReadOperation* readOperationFragment,
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08** outCompletedPacketWorkItem) {
    if (outCompletedPacketWorkItem) {
        *outCompletedPacketWorkItem = nullptr;
    }
    if (!currentPacketWorkItem14) {
        currentPacketWorkItem14 = AllocatePacketBuffer();
        if (!currentPacketWorkItem14) {
            if (readOperationFragment) {
                if (readOperationFragment) { readOperationFragment->Release(); }
            }
            return 1u;
        }
    }

    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* currentWorkItem = currentPacketWorkItem14;
    CLTTCPReadOperation* inputFragment = readOperationFragment;
    if (readOperationFragment && readOperationFragment->byteCount08 != 0u) {
        // `0x469bf0` takes a transient fragment ref immediately before `0x435e60`, and the helper
        // now mirrors the original trailing `Release(param_1)` again. That keeps the Parse-side
        // handoff narrower and closer to the original worker->OnReceive->Parse ownership chain.
        if (readOperationFragment) { readOperationFragment->AddRef(); }
        const bool appended =
            CParsedPacketWorkItem_AppendFragmentScaffold(currentWorkItem, readOperationFragment);
        if (currentCursorFragmentRef04.retainedFragment00 == nullptr) {
            (void)CLTTCPReadOperationRefHandle_AssignRetained(
                &currentCursorFragmentRef04,
                &readOperationFragment);
        }
        if (unreadBufferedByteCount0C == 0u) {
            currentCursor08 =
                reinterpret_cast<uint8_t*>(readOperationFragment + 1);
        }
        unreadBufferedByteCount0C += readOperationFragment->byteCount08;
        if (!appended) {
            if (inputFragment) {
                if (inputFragment) { inputFragment->Release(); }
            }
            return 1u;
        }
    }

    if (unreadBufferedByteCount0C != 0u) {
        uint32_t packetBodyByteCount =
            CParsedPacketWorkItem_GetAssembledByteCount(currentWorkItem);
        if (packetBodyByteCount == 0u) {
            if (unreadBufferedByteCount0C < 2u) {
                if (inputFragment) {
                    if (inputFragment) { inputFragment->Release(); }
                }
                return 0x7000000u;
            }
            if (!CVariableLengthPrefixedTCPStreamParser_NormalizeCursorFragmentScaffold(this)) {
                spdlog::debug(
                    "CVariableLengthPrefixedTCPStreamParser::Parse inconsistent buffered cursor parser={} unreadBuffered=0x{:08x} retainedFragmentCount={} currentCursor={} currentFragment={}",
                    fmt::ptr(this),
                    static_cast<unsigned>(unreadBufferedByteCount0C),
                    currentWorkItem ? static_cast<unsigned>(currentWorkItem->retainedFragmentCount0C) : 0u,
                    fmt::ptr(currentCursor08),
                    fmt::ptr(currentCursorFragmentRef04.retainedFragment00));
                if (inputFragment) {
                    if (inputFragment) { inputFragment->Release(); }
                }
                return 0x7000000u;
            }

            const uint8_t firstPrefixByte = *currentCursor08;
            packetBodyByteCount = static_cast<uint32_t>(firstPrefixByte);
            if ((firstPrefixByte & 0x80u) != 0u) {
                if (!CVariableLengthPrefixedTCPStreamParser_AdvanceBufferedCursorScaffold(this, 1u, 1u) ||
                    !CVariableLengthPrefixedTCPStreamParser_NormalizeCursorFragmentScaffold(this)) {
                    if (inputFragment) {
                        if (inputFragment) { inputFragment->Release(); }
                    }
                    return 0x7000000u;
                }

                packetBodyByteCount =
                    (static_cast<uint32_t>(firstPrefixByte & 0x7fu) << 8u) |
                    static_cast<uint32_t>(*currentCursor08);
                if (packetBodyByteCount < 0x80u) {
                    if (inputFragment) {
                        if (inputFragment) { inputFragment->Release(); }
                    }
                    return 0x700000bu;
                }
            }

            if (!CVariableLengthPrefixedTCPStreamParser_AdvanceBufferedCursorScaffold(this, 1u, 1u)) {
                spdlog::debug(
                    "CVariableLengthPrefixedTCPStreamParser::Parse cursor advance failed during prefix consume parser={} unreadBuffered=0x{:08x} packetBodyByteCount=0x{:08x} retainedFragmentCount={}",
                    fmt::ptr(this),
                    static_cast<unsigned>(unreadBufferedByteCount0C),
                    static_cast<unsigned>(packetBodyByteCount),
                    static_cast<unsigned>(currentWorkItem->retainedFragmentCount0C));
                if (inputFragment) {
                    if (inputFragment) { inputFragment->Release(); }
                }
                return 0x7000000u;
            }
            if (packetBodyByteCount > 0x1000u) {
                if (inputFragment) {
                    if (inputFragment) { inputFragment->Release(); }
                }
                return 0x700000bu;
            }

            currentWorkItem->assembledByteCount28 = packetBodyByteCount;
            if (currentCursorFragmentRef04.retainedFragment00 &&
                !CParsedPacketWorkItem_RebaseFirstFragmentToScaffold(
                    currentWorkItem,
                    currentCursorFragmentRef04.retainedFragment00)) {
                spdlog::debug(
                    "CVariableLengthPrefixedTCPStreamParser::Parse failed to rebase current work-item first fragment after prefix consume parser={} retainedFragmentCount={} currentCursorFragment={} currentCursor={}",
                    fmt::ptr(this),
                    static_cast<unsigned>(currentWorkItem->retainedFragmentCount0C),
                    fmt::ptr(currentCursorFragmentRef04.retainedFragment00),
                    fmt::ptr(currentCursor08));
                if (inputFragment) {
                    if (inputFragment) { inputFragment->Release(); }
                }
                return 1u;
            }
        }

        packetBodyByteCount = CParsedPacketWorkItem_GetAssembledByteCount(currentWorkItem);
        if (packetBodyByteCount <= unreadBufferedByteCount0C) {
            if (!CVariableLengthPrefixedTCPStreamParser_NormalizeCursorFragmentScaffold(this)) {
                if (inputFragment) {
                    if (inputFragment) { inputFragment->Release(); }
                }
                return 0x7000000u;
            }

            currentWorkItem->currentCursor24 = currentCursor08;
            if (!CVariableLengthPrefixedTCPStreamParser_AdvanceBufferedCursorScaffold(
                    this,
                    packetBodyByteCount,
                    1u)) {
                spdlog::debug(
                    "CVariableLengthPrefixedTCPStreamParser::Parse cursor advance failed during packet-body consume parser={} unreadBuffered=0x{:08x} packetBodyByteCount=0x{:08x} retainedFragmentCount={}",
                    fmt::ptr(this),
                    static_cast<unsigned>(unreadBufferedByteCount0C),
                    static_cast<unsigned>(packetBodyByteCount),
                    static_cast<unsigned>(currentWorkItem->retainedFragmentCount0C));
                if (inputFragment) {
                    if (inputFragment) { inputFragment->Release(); }
                }
                return 0x7000000u;
            }
            if (outCompletedPacketWorkItem) {
                *outCompletedPacketWorkItem = currentWorkItem;
            }
            ResetAfterPacket();
            if (inputFragment) {
                if (inputFragment) { inputFragment->Release(); }
            }
            return 0u;
        }
    }

    if (inputFragment) {
        if (inputFragment) { inputFragment->Release(); }
    }
    return 0x7000000u;
}

}  // namespace mxo::liblttcp
