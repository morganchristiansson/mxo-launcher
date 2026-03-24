// Focused ILTLoginMediator.Default mcd/persistence surface.
// Included by `src/launcher_mediator_abi.cpp`; relies on shared ABI-shell state/helpers
// already defined there.

using DiagnosticMediatorProfileCharacterInfoF4 =
    mxo::ltlogin::ProcessLoginCredentialsInputSketch;

static constexpr size_t kDiagnosticMediatorState8BodyF88Size = 0x465;
static constexpr size_t kDiagnosticMediatorState8OverflowMax13f0 = 0x1000;

struct DiagnosticMediatorState8PersistenceF1c {
    std::array<char, 0x20> string00{};               // owner `+0xf1c .. +0xf3b`
    uint32_t field20 = 0;                            // owner `+0xf3c`
    uint32_t field24 = 0;                            // owner `+0xf40`
    uint32_t field28 = 0;                            // owner `+0xf44`
    std::array<uint32_t, 8> header2c{};             // owner `+0xf48 .. +0xf67`
    std::array<uint32_t, 8> secondary4c{};          // owner `+0xf68 .. +0xf87`
    std::array<uint8_t, kDiagnosticMediatorState8BodyF88Size> body6c{}; // owner `+0xf88 .. +0x13ec`
};

static_assert(
    sizeof(mxo::ltlogin::State3SelectionContextInputSketch) == kDiagnosticSelectionContextSize,
    "State3SelectionContextInputSketch must stay layout-compatible with the recovered arg6 +0xec 0xb4 snapshot");
static_assert(offsetof(DiagnosticMediatorState8PersistenceF1c, string00) == 0x00);
static_assert(offsetof(DiagnosticMediatorState8PersistenceF1c, field20) == 0x20);
static_assert(offsetof(DiagnosticMediatorState8PersistenceF1c, field24) == 0x24);
static_assert(offsetof(DiagnosticMediatorState8PersistenceF1c, field28) == 0x28);
static_assert(offsetof(DiagnosticMediatorState8PersistenceF1c, header2c) == 0x2c);
static_assert(offsetof(DiagnosticMediatorState8PersistenceF1c, secondary4c) == 0x4c);
static_assert(offsetof(DiagnosticMediatorState8PersistenceF1c, body6c) == 0x6c);

static DiagnosticMediatorProfileCharacterInfoF4 g_MediatorProfileCharacterInfoF4 = {};
static DiagnosticMediatorState8PersistenceF1c g_MediatorState8PersistenceF1c = {};
static std::array<uint8_t, kDiagnosticMediatorState8OverflowMax13f0> g_MediatorState8Overflow13f0 = {};
static uint16_t g_MediatorState8Overflow13f4 = 0u;
static std::string g_MediatorState8Section11String1460Owned;
static DiagnosticSmallStringLike g_MediatorState8Section11String1460 = {};

template <size_t N>
static void CopyCStringIntoFixed(std::array<char, N>& dest, const char* src) {
    dest.fill('\0');
    if (!src || !src[0]) {
        return;
    }
    const size_t sourceLength = std::char_traits<char>::length(src);
    const size_t copyCount = (sourceLength < (N - 1u)) ? sourceLength : (N - 1u);
    std::memcpy(dest.data(), src, copyCount);
    dest[copyCount] = '\0';
}

static void CopyCStringIntoByteSpan(uint8_t* dest, size_t destSize, const char* src) {
    if (!dest || destSize == 0u) {
        return;
    }

    std::memset(dest, 0, destSize);
    if (!src || !src[0]) {
        return;
    }

    const size_t sourceLength = std::char_traits<char>::length(src);
    const size_t copyCount = (sourceLength < (destSize - 1u)) ? sourceLength : (destSize - 1u);
    std::memcpy(dest, src, copyCount);
    dest[copyCount] = '\0';
}

static const char* PreferNonEmpty(const char* primary, const char* fallback) {
    return (primary && primary[0]) ? primary : fallback;
}

