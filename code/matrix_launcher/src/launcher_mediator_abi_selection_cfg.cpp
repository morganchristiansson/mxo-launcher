// Focused ILTLoginMediator.Default non-mcd selection cfg corpus fallback surface.
// Included by `src/launcher_mediator_abi.cpp`; relies on shared ABI-shell state/helpers
// already defined there.

static uint32_t LogMediatorPreMcdLiveCorpusFlagMissing(const char* slotLabel, void* returnAddress) {
    spdlog::info(
        "MediatorStub::{} caller={} [{}] -> 0 [unrecovered pre-mcd live-corpus flag; forcing client fallback-to-disk/default path before 0x62198fa0]",
        slotLabel ? slotLabel : "<slot>",
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress));
    return 0u;
}

static void* LogMediatorPreMcdLiveCorpusGetterMissing(
    const char* slotLabel,
    void* returnAddress,
    uint32_t* outLength) {
    if (outLength) {
        *outLength = 0u;
    }
    spdlog::info(
        "MediatorStub::{} caller={} [{}] -> {} [length=0 unrecovered pre-mcd live-corpus getter; forcing client fallback-to-disk/default path before 0x62198fa0]",
        slotLabel ? slotLabel : "<slot>",
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress),
        fmt::ptr(static_cast<void*>(nullptr)));
    return nullptr;
}

static uint32_t __thiscall Mediator_HasLiveCorpus68(MinimalLoginMediatorStub* self) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusFlagMissing("HasLiveCorpus68(+0x68)", __builtin_return_address(0));
}

static uint32_t __thiscall Mediator_HasLiveCorpus6c(MinimalLoginMediatorStub* self) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusFlagMissing("HasLiveCorpus6C(+0x6c)", __builtin_return_address(0));
}

static uint32_t __thiscall Mediator_HasLiveCorpus70(MinimalLoginMediatorStub* self) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusFlagMissing("HasLiveCorpus70(+0x70)", __builtin_return_address(0));
}

static uint32_t __thiscall Mediator_HasLiveCorpus74(MinimalLoginMediatorStub* self) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusFlagMissing("HasLiveCorpus74(+0x74)", __builtin_return_address(0));
}

static uint32_t __thiscall Mediator_HasLiveCorpus78(MinimalLoginMediatorStub* self) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusFlagMissing("HasLiveCorpus78(+0x78)", __builtin_return_address(0));
}

static uint32_t __thiscall Mediator_HasLiveCorpus7c(MinimalLoginMediatorStub* self) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusFlagMissing("HasLiveCorpus7C(+0x7c)", __builtin_return_address(0));
}

static uint32_t __thiscall Mediator_HasLiveCorpus80(MinimalLoginMediatorStub* self) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusFlagMissing("HasLiveCorpus80(+0x80)", __builtin_return_address(0));
}

static uint32_t __thiscall Mediator_HasLiveCorpus84(MinimalLoginMediatorStub* self) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusFlagMissing("HasLiveCorpus84(+0x84)", __builtin_return_address(0));
}

static uint32_t __thiscall Mediator_HasLiveCorpus88(MinimalLoginMediatorStub* self) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusFlagMissing("HasLiveCorpus88(+0x88)", __builtin_return_address(0));
}

static uint32_t __thiscall Mediator_HasLiveCorpus90(MinimalLoginMediatorStub* self) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusFlagMissing("HasLiveCorpus90(+0x90)", __builtin_return_address(0));
}

static void* __thiscall Mediator_GetLiveCorpus94(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusGetterMissing("GetLiveCorpus94(+0x94)", __builtin_return_address(0), outLength);
}

static void* __thiscall Mediator_GetLiveCorpus98(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusGetterMissing("GetLiveCorpus98(+0x98)", __builtin_return_address(0), outLength);
}

static void* __thiscall Mediator_GetLiveCorpus9c(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusGetterMissing("GetLiveCorpus9C(+0x9c)", __builtin_return_address(0), outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA0(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusGetterMissing("GetLiveCorpusA0(+0xa0)", __builtin_return_address(0), outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA4(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusGetterMissing("GetLiveCorpusA4(+0xa4)", __builtin_return_address(0), outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA8(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusGetterMissing("GetLiveCorpusA8(+0xa8)", __builtin_return_address(0), outLength);
}

static void* __thiscall Mediator_GetLiveCorpusAc(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusGetterMissing("GetLiveCorpusAC(+0xac)", __builtin_return_address(0), outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB0(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusGetterMissing("GetLiveCorpusB0(+0xb0)", __builtin_return_address(0), outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB4(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusGetterMissing("GetLiveCorpusB4(+0xb4)", __builtin_return_address(0), outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB8(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return LogMediatorPreMcdLiveCorpusGetterMissing("GetLiveCorpusB8(+0xb8)", __builtin_return_address(0), outLength);
}
