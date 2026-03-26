// Focused ILTLoginMediator.Default non-mcd selection cfg corpus fallback surface.
// Included by `src/launcher_mediator_abi.cpp`; relies on shared ABI-shell state/helpers
// already defined there.

static uint32_t LogMediatorResolvedLiveCorpusFlag(
    const char* slotLabel,
    void* returnAddress,
    const char* corpusLabel,
    const char* storageLabel,
    uint32_t ready,
    const void* buffer,
    uint32_t length) {
    spdlog::info(
        "MediatorStub::{} caller={} [{}] -> {} [live {} via {} ptr={} length=0x{:04x}]",
        slotLabel ? slotLabel : "<slot>",
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress),
        static_cast<unsigned>(ready),
        corpusLabel ? corpusLabel : "<corpus>",
        storageLabel ? storageLabel : "<owner fields>",
        fmt::ptr(buffer),
        static_cast<unsigned>(length));
    if (ready == 0u) {
        LogMediatorCharacterStateContext(slotLabel, returnAddress);
    }
    return ready;
}

static void* LogMediatorResolvedLiveCorpusGetter(
    const char* slotLabel,
    void* returnAddress,
    const char* corpusLabel,
    const char* storageLabel,
    uint32_t flag,
    void* buffer,
    uint32_t length,
    uint32_t* outLength) {
    if (outLength) {
        *outLength = length;
    }
    spdlog::info(
        "MediatorStub::{} caller={} [{}] -> {} [live {} via {} flag={} length=0x{:04x}]",
        slotLabel ? slotLabel : "<slot>",
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress),
        fmt::ptr(buffer),
        corpusLabel ? corpusLabel : "<corpus>",
        storageLabel ? storageLabel : "<owner fields>",
        static_cast<unsigned>(flag),
        static_cast<unsigned>(length));
    if (buffer == nullptr || length == 0u) {
        LogMediatorCharacterStateContext(slotLabel, returnAddress);
    }
    return buffer;
}

// anchor: client.dll:0x62198670 / launcher.exe vtable +0x68 -> 0x41f0c0
// Exact current closed pair:
// - client helper `0x62198670` uses arg6 `+0x68`, then `+0x94`, for `hl.cfg`
// - original launcher `+0x68` returns owner byte `+0x140e`
// - original launcher `+0x94` returns owner pointer `+0x1408` and writes out-length `+0x140c`
// - recovered state8 slot-6 producer writes that same owner family from section selector `6`
static uint32_t __thiscall Mediator_HasLiveCorpus68(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveHlCfg68();
}

// anchor: client.dll:0x62198770 / launcher.exe vtable +0x6c -> 0x41f0d0
// Exact current next narrow pair:
// - client helper `0x62198770` uses arg6 `+0x6c`, then `+0x98`, for `an.cfg`
// - original launcher `+0x6c` returns owner byte `+0x1416`
// - original launcher `+0x98` returns owner pointer `+0x1410` and writes out-length `+0x1414`
// - recovered state8 slot-6 producer writes that same owner family from section selector `7`
static uint32_t __thiscall Mediator_HasLiveCorpus6c(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveAnCfg6c();
}

// anchor: client.dll:0x62198870 / launcher.exe vtable +0x70
// Exact current inventory/loadout-targeted pair:
// - client helper `0x62198870` uses arg6 `+0x70`, then `+0x9c`, for `pi.cfg`
// - original launcher `+0x70` returns owner byte `+0x141e`
// - original launcher `+0x9c` returns owner pointer `+0x1418` and writes out-length `+0x141c`
// - recovered state8 slot-6 producer writes that same owner family from section selector `3`
static uint32_t __thiscall Mediator_HasLiveCorpus70(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLivePiCfg70();
}

// anchor: client.dll:0x62198970 / launcher.exe vtable +0x74 -> raw bytes 0x41f0f0 = owner byte +0x1426
// Exact current symptom-targeted pair:
// - client helper `0x62198970` uses arg6 `+0x74`, then `+0xa0`, for `ai.cfg`
// - original launcher `+0x74` returns owner byte `+0x1426`
// - original launcher `+0xa0` returns owner pointer `+0x1420` and writes out-length `+0x1424`
// - recovered state8 slot-6 producer writes that same owner family from section selector `4`
// - client live consumer `0x621e2310` directly calls `0x621e0b90(...)` and marks an availability byte at
//   `param_1 + 0x6e0 + actionId*8`, making this pair materially closer to the missing Actions-window symptom
static uint32_t __thiscall Mediator_HasLiveCorpus74(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveAiCfg74();
}

