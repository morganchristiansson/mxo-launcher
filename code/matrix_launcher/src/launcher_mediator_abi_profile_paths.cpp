// Focused ILTLoginMediator.Default profile-path / current-slot / selection-descriptor surface.
// Included by `src/launcher_mediator_abi.cpp`; relies on shared ABI-shell state/helpers
// already defined there.

struct __attribute__((packed)) DiagnosticMediatorSelectionPacked {
    uint8_t reserved0;
    uint8_t reserved1;
    uint8_t reserved2;
    uint32_t field03; // read by client as dword at +0x03
    uint32_t field07; // read by client as dword at +0x07
};

struct DiagnosticMediatorSelectionObject {
    uint8_t reserved[0x10];
    DiagnosticMediatorSelectionPacked* packed; // read by client as dword at +0x10
};

static_assert(offsetof(DiagnosticMediatorSelectionPacked, field03) == 0x03);
static_assert(offsetof(DiagnosticMediatorSelectionPacked, field07) == 0x07);
static_assert(offsetof(DiagnosticMediatorSelectionObject, packed) == 0x10);

static DiagnosticMediatorSelectionPacked g_MediatorSelectionPacked = {0, 0, 0, 0u, 0u};
static DiagnosticMediatorSelectionObject g_MediatorSelectionObject = {};

struct __attribute__((packed)) DiagnosticMediatorCurrentSlotRecordPayload {
    uint8_t reserved0;
    uint8_t reserved1;
    uint8_t reserved2;
    uint32_t characterIdLow03;  // payload `+0x03`
    uint32_t characterIdHigh07; // payload `+0x07`
    uint8_t status0b;           // payload `+0x0b`
    uint16_t worldId0c;         // payload `+0x0c`
    uint8_t reserved0e;
    uint8_t reserved0f;
};

struct DiagnosticMediatorCurrentSlotRecordObject {
    void** vtable;                                      // object `+0x00`
    void* bufferBase04;                                 // object `+0x04`
    void* backingObject08;                              // object `+0x08`
    uint8_t flag0c;                                     // object `+0x0c`
    uint8_t padding0d[3];
    DiagnosticMediatorCurrentSlotRecordPayload* payload10; // object `+0x10`
    const char* heapString14;                           // object `+0x14`
    uint16_t heapStringLen18;                           // object `+0x18`
    uint8_t padding1a[2];
};

static_assert(offsetof(DiagnosticMediatorCurrentSlotRecordPayload, characterIdLow03) == 0x03);
static_assert(offsetof(DiagnosticMediatorCurrentSlotRecordPayload, characterIdHigh07) == 0x07);
static_assert(offsetof(DiagnosticMediatorCurrentSlotRecordPayload, status0b) == 0x0b);
static_assert(offsetof(DiagnosticMediatorCurrentSlotRecordPayload, worldId0c) == 0x0c);
static_assert(offsetof(DiagnosticMediatorCurrentSlotRecordObject, payload10) == 0x10);
static_assert(offsetof(DiagnosticMediatorCurrentSlotRecordObject, heapString14) == 0x14);
static_assert(offsetof(DiagnosticMediatorCurrentSlotRecordObject, heapStringLen18) == 0x18);
static_assert(sizeof(DiagnosticMediatorCurrentSlotRecordObject) == 0x1c);

static DiagnosticMediatorCurrentSlotRecordPayload g_MediatorCurrentSlotRecordPayload = {0, 0, 0, 0, 0, 0, 0, 0, 0};
static DiagnosticMediatorCurrentSlotRecordObject g_MediatorCurrentSlotRecordObject = {};
static void* g_MediatorCurrentSlotRecordVtable[5] = {0};
static std::string g_MediatorCurrentSlotRecordNameOwned;

// anchor: client.dll profile-root formatting path uses arg6 +0x38 for Profiles\%s\... construction
// vtable: ILTLoginMediator.Default slot +0x38
static const char* __thiscall Mediator_GetDisplayName(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetProfileRootName();
}

static bool DiagnosticMediatorWorldIndexMatchesConfiguredSelection(uint32_t worldIndex);
static void PopulateMediatorCurrentSlotRecordObject();