static void PopulateMediatorProfileCharacterInfoF4() {
    g_MediatorProfileCharacterInfoF4 = {};

    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const char* characterName = nullptr;
    const char* realFirstName = nullptr;
    const char* realLastName = nullptr;
    const char* background = nullptr;

    if (mediator) {
        if (const auto* currentSlotRecord = mediator->GetCurrentSlotRecord()) {
            if (!currentSlotRecord->heapString14.empty()) {
                characterName = currentSlotRecord->heapString14.c_str();
            }
        }
        characterName = PreferNonEmpty(characterName, mediator->GetSlotRecordHeapStringByIndex(0));
        characterName = PreferNonEmpty(characterName, mediator->CharacterNameBufferF1c());
        characterName = PreferNonEmpty(characterName, mediator->SourceLeadString108().data());

        const auto& ownerState = mediator->PostAuthMarginLoadingStateView();
        const char* sourceBlock178 = reinterpret_cast<const char*>(mediator->SourceBlock178().data());
        const char* sourceBlock198 = reinterpret_cast<const char*>(mediator->SourceBlock198().data());
        const char* sourceBlock1b8 = reinterpret_cast<const char*>(mediator->SourceBlock1b8().data());
        const char* section0F8c = ownerState.section0StringF8c[0] ? ownerState.section0StringF8c.data() : nullptr;
        const char* section0Fac = ownerState.section0StringFac[0] ? ownerState.section0StringFac.data() : nullptr;
        const char* section0Fcc = ownerState.section0StringFcc[0] ? ownerState.section0StringFcc.data() : nullptr;
        const bool section0LooksLikeMiddleFirstLast =
            section0F8c != nullptr &&
            std::char_traits<char>::length(section0F8c) == 1u &&
            section0Fac != nullptr && section0Fac[0] != '\0' &&
            section0Fcc != nullptr && section0Fcc[0] != '\0';

        if (section0LooksLikeMiddleFirstLast) {
            realFirstName = PreferNonEmpty(sourceBlock178, section0Fac);
            realLastName = PreferNonEmpty(sourceBlock198, section0Fcc);
            background = PreferNonEmpty(sourceBlock1b8, nullptr);
        } else {
            realFirstName = PreferNonEmpty(sourceBlock178, section0F8c);
            realLastName = PreferNonEmpty(sourceBlock198, section0Fac);
            background = PreferNonEmpty(sourceBlock1b8, section0Fcc);
        }

        g_MediatorProfileCharacterInfoF4.field24 = mediator->SourceField12c();
    }

    characterName = PreferNonEmpty(characterName, DiagnosticAuthCurrentCharacterName());
    realFirstName = PreferNonEmpty(realFirstName, DiagnosticAuthCurrentRealFirstName());
    realLastName = PreferNonEmpty(realLastName, DiagnosticAuthCurrentRealLastName());
    background = PreferNonEmpty(background, DiagnosticAuthCurrentBackground());

    if ((!realFirstName || !realFirstName[0]) && characterName && characterName[0]) {
        realFirstName = characterName;
    }

    CopyCStringIntoFixed(g_MediatorProfileCharacterInfoF4.string00, characterName);
    CopyCStringIntoFixed(g_MediatorProfileCharacterInfoF4.string70, realFirstName);
    CopyCStringIntoFixed(g_MediatorProfileCharacterInfoF4.string90, realLastName);
    CopyCStringIntoFixed(g_MediatorProfileCharacterInfoF4.stringB0, background);

    if (g_MediatorProfileCharacterInfoF4.field24 == 0u) {
        g_MediatorProfileCharacterInfoF4.field24 = DiagnosticMediatorSelectedWorldIndexLow24();
    }
}

