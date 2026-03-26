// Focused ILTLoginMediator.Default mcd/persistence surface.
// Included by `src/launcher_mediator_abi.cpp`; relies on shared ABI-shell state/helpers
// already defined there.

static_assert(
    sizeof(mxo::ltlogin::State3SelectionContextInputSketch) == kDiagnosticSelectionContextSize,
    "State3SelectionContextInputSketch must stay layout-compatible with the recovered arg6 +0xec 0xb4 snapshot");

static std::string g_MediatorState8Section11String1460Owned;
static mxo::ltlogin::RouteDescriptor30SmallStringLikeSketch g_MediatorState8Section11String1460 = {};

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
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    return mediator ? const_cast<uint32_t*>(mediator->State8PersistenceF1cView().header2c.data()) : nullptr;
}

// anchor: launcher.exe:0x41f180
// vtable: ILTLoginMediator.Default slot +0xc0
// Live original `client.dll:0x62198fa0` copies 0x465 bytes from this pointer into DAT_629ea648-backed state.
static void* __thiscall Mediator_GetState8PersistenceBodyC0(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    return mediator ? const_cast<uint8_t*>(mediator->State8PersistenceF1cView().body6c.data()) : nullptr;
}

// anchor: launcher.exe:0x41aec0
// vtable: ILTLoginMediator.Default slot +0xc4
// Live original `client.dll:0x62198fa0` asks for the optional overflow tail pointer plus out-length.
static void* __thiscall Mediator_GetState8PersistenceOverflowC4(MinimalLoginMediatorStub* self, uint16_t* outLength) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    if (!mediator) {
        if (outLength) {
            *outLength = 0u;
        }
        return nullptr;
    }

    const auto& ownerState = mediator->PostAuthMarginLoadingStateView();
    if (outLength) {
        *outLength = ownerState.state8Section0OverflowLength13f4;
    }
    return ownerState.state8Section0OverflowLength13f4 ? ownerState.state8Section0OverflowBuffer13f0 : nullptr;
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
static mxo::ltlogin::RouteDescriptor30SmallStringLikeSketch* __thiscall Mediator_GetState8Section11StringD0(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const char* beginText = mediator ? mediator->PostAuthMarginLoadingStateView().state8Section11String1460.c_str() : "";
    g_MediatorState8Section11String1460Owned = beginText;
    if (!g_MediatorState8Section11String1460Owned.empty()) {
        g_MediatorState8Section11String1460.begin = g_MediatorState8Section11String1460Owned.c_str();
        g_MediatorState8Section11String1460.current =
            g_MediatorState8Section11String1460.begin + g_MediatorState8Section11String1460Owned.size();
        g_MediatorState8Section11String1460.capacity = g_MediatorState8Section11String1460.current;
    } else {
        g_MediatorState8Section11String1460 = {};
    }
    spdlog::info(
        "MediatorStub::GetState8Section11StringD0(+0xd0) -> begin={} current={} text='{}'",
        fmt::ptr(g_MediatorState8Section11String1460.begin),
        fmt::ptr(g_MediatorState8Section11String1460.current),
        g_MediatorState8Section11String1460Owned.empty() ? "<empty>" : g_MediatorState8Section11String1460Owned.c_str());
    return &g_MediatorState8Section11String1460;
}