// UNANCHORED: scaffold selection-resolution helpers layered over the CLTLoginMediator sidecar model.
static const char* DiagnosticMediatorWorldNameForIndex(uint32_t worldIndex) {
    if (worldIndex >= DiagnosticMediatorWorldUpperBoundExclusive()) {
        return NULL;
    }
    if (!DiagnosticMediatorWorldIndexMatchesConfiguredSelection(worldIndex)) {
        return NULL;
    }
    return DiagnosticMediatorMappedSelectionName();
}

static bool DiagnosticMediatorWorldIndexMatchesConfiguredSelection(uint32_t worldIndex) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6WorldIndexMatchesSelection(worldIndex) : false;
}

static uint32_t DiagnosticMediatorExpectedSelectionDescriptorScratchRequest() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6ExpectedSelectionDescriptorScratchRequest() : 0u;
}

static bool DiagnosticMediatorSelectionDescriptorMatchesConfiguredRequest(uint32_t selectionIndex) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6SelectionDescriptorMatchesRequest(selectionIndex) : false;
}

// anchor: client.dll fallback-selection path asks arg6 +0x3c for the default selection index when given 0xff
// vtable: ILTLoginMediator.Default slot +0x3c
static uint32_t __thiscall Mediator_GetDefaultSelectionIndex(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetDefaultSelectionIndex();
}

// anchor: client.dll:0x62170dc1..0x62170e59 later asks arg6 +0x40 with the scratch-shaped arg7 request
// vtable: ILTLoginMediator.Default slot +0x40
static void* __thiscall Mediator_GetSelectionDescriptor(MinimalLoginMediatorStub* self, uint32_t selectionIndex) {
    (void)self;

    void* returnAddress = __builtin_return_address(0);
    const uint32_t low24 = selectionIndex & 0x00ffffffu;
    const uint32_t high8 = (selectionIndex >> 24) & 0xffu;
    const uint32_t expectedScratchRequest = DiagnosticMediatorExpectedSelectionDescriptorScratchRequest();
    const bool matchedConfiguredRequest = DiagnosticMediatorSelectionDescriptorMatchesConfiguredRequest(selectionIndex);
    const char* worldName = matchedConfiguredRequest ? DiagnosticMediatorMappedSelectionName() : NULL;

    if (!worldName) {
        spdlog::debug(
            "MediatorStub::GetSelectionDescriptor(selectionIndex=0x{:08x} low24=0x{:06x} high8=0x{:02x} caller={} [{}]) -> NULL (configuredWorld=0x{:06x} configuredVariant=0x{:02x} expectedScratchRequest=0x{:08x} worldUpperBoundExclusive={})",
            static_cast<unsigned>(selectionIndex),
            static_cast<unsigned>(low24),
            static_cast<unsigned>(high8),
            fmt::ptr(returnAddress),
            DescribeMediatorCaller(returnAddress),
            static_cast<unsigned>(DiagnosticMediatorSelectedWorldIndexLow24()),
            static_cast<unsigned>(DiagnosticMediatorSelectedVariantIndexHigh8()),
            static_cast<unsigned>(expectedScratchRequest),
            static_cast<unsigned>(DiagnosticMediatorWorldUpperBoundExclusive()));
        LogMediatorCharacterStateContext("GetSelectionDescriptor(+0x40)", returnAddress);
        return NULL;
    }

    PopulateMediatorCurrentSlotRecordObject();

    const bool profilePathCaller = IsProfilePathBuilderCaller(returnAddress);
    const char* descriptorShape = "world-shaped";
    uint32_t field03 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(worldName));
    uint32_t field07 = DiagnosticMediatorMappedSelectionId();
    if (profilePathCaller) {
        descriptorShape = "current-slot-id-shaped";
        field03 = g_MediatorCurrentSlotRecordPayload.characterIdLow03;
        field07 = g_MediatorCurrentSlotRecordPayload.characterIdHigh07;
        if (field03 == 0u && field07 == 0u) {
            field03 = DiagnosticAuthCurrentCharacterIdLow();
            field07 = DiagnosticAuthCurrentCharacterIdHigh();
        }
    }

    g_MediatorSelectionPacked.field03 = field03;
    g_MediatorSelectionPacked.field07 = field07;
    g_MediatorSelectionObject.packed = &g_MediatorSelectionPacked;

    const char* matchMode =
        (selectionIndex == expectedScratchRequest) ? "arg7-scratch-shape" :
        ((low24 == DiagnosticMediatorSelectedWorldIndexLow24()) ? "low24-world-match" : "other-match");
    spdlog::debug(
        "MediatorStub::GetSelectionDescriptor(selectionIndex=0x{:08x} low24=0x{:06x} high8=0x{:02x} caller={} [{}]) -> {} (matchMode={} descriptorShape={} mappedName='{}' field03=0x{:08x} field07=0x{:08x} field03AsPtr={} configuredWorld=0x{:06x} configuredVariant=0x{:02x} expectedScratchRequest=0x{:08x})",
        static_cast<unsigned>(selectionIndex),
        static_cast<unsigned>(low24),
        static_cast<unsigned>(high8),
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress),
        fmt::ptr(&g_MediatorSelectionObject),
        matchMode,
        descriptorShape,
        worldName,
        static_cast<unsigned>(g_MediatorSelectionPacked.field03),
        static_cast<unsigned>(g_MediatorSelectionPacked.field07),
        fmt::ptr(reinterpret_cast<const void*>(static_cast<uintptr_t>(g_MediatorSelectionPacked.field03))),
        static_cast<unsigned>(DiagnosticMediatorSelectedWorldIndexLow24()),
        static_cast<unsigned>(DiagnosticMediatorSelectedVariantIndexHigh8()),
        static_cast<unsigned>(expectedScratchRequest));
    LogMediatorCharacterStateContext("GetSelectionDescriptor(+0x40)", returnAddress);
    return &g_MediatorSelectionObject;
}