static void PopulateMediatorState8PersistenceF1c() {
    PopulateMediatorProfileCharacterInfoF4();
    g_MediatorState8PersistenceF1c = {};
    g_MediatorState8Overflow13f0.fill(0u);
    g_MediatorState8Overflow13f4 = 0u;
    g_MediatorState8Section11String1460Owned.clear();
    g_MediatorState8Section11String1460 = {};

    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const char* characterName = g_MediatorProfileCharacterInfoF4.string00.data();
    const char* firstName = g_MediatorProfileCharacterInfoF4.string70.data();
    const char* lastName = g_MediatorProfileCharacterInfoF4.string90.data();
    const char* background = g_MediatorProfileCharacterInfoF4.stringB0.data();

    if (!mediator) {
        CopyCStringIntoFixed(g_MediatorState8PersistenceF1c.string00, characterName);
        g_MediatorState8PersistenceF1c.field24 = g_MediatorProfileCharacterInfoF4.field24;
        CopyCStringIntoByteSpan(g_MediatorState8PersistenceF1c.body6c.data() + 0x04, 0x20, firstName);
        CopyCStringIntoByteSpan(g_MediatorState8PersistenceF1c.body6c.data() + 0x24, 0x20, lastName);
        CopyCStringIntoByteSpan(g_MediatorState8PersistenceF1c.body6c.data() + 0x44, 0x400, background);
        return;
    }

    const auto& ownerState = mediator->PostAuthMarginLoadingStateView();
    characterName = PreferNonEmpty(ownerState.characterNameBufferF1c, characterName);
    firstName = PreferNonEmpty(g_MediatorProfileCharacterInfoF4.string70.data(), firstName);
    lastName = PreferNonEmpty(g_MediatorProfileCharacterInfoF4.string90.data(), lastName);
    background = PreferNonEmpty(g_MediatorProfileCharacterInfoF4.stringB0.data(), background);

    CopyCStringIntoFixed(g_MediatorState8PersistenceF1c.string00, characterName);
    g_MediatorState8PersistenceF1c.field20 = ownerState.characterReplyFieldF3c;
    g_MediatorState8PersistenceF1c.field24 =
        ownerState.characterReplyFieldF40 ? ownerState.characterReplyFieldF40 : g_MediatorProfileCharacterInfoF4.field24;
    g_MediatorState8PersistenceF1c.field28 = 0u;
    std::copy(
        ownerState.characterFlagsF48.begin(),
        ownerState.characterFlagsF48.end(),
        g_MediatorState8PersistenceF1c.header2c.begin());
    std::copy(
        ownerState.secondaryCharacterDataF68.begin(),
        ownerState.secondaryCharacterDataF68.end(),
        g_MediatorState8PersistenceF1c.secondary4c.begin());
    std::memcpy(
        g_MediatorState8PersistenceF1c.body6c.data(),
        ownerState.state8Section0RawF88.data(),
        g_MediatorState8PersistenceF1c.body6c.size());

    CopyCStringIntoByteSpan(g_MediatorState8PersistenceF1c.body6c.data() + 0x04, 0x20, firstName);
    CopyCStringIntoByteSpan(g_MediatorState8PersistenceF1c.body6c.data() + 0x24, 0x20, lastName);
    CopyCStringIntoByteSpan(g_MediatorState8PersistenceF1c.body6c.data() + 0x44, 0x400, background);

    if (ownerState.replySectionData13cc != 0u) {
        std::memcpy(
            g_MediatorState8PersistenceF1c.body6c.data() + 0x444,
            &ownerState.replySectionData13cc,
            sizeof(uint32_t));
    }
    if (ownerState.replySectionData13d0 != 0u) {
        std::memcpy(
            g_MediatorState8PersistenceF1c.body6c.data() + 0x448,
            &ownerState.replySectionData13d0,
            sizeof(uint32_t));
    }

    if (ownerState.state8Section0OverflowBuffer13f0 != nullptr && ownerState.state8Section0OverflowLength13f4 != 0u) {
        g_MediatorState8Overflow13f4 = std::min<uint16_t>(
            ownerState.state8Section0OverflowLength13f4,
            static_cast<uint16_t>(g_MediatorState8Overflow13f0.size()));
        std::memcpy(
            g_MediatorState8Overflow13f0.data(),
            ownerState.state8Section0OverflowBuffer13f0,
            g_MediatorState8Overflow13f4);
    }

    g_MediatorState8Section11String1460Owned = ownerState.state8Section11String1460;
    if (!g_MediatorState8Section11String1460Owned.empty()) {
        const char* begin = g_MediatorState8Section11String1460Owned.c_str();
        g_MediatorState8Section11String1460.begin = begin;
        g_MediatorState8Section11String1460.current = begin + g_MediatorState8Section11String1460Owned.size();
        g_MediatorState8Section11String1460.capacity = g_MediatorState8Section11String1460.current;
    }
}

