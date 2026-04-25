#include <spdlog/spdlog.h>
#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "client_chunk_hashes.h"
#include "../../../../src/diagnostics.h"

#include <cstring>
#include <algorithm>
#include <sstream>
#include <string>

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b5104
const char* CLTLoginState_State8_0x4b5104::DebugName() const {
  return "CLTLoginState_State8_0x4b5104";
}

// anchor: launcher.exe:0x43bd20 (vtable 0x4b5104 slot 3)
void CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
  (void)upstreamOrArg;
  if (!g_CurrentLoginMediator) {
    return;
  }

  if (!g_CurrentLoginMediator->State10HasReadyConnectionState2()) {
    const uint32_t fallbackResult = g_CurrentLoginMediator->SetCurrentState(4u);
    return;
  }
  if (g_CurrentLoginMediator->postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 == 0) {
    const uint32_t fallbackResult = g_CurrentLoginMediator->SetCurrentState(6u);
    return;
  }

  const SlotRecordState_0x4b5328* currentSlotRecord = g_CurrentLoginMediator->GetCurrentSlotRecord();

  Packet_MsLoadCharacterRequest_0x4b5418 packetBuilder;
  packetBuilder.ResetAndInitialize();

  uint8_t* payload = static_cast<uint8_t*>(packetBuilder.payloadAlias10);
  if (payload) {
    *reinterpret_cast<uint32_t*>(payload + 0x01) = currentSlotRecord ? currentSlotRecord->characterIdLow32 : 0u;
    *reinterpret_cast<uint32_t*>(payload + 0x05) = currentSlotRecord ? currentSlotRecord->characterIdHigh36 : 0u;
  }

  ::mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope envelope{};
  envelope.payloadBase04 = payload;
  envelope.messageRef08 = packetBuilder.messageRef08;
  const uint32_t sendResult = g_CurrentLoginMediator->SendCurrentMarginPacket(envelope);
  g_CurrentLoginMediator->PostEvent(0x09u);
}

