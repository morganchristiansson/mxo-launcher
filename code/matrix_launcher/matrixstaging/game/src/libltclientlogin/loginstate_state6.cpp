#include <spdlog/spdlog.h>
#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "client_chunk_hashes.h"
#include "../../../../src/diagnostics.h"

#include <array>
#include <cstring>
#include <sstream>
#include <string>
#include <windows.h>

namespace mxo::ltlogin {
namespace {

// State6Packet0x06FixedPayload is defined in loginstate_packet_builder_scaffold.h

static std::array<uint32_t, 4> ResolveState6GobFileGuidWords(CLTLoginMediator* mediator) {
  // anchor: launcher.exe:0x438e60 = GetGOBFileGUID
  // Original resolution order:
  // - call mediator vtable +0x12c / 0x41f210 to fetch owner +0x8c
  // - if non-null, call that object's vtable +0x1c to get a 16-byte LTGUID pointer
  // - otherwise fall back to the baked-in dwords at 0x4ae6c0..0x4ae6cc (all zeros in launcher.exe)
  std::array<uint32_t, 4> gobGuidWords = {0u, 0u, 0u, 0u};
  if (mediator == nullptr) {
    return gobGuidWords;
  }

  void* const startupDistrObjExecutive8c = mediator->GetStartupDistrObjExecutive8c();
  if (startupDistrObjExecutive8c == nullptr) {
    return gobGuidWords;
  }

  void** const vtable = *reinterpret_cast<void***>(startupDistrObjExecutive8c);
  if (vtable == nullptr || vtable[7] == nullptr) {
    return gobGuidWords;
  }

  using GetGuidWordsFn = const uint32_t*(__thiscall*)(void*);
  const auto getGuidWords = reinterpret_cast<GetGuidWordsFn>(vtable[7]); // vtable +0x1c
  const uint32_t* const guidWords = getGuidWords(startupDistrObjExecutive8c);
  if (guidWords != nullptr) {
    std::copy_n(guidWords, gobGuidWords.size(), gobGuidWords.begin());
  }
  return gobGuidWords;
}

static const char* ResolveClientMetricFilenameById(uint16_t metricId) {
  // Original launcher `0x48ce00` maps one METRID to its Filename by iterating the launcher-side
  // METR metadata table. The replacement keeps that question local to the post-state9/state12
  // path by using the already loaded client-side copy of the same METR metadata instead of
  // widening this pass into a launcher-owned SetMasterDatabase reconstruction first.
  static constexpr uintptr_t kClientMetricArrayBeginRva = 0x009f1934u;
  static constexpr uintptr_t kClientMetricArrayEndRva = 0x009f1938u;
  static constexpr uintptr_t kClientFindFieldRva = 0x002abb60u;
  static constexpr uintptr_t kClientFieldGetStringRva = 0x002add50u;
  static constexpr uintptr_t kClientFieldGetDwordRva = 0x002add30u;

  using FindFieldFn = void*(__thiscall*)(void*, const char*);
  using GetStringFn = const char*(__thiscall*)(void*);
  using GetDwordFn = uint32_t(__thiscall*)(void*);

  void* const clientModule = GetModuleHandleA("client.dll");
  if (clientModule == nullptr) {
    return nullptr;
  }

  const uintptr_t clientBase = reinterpret_cast<uintptr_t>(clientModule);
  const auto findField = reinterpret_cast<FindFieldFn>(clientBase + kClientFindFieldRva);
  const auto getString = reinterpret_cast<GetStringFn>(clientBase + kClientFieldGetStringRva);
  const auto getDword = reinterpret_cast<GetDwordFn>(clientBase + kClientFieldGetDwordRva);

  const uintptr_t entriesBegin = *reinterpret_cast<const uintptr_t*>(clientBase + kClientMetricArrayBeginRva);
  const uintptr_t entriesEnd = *reinterpret_cast<const uintptr_t*>(clientBase + kClientMetricArrayEndRva);
  if (entriesBegin == 0u || entriesEnd <= entriesBegin) {
    return nullptr;
  }

  for (uintptr_t current = entriesBegin; current < entriesEnd; current += sizeof(uint32_t)) {
    void* const entry = *reinterpret_cast<void* const*>(current);
    if (entry == nullptr) {
      continue;
    }

    const char* const tag = *reinterpret_cast<const char* const*>(entry);
    if (!tag || std::strcmp(tag, "METR") != 0) {
      continue;
    }

    void* const metricIdField = findField(entry, "METRID");
    if (metricIdField == nullptr) {
      continue;
    }
    void* const metricIdValueHolder = *reinterpret_cast<void**>(static_cast<uint8_t*>(metricIdField) + 4u);
    void* const metricIdValueObject = metricIdValueHolder ? *reinterpret_cast<void**>(metricIdValueHolder) : nullptr;
    if (metricIdValueObject == nullptr) {
      continue;
    }
    const uint16_t currentMetricId = static_cast<uint16_t>(getDword(metricIdValueObject) & 0xffffu);
    if (currentMetricId != metricId) {
      continue;
    }

    void* const filenameField = findField(entry, "Filename");
    if (filenameField == nullptr) {
      return nullptr;
    }
    void* const filenameValueHolder = *reinterpret_cast<void**>(static_cast<uint8_t*>(filenameField) + 4u);
    void* const filenameValueObject = filenameValueHolder ? *reinterpret_cast<void**>(filenameValueHolder) : nullptr;
    return filenameValueObject ? getString(filenameValueObject) : nullptr;
  }

  return nullptr;
}

static std::string BuildMetricIdPreview(const uint16_t* metricIds, uint16_t count) {
  if (count == 0 || metricIds == nullptr) {
    return "[]";
  }

  std::ostringstream out;
  out << '[';
  for (uint16_t i = 0; i < count; ++i) {
    if (i != 0u) {
      out << ", ";
    }
    out << "0x" << std::hex << std::uppercase << static_cast<unsigned>(metricIds[i]);
  }
  out << ']';
  return out.str();
}

} // namespace

// anchor: launcher.exe vtable 0x4b508c
const char* CLTLoginState_State6_0x4b508c::DebugName() const {
  return "CLTLoginState_State6_0x4b508c";
}

// anchor: launcher.exe:0x43b8f0 (vtable 0x4b508c slot 3)
void CLTLoginState_State6_0x4b508c::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
  // anchor: launcher.exe:0x43b8f0 upstream caching logic at `0x43b8f9..0x43b91c`
  // - `0x43b8f9` loads existing cached from this+4
  // - `0x43b8fc` tests if cached is null - if so, jumps to store at `0x43b91c`
  // - `0x43b904..0x43b91a` calls incoming upstream's vtable `+0x18` to get phase code
  // - if phase==4 or phase==5, jumps to `0x43b91f` preserving existing cached
  // - otherwise `0x43b91c` overwrites this+4 with incoming upstream pointer
  // That cached upstream is later used on opcode-9 success (`0x440acc..0x440ae0`) to choose
  // the next helper-state target via vtable+0x18.
  if (upstreamOrArg != nullptr) {
    const uint32_t upstreamPhaseCode = RecoverCachedUpstreamPhaseCode(upstreamOrArg);
    if (cachedUpstreamOrArg_0x4 == nullptr || (upstreamPhaseCode != 4u && upstreamPhaseCode != 5u)) {
      cachedUpstreamOrArg_0x4 = upstreamOrArg;
    }
  }
  // anchor: launcher.exe:0x43b91f..0x43b92a - direct g_CurrentLoginMediator access, no null check
  if (!g_CurrentLoginMediator->State10HasReadyConnectionState2()) {
    g_CurrentLoginMediator->SetCurrentState(4u);
    return;
  }

  auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection_0x4aff38*>(g_CurrentLoginMediator->marginConnection_);
  const bool marginConnectionReady84 =
    marginConnection != nullptr && marginConnection->MessageCode4SuccessFlag84();
  // anchor: launcher.exe:0x43b94a..0x43b95e - no logging on fallback, just return void
  if (!marginConnectionReady84) {
    g_CurrentLoginMediator->SetCurrentState(5u);
    return;
  }

  // anchor: launcher.exe:0x43b8f0 / local packet-builder family `0x4b5364`
  // Current tighter send-side mirror:
  // - payload `+0x00` = raw opcode `0x06` (`MS_ConnectRequest`)
  // - payload `+0x01/+0x05` = owner `+0x08/+0x0c` launcher/client version dwords
  // - payload `+0x09` = fixed byte `1`
  // - payload `+0x0a/+0x0e` = fixed dwords `0x11186887` / `0x7460a4b0`
  // - payload `+0x12..+0x21` = packed-GOB LTGUID from `0x438e60` fallback family
  // - payload `+0x22` = owner current helper phase byte
  // anchor: launcher.exe:0x43b8f0 = Packet_MsConnectRequest_0x4b5364::ResetAndInitialize
  Packet_MsConnectRequest_0x4b5364 packetBuilder;
  packetBuilder.ResetAndInitialize();