static uint32_t DiagnosticReadMediatorState8PersistenceBodyDword(size_t offset) {
    uint32_t value = 0u;
    if (offset + sizeof(value) <= g_MediatorState8PersistenceF1c.body6c.size()) {
        std::memcpy(&value, g_MediatorState8PersistenceF1c.body6c.data() + offset, sizeof(value));
    }
    return value;
}

static void LogMediatorState8PersistenceSummary(const char* slotLabel, void* returnAddress) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    spdlog::info(
        "MediatorStub::{} caller={} [{}] persistence{f1c='{}' f3c=0x{:08x} f40=0x{:08x} f48[0..3]=[{:08x} {:08x} {:08x} {:08x}] f68[0..1]=[{:08x} {:08x}] f88_00=0x{:08x} f88_444=0x{:08x} f88_448=0x{:08x} overflow13f4=0x{:04x} mcdGate13f6={:u} clGate1452={:u} sec11_145c=0x{:08x} sec11_len={:u}}",
        slotLabel ? slotLabel : "State8Persistence",
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress),
        NonEmptyOrPlaceholder(g_MediatorState8PersistenceF1c.string00.data()),
        g_MediatorState8PersistenceF1c.field20,
        g_MediatorState8PersistenceF1c.field24,
        g_MediatorState8PersistenceF1c.header2c[0],
        g_MediatorState8PersistenceF1c.header2c[1],
        g_MediatorState8PersistenceF1c.header2c[2],
        g_MediatorState8PersistenceF1c.header2c[3],
        g_MediatorState8PersistenceF1c.secondary4c[0],
        g_MediatorState8PersistenceF1c.secondary4c[1],
        DiagnosticReadMediatorState8PersistenceBodyDword(0x00),
        DiagnosticReadMediatorState8PersistenceBodyDword(0x444),
        DiagnosticReadMediatorState8PersistenceBodyDword(0x448),
        g_MediatorState8Overflow13f4,
        ownerState ? ownerState->section0Flag13f6 : 0u,
        ownerState ? ownerState->flag1452 : 0u,
        ownerState ? ownerState->state8Section11Dword145c : 0u,
        ownerState ? ownerState->state8Section11String1460.size() : 0u);
}

// anchor: launcher.exe:0x41f150
// vtable: ILTLoginMediator.Default slot +0x8c
// Live original `client.dll:0x62198fa0` mcd.cfg family uses this as the mediator-backed/live-data gate.
// Exact corrected original getter proof from launcher disassembly:
// - `0x41f150` returns owner byte `+0x13f6`
// - `+0x1452` is instead the neighboring `cl.cfg` gate used by arg6 `+0x88`
static uint32_t __thiscall Mediator_HasState8PersistenceData8c(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const uint32_t ready = mediator && mediator->PostAuthMarginLoadingStateView().section0Flag13f6 ? 1u : 0u;
    spdlog::info("MediatorStub::HasState8PersistenceData8c(+0x8c) -> {}", ready);
    return ready;
}