// anchor: client.dll:0x62198a70 / launcher.exe vtable +0x78 -> raw bytes 0x41f100 = owner byte +0x142e
// Exact current skills-targeted pair:
// - client helper `0x62198a70` uses arg6 `+0x78`, then `+0xa4`, for `cs.cfg`
// - original launcher `+0x78` returns owner byte `+0x142e`
// - original launcher `+0xa4` returns owner pointer `+0x1428` and writes out-length `+0x142c`
// - recovered state8 slot-6 producer writes that same owner family from section selector `5`
// - client live adopter `0x621cd550` consumes compact 6-byte records and writes only the first dword of
//   8-byte entries at `param_1 + 0x6dc + index*8`; that explains why the live state8 payload can be
//   shorter than the later saved on-disk `cs.cfg` file emitted by `0x621966d0 -> 0x621c9e20`
// - newer client-side tightening also makes the saved second dword less scary than before:
//   `0x621ca0c0/0x621e0a10/0x621e3b50` only use its low byte as availability state, while the upper
//   24 bits currently read best as stack-carried noise from the client's `0x621e1a70` full-slot copy path
// - this is the same broader table family that `ai.cfg` availability processing touches at
//   `+0x6e0 + actionId*8`
static uint32_t __thiscall Mediator_HasLiveCorpus78(MinimalLoginMediatorStub* self) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t ready = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferFlag142e != 0u) : 0u;
    const void* buffer = ownerState ? ownerState->allocatedBuffer1428 : nullptr;
    const uint32_t length = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferLength142c) : 0u;
    return LogMediatorResolvedLiveCorpusFlag(
        "HasLiveCorpus78(+0x78)",
        returnAddress,
        "cs.cfg / state8 section5",
        "owner+0x142e/0x1428/0x142c",
        ready,
        buffer,
        length);
}

// anchor: client.dll:0x62198b70 / launcher.exe vtable +0x7c -> raw bytes 0x41f110 = owner byte +0x13fe
// Exact current bl.cfg pair:
// - client helper `0x62198b70` uses arg6 `+0x7c`, then `+0xa8`, for `bl.cfg`
// - original launcher `+0x7c` returns owner byte `+0x13fe`
// - original launcher `+0xa8` returns owner pointer `+0x13f8` and writes out-length `+0x13fc`
// - recovered state8 slot-6 producer writes that same owner family from section selector `1`
static uint32_t __thiscall Mediator_HasLiveCorpus7c(MinimalLoginMediatorStub* self) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t ready = ownerState ? static_cast<uint32_t>(ownerState->flag13fe != 0u) : 0u;
    const void* buffer = ownerState ? ownerState->allocatedBuffer13f8 : nullptr;
    const uint32_t length = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferLength13fc) : 0u;
    return LogMediatorResolvedLiveCorpusFlag(
        "HasLiveCorpus7C(+0x7c)",
        returnAddress,
        "bl.cfg / state8 section1",
        "owner+0x13fe/0x13f8/0x13fc",
        ready,
        buffer,
        length);
}

// anchor: client.dll:0x62198c60 / launcher.exe vtable +0x80 -> raw bytes 0x41f120 = owner byte +0x1406
// Exact current il.cfg pair:
// - client helper `0x62198c60` uses arg6 `+0x80`, then `+0xac`, for `il.cfg`
// - original launcher `+0x80` returns owner byte `+0x1406`
// - original launcher `+0xac` returns owner pointer `+0x1400` and writes out-length `+0x1404`
// - recovered state8 slot-6 producer writes that same owner family from section selector `2`
static uint32_t __thiscall Mediator_HasLiveCorpus80(MinimalLoginMediatorStub* self) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t ready = ownerState ? static_cast<uint32_t>(ownerState->flag1406 != 0u) : 0u;
    const void* buffer = ownerState ? ownerState->allocatedBuffer1400 : nullptr;
    const uint32_t length = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferLength1404) : 0u;
    return LogMediatorResolvedLiveCorpusFlag(
        "HasLiveCorpus80(+0x80)",
        returnAddress,
        "il.cfg / state8 section2",
        "owner+0x1406/0x1400/0x1404",
        ready,
        buffer,
        length);
}