static uint32_t __thiscall MediatorCurrentSlotRecord_Destroy(DiagnosticMediatorCurrentSlotRecordObject* self) {
    (void)self;
    return 1u;
}

static uint32_t __thiscall MediatorCurrentSlotRecord_TinyGetter(DiagnosticMediatorCurrentSlotRecordObject* self) {
    (void)self;
    return 0u;
}

static uint32_t __thiscall MediatorCurrentSlotRecord_AppendDebugString(DiagnosticMediatorCurrentSlotRecordObject* self) {
    (void)self;
    return 1u;
}

static uint32_t __thiscall MediatorCurrentSlotRecord_ResetPayloadForSourceDescriptor(DiagnosticMediatorCurrentSlotRecordObject* self) {
    (void)self;
    return 1u;
}

static uint32_t __thiscall MediatorCurrentSlotRecord_TinyHelper(DiagnosticMediatorCurrentSlotRecordObject* self) {
    (void)self;
    return 0u;
}

static void PopulateMediatorCurrentSlotRecordObject() {
    g_MediatorCurrentSlotRecordPayload = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    g_MediatorCurrentSlotRecordObject = {};
    g_MediatorCurrentSlotRecordObject.vtable = g_MediatorCurrentSlotRecordVtable;
    g_MediatorCurrentSlotRecordObject.payload10 = &g_MediatorCurrentSlotRecordPayload;
    g_MediatorCurrentSlotRecordNameOwned.clear();

    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const mxo::ltlogin::SlotRecordState004b5328* currentSlotRecord =
        mediator ? mediator->GetCurrentSlotRecord() : nullptr;
    if (!currentSlotRecord && mediator) {
        currentSlotRecord = mediator->GetSlotRecordByIndex(0u);
    }

    if (currentSlotRecord) {
        g_MediatorCurrentSlotRecordPayload.characterIdLow03 = currentSlotRecord->globalCharacterIdLow03;
        g_MediatorCurrentSlotRecordPayload.characterIdHigh07 = currentSlotRecord->globalCharacterIdHigh07;
        g_MediatorCurrentSlotRecordPayload.status0b = currentSlotRecord->status0b;
        g_MediatorCurrentSlotRecordPayload.worldId0c = currentSlotRecord->worldId0c;
        g_MediatorCurrentSlotRecordNameOwned = currentSlotRecord->heapString14;
    } else {
        g_MediatorCurrentSlotRecordPayload.characterIdLow03 = DiagnosticAuthCurrentCharacterIdLow();
        g_MediatorCurrentSlotRecordPayload.characterIdHigh07 = DiagnosticAuthCurrentCharacterIdHigh();
        const char* authCharacterName = DiagnosticAuthCurrentCharacterName();
        if (authCharacterName && authCharacterName[0]) {
            g_MediatorCurrentSlotRecordNameOwned = authCharacterName;
        }
    }

    if (!g_MediatorCurrentSlotRecordNameOwned.empty()) {
        g_MediatorCurrentSlotRecordObject.heapString14 = g_MediatorCurrentSlotRecordNameOwned.c_str();
        const size_t nameLength = g_MediatorCurrentSlotRecordNameOwned.size();
        g_MediatorCurrentSlotRecordObject.heapStringLen18 =
            static_cast<uint16_t>((nameLength < 0xffffu) ? nameLength : 0xffffu);
    }
}

