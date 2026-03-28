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

static void CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(
    CVariableLengthPrefixedTCPStreamParser* parser,
    CLTTCPReadOperationFragmentScaffold* fragment) {
    if (!parser || parser->currentCursorFragment04 == fragment) {
        return;
    }

    CLTTCPReadOperationFragment_ReleaseScaffold(parser->currentCursorFragment04);
    parser->currentCursorFragment04 = fragment;
    if (fragment) {
        CLTTCPReadOperationFragment_AddRefScaffold(fragment);
    }
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
        CLTTCPReadOperationFragment_ReleaseScaffold(node->retainedFragment08);
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

    CLTTCPReadOperationFragment_ReleaseScaffold(workItem->firstRetainedFragment10);
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
// Static body `0x435e60` now proves one narrower point than the old source comment did:
// - the original helper ends with `if (param_1 != 0) param_1->Release();`
// - so original callers may hand it a temporary retained fragment reference and let the helper
//   consume that temporary on return
// Current source still intentionally does *not* restore that trailing release here on the live
// path. The full cross-caller reconciliation across
// `0x42fe50 -> 0x449d40 -> 0x469bf0 -> 0x435e60` remains open, and an earlier attempt to tighten
// this helper body globally caused the known fragment-teardown crash after successful
// parse/enqueue/copy. Keep the surviving live retain balance here until that broader chain is
// fully proven.
static bool CParsedPacketWorkItem_AppendFragmentScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
    CLTTCPReadOperationFragmentScaffold* fragment) {
    if (!workItem || !fragment) {
        return false;
    }

    if (workItem->retainedFragmentCount0C == 0u) {
        workItem->retainedFragmentCount0C = 1u;
        if (workItem->firstRetainedFragment10 != fragment) {
            CLTTCPReadOperationFragment_ReleaseScaffold(workItem->firstRetainedFragment10);
            workItem->firstRetainedFragment10 = fragment;
            CLTTCPReadOperationFragment_AddRefScaffold(fragment);
        }
        return true;
    }

    if (!CParsedPacketWorkItem_EnsureAdditionalFragmentListScaffold(workItem)) {
        return false;
    }

    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* node =
        static_cast<CParsedPacketWorkItem_RetainedFragmentNodeScaffold*>(
            std::calloc(1, sizeof(CParsedPacketWorkItem_RetainedFragmentNodeScaffold)));
    if (!node) {
        return false;
    }

    node->retainedFragment08 = fragment;
    CLTTCPReadOperationFragment_AddRefScaffold(fragment);

    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* sentinel =
        workItem->retainedFragmentListOwner14->sentinel;
    node->next = sentinel;
    node->prev = sentinel->prev;
    sentinel->prev->next = node;
    sentinel->prev = node;
    ++workItem->retainedFragmentCount0C;
    return true;
}