// anchor: client.dll:0x62198d50 / launcher.exe vtable +0x84 -> raw bytes 0x41f130 = owner byte +0x1448
// Exact current rl.cfg pair:
// - client helper `0x62198d50` uses arg6 `+0x84`, then `+0xb0`, for `rl.cfg`
// - original launcher `+0x84` returns owner byte `+0x1448`
// - original launcher `+0xb0` returns owner pointer `+0x1440` and writes out-length `+0x1444`
// - recovered state8 slot-6 producer writes that same owner family from section selector `8`
static uint32_t __thiscall Mediator_HasLiveCorpus84(MinimalLoginMediatorStub* self) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t ready = ownerState ? static_cast<uint32_t>(ownerState->flag1448 != 0u) : 0u;
    const void* buffer = ownerState ? ownerState->allocatedBuffer1440 : nullptr;
    const uint32_t length = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferLength1444) : 0u;
    return LogMediatorResolvedLiveCorpusFlag(
        "HasLiveCorpus84(+0x84)",
        returnAddress,
        "rl.cfg / state8 section8",
        "owner+0x1448/0x1440/0x1444",
        ready,
        buffer,
        length);
}

// anchor: client.dll:0x62198e50 / launcher.exe vtable +0x88 -> raw bytes 0x41f140 = owner byte +0x1452
// Exact current cl.cfg pair:
// - client helper `0x62198e50` uses arg6 `+0x88`, then `+0xb4`, for `cl.cfg`
// - original launcher `+0x88` returns owner byte `+0x1452`
// - original launcher `+0xb4` returns owner pointer `+0x144c` and writes out-length `+0x1450`
// - recovered state8 slot-6 producer writes that same owner family from section selector `9`
static uint32_t __thiscall Mediator_HasLiveCorpus88(MinimalLoginMediatorStub* self) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t ready = ownerState ? static_cast<uint32_t>(ownerState->flag1452 != 0u) : 0u;
    const void* buffer = ownerState ? ownerState->allocatedBuffer144c : nullptr;
    const uint32_t length = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferLength1450) : 0u;
    return LogMediatorResolvedLiveCorpusFlag(
        "HasLiveCorpus88(+0x88)",
        returnAddress,
        "cl.cfg / state8 section9",
        "owner+0x1452/0x144c/0x1450",
        ready,
        buffer,
        length);
}

// anchor: client.dll:0x621993d0 / launcher.exe vtable +0x90 -> raw bytes 0x41f160 = owner byte +0x145a
// Exact current cui.cfg pair:
// - client helper `0x621993d0` uses arg6 `+0x90`, then `+0xb8`, for `cui.cfg`
// - original launcher `+0x90` returns owner byte `+0x145a`
// - original launcher `+0xb8` returns owner pointer `+0x1454` and writes out-length `+0x1458`
// - recovered state8 slot-6 producer writes that same owner family from section selector `10`
// - newer client-side tightening matters for the current remaining narrow mismatch:
//   - if `+0x90` is false, `0x621993d0` only tries to load an existing on-disk `cui.cfg`
//   - later direct-save helper `0x62198490` can still call `0x62197050` and write `cui.cfg`
//     from the client-owned `0x629e05bc` object during shutdown-side persistence
//   - current bounded replacement route still emits on-disk `cui.cfg` that way even though the
//     live mediator pair stays absent, while the bounded original route still omits the file
static uint32_t __thiscall Mediator_HasLiveCorpus90(MinimalLoginMediatorStub* self) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t ready = ownerState ? static_cast<uint32_t>(ownerState->flag145a != 0u) : 0u;
    const void* buffer = ownerState ? ownerState->allocatedBuffer1454 : nullptr;
    const uint32_t length = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferLength1458) : 0u;
    const uint32_t result = LogMediatorResolvedLiveCorpusFlag(
        "HasLiveCorpus90(+0x90)",
        returnAddress,
        "cui.cfg / state8 section10",
        "owner+0x145a/0x1454/0x1458",
        ready,
        buffer,
        length);
    if (result == 0u) {
        static bool loggedDeferredCuiSaveMismatch = false;
        if (!loggedDeferredCuiSaveMismatch) {
            loggedDeferredCuiSaveMismatch = true;
            spdlog::info(
                "MediatorStub::HasLiveCorpus90(+0x90) note: live cui.cfg is absent on the current path; bounded original reruns also omit final cui.cfg, while replacement may still emit an on-disk cui.cfg later through the client-owned direct-save path 0x62198490 -> 0x62197050");
        }
    }
    return result;
}