// anchor: launcher.exe:0x41f300
// vtable: ILTLoginMediator.Default slot +0x44
// Current mcd.cfg crash stopper: `client.dll:0x62197560` only gates on non-null here before the
// later dirty-corpus save walk continues into the actual save-enable/save-writer chain.
static void* __thiscall Mediator_GetCurrentSlotRecord44(MinimalLoginMediatorStub* self) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    PopulateMediatorCurrentSlotRecordObject();

    const bool hasCurrentSlot =
        g_MediatorCurrentSlotRecordObject.heapString14 != nullptr ||
        g_MediatorCurrentSlotRecordPayload.characterIdLow03 != 0u ||
        g_MediatorCurrentSlotRecordPayload.characterIdHigh07 != 0u;
    const void* currentSlotRecordPtr = hasCurrentSlot
        ? static_cast<const void*>(&g_MediatorCurrentSlotRecordObject)
        : nullptr;

    spdlog::info(
        "MediatorStub::GetCurrentSlotRecord(+0x44) caller={} [{}] -> {} [name='{}' idLow=0x{:08x} idHigh=0x{:08x} status=0x{:02x} worldId=0x{:04x}]",
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress),
        fmt::ptr(currentSlotRecordPtr),
        g_MediatorCurrentSlotRecordObject.heapString14 ? g_MediatorCurrentSlotRecordObject.heapString14 : "<empty>",
        static_cast<unsigned>(g_MediatorCurrentSlotRecordPayload.characterIdLow03),
        static_cast<unsigned>(g_MediatorCurrentSlotRecordPayload.characterIdHigh07),
        static_cast<unsigned>(g_MediatorCurrentSlotRecordPayload.status0b),
        static_cast<unsigned>(g_MediatorCurrentSlotRecordPayload.worldId0c));
    LogMediatorCharacterStateContext("GetCurrentSlotRecord(+0x44)", returnAddress);
    return hasCurrentSlot ? &g_MediatorCurrentSlotRecordObject : nullptr;
}

// anchor: later client startup path calls arg6 +0x48 before the now-better-understood
// observer registration / startup-triple handoff sequence
// practical current note from client.dll profile-path work:
// - the broader client path later formats `Profiles\%s\%s_%X\`
// - and the middle `%s` is sourced from client-global `DAT_629de48c`
// - current replacement evidence points to the earlier +0x48-fed name path as the highest-value
//   narrow source to keep character-shaped instead of world-shaped on the active route
// vtable: ILTLoginMediator.Default slot +0x48
static const char* __thiscall Mediator_GetWorldOrSelectionName(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetWorldOrSelectionName();
}

// anchor: later client startup path calls arg6 +0x4c immediately after +0x48
// vtable: ILTLoginMediator.Default slot +0x4c
static const char* __thiscall Mediator_GetProfileOrSessionName(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetProfileOrSessionName();
}