// anchor: launcher.exe:0x41f170
// vtable: ILTLoginMediator.Default slot +0xbc
// Live original `client.dll:0x62198fa0` copies 0x20 bytes from this pointer into DAT_629ea67c.
static void* __thiscall Mediator_GetState8PersistenceHeaderBc(MinimalLoginMediatorStub* self) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    PopulateMediatorState8PersistenceF1c();
    LogMediatorState8PersistenceSummary("GetState8PersistenceHeaderBc(+0xbc)", returnAddress);
    LogPointerWords("MediatorStub::GetState8PersistenceHeaderBc(+0xbc)", g_MediatorState8PersistenceF1c.header2c.data(), 8);
    return g_MediatorState8PersistenceF1c.header2c.data();
}

// anchor: launcher.exe:0x41f180
// vtable: ILTLoginMediator.Default slot +0xc0
// Live original `client.dll:0x62198fa0` copies 0x465 bytes from this pointer into DAT_629ea648-backed state.
static void* __thiscall Mediator_GetState8PersistenceBodyC0(MinimalLoginMediatorStub* self) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    PopulateMediatorState8PersistenceF1c();
    LogMediatorState8PersistenceSummary("GetState8PersistenceBodyC0(+0xc0)", returnAddress);
    LogPointerWords("MediatorStub::GetState8PersistenceBodyC0(+0xc0)", g_MediatorState8PersistenceF1c.body6c.data(), 8);
    return g_MediatorState8PersistenceF1c.body6c.data();
}

// anchor: launcher.exe:0x41aec0
// vtable: ILTLoginMediator.Default slot +0xc4
// Live original `client.dll:0x62198fa0` asks for the optional overflow tail pointer plus out-length.
static void* __thiscall Mediator_GetState8PersistenceOverflowC4(MinimalLoginMediatorStub* self, uint16_t* outLength) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    PopulateMediatorState8PersistenceF1c();
    if (outLength) {
        *outLength = g_MediatorState8Overflow13f4;
    }
    LogMediatorState8PersistenceSummary("GetState8PersistenceOverflowC4(+0xc4)", returnAddress);
    spdlog::info(
        "MediatorStub::GetState8PersistenceOverflowC4(+0xc4) -> {} [length=0x{:04x}]",
        fmt::ptr(g_MediatorState8Overflow13f4 ? g_MediatorState8Overflow13f0.data() : nullptr),
        g_MediatorState8Overflow13f4);
    return g_MediatorState8Overflow13f4 ? g_MediatorState8Overflow13f0.data() : nullptr;
}

// anchor: launcher.exe:0x41f190
// vtable: ILTLoginMediator.Default slot +0xc8
static uint32_t __thiscall Mediator_HasState8Section11DataC8(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const uint32_t ready =
        mediator && mediator->PostAuthMarginLoadingStateView().state8Section11Dword145c != 0u ? 1u : 0u;
    spdlog::info("MediatorStub::HasState8Section11DataC8(+0xc8) -> {}", ready);
    return ready;
}

// anchor: launcher.exe:0x41f1a0
// vtable: ILTLoginMediator.Default slot +0xcc
static uint32_t __thiscall Mediator_GetState8Section11DwordCc(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const uint32_t value = mediator ? mediator->PostAuthMarginLoadingStateView().state8Section11Dword145c : 0u;
    spdlog::info("MediatorStub::GetState8Section11DwordCc(+0xcc) -> 0x{:08x}", value);
    return value;
}

// anchor: launcher.exe:0x41f1b0
// vtable: ILTLoginMediator.Default slot +0xd0
static DiagnosticSmallStringLike* __thiscall Mediator_GetState8Section11StringD0(MinimalLoginMediatorStub* self) {
    (void)self;
    PopulateMediatorState8PersistenceF1c();
    spdlog::info(
        "MediatorStub::GetState8Section11StringD0(+0xd0) -> begin={} current={} text='{}'",
        fmt::ptr(g_MediatorState8Section11String1460.begin),
        fmt::ptr(g_MediatorState8Section11String1460.current),
        g_MediatorState8Section11String1460Owned.empty() ? "<empty>" : g_MediatorState8Section11String1460Owned.c_str());
    return &g_MediatorState8Section11String1460;
}
