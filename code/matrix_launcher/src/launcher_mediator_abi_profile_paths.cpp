// Focused ILTLoginMediator.Default profile-path / current-slot / selection-descriptor surface.
// Included by `src/launcher_mediator_abi.cpp`; relies on shared ABI-shell helpers already defined
// there.
//
// ILTLoginMediator.Default wrapper minimization:
// - keep `g_LoginMediatorVtable` in the ABI shell
// - keep wrapper-facing `+0x40/+0x44` object shapes explicit
// - move object ownership, scratch state, and logging into `CLTLoginMediator`

// anchor: client.dll profile-root formatting path uses arg6 +0x38 for Profiles\%s\... construction
// vtable: ILTLoginMediator.Default slot +0x38
static const char* __thiscall Mediator_GetDisplayName(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetProfileRootName();
}

// anchor: client.dll fallback-selection path asks arg6 +0x3c for the default selection index when given 0xff
// vtable: ILTLoginMediator.Default slot +0x3c
static uint32_t __thiscall Mediator_GetDefaultSelectionIndex(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetDefaultSelectionIndex();
}

// anchor: client.dll:0x62170dc1..0x62170e59 later asks arg6 +0x40 with the scratch-shaped arg7 request
// vtable: ILTLoginMediator.Default slot +0x40
// Keep this wrapper-facing selection-descriptor family explicit instead of forcing the owner-side
// `0x004b01c8 +0x40/+0x44` slot-record accessor names onto it.
static void* __thiscall Mediator_GetSelectionDescriptor40(
    MinimalLoginMediatorStub* self,
    uint32_t selectionIndex) {
    (void)self;

    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator
        ? static_cast<void*>(mediator->GetArg6SelectionDescriptorObject40(
            selectionIndex,
            __builtin_return_address(0)))
        : nullptr;
}

// anchor: launcher.exe:0x41f300
// vtable: ILTLoginMediator.Default slot +0x44
// Current wrapper-facing read from `0x4d2c58_ILTLoginMediator_Default.md`:
// - returns a current-slot record object on the later profile/save path
// - keep that split explicit from the owner-side `0x004b01c8 +0x44` family
static void* __thiscall Mediator_GetCurrentSlotRecordObject44(MinimalLoginMediatorStub* self) {
    (void)self;

    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator
        ? static_cast<void*>(mediator->GetArg6CurrentSlotRecordObject44(
            __builtin_return_address(0)))
        : nullptr;
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