  // Get payload base for writes
  uint8_t* payloadBase = packetBuilder.PayloadBase();
  // Get version values from mediator and write directly to payload
  const uint32_t* launcherVersionPtr = g_CurrentLoginMediator->GetNoPatchLauncherVersionValuePtr08();
  const uint32_t* clientVersionPtr = g_CurrentLoginMediator->GetNoPatchClientVersionValuePtr0c();

  // Write fields directly to payload (static-RE faithful)
  if (payloadBase) {
    // Version dwords at +0x01 and +0x05
    *reinterpret_cast<uint32_t*>(payloadBase + 1) =
      launcherVersionPtr ? *launcherVersionPtr : 0u;
    *reinterpret_cast<uint32_t*>(payloadBase + 5) =
      clientVersionPtr ? *clientVersionPtr : 0u;
    // Fixed 9-byte block at +0x09: byte=1, dword=0x11186887, dword=0x7460a4b0
    payloadBase[9] = 1;
    payloadBase[10] = 0x87;
    payloadBase[0xb] = 0x68;
    payloadBase[0xc] = 0x18;
    payloadBase[0xd] = 0x11;
    payloadBase[0xe] = 0xb0;
    payloadBase[0xf] = 0xa4;
    payloadBase[0x10] = 0x60;
    payloadBase[0x11] = 0x74;
    // Packed GOB LTGUID from launcher resource at +0x12..+0x21
    const auto gobGuidWords = ResolveState6GobFileGuidWords(g_CurrentLoginMediator);
    *reinterpret_cast<uint32_t*>(payloadBase + 0x12) = gobGuidWords[0];
    *reinterpret_cast<uint32_t*>(payloadBase + 0x16) = gobGuidWords[1];
    *reinterpret_cast<uint32_t*>(payloadBase + 0x1a) = gobGuidWords[2];
    *reinterpret_cast<uint32_t*>(payloadBase + 0x1e) = gobGuidWords[3];
    // mbr_0x5 is a boolean: 0 by default, set to 1 by SetUnknownByte05()
    // Static-RE faithful: call GetUnknownByte05() (vtable +0x18)
    payloadBase[0x22] = g_CurrentLoginMediator->GetUnknownByte05();
  }

  // Build envelope for send
  ::mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope envelope{};
  envelope.payloadBase04 = payloadBase;
  envelope.messageRef08 = packetBuilder.messageRef08;
  const uint32_t sendResult = g_CurrentLoginMediator->SendCurrentMarginPacket(envelope);
  g_CurrentLoginMediator->PostEvent(0x11u);
  // Log version values for diagnostics (dereferenced from pointers)
  const uint32_t loggedLauncherVersion = launcherVersionPtr ? *launcherVersionPtr : 0u;
  const uint32_t loggedClientVersion = clientVersionPtr ? *clientVersionPtr : 0u;
  const auto loggedGobGuid = ResolveState6GobFileGuidWords(g_CurrentLoginMediator);
  const uint8_t loggedHelperPhase = static_cast<uint8_t>(
    g_CurrentLoginMediator->currentState_
    ? g_CurrentLoginMediator->currentState_->DispatchPhaseCode()
    : 0u);
  spdlog::info(
    "CLTLoginState_State6_0x4b508c::Slot3_BeginOrContinue built fixed raw-0x06 margin packet fixedBytes=0x{:02x} launcherVersion=0x{:08x} clientVersion=0x{:08x} gobGuid=[0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x}] helperPhaseByte=0x{:02x} sendResult=0x{:08x} currentState={} then posts event=0x11",
    State6Packet0x06FixedPayload::kFixedByteCount,
    static_cast<unsigned>(loggedLauncherVersion),
    static_cast<unsigned>(loggedClientVersion),
    static_cast<unsigned>(loggedGobGuid[0]),
    static_cast<unsigned>(loggedGobGuid[1]),
    static_cast<unsigned>(loggedGobGuid[2]),
    static_cast<unsigned>(loggedGobGuid[3]),
    static_cast<unsigned>(loggedHelperPhase),
    static_cast<unsigned>(sendResult),
    g_CurrentLoginMediator->currentState_
    ? g_CurrentLoginMediator->currentState_->DebugName()
    : "<null>");
  return;
}

