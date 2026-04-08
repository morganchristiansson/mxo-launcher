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
    FramedPacket* outPacket) {
    if (!payload || !outPacket) {
        return false;
    }

    outPacket->headerBytes.clear();
    outPacket->payloadBytes.assign(payload, payload + payloadSize);
    outPacket->bytes.clear();

    if (mode == kFrameModeForceOneByte) {
        if (payloadSize > 0x7fu) {
            return false;
        }
        outPacket->headerBytes.push_back(static_cast<uint8_t>(payloadSize));
    } else {
        if (payloadSize > 0x7fffu) {
            return false;
        }
        if (mode == kFrameModeForceTwoByte || payloadSize >= 0x80u) {
            outPacket->headerBytes.push_back(
                static_cast<uint8_t>(0x80u | ((payloadSize >> 8u) & 0x7fu)));
            outPacket->headerBytes.push_back(static_cast<uint8_t>(payloadSize & 0xffu));
        } else {
            outPacket->headerBytes.push_back(static_cast<uint8_t>(payloadSize));
        }
    }

    outPacket->bytes.reserve(outPacket->headerBytes.size() + outPacket->payloadBytes.size());
    outPacket->bytes.insert(
        outPacket->bytes.end(),
        outPacket->headerBytes.begin(),
        outPacket->headerBytes.end());
    outPacket->bytes.insert(
        outPacket->bytes.end(),
        outPacket->payloadBytes.begin(),
        outPacket->payloadBytes.end());
    return true;
}

bool ParseVariableLengthPacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    FramedPacket* outPacket) {
    if (!packetBytes || !outPacket || packetSize < 2u) {
        return false;
    }

    size_t headerSize = 1u;
    size_t payloadSize = 0u;
    if (packetBytes[0] & 0x80u) {
        if (packetSize < 3u) {
            return false;
        }
        headerSize = 2u;
        payloadSize =
            (static_cast<size_t>(packetBytes[0] & 0x7fu) << 8u) |
            static_cast<size_t>(packetBytes[1]);
    } else {
        payloadSize = static_cast<size_t>(packetBytes[0]);
    }

    if (headerSize + payloadSize > packetSize) {
        return false;
    }

    outPacket->headerBytes.assign(packetBytes, packetBytes + headerSize);
    outPacket->payloadBytes.assign(
        packetBytes + headerSize,
        packetBytes + headerSize + payloadSize);
    outPacket->bytes.assign(
        packetBytes,
        packetBytes + headerSize + payloadSize);
    return true;
}

}  // namespace mxo::auth

