// Focused ILTLoginMediator.Default non-mcd selection cfg corpus fallback surface.
// Included by `src/launcher_mediator_abi.cpp`; relies on shared ABI-shell state/helpers
// already defined there.

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
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveCsCfg78();
}

// anchor: client.dll:0x62198b70 / launcher.exe vtable +0x7c -> raw bytes 0x41f110 = owner byte +0x13fe
// Exact current bl.cfg pair:
// - client helper `0x62198b70` uses arg6 `+0x7c`, then `+0xa8`, for `bl.cfg`
// - original launcher `+0x7c` returns owner byte `+0x13fe`
// - original launcher `+0xa8` returns owner pointer `+0x13f8` and writes out-length `+0x13fc`
// - recovered state8 slot-6 producer writes that same owner family from section selector `1`
static uint32_t __thiscall Mediator_HasLiveCorpus7c(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveBlCfg7c();
}

// anchor: client.dll:0x62198c60 / launcher.exe vtable +0x80 -> raw bytes 0x41f120 = owner byte +0x1406
// Exact current il.cfg pair:
// - client helper `0x62198c60` uses arg6 `+0x80`, then `+0xac`, for `il.cfg`
// - original launcher `+0x80` returns owner byte `+0x1406`
// - original launcher `+0xac` returns owner pointer `+0x1400` and writes out-length `+0x1404`
// - recovered state8 slot-6 producer writes that same owner family from section selector `2`
static uint32_t __thiscall Mediator_HasLiveCorpus80(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveIlCfg80();
}

// anchor: client.dll:0x62198d50 / launcher.exe vtable +0x84 -> raw bytes 0x41f130 = owner byte +0x1448
// Exact current rl.cfg pair:
// - client helper `0x62198d50` uses arg6 `+0x84`, then `+0xb0`, for `rl.cfg`
// - original launcher `+0x84` returns owner byte `+0x1448`
// - original launcher `+0xb0` returns owner pointer `+0x1440` and writes out-length `+0x1444`
// - recovered state8 slot-6 producer writes that same owner family from section selector `8`
static uint32_t __thiscall Mediator_HasLiveCorpus84(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveRlCfg84();
}

// anchor: client.dll:0x62198e50 / launcher.exe vtable +0x88 -> raw bytes 0x41f140 = owner byte +0x1452
// Exact current cl.cfg pair:
// - client helper `0x62198e50` uses arg6 `+0x88`, then `+0xb4`, for `cl.cfg`
// - original launcher `+0x88` returns owner byte `+0x1452`
// - original launcher `+0xb4` returns owner pointer `+0x144c` and writes out-length `+0x1450`
// - recovered state8 slot-6 producer writes that same owner family from section selector `9`
static uint32_t __thiscall Mediator_HasLiveCorpus88(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveClCfg88();
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
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveCuiCfg90();
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
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveCsCfgA4(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA8(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveBlCfgA8(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusAc(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveIlCfgAc(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB0(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveRlCfgB0(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB4(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveClCfgB4(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB8(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveCuiCfgB8(outLength);
}