// anchor: launcher.exe:0x440780 (vtable 0x4b508c slot 6)
uint32_t CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
  spdlog::info(
    "DIAGNOSTIC: CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage entered this={} currentState={}",
    fmt::ptr(this),
    (g_CurrentLoginMediator && g_CurrentLoginMediator->currentState_) ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
  if (!g_CurrentLoginMediator) {
    return 0u;
  }

  // anchor: launcher.exe:0x440793 - CMessageConnectionMessageRef_DecodeMessageCode returns opcode in AX
  uint16_t messageCode = 0;
  // Get payload directly from message ref (matching state10 pattern)
  const uint8_t* payloadBytes = workItem->messageStorage0c->PayloadBase();
  const uint16_t payloadByteCount = workItem->PayloadByteCount();
  if (!CMessageConnection_0x4b7928_DecodeMessageCode(*workItem, &messageCode, nullptr)) {
    // Decode failed - original dispatches as opcode 0
  }

  spdlog::debug("CLTLoginState_State6_Slot6: decoded messageCode=0x{:02x} payloadBytes={}",
      static_cast<unsigned>(messageCode), static_cast<unsigned>(payloadByteCount));
  if (!payloadBytes || payloadByteCount == 0u) {
    spdlog::info(
      "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage rejected empty payload");
    return 0u;
  }

  // anchor: launcher.exe:0x44079b - CMP AX, 0x7; JZ opcode7_path
  if (messageCode == 0x07u) {
    // anchor: launcher.exe:0x44079e..0x440a30 - opcode 7 MS_ConnectChallenge handling
    // The original builds and sends a challenge response packet (opcode 0x08)

    if (payloadByteCount < 0x0du) {
      spdlog::info(
        "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage opcode-0x07 payload too short={}",
        static_cast<unsigned>(payloadByteCount));
      return 0u;
    }

    const uint32_t goHereAddr = ReadU32LE(payloadBytes + 0x01u);
    const uint32_t goHerePort = ReadU32LE(payloadBytes + 0x05u);
    const uint32_t sessionSecret = ReadU32LE(payloadBytes + 0x09u);

    spdlog::info(
      "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage opcode-0x07 goHereAddr=0x{:08x} goHerePort=0x{:08x} sessionSecret=0x{:08x}; mirroring original challenge-response flow",
      static_cast<unsigned>(goHereAddr),
      static_cast<unsigned>(goHerePort),
      static_cast<unsigned>(sessionSecret));

    // anchor: launcher.exe:0x43d800 = GenerateClientChunkHashes
    // State6 Slot6 generates client.dll chunk hashes on MS_ConnectChallenge
    g_ClientChunkHashStorage.Clear();

    // Build client.dll path from game directory
    std::string clientDllPath;
    {
      wchar_t exePathW[MAX_PATH] = {};
      GetModuleFileNameW(nullptr, exePathW, MAX_PATH);
      std::wstring exeDirW(exePathW);
      size_t lastSlash = exeDirW.find_last_of(L"\\/");
      if (lastSlash != std::wstring::npos) {
        exeDirW = exeDirW.substr(0, lastSlash);
      }
      clientDllPath.assign(exeDirW.begin(), exeDirW.end());
      if (!clientDllPath.empty() && clientDllPath.back() != '\\' && clientDllPath.back() != '/') {
        clientDllPath += "/";
      }
      clientDllPath += "client.dll";
    }

    std::vector<std::string> clientFiles = {clientDllPath};
    const bool hashGenerationSuccess = GenerateClientChunkHashes(clientFiles);
    if (!hashGenerationSuccess) {
      // anchor: launcher.exe:0x4409e1..0x4409ed - hash generation failure halts login
      g_CurrentLoginMediator->worldListCountOrStatus80 = 0x1200000c;
      g_CurrentLoginMediator->SetCurrentState(0u);
      g_CurrentLoginMediator->PostError(3u);
      return 1u;
    }
    // Hashes are already stored in global g_ClientChunkHashStorage by GenerateClientChunkHashes.
    // Important fidelity correction from launcher.exe:0x440780:
    // - state6 slot6 uses 0x43d800 + 0x4566a0 only for the opcode-0x08 challenge response path
    // - it does NOT overwrite owner +0xcd0..+0xd7f / persisted selection-context blocks here
    // - state8 later serializes the earlier owner-side 0x41c1f0 snapshot directly
    const auto hashes = g_ClientChunkHashStorage.GetHashes();

    spdlog::info("State6 Slot6: Generated {} chunk hashes for client verification", hashes.size());

  // Store for use in challenge-response construction
  g_CurrentLoginMediator->state6UdpSessionSecretF18_ = sessionSecret;
  g_CurrentLoginMediator->postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 = 1u;

  // anchor: launcher.exe:0x43fd20..0x440a30 - Build and send opcode 0x08 challenge response
  // Original constructs packet via vtable +0x68 / SendCurrentMarginPacket at 0x440a18
  // Packet layout: [0x08][statusCode:4][metricIdBase:4][goHereAddr:4][sessionSecret:4]
  // Using Packet_MsConnectChallengeResponse_0x4b5378 builder faithful to static-RE
  Packet_MsConnectChallengeResponse_0x4b5378 packetBuilder;
  packetBuilder.ResetAndInitialize();
  packetBuilder.SetChallengeResponseFields(goHereAddr, sessionSecret);

  // Build envelope for send
  ::mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope envelope{};
  envelope.payloadBase04 = static_cast<uint8_t*>(packetBuilder.payloadAlias10);
  envelope.messageRef08 = packetBuilder.messageRef08;
  const uint32_t sendResult = g_CurrentLoginMediator->SendCurrentMarginPacket(envelope);

  spdlog::info(
      "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage sent opcode-0x08 challenge response goHereAddr=0x{:08x} sessionSecret=0x{:08x} sendResult=0x{:08x}",
      static_cast<unsigned>(goHereAddr), static_cast<unsigned>(sessionSecret),
      static_cast<unsigned>(sendResult));

  // anchor: launcher.exe:0x440a18..0x440a30 - no state transition after sending 0x08
  // In original, state transition happens AFTER receiving the ConnectReply (0x09), not after sending 0x08.

  // Stay in state6 - wait for next message (should be 0x09 ConnectReply)
  return 1u;
  }

  // anchor: launcher.exe:0x440a33 - CMP AX, 0x9; JNZ not_opcode9
  if (messageCode != 0x09u) {
    // anchor: launcher.exe:0x440b7f
    g_CurrentLoginMediator->worldListCountOrStatus80 = 0x12000005;
    return 0u;
  }

  // anchor: launcher.exe:0x440a3a..0x440b81 - opcode 9 handling
  // Original reads directly from payload, no helper struct
  if (payloadByteCount < 0x0fu) {
    spdlog::info(
      "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage opcode-0x09 payload too short={}",
      static_cast<unsigned>(payloadByteCount));
    return 0u;
  }

  // Parse directly from payload (no helper function - all inline in original)
  // anchor: launcher.exe:0x440a40 - reads status from +0x01
  const uint32_t status01 = ReadU32LE(payloadBytes + 0x01u);
  g_CurrentLoginMediator->worldListCountOrStatus80 = status01;

  // anchor: launcher.exe:0x440a4b - tests status != 0
  if (status01 != 0u) {
    // anchor: launcher.exe:0x440af7..0x440b08 - UNCONDITIONALLY call vtable +0x16c first
    // Original at 0x440af7: MOV ECX,[0x004f78b8] -> MOV EAX,[ECX] -> CALL [EAX+0x16c]
    // This calls HandleState9Opcode11SuccessSideEffect_364 unconditionally before checking status
    g_CurrentLoginMediator->HandleState9Opcode11SuccessSideEffect();

    // anchor: launcher.exe:0x440b08..0x440b6d - failure handling AFTER side effect
    // Status values: 0x0b000012 = try next margin, 0x19000001 = auth error, other = error
    if (status01 == 0x0b000012u) {
      // Try next margin address if available
      // anchor: launcher.exe:0x440b0e - compares marginBeginCount24_ < address count
      // Original: (uint)((int)(marginAddressList3c_End - marginAddressList3c_Begin) >> 2)
      const uint32_t marginCount = g_CurrentLoginMediator->marginAddressList3c_.Count();
      const uint32_t currentMarginIndex = g_CurrentLoginMediator->marginBeginCount24_;
      if (currentMarginIndex < marginCount) {
        // anchor: launcher.exe:0x440b18..0x440b32 - store goHereAddr at +0x7c (marginSelectedIpv4_7c_) and switch to state 4
        const uint32_t goHereAddr05 = ReadU32LE(payloadBytes + 0x05u);
        g_CurrentLoginMediator->marginSelectedIpv4_7c_ = goHereAddr05;
        g_CurrentLoginMediator->SetCurrentState(4u);
        return 1u;
      }
    } else if (status01 == 0x19000001u) {
      // Auth error - go to state 2
      // anchor: launcher.exe:0x440b1c..0x440b23 - PUSH 0x2, CALL 0x41b450
      g_CurrentLoginMediator->SetCurrentState(2u);
      return 1u;
    }

    // General error path
    // anchor: launcher.exe:0x440b61..0x440b7e - [ECX+0x24]=0, SetCurrentState(3), PostError(8)
    g_CurrentLoginMediator->marginBeginCount24_ = 0;
    g_CurrentLoginMediator->SetCurrentState(3u);
    g_CurrentLoginMediator->PostError(8u);
    return 1u;
  }

  // anchor: launcher.exe:0x440a5e..0x440ae0 - success path (status == 0)
  g_CurrentLoginMediator->marginBeginCount24_ = 0;
  g_CurrentLoginMediator->ClearLateEntryList1470Scaffold();

  // Original at 0x440a42 calls FUN_0048dbc0 with (DAT_004f9cd4 + 0x200):
  // - Constructs a logrouter hook path string and calls into cls_0x48d020 registration system
  // - This is an internal launcher infrastructure hook, not needed for the replacement path
  // Original at 0x440ab4 calls FUN_0048cfd0 -> meth_0x48cea0:
  // - Iterates internal margin state arrays and calls destructors (meth_0x48e010)
  // - Cleanup of per-connection state; not needed for the self-contained replacement
  // Both functions omitted with comment documenting intentional fidelity gap.

  // anchor: launcher.exe:0x440a60..0x440aa6 - read metric IDs from payload and resolve filenames
  // Original directly accesses payload[0x0d] for metricIdsOffset
  const uint16_t metricIdsOffset0d = ReadU16LE(payloadBytes + 0x0du);
  const uint16_t metricIdCount = (metricIdsOffset0d != 0u && (metricIdsOffset0d + 2u) <= payloadByteCount)
    ? ReadU16LE(payloadBytes + metricIdsOffset0d)
    : 0u;

  uint32_t resolvedMetricFilenameCount = 0u;
  uint32_t unresolvedMetricFilenameCount = 0u;
  for (uint16_t i = 0; i < metricIdCount; ++i) {
    const size_t metricOffset = static_cast<size_t>(metricIdsOffset0d) + 2u + (static_cast<size_t>(i) * sizeof(uint16_t));
    if (metricOffset + sizeof(uint16_t) > payloadByteCount) {
      break;
    }
    const uint16_t metricId = ReadU16LE(payloadBytes + metricOffset);
    const char* const filename = ResolveClientMetricFilenameById(metricId);
    if (filename && filename[0] != '\0') {
      const size_t filenameLength = std::strlen(filename);
      LateEntryList1470EntrySketch metricFilenameStringTriple{};
      metricFilenameStringTriple.begin = const_cast<char*>(filename);
      metricFilenameStringTriple.current = metricFilenameStringTriple.begin + filenameLength;
      metricFilenameStringTriple.capacity = metricFilenameStringTriple.current + 1u;
      g_CurrentLoginMediator->AppendLateEntryStringTriple1470Scaffold(&metricFilenameStringTriple);
      ++resolvedMetricFilenameCount;
      continue;
    }

    ++unresolvedMetricFilenameCount;
    spdlog::info(
      "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage could not resolve opcode-0x09 metricId=0x{:04x} to a Filename through the loaded client METR table",
      static_cast<unsigned>(metricId));
  }

  // anchor: launcher.exe:0x440ab9..0x440ac9 - write owner +0xf14 = 1 and owner +0xf18 = session secret
  const uint32_t udpSessionSecret09 = ReadU32LE(payloadBytes + 0x09u);
  g_CurrentLoginMediator->postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 = 1u;
  g_CurrentLoginMediator->state6UdpSessionSecretF18_ = udpSessionSecret09;

  // anchor: launcher.exe:0x440acc..0x440ae0 - get next helper state from cached upstream vtable+0x18
  if (cachedUpstreamOrArg_0x4 == nullptr) {
    spdlog::info(
      "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage mirrored opcode-0x09 success owner+0xf14=1 owner+0xf18=0x{:08x} metricIdCount={} resolvedMetricFilenameCount={} unresolvedMetricFilenameCount={} but has no cached upstream helper at this+4 yet; leaving helper-switch/event-0x12 to the broader caller flow currentState={}",
      static_cast<unsigned>(udpSessionSecret09),
      static_cast<unsigned>(metricIdCount),
      resolvedMetricFilenameCount,
      unresolvedMetricFilenameCount,
      g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
    return 1u;
  }

  const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
  const uint32_t switchDispatchResult = g_CurrentLoginMediator->SetCurrentState(nextHelperStateId);

  // anchor: launcher.exe:0x440ae1 - PostEvent(0x12)
  g_CurrentLoginMediator->PostEvent(0x12u);

  const uint32_t goHereAddr05 = ReadU32LE(payloadBytes + 0x05u);
  spdlog::info(
    "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage opcode-0x09 success wrote owner+0xf14=1 owner+0xf18=0x{:08x} and re-entered helperState=0x{:02x} via 0x41b450 oldState=state6 semantics switchDispatchResult=0x{:08x}",
    static_cast<unsigned>(udpSessionSecret09),
    static_cast<unsigned>(nextHelperStateId),
    static_cast<unsigned>(switchDispatchResult));

  spdlog::info(
    "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage mirrored opcode-0x09 success status=0x{:08x} goHereAddr=0x{:08x} udpSessionSecret=0x{:08x} metricIdCount={} resolvedMetricFilenameCount={} unresolvedMetricFilenameCount={} metricIds={} -> nextHelperState={} currentState={}",
    static_cast<unsigned>(status01),
    static_cast<unsigned>(goHereAddr05),
    static_cast<unsigned>(udpSessionSecret09),
    static_cast<unsigned>(metricIdCount),
    resolvedMetricFilenameCount,
    unresolvedMetricFilenameCount,
    BuildMetricIdPreview(reinterpret_cast<const uint16_t*>(payloadBytes + metricIdsOffset0d + 2), metricIdCount),
    static_cast<unsigned>(nextHelperStateId),
    g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");

  return 1u;
}

// anchor: launcher.exe:0x438c70 (vtable 0x4b508c slot 7)
uint32_t CLTLoginState_State6_0x4b508c::GetStateId() const {
  return 6;
}

} // namespace mxo::ltlogin