static void* __thiscall Mediator_GetLiveCorpus94(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveHlCfg94(outLength);
}

static void* __thiscall Mediator_GetLiveCorpus98(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveAnCfg98(outLength);
}

static void* __thiscall Mediator_GetLiveCorpus9c(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLivePiCfg9c(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA0(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveAiCfgA0(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA4(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t flag = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferFlag142e != 0u) : 0u;
    void* buffer = ownerState ? ownerState->allocatedBuffer1428 : nullptr;
    const uint32_t length = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferLength142c) : 0u;
    return LogMediatorResolvedLiveCorpusGetter(
        "GetLiveCorpusA4(+0xa4)",
        returnAddress,
        "cs.cfg / state8 section5",
        "owner+0x1428/0x142c",
        flag,
        buffer,
        length,
        outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA8(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t flag = ownerState ? static_cast<uint32_t>(ownerState->flag13fe != 0u) : 0u;
    void* buffer = ownerState ? ownerState->allocatedBuffer13f8 : nullptr;
    const uint32_t length = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferLength13fc) : 0u;
    return LogMediatorResolvedLiveCorpusGetter(
        "GetLiveCorpusA8(+0xa8)",
        returnAddress,
        "bl.cfg / state8 section1",
        "owner+0x13f8/0x13fc",
        flag,
        buffer,
        length,
        outLength);
}

static void* __thiscall Mediator_GetLiveCorpusAc(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t flag = ownerState ? static_cast<uint32_t>(ownerState->flag1406 != 0u) : 0u;
    void* buffer = ownerState ? ownerState->allocatedBuffer1400 : nullptr;
    const uint32_t length = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferLength1404) : 0u;
    return LogMediatorResolvedLiveCorpusGetter(
        "GetLiveCorpusAC(+0xac)",
        returnAddress,
        "il.cfg / state8 section2",
        "owner+0x1400/0x1404",
        flag,
        buffer,
        length,
        outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB0(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t flag = ownerState ? static_cast<uint32_t>(ownerState->flag1448 != 0u) : 0u;
    void* buffer = ownerState ? ownerState->allocatedBuffer1440 : nullptr;
    const uint32_t length = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferLength1444) : 0u;
    return LogMediatorResolvedLiveCorpusGetter(
        "GetLiveCorpusB0(+0xb0)",
        returnAddress,
        "rl.cfg / state8 section8",
        "owner+0x1440/0x1444",
        flag,
        buffer,
        length,
        outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB4(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t flag = ownerState ? static_cast<uint32_t>(ownerState->flag1452 != 0u) : 0u;
    void* buffer = ownerState ? ownerState->allocatedBuffer144c : nullptr;
    const uint32_t length = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferLength1450) : 0u;
    return LogMediatorResolvedLiveCorpusGetter(
        "GetLiveCorpusB4(+0xb4)",
        returnAddress,
        "cl.cfg / state8 section9",
        "owner+0x144c/0x1450",
        flag,
        buffer,
        length,
        outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB8(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t flag = ownerState ? static_cast<uint32_t>(ownerState->flag145a != 0u) : 0u;
    void* buffer = ownerState ? ownerState->allocatedBuffer1454 : nullptr;
    const uint32_t length = ownerState ? static_cast<uint32_t>(ownerState->allocatedBufferLength1458) : 0u;
    return LogMediatorResolvedLiveCorpusGetter(
        "GetLiveCorpusB8(+0xb8)",
        returnAddress,
        "cui.cfg / state8 section10",
        "owner+0x1454/0x1458",
        flag,
        buffer,
        length,
        outLength);
}