// anchor: launcher.exe:0x43f930 (vtable 0x4b5104 slot 6)
uint32_t CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage(
    mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
  if (!g_CurrentLoginMediator) {
    return 0u;
  }

  // anchor: launcher.exe:0x43f933 = DecodeMessageCode
  uint16_t messageCode = 0;
  if (!CMessageConnection_0x4b7928_DecodeMessageCode(*workItem, &messageCode, nullptr)) {
    // Decode failure path
  }

  // anchor: launcher.exe:0x43f93a - CMP AX, 0x10; JNZ non-0x10 path
  if (messageCode != 0x10u) {
    const uint32_t fallbackResult = g_CurrentLoginMediator->DispatchSecondaryMessageToOwnerCallback84(workItem);
    if (fallbackResult == 0u) {
      g_CurrentLoginMediator->worldListCountOrStatus80 = 0x12000005u;
      return 0u;
    }
    return 1u;
  }

  // anchor: launcher.exe:0x43f943 - LoadCharacterReplyEnvelope_0x4b542c::LoadCharacterReplyEnvelope
  LoadCharacterReplyEnvelope_0x4b542c loadCharacterReplyEnvelope(workItem, 1);
  if (!loadCharacterReplyEnvelope.valid) {
    const uint32_t fallbackResult = g_CurrentLoginMediator->DispatchSecondaryMessageToOwnerCallback84(workItem);
    if (fallbackResult < 1u) {
      g_CurrentLoginMediator->worldListCountOrStatus80 = 0x12000005u;
      return 0u;
    }
    return 1u;
  }

  auto& ownerState = g_CurrentLoginMediator->postAuthMarginLoadingState_0xf14;
  g_CurrentLoginMediator->worldListCountOrStatus80 = loadCharacterReplyEnvelope.status;

  // anchor: launcher.exe:0x43f964 - TEST EAX, EAX; JS failure path
  if (loadCharacterReplyEnvelope.status >= 1u) {
    g_CurrentLoginMediator->SetCurrentState(3u);
    g_CurrentLoginMediator->PostError(10u);
    return 1u;
  }

  // anchor: launcher.exe:0x43f993 - first-fragment check: this[4] == 0
  // The original checks if state tracking indicates first fragment
  // For fidelity, check if section0 hasn't been seen yet
  const bool firstFragment = (ownerState.section0Flag13f6 == 0u);
  if (firstFragment) {
    // anchor: launcher.exe:0x438a50 = ResetReplyState
    ownerState.state8PersistenceDataF1c = {};
    std::fill(std::begin(ownerState.characterNameBufferF1c), std::end(ownerState.characterNameBufferF1c), '\0');
    ownerState.characterReplyFieldF3c = 0u;
    ownerState.characterReplyFieldF40 = 0u;
    ownerState.characterReplyFieldF44 = 0x1000u;
    std::fill(ownerState.characterFlagsF48.begin(), ownerState.characterFlagsF48.end(), 0u);
    std::fill(ownerState.secondaryCharacterDataF68.begin(), ownerState.secondaryCharacterDataF68.end(), 0u);
    std::fill(ownerState.characterRecordPointersF88.begin(), ownerState.characterRecordPointersF88.end(), 0u);
    std::fill(ownerState.section0StringF8c.begin(), ownerState.section0StringF8c.end(), '\0');
    std::fill(ownerState.section0StringFac.begin(), ownerState.section0StringFac.end(), '\0');
    std::fill(ownerState.section0StringFcc.begin(), ownerState.section0StringFcc.end(), '\0');
    std::fill(ownerState.state8Section0RawF88.begin(), ownerState.state8Section0RawF88.end(), 0u);
    ownerState.replySectionData13cc = 0u;
    ownerState.replySectionData13d0 = 0u;
    ownerState.section0Flag13f6 = 0u;

    // Reset allocated buffers
    if (ownerState.state8Section0OverflowBuffer13f0) {
      std::free(ownerState.state8Section0OverflowBuffer13f0);
      ownerState.state8Section0OverflowBuffer13f0 = nullptr;
    }
    ownerState.state8Section0OverflowLength13f4 = 0u;

    // Initialize from first fragment
    ownerState.characterReplyFieldF3c = loadCharacterReplyEnvelope.field05;
    ownerState.state8PersistenceDataF1c.replyField20 = loadCharacterReplyEnvelope.field05;

    const SlotRecordState_0x4b5328* currentSlotRecord = g_CurrentLoginMediator->GetCurrentSlotRecord();
    if (currentSlotRecord != nullptr && currentSlotRecord->debugString14) {
      const size_t copyCount = std::min(
        std::strlen(currentSlotRecord->debugString14),
        sizeof(ownerState.characterNameBufferF1c) - 1u);
      std::copy_n(currentSlotRecord->debugString14, copyCount, ownerState.characterNameBufferF1c);
      ownerState.characterNameBufferF1c[copyCount] = '\0';
      ownerState.characterReplyFieldF40 = currentSlotRecord->worldId3c;
      ownerState.secondaryCharacterDataF68[0] = currentSlotRecord->worldId3c;
      ownerState.secondaryCharacterDataF68[1] = currentSlotRecord->status3a;
    }
  }

  auto& persistence = ownerState.state8PersistenceDataF1c;

  // anchor: launcher.exe:0x43fa55..0x4408ea - switch on section selector
  const uint16_t sectionIndex = loadCharacterReplyEnvelope.sectionSelectorMinus2;

  switch (sectionIndex) {
    case 0u:
      // Section 0
      if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
        const size_t fixedPrefix = std::min<size_t>(loadCharacterReplyEnvelope.sectionByteCount, 0x20u);
        if (fixedPrefix != 0u) {
          std::memcpy(ownerState.characterFlagsF48.data(), loadCharacterReplyEnvelope.sectionData, fixedPrefix);
          std::memcpy(persistence.header2c.data(), loadCharacterReplyEnvelope.sectionData, fixedPrefix);
        }
        if (loadCharacterReplyEnvelope.sectionByteCount > 0x20u) {
          const size_t bodyBytes = std::min(
            loadCharacterReplyEnvelope.sectionByteCount - 0x20u,
            ownerState.state8Section0RawF88.size());
          std::memcpy(ownerState.state8Section0RawF88.data(), loadCharacterReplyEnvelope.sectionData + 0x20u, bodyBytes);
        }
        if (loadCharacterReplyEnvelope.sectionByteCount >= 4u) {
          ownerState.characterRecordPointersF88[0] = ReadU32LE(loadCharacterReplyEnvelope.sectionData + 0x00u);
        }
        ownerState.section0Flag13f6 = 1u;
        persistence.section0PresentFlag4da = 1u;
      }
      break;

    case 1u:
    case 2u:
    case 3u:
    case 4u:
    case 5u:
    case 6u:
    case 7u:
    case 8u:
      // Sections 1-8: accumulate data
      break;

    case 9u:
      // Section 10 (chunked)
      break;

    case 10u:
      // Section 11
      if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount > 4u) {
        ownerState.state8Section11Dword145c = ReadU32LE(loadCharacterReplyEnvelope.sectionData);
        ownerState.state8Section11String1460.assign(
          reinterpret_cast<const char*>(loadCharacterReplyEnvelope.sectionData + 4u),
          loadCharacterReplyEnvelope.sectionByteCount - 4u);
      }
      break;

    default:
      break;
  }

  // anchor: launcher.exe:0x4408ed - completion check
  const bool haveSection0 = (ownerState.section0Flag13f6 != 0u);
  const bool isComplete = haveSection0;

  if (isComplete) {
    if (auto* nextState = dynamic_cast<CLTLoginState_State9_0x4b517c*>(
        g_CurrentLoginMediator->LoginHelperStateByIdScaffold(9u))) {
      nextState->SetPendingPayload(0, loadCharacterReplyEnvelope.handoffWord09);
    }
    g_CurrentLoginMediator->SetCurrentState(9u);
    g_CurrentLoginMediator->PostEvent(0x0bu);
  }

  return 1u;
}

// anchor: launcher.exe:0x438c90 (vtable 0x4b5104 slot 7)
uint32_t CLTLoginState_State8_0x4b5104::GetStateId() const {
  return 8;
}

} // namespace mxo::ltlogin