namespace mxo::liblttcp {

namespace {

static void* g_ParsedPacketWorkItemVtable[2] = {nullptr, nullptr};

static void EnsureParsedPacketWorkItemVtableInitialized() {
    if (!g_ParsedPacketWorkItemVtable[1]) {
        g_ParsedPacketWorkItemVtable[1] =
            reinterpret_cast<void*>(CParsedPacketWorkItem_ReleaseScaffold);
    }
}

static CLTTCPReadOperation* CLTTCPReadOperationRefHandle_AssignRetained(
    CLTTCPReadOperationRefHandle* handle,
    CLTTCPReadOperation* newRetainedFragment) {
    if (!handle) {
        return nullptr;
    }
    CLTTCPReadOperation* const oldRetainedFragment = handle->retainedFragment00;
    if (oldRetainedFragment != newRetainedFragment) {
        if (oldRetainedFragment) {
            if (oldRetainedFragment) { oldRetainedFragment->Release(); }
        }
        handle->retainedFragment00 = newRetainedFragment;
        if (newRetainedFragment) {
            if (newRetainedFragment) { newRetainedFragment->AddRef(); }
        }
    }
    return handle->retainedFragment00;
}

static void CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(
    CVariableLengthPrefixedTCPStreamParser* parser,
    CLTTCPReadOperation* fragment) {
    if (!parser) {
        return;
    }
    (void)CLTTCPReadOperationRefHandle_AssignRetained(
        &parser->currentCursorFragmentRef04,
        fragment);
}

static void CParsedPacketWorkItem_ClearFragmentListScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
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
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
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

static CLTTCPConnection_ParsedPacketWorkItemScaffold* CParsedPacketWorkItem_AllocateScaffold() {
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
static bool CParsedPacketWorkItem_EnsureAdditionalFragmentListScaffold(
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
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
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

static CLTTCPReadOperation* CParsedPacketWorkItem_GetTailFragmentScaffold(
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

// UNANCHORED: source-owned work-item rebase helper used after prefix consume whenever the parser
// cursor has advanced into a later retained fragment.
static bool CParsedPacketWorkItem_RebaseFirstFragmentToScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
    CLTTCPReadOperation* firstFragmentToKeep) {
    if (!workItem || !firstFragmentToKeep || workItem->retainedFragmentCount0C == 0u ||
        workItem->firstRetainedFragment10 == firstFragmentToKeep) {
        return true;
    }

    std::vector<CLTTCPReadOperation*> retainedFragmentsToKeep;
    CLTTCPReadOperation* fragment =
        CParsedPacketWorkItem_BeginFragmentTraversalScaffold(workItem);
    bool keepFragment = false;
    while (fragment) {
        if (fragment == firstFragmentToKeep) {
            keepFragment = true;
        }
        if (keepFragment) {
            if (fragment) { fragment->AddRef(); }
            retainedFragmentsToKeep.push_back(fragment);
        }

        CLTTCPReadOperation* nextFragment =
            CParsedPacketWorkItem_GetNextFragmentScaffold(workItem);
        if (fragment) { fragment->Release(); }
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
                if (retainedFragmentsToKeep[remaining]) { retainedFragmentsToKeep[remaining]->Release(); }
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
    if (!parser->currentPacketWorkItem14 || !parser->currentCursorFragmentRef04.retainedFragment00 || !parser->currentCursor08) {
        return false;
    }

    const uint8_t* const currentFragmentBegin =
        reinterpret_cast<const uint8_t*>(parser->currentCursorFragmentRef04.retainedFragment00 + 1);
    const uint8_t* const currentFragmentEnd =
        currentFragmentBegin + parser->currentCursorFragmentRef04.retainedFragment00->byteCount08;
    if (parser->currentCursor08 < currentFragmentBegin || parser->currentCursor08 > currentFragmentEnd) {
        return false;
    }
    const uint32_t remainingInCurrentFragment =
        static_cast<uint32_t>(currentFragmentEnd - parser->currentCursor08);
    if (remainingInCurrentFragment != 0u) {
        return true;
    }

    CLTTCPReadOperation* fragment =
        CParsedPacketWorkItem_BeginFragmentTraversalScaffold(parser->currentPacketWorkItem14);
    while (fragment) {
        CLTTCPReadOperation* nextFragment =
            CParsedPacketWorkItem_GetNextFragmentScaffold(parser->currentPacketWorkItem14);
        if (fragment == parser->currentCursorFragmentRef04.retainedFragment00) {
            if (fragment) { fragment->Release(); }
            if (!nextFragment) {
                return false;
            }
            CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(parser, nextFragment);
            parser->currentCursor08 =
                reinterpret_cast<uint8_t*>(parser->currentCursorFragmentRef04.retainedFragment00 + 1);
            if (nextFragment) { nextFragment->Release(); }
            return true;
        }

        if (fragment) { fragment->Release(); }
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
    if (!parser->currentPacketWorkItem14 || !parser->currentCursorFragmentRef04.retainedFragment00 || !parser->currentCursor08 ||
        parser->unreadBufferedByteCount0C < byteCountToConsume) {
        return false;
    }

    const uint32_t boundaryBias = ((cursorAdvanceFlags & 0xffu) == 0u) ? 1u : 0u;
    const uint8_t* const currentFragmentBegin =
        reinterpret_cast<const uint8_t*>(parser->currentCursorFragmentRef04.retainedFragment00 + 1);
    const uint8_t* const currentFragmentEnd =
        currentFragmentBegin + parser->currentCursorFragmentRef04.retainedFragment00->byteCount08;
    if (parser->currentCursor08 < currentFragmentBegin || parser->currentCursor08 > currentFragmentEnd) {
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
            CLTTCPReadOperation* traversedFragment =
                CParsedPacketWorkItem_BeginFragmentTraversalScaffold(parser->currentPacketWorkItem14);
            while (traversedFragment) {
                CLTTCPReadOperation* nextFragment =
                    CParsedPacketWorkItem_GetNextFragmentScaffold(parser->currentPacketWorkItem14);
                if (traversedFragment == parser->currentCursorFragmentRef04.retainedFragment00) {
                    if (traversedFragment) { traversedFragment->Release(); }
                    if (!nextFragment) {
                        return false;
                    }
                    CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(
                        parser,
                        nextFragment);
                    newCursorBase =
                        reinterpret_cast<uint8_t*>(parser->currentCursorFragmentRef04.retainedFragment00 + 1);
                    cursorByteOffsetWithinFragment = 0u;
                    if (nextFragment) { nextFragment->Release(); }
                    break;
                }
                if (traversedFragment) { traversedFragment->Release(); }
                traversedFragment = nextFragment;
            }
            if (!newCursorBase) {
                return false;
            }
        }
    } else {
        CLTTCPReadOperation* traversedFragment =
            CParsedPacketWorkItem_BeginFragmentTraversalScaffold(parser->currentPacketWorkItem14);
        bool passedCurrentCursorFragment = false;
        while (traversedFragment) {
            if (!passedCurrentCursorFragment) {
                if (traversedFragment == parser->currentCursorFragmentRef04.retainedFragment00) {
                    passedCurrentCursorFragment = true;
                }
            } else {
                if (remainingToConsume <= traversedFragment->byteCount08) {
                    if (traversedFragment != parser->currentCursorFragmentRef04.retainedFragment00) {
                        CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(
                            parser,
                            traversedFragment);
                    }
                    newCursorBase =
                        reinterpret_cast<uint8_t*>(parser->currentCursorFragmentRef04.retainedFragment00 + 1);
                    cursorByteOffsetWithinFragment = remainingToConsume - boundaryBias;
                    if (traversedFragment) { traversedFragment->Release(); }
                    traversedFragment = nullptr;
                    break;
                }
                remainingToConsume -= traversedFragment->byteCount08;
            }

            CLTTCPReadOperation* nextFragment =
                CParsedPacketWorkItem_GetNextFragmentScaffold(parser->currentPacketWorkItem14);
            if (traversedFragment) { traversedFragment->Release(); }
            traversedFragment = nextFragment;
        }
        if (traversedFragment) {
            if (traversedFragment) { traversedFragment->Release(); }
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

static CLTTCPConnection_ParsedPacketWorkItemScaffold*
CVariableLengthPrefixedTCPStreamParser_AllocatePacketBufferScaffold() {
    return CParsedPacketWorkItem_AllocateScaffold();
}

}  // namespace

uint32_t __thiscall CParsedPacketWorkItem_ReleaseScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* self) {
    if (!self) {
        return 1u;
    }

    CParsedPacketWorkItem_ResetFragmentStateScaffold(self);
    std::free(self);
    return 1u;
}

// anchor: launcher.exe:0x4350c0
CLTTCPReadOperation* CParsedPacketWorkItem_BeginFragmentTraversalScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    if (!workItem || workItem->retainedFragmentCount0C == 0u) {
        return nullptr;
    }

    workItem->directFragmentTraversalPhase18 = 1u;
    workItem->fragmentTraversalIndex1C = 0u;
    workItem->fragmentTraversalNode20 = nullptr;
    CLTTCPReadOperation* fragment = workItem->firstRetainedFragment10;
    if (fragment) { fragment->AddRef(); }
    return fragment;
}

// anchor: launcher.exe:0x435510
CLTTCPReadOperation* CParsedPacketWorkItem_GetNextFragmentScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    if (!workItem) {
        return nullptr;
    }

    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* traversalNode = nullptr;
    if (workItem->directFragmentTraversalPhase18 == 0u) {
        traversalNode = workItem->fragmentTraversalNode20;
    } else {
        const uint32_t traversalIndex = workItem->fragmentTraversalIndex1C + 1u;
        workItem->fragmentTraversalIndex1C = traversalIndex;
        if (traversalIndex == 0u) {
            CLTTCPReadOperation* fragment = workItem->firstRetainedFragment10;
            if (fragment) { fragment->AddRef(); }
            return fragment;
        }
        if (!workItem->retainedFragmentListOwner14 || !workItem->retainedFragmentListOwner14->sentinel) {
            return nullptr;
        }
        workItem->directFragmentTraversalPhase18 = 0u;
        traversalNode = workItem->retainedFragmentListOwner14->sentinel;
    }

    if (!traversalNode || !workItem->retainedFragmentListOwner14 ||
        !workItem->retainedFragmentListOwner14->sentinel) {
        return nullptr;
    }

    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* nextNode = traversalNode->next;
    workItem->fragmentTraversalNode20 = nextNode;
    if (nextNode == workItem->retainedFragmentListOwner14->sentinel) {
        return nullptr;
    }

    CLTTCPReadOperation* fragment = nextNode->retainedFragment08;
    if (fragment) { fragment->AddRef(); }
    return fragment;
}

// anchor: launcher.exe:0x4355c0
CLTTCPReadOperation* CParsedPacketWorkItem_GetTailFragmentTempRefScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    CLTTCPReadOperation* tailFragment =
        CParsedPacketWorkItem_GetTailFragmentScaffold(workItem);
    if (tailFragment) { tailFragment->AddRef(); }
    return tailFragment;
}

// anchor: launcher.exe:0x434f80
uint8_t* CParsedPacketWorkItem_GetCurrentCursorScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    return workItem ? workItem->currentCursor24 : nullptr;
}

// anchor: launcher.exe:0x434f60
uint32_t CParsedPacketWorkItem_GetAssembledByteCountScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
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
CLTTCPConnection_ParsedPacketWorkItemScaffold*
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
    CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(this, nullptr);
}

// anchor: launcher.exe:0x4725c0 / vtable 0x004baf90
void CVariableLengthPrefixedTCPStreamParser::ResetAfterPacket() {
    CLTTCPConnection_ParsedPacketWorkItemScaffold* emittedWorkItem = currentPacketWorkItem14;
    CLTTCPConnection_ParsedPacketWorkItemScaffold* replacementWorkItem = AllocatePacketBuffer();
    currentPacketWorkItem14 = replacementWorkItem;
    CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(this, nullptr);
    advancedBufferedByteCount10 = 0u;

    if (unreadBufferedByteCount0C == 0u || !replacementWorkItem || !emittedWorkItem) {
        return;
    }

    CLTTCPReadOperation* tailFragmentTempRef =
        CParsedPacketWorkItem_GetTailFragmentTempRefScaffold(emittedWorkItem);
    if (tailFragmentTempRef != currentCursorFragmentRef04.retainedFragment00) {
        CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(
            this,
            tailFragmentTempRef);
    }
    if (tailFragmentTempRef) { tailFragmentTempRef->Release(); }
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
    CLTTCPConnection_ParsedPacketWorkItemScaffold** outCompletedPacketWorkItem) {
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

    CLTTCPConnection_ParsedPacketWorkItemScaffold* currentWorkItem = currentPacketWorkItem14;
    CLTTCPReadOperation* inputFragment = readOperationFragment;
    if (readOperationFragment && readOperationFragment->byteCount08 != 0u) {
        // `0x469bf0` takes a transient fragment ref immediately before `0x435e60`, and the helper
        // now mirrors the original trailing `Release(param_1)` again. That keeps the Parse-side
        // handoff narrower and closer to the original worker->OnReceive->Parse ownership chain.
        if (readOperationFragment) { readOperationFragment->AddRef(); }
        const bool appended =
            CParsedPacketWorkItem_AppendFragmentScaffold(currentWorkItem, readOperationFragment);
        if (currentCursorFragmentRef04.retainedFragment00 == nullptr) {
            CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(
                this,
                readOperationFragment);
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
            CParsedPacketWorkItem_GetAssembledByteCountScaffold(currentWorkItem);
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

        packetBodyByteCount = CParsedPacketWorkItem_GetAssembledByteCountScaffold(currentWorkItem);
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
