// Focused ILTLoginMediator.Default mcd/persistence surface.
// Included by `src/launcher_mediator_abi.cpp`; relies on shared ABI-shell state/helpers
// already defined there.

static_assert(
    sizeof(mxo::ltlogin::State3SelectionContextInputSketch) == kDiagnosticSelectionContextSize,
    "State3SelectionContextInputSketch must stay layout-compatible with the recovered arg6 +0xec 0xb4 snapshot");

// anchor: launcher.exe:0x41f150
// vtable: ILTLoginMediator.Default slot +0x8c
// Live original `client.dll:0x62198fa0` mcd.cfg family uses this as the mediator-backed/live-data gate.
// Exact corrected original getter proof from launcher disassembly:
// - `0x41f150` returns owner byte `+0x13f6`
// - `+0x1452` is instead the neighboring `cl.cfg` gate used by arg6 `+0x88`
static uint32_t __thiscall Mediator_HasState8PersistenceData8c(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasState8PersistenceData8c();
}

// anchor: launcher.exe:0x41f170
// vtable: ILTLoginMediator.Default slot +0xbc
// Live original `client.dll:0x62198fa0` copies 0x20 bytes from this pointer into DAT_629ea67c.
static void* __thiscall Mediator_GetState8PersistenceHeaderBc(MinimalLoginMediatorStub* self) {
    (void)self;
    return const_cast<void*>(mxo::ltlogin::ILTLoginMediator::Default->GetState8PersistenceHeaderBc());
}

// anchor: launcher.exe:0x41f180
// vtable: ILTLoginMediator.Default slot +0xc0
// Live original `client.dll:0x62198fa0` copies 0x465 bytes from this pointer into DAT_629ea648-backed state.
static void* __thiscall Mediator_GetState8PersistenceBodyC0(MinimalLoginMediatorStub* self) {
    (void)self;
    return const_cast<void*>(mxo::ltlogin::ILTLoginMediator::Default->GetState8PersistenceBodyC0());
}

// anchor: launcher.exe:0x41aec0
// vtable: ILTLoginMediator.Default slot +0xc4
// Live original `client.dll:0x62198fa0` asks for the optional overflow tail pointer plus out-length.
static void* __thiscall Mediator_GetState8PersistenceOverflowC4(MinimalLoginMediatorStub* self, uint16_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetState8PersistenceOverflowC4(outLength);
}

// anchor: launcher.exe:0x41f190
// vtable: ILTLoginMediator.Default slot +0xc8
static uint32_t __thiscall Mediator_HasState8Section11DataC8(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasState8Section11Dword145c();
}

// anchor: launcher.exe:0x41f1a0
// vtable: ILTLoginMediator.Default slot +0xcc
static uint32_t __thiscall Mediator_GetState8Section11DwordCc(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetState8Section11Dword145c();
}

// anchor: launcher.exe:0x41f1b0
// vtable: ILTLoginMediator.Default slot +0xd0
static mxo::ltlogin::RouteDescriptor30SmallStringLikeSketch* __thiscall Mediator_GetState8Section11StringD0(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetState8Section11String1460();
}