static CLTTCPReadOperationFragmentScaffold* CParsedPacketWorkItem_GetTailFragmentScaffold(
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
    CLTTCPReadOperationFragmentScaffold* firstFragmentToKeep) {
    if (!workItem || !firstFragmentToKeep || workItem->retainedFragmentCount0C == 0u ||
        workItem->firstRetainedFragment10 == firstFragmentToKeep) {
        return true;
    }

    std::vector<CLTTCPReadOperationFragmentScaffold*> retainedFragmentsToKeep;
    CLTTCPReadOperationFragmentScaffold* fragment =
        CParsedPacketWorkItem_BeginFragmentTraversalScaffold(workItem);
    bool keepFragment = false;
    while (fragment) {
        if (fragment == firstFragmentToKeep) {
            keepFragment = true;
        }
        if (keepFragment) {
            CLTTCPReadOperationFragment_AddRefScaffold(fragment);
            retainedFragmentsToKeep.push_back(fragment);
        }

        CLTTCPReadOperationFragmentScaffold* nextFragment =
            CParsedPacketWorkItem_GetNextFragmentScaffold(workItem);
        CLTTCPReadOperationFragment_ReleaseScaffold(fragment);
        fragment = nextFragment;
    }

    if (retainedFragmentsToKeep.empty()) {
        return false;
    }

    CParsedPacketWorkItem_ResetFragmentStateScaffold(workItem);
    for (size_t index = 0; index < retainedFragmentsToKeep.size(); ++index) {
        CLTTCPReadOperationFragmentScaffold* retainedFragmentTempRef = retainedFragmentsToKeep[index];
        if (!CParsedPacketWorkItem_AppendFragmentScaffold(workItem, retainedFragmentTempRef)) {
            CLTTCPReadOperationFragment_ReleaseScaffold(retainedFragmentTempRef);
            for (size_t remaining = index + 1; remaining < retainedFragmentsToKeep.size(); ++remaining) {
                CLTTCPReadOperationFragment_ReleaseScaffold(retainedFragmentsToKeep[remaining]);
            }
            return false;
        }

        // These vector-held retains are source-owned rebase temporaries, not the original
        // `0x435e60` trailing `Release(param_1)` behavior.
        CLTTCPReadOperationFragment_ReleaseScaffold(retainedFragmentTempRef);
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
    if (!parser->currentPacketWorkItem14 || !parser->currentCursorFragment04 || !parser->currentCursor08) {
        return false;
    }

    const uint32_t remainingInCurrentFragment =
        CLTTCPReadOperationFragment_BytesRemainingFromCursorScaffold(
            parser->currentCursorFragment04,
            parser->currentCursor08);
    if (remainingInCurrentFragment != 0u) {
        return true;
    }

    CLTTCPReadOperationFragmentScaffold* fragment =
        CParsedPacketWorkItem_BeginFragmentTraversalScaffold(parser->currentPacketWorkItem14);
    while (fragment) {
        CLTTCPReadOperationFragmentScaffold* nextFragment =
            CParsedPacketWorkItem_GetNextFragmentScaffold(parser->currentPacketWorkItem14);
        if (fragment == parser->currentCursorFragment04) {
            CLTTCPReadOperationFragment_ReleaseScaffold(fragment);
            if (!nextFragment) {
                return false;
            }
            CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(parser, nextFragment);
            parser->currentCursor08 =
                CLTTCPReadOperationFragment_PayloadBeginScaffold(parser->currentCursorFragment04);
            CLTTCPReadOperationFragment_ReleaseScaffold(nextFragment);
            return true;
        }

        CLTTCPReadOperationFragment_ReleaseScaffold(fragment);
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
    if (!parser->currentPacketWorkItem14 || !parser->currentCursorFragment04 || !parser->currentCursor08 ||
        parser->unreadBufferedByteCount0C < byteCountToConsume) {
        return false;
    }

    const uint32_t boundaryBias = ((cursorAdvanceFlags & 0xffu) == 0u) ? 1u : 0u;
    const uint32_t bytesRemainingInCurrentFragment =
        CLTTCPReadOperationFragment_BytesRemainingFromCursorScaffold(
            parser->currentCursorFragment04,
            parser->currentCursor08);
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
            CLTTCPReadOperationFragmentScaffold* traversedFragment =
                CParsedPacketWorkItem_BeginFragmentTraversalScaffold(parser->currentPacketWorkItem14);
            while (traversedFragment) {
                CLTTCPReadOperationFragmentScaffold* nextFragment =
                    CParsedPacketWorkItem_GetNextFragmentScaffold(parser->currentPacketWorkItem14);
                if (traversedFragment == parser->currentCursorFragment04) {
                    CLTTCPReadOperationFragment_ReleaseScaffold(traversedFragment);
                    if (!nextFragment) {
                        return false;
                    }
                    CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(
                        parser,
                        nextFragment);
                    newCursorBase =
                        CLTTCPReadOperationFragment_PayloadBeginScaffold(parser->currentCursorFragment04);
                    cursorByteOffsetWithinFragment = 0u;
                    CLTTCPReadOperationFragment_ReleaseScaffold(nextFragment);
                    break;
                }
                CLTTCPReadOperationFragment_ReleaseScaffold(traversedFragment);
                traversedFragment = nextFragment;
            }
            if (!newCursorBase) {
                return false;
            }
        }
    } else {
        CLTTCPReadOperationFragmentScaffold* traversedFragment =
            CParsedPacketWorkItem_BeginFragmentTraversalScaffold(parser->currentPacketWorkItem14);
        bool passedCurrentCursorFragment = false;
        while (traversedFragment) {
            if (!passedCurrentCursorFragment) {
                if (traversedFragment == parser->currentCursorFragment04) {
                    passedCurrentCursorFragment = true;
                }
            } else {
                if (remainingToConsume <= traversedFragment->byteCount) {
                    if (traversedFragment != parser->currentCursorFragment04) {
                        CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(
                            parser,
                            traversedFragment);
                    }
                    newCursorBase =
                        CLTTCPReadOperationFragment_PayloadBeginScaffold(parser->currentCursorFragment04);
                    cursorByteOffsetWithinFragment = remainingToConsume - boundaryBias;
                    CLTTCPReadOperationFragment_ReleaseScaffold(traversedFragment);
                    traversedFragment = nullptr;
                    break;
                }
                remainingToConsume -= traversedFragment->byteCount;
            }

            CLTTCPReadOperationFragmentScaffold* nextFragment =
                CParsedPacketWorkItem_GetNextFragmentScaffold(parser->currentPacketWorkItem14);
            CLTTCPReadOperationFragment_ReleaseScaffold(traversedFragment);
            traversedFragment = nextFragment;
        }
        if (traversedFragment) {
            CLTTCPReadOperationFragment_ReleaseScaffold(traversedFragment);
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

// anchor: launcher.exe:0x42f850 / vtable 0x004b2300 +0x04
void CLTTCPReadOperationFragment_AddRefScaffold(CLTTCPReadOperationFragmentScaffold* fragment) {
    if (!fragment || !fragment->vtable || !fragment->vtable->addRef) {
        return;
    }

    fragment->vtable->addRef(fragment);
}

// anchor: launcher.exe:0x42f860 / vtable 0x004b2300 +0x08
void CLTTCPReadOperationFragment_ReleaseScaffold(CLTTCPReadOperationFragmentScaffold* fragment) {
    if (!fragment || !fragment->vtable || !fragment->vtable->release) {
        return;
    }

    fragment->vtable->release(fragment);
}

uint8_t* CLTTCPReadOperationFragment_PayloadBeginScaffold(
    CLTTCPReadOperationFragmentScaffold* fragment) {
    return fragment ? fragment->bytes0C : nullptr;
}

const uint8_t* CLTTCPReadOperationFragment_PayloadEndScaffold(
    const CLTTCPReadOperationFragmentScaffold* fragment) {
    return fragment ? (fragment->bytes0C + fragment->byteCount) : nullptr;
}

uint32_t CLTTCPReadOperationFragment_BytesRemainingFromCursorScaffold(
    const CLTTCPReadOperationFragmentScaffold* fragment,
    const uint8_t* cursor) {
    if (!fragment || !cursor) {
        return 0u;
    }

    const uint8_t* begin = fragment->bytes0C;
    const uint8_t* end = CLTTCPReadOperationFragment_PayloadEndScaffold(fragment);
    if (cursor < begin || cursor > end) {
        return 0u;
    }

    return static_cast<uint32_t>(end - cursor);
}

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
CLTTCPReadOperationFragmentScaffold* CParsedPacketWorkItem_BeginFragmentTraversalScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    if (!workItem || workItem->retainedFragmentCount0C == 0u) {
        return nullptr;
    }

    workItem->directFragmentTraversalPhase18 = 1u;
    workItem->fragmentTraversalIndex1C = 0u;
    workItem->fragmentTraversalNode20 = nullptr;
    CLTTCPReadOperationFragmentScaffold* fragment = workItem->firstRetainedFragment10;
    CLTTCPReadOperationFragment_AddRefScaffold(fragment);
    return fragment;
}

// anchor: launcher.exe:0x435510
CLTTCPReadOperationFragmentScaffold* CParsedPacketWorkItem_GetNextFragmentScaffold(
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
            CLTTCPReadOperationFragmentScaffold* fragment = workItem->firstRetainedFragment10;
            CLTTCPReadOperationFragment_AddRefScaffold(fragment);
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

    CLTTCPReadOperationFragmentScaffold* fragment = nextNode->retainedFragment08;
    CLTTCPReadOperationFragment_AddRefScaffold(fragment);
    return fragment;
}

// anchor: launcher.exe:0x4355c0
CLTTCPReadOperationFragmentScaffold* CParsedPacketWorkItem_GetTailFragmentTempRefScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    CLTTCPReadOperationFragmentScaffold* tailFragment =
        CParsedPacketWorkItem_GetTailFragmentScaffold(workItem);
    CLTTCPReadOperationFragment_AddRefScaffold(tailFragment);
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
    currentCursorFragment04 = nullptr;
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

    CLTTCPReadOperationFragmentScaffold* tailFragmentTempRef =
        CParsedPacketWorkItem_GetTailFragmentTempRefScaffold(emittedWorkItem);
    if (tailFragmentTempRef != currentCursorFragment04) {
        CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(
            this,
            tailFragmentTempRef);
    }
    CLTTCPReadOperationFragment_ReleaseScaffold(tailFragmentTempRef);

    CLTTCPReadOperationFragmentScaffold* parserCurrentCursorFragment = currentCursorFragment04;
    if (parserCurrentCursorFragment) {
        // `0x4725c0` explicitly takes one transient ref on parser `+0x04` immediately before
        // `0x435e60`. Original `0x435e60` then consumes that temp with its trailing
        // `Release(param_1)`. The live source helper still keeps its own source-owned no-trailing-
        // release body, so drop the carry-over temp explicitly here instead of broadening that
        // tighter-but-still-risky helper correction back onto the active parse path.
        CLTTCPReadOperationFragment_AddRefScaffold(parserCurrentCursorFragment);
        const bool appended = CParsedPacketWorkItem_AppendFragmentScaffold(
            replacementWorkItem,
            parserCurrentCursorFragment);
        CLTTCPReadOperationFragment_ReleaseScaffold(parserCurrentCursorFragment);
        if (!appended) {
            return;
        }
    }

    replacementWorkItem->currentCursor24 = currentCursor08;
}

// anchor: launcher.exe:0x469bf0 / vtable 0x004baf88
uint32_t CVariableLengthPrefixedTCPStreamParser::Parse(
    CLTTCPReadOperationFragmentScaffold* readOperationFragment,
    CLTTCPConnection_ParsedPacketWorkItemScaffold** outCompletedPacketWorkItem) {
    if (outCompletedPacketWorkItem) {
        *outCompletedPacketWorkItem = nullptr;
    }
    if (!currentPacketWorkItem14) {
        currentPacketWorkItem14 = AllocatePacketBuffer();
        if (!currentPacketWorkItem14) {
            if (readOperationFragment) {
                CLTTCPReadOperationFragment_ReleaseScaffold(readOperationFragment);
            }
            return 1u;
        }
    }

    CLTTCPConnection_ParsedPacketWorkItemScaffold* currentWorkItem = currentPacketWorkItem14;
    CLTTCPReadOperationFragmentScaffold* inputFragment = readOperationFragment;
    if (readOperationFragment && readOperationFragment->byteCount != 0u) {
        // `0x469bf0` takes a transient fragment ref immediately before `0x435e60`.
        // Original `0x435e60` consumes one caller-held temp on return, but the live source helper
        // still intentionally keeps its source-owned no-trailing-release behavior on this active
        // Parse path until the full worker->OnReceive->Parse->queue-consumer chain is proven safe.
        CLTTCPReadOperationFragment_AddRefScaffold(readOperationFragment);
        const bool appended =
            CParsedPacketWorkItem_AppendFragmentScaffold(currentWorkItem, readOperationFragment);
        if (currentCursorFragment04 == nullptr) {
            CVariableLengthPrefixedTCPStreamParser_AssignCurrentCursorFragmentScaffold(
                this,
                readOperationFragment);
        }
        if (unreadBufferedByteCount0C == 0u) {
            currentCursor08 =
                CLTTCPReadOperationFragment_PayloadBeginScaffold(readOperationFragment);
        }
        unreadBufferedByteCount0C += readOperationFragment->byteCount;
        if (!appended) {
            if (inputFragment) {
                CLTTCPReadOperationFragment_ReleaseScaffold(inputFragment);
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
                    CLTTCPReadOperationFragment_ReleaseScaffold(inputFragment);
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
                    fmt::ptr(currentCursorFragment04));
                if (inputFragment) {
                    CLTTCPReadOperationFragment_ReleaseScaffold(inputFragment);
                }
                return 0x7000000u;
            }

            const uint8_t firstPrefixByte = *currentCursor08;
            packetBodyByteCount = static_cast<uint32_t>(firstPrefixByte);
            if ((firstPrefixByte & 0x80u) != 0u) {
                if (!CVariableLengthPrefixedTCPStreamParser_AdvanceBufferedCursorScaffold(this, 1u, 1u) ||
                    !CVariableLengthPrefixedTCPStreamParser_NormalizeCursorFragmentScaffold(this)) {
                    if (inputFragment) {
                        CLTTCPReadOperationFragment_ReleaseScaffold(inputFragment);
                    }
                    return 0x7000000u;
                }

                packetBodyByteCount =
                    (static_cast<uint32_t>(firstPrefixByte & 0x7fu) << 8u) |
                    static_cast<uint32_t>(*currentCursor08);
                if (packetBodyByteCount < 0x80u) {
                    if (inputFragment) {
                        CLTTCPReadOperationFragment_ReleaseScaffold(inputFragment);
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
                    CLTTCPReadOperationFragment_ReleaseScaffold(inputFragment);
                }
                return 0x7000000u;
            }
            if (packetBodyByteCount > 0x1000u) {
                if (inputFragment) {
                    CLTTCPReadOperationFragment_ReleaseScaffold(inputFragment);
                }
                return 0x700000bu;
            }

            currentWorkItem->assembledByteCount28 = packetBodyByteCount;
            if (currentCursorFragment04 &&
                !CParsedPacketWorkItem_RebaseFirstFragmentToScaffold(
                    currentWorkItem,
                    currentCursorFragment04)) {
                spdlog::debug(
                    "CVariableLengthPrefixedTCPStreamParser::Parse failed to rebase current work-item first fragment after prefix consume parser={} retainedFragmentCount={} currentCursorFragment={} currentCursor={}",
                    fmt::ptr(this),
                    static_cast<unsigned>(currentWorkItem->retainedFragmentCount0C),
                    fmt::ptr(currentCursorFragment04),
                    fmt::ptr(currentCursor08));
                if (inputFragment) {
                    CLTTCPReadOperationFragment_ReleaseScaffold(inputFragment);
                }
                return 1u;
            }
        }

        packetBodyByteCount = CParsedPacketWorkItem_GetAssembledByteCountScaffold(currentWorkItem);
        if (packetBodyByteCount <= unreadBufferedByteCount0C) {
            if (!CVariableLengthPrefixedTCPStreamParser_NormalizeCursorFragmentScaffold(this)) {
                if (inputFragment) {
                    CLTTCPReadOperationFragment_ReleaseScaffold(inputFragment);
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
                    CLTTCPReadOperationFragment_ReleaseScaffold(inputFragment);
                }
                return 0x7000000u;
            }
            if (outCompletedPacketWorkItem) {
                *outCompletedPacketWorkItem = currentWorkItem;
            }
            ResetAfterPacket();
            if (inputFragment) {
                CLTTCPReadOperationFragment_ReleaseScaffold(inputFragment);
            }
            return 0u;
        }
    }

    if (inputFragment) {
        CLTTCPReadOperationFragment_ReleaseScaffold(inputFragment);
    }
    return 0x7000000u;
}

}  // namespace mxo::liblttcp
