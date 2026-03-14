#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../auth/auth_crypto.h"
#include "../liblttcp/cmessageconnection.h"
#include "../liblttcp/ltthreadperclienttcpengine.h"

namespace mxo::ltlogin {

class CLTLoginState;
class CLTLoginState_AuthenticatePending;
class CLTLoginState_WorldListPending;

// Reimplementation note:
// This file is intended to mirror the concrete launcher-side login/controller structure
// now being recovered around global `0x4f78b8`.
// Keep names stable where they are string-backed or strongly implied by surrounding code,
// but keep uncertain field meanings clearly labeled in comments.
// Canonical runtime/cross-component references remain:
// - docs/launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// - docs/client.dll/RunClientDLL/README.md
//
// Current best identity:
// - concrete launcher-side owner/controller rooted at global `0x4f78b8`
// - strong nearby string family: `CLTLoginMediator`, `CLTLoginMediator::PostEvent()`,
//   `CLTLoginMediator::PostError()`
// - current best read: this object owns the launcher-side auth/margin connection flow,
//   while `ILTLoginMediator.Default` remains the runtime interface slot passed into client.dll
class CLTLoginMediator {
public:
    static constexpr uint32_t kRecoveredWorldSlotCapacity = 100;

    // String-backed config anchors recovered from launcher/client registration code.
    static constexpr const char* kConfigQsAuthServerDnsName = "qsAuthServerDNSName";
    static constexpr const char* kConfigAuthServerPort = "AuthServerPort";
    static constexpr const char* kConfigMarginServerDnsSuffix = "MarginServerDNSSuffix";
    static constexpr const char* kConfigMarginServerPort = "MarginServerPort";
    static constexpr const char* kConfigIgnoreHostsFileForAuth = "IgnoreHostsFileForAuth";
    static constexpr const char* kConfigIgnoreHostsFileForMargin = "IgnoreHostsFileForMargin";

    // String-backed network/auth message anchors near the same owner paths.
    static constexpr const char* kMessageAsRouteToAuthServer = "AS_RouteToAuthServer";
    static constexpr const char* kMessageAsGetPublicKeyRequest = "AS_GetPublicKeyRequest";
    static constexpr const char* kMessageAsGetPublicKeyReply = "AS_GetPublicKeyReply";
    static constexpr const char* kMessageAsAuthRequest = "AS_AuthRequest";
    static constexpr const char* kMessageAsAuthReply = "AS_AuthReply";
    static constexpr const char* kMessageAsGetWorldListRequest = "AS_GetWorldListRequest";
    static constexpr const char* kMessageMsConnectRequest = "MS_ConnectRequest";

    // Current high-value raw auth-code anchors on the launcher-owned helper path:
    // - `0x447eb0` currently builds/sends raw code `0x06`
    //   -> strongest current `AS_GetPublicKeyRequest` candidate
    // - live diagnostic reply parsing now also confirms raw `0x07`
    //   -> `AS_GetPublicKeyReply`
    // - `0x4474f0` currently builds/sends raw code `0x08`
    //   -> strongest current `AS_AuthRequest` candidate
    // - `0x43b830` currently builds/sends raw code `0x35`
    //   -> later `AS_GetWorldListRequest`
    static constexpr uint8_t kAuthRawCodeGetPublicKeyRequest = 0x06;
    static constexpr uint8_t kAuthRawCodeGetPublicKeyReply = 0x07;
    static constexpr uint8_t kAuthRawCodeAuthRequest = 0x08;
    static constexpr uint8_t kAuthRawCodeGetWorldListRequest = 0x35;
    static constexpr const char* kMessageMsConnectReply = "MS_ConnectReply";
    static constexpr const char* kMessageMsLoadCharacterReply = "MS_LoadCharacterReply";

    struct ConnectionHelperFamily {
        // launcher.exe:0x43b300 initializes a contiguous 15-slot helper/state array rooted at
        // `0x4f7868`, immediately after `0x4f78b8 = esi`.
        //
        // Current highest-value slot anchors:
        // - slot 1 / `0x4f786c` / phase-code `1`
        //   - `+0x08 / 0x439090` starts auth connect through `0x41d170`
        // - slot 2 / `0x4f7870` / phase-code `2`
        //   - `+0x08 / 0x439210` is the strongest current earlier credential/bootstrap auth lead
        //   - on the connected branch it reaches `0x448050`, which then branches to:
        //     - `0x447eb0` building/sending raw auth code `0x06`
        //       -> strongest current `AS_GetPublicKeyRequest` candidate
        //     - `0x4474f0` building/sending raw auth code `0x08`
        //       -> strongest current `AS_AuthRequest` candidate
        // - slot 10 / `0x4f7890` / phase-code `10`
        //   - `+0x14 / 0x4401a0` handles later incoming `AS_AuthReply`
        // - slot 14 / `0x4f78a0` / phase-code `14`
        //   - `+0x08 / 0x43b830` sends later `AS_GetWorldListRequest`
        void* helper7868 = nullptr;  // slot 0 / phase-code 0
        void* helper786C = nullptr;  // slot 1 / phase-code 1
        void* helper7870 = nullptr;  // slot 2 / phase-code 2
        void* helper7874 = nullptr;  // slot 3 / phase-code 3
        void* helper7878 = nullptr;  // slot 4 / phase-code 4
        void* helper787C = nullptr;  // slot 5 / phase-code 5
        void* helper7880 = nullptr;  // slot 6 / phase-code 6
        void* helper7884 = nullptr;  // slot 7 / phase-code 7
        void* helper7888 = nullptr;  // slot 8 / phase-code 8
        void* helper788C = nullptr;  // slot 9 / phase-code 9
        void* helper7890 = nullptr;  // slot 10 / phase-code 10
        void* helper7894 = nullptr;  // slot 11 / phase-code 11
        void* helper7898 = nullptr;  // slot 12 / phase-code 12
        void* helper789C = nullptr;  // slot 13 / phase-code 13
        void* helper78A0 = nullptr;  // slot 14 / phase-code 14
    };

    struct MarginRouteState {
        // Current concrete inputs recovered from launcher `0x439300`:
        // - owner byte `+0xcc8`
        // - owner dword `+0x12c`
        // - owner dword `+0x104`
        // - owner vtable surfaces `+0xe0 / +0xfc / +0x10c`
        uint8_t currentCharacterOrRouteIndex = 0;
        uint32_t pendingWorldId = 0;
        int32_t currentWorldId = -1;
        std::string routeHostPrefix;
        std::string exactMarginHostName;
    };

    struct AuthBootstrapSelectedSource38Sketch {
        struct SmallStringLike60Sketch {
            // Same three-dword small-string family used by `0x407dd0`:
            // - `+0x00` = begin/data pointer
            // - `+0x04` = current/end pointer
            // - `+0x08` = capacity/end-of-storage pointer
            // `0x448050` only consumes the first dword here as a raw `char*` for its arg9 path,
            // but `0x41eb80` proves the full embedded small-string object still lives here.
            const char* begin = nullptr;
            const char* current = nullptr;
            const char* capacity = nullptr;
        };

        // Current best concrete family returned by owner vtable `+0x38`:
        // - getter is tiny function `0x41f0a0 = lea eax,[ecx+0x94] ; ret`
        // - so this is an **embedded owner subobject at `0x4f78b8 + 0x94`**, not a separate heap object
        // - owner vtable `+0x30` / `0x41ecd0` then acts as the corresponding setter/consumer and
        //   uses `0x41eb80` to copy the same family into owner `+0x94`
        // - owner vtable `+0x150` / `0x41f270` is now also a direct first-string writer for the
        //   same block and copies up to `0x20` bytes into owner `+0x94`
        //
        // Current best recovered layout from `0x41f0a0` + `0x41ecd0` + `0x41eb80` + `0x439210`
        // plus later owner-path uses like `0x43f300`, `0x41330`, `0x21a50`, and `0x20720`:
        // - `+0x00 .. +0x1f` = first inline 32-byte NUL-terminated string
        // - `+0x20 .. +0x3f` = second inline 32-byte NUL-terminated string
        // - `+0x40 .. +0x4f` = first copied 16-byte block
        // - `+0x50 .. +0x5f` = second copied 16-byte block
        // - `+0x60 .. +0x68` = embedded small-string object
        // - `+0x6c` = trailing byte/flag
        //
        // Newer semantic anchors on individual fields:
        // - `+0x00` first string:
        //   - owner vtable `+0x150` / `0x41f270` writes it directly
        //   - later auth-reply path `0x43f300 -> owner +0x150` feeds it from `0x43d480(...)`
        // - `+0x20` second string:
        //   - later copied into bootstrap `+0xf8` by `0x41330`
        //   - `0x41330 -> 0x456c40` validates it against a concrete slash+6-digit shape
        // - `+0x60` embedded small string:
        //   - empty case falls through a literal `"STATION"` default path in `0x489bc0`
        //   - non-empty case is later copied into owner `+0x65c + 0x18` through `0x21a50`
        //
        // Current best semantic read is therefore stronger than a generic auth blob but still
        // deliberately provisional on exact original class name:
        // an owner-side **station/launchpad-flavored phase-2 auth/bootstrap source block**
        // that feeds `0x448050` and later session/bootstrap helpers.
        std::array<char, 0x20> inlineString00{};
        std::array<char, 0x20> inlineString20{};
        std::array<uint8_t, 16> block40{};
        std::array<uint8_t, 16> block50{};
        SmallStringLike60Sketch string60;
        uint8_t flag6C = 0;
    };

    struct AuthBootstrapState680Sketch {
        // Current best read of the extra owner child allocated through `0x41290` and stored at
        // owner `+0x680` by `0x41b160`.
        //
        // High-value phase-2 auth/bootstrap anchors:
        // - base ctor `0x45500`, size `0x11c`
        // - preparation/fill helper `0x448050`
        // - branch condition at `0x44811e`: low-byte null test on dword field `+0xa0`
        //   - later `0x429b0` still uses that same field as a helper/pointer object via `+0x1c`
        //   - current best read therefore remains a helper/pointer family at `+0xa0`
        // - later challenge/crypto continuation `0x429b0`:
        //   - writes 16-byte material to `+0x85`
        //   - derives / caches the current/public key id at `+0x9c` via `0x41470`
        //
        // Current field sketch from `0x45500` + `0x448050` + `0x447eb0` + `0x4474f0`:
        std::string string04;               // `+0x04`
        std::string string10;               // `+0x10`
        std::string string1C;               // `+0x1c`
        uint32_t loginType28 = 0;           // `+0x28`
        uint32_t launcherVersion2C = 0;     // `+0x2c`
        std::array<uint8_t, 16> block30{};  // `+0x30 .. +0x3f`
        std::array<uint8_t, 16> block40{};  // `+0x40 .. +0x4f`
        void* sendTarget50 = nullptr;       // `+0x50`
        uint32_t timestamp80 = 0;           // `+0x80`
        std::array<uint8_t, 16> material85{}; // `+0x85 .. +0x94`
        void* sideObject94 = nullptr;       // `+0x94`
        void* sideObject98 = nullptr;       // `+0x98`
        uint32_t currentPublicKeyId9C = 0;  // `+0x9c`
        void* helperA0 = nullptr;           // `+0xa0`
        void* lazyRaw06StateA4 = nullptr;   // `+0xa4`
        void* raw08AuxHandleA8 = nullptr;   // `+0xa8`
        uint32_t fieldAC = 0;               // `+0xac`
        uint32_t stateFlagEC = 1;           // `+0xec` from base ctor `0x45500`
        void* fieldF0 = nullptr;            // `+0xf0`
        void* fieldF4 = nullptr;            // `+0xf4`
        void* fieldF8 = nullptr;            // `+0xf8`
        void* fieldFC = nullptr;            // `+0xfc`
        void* field100 = nullptr;           // `+0x100`
        uint32_t field108 = 0;              // `+0x108`
        uint32_t field10C = 0;              // `+0x10c`
        uint32_t field110 = 0;              // `+0x110`
        uint32_t field114 = 0;              // `+0x114`
        uint32_t field118 = 0;              // `+0x118`
    };

    CLTLoginMediator();
    ~CLTLoginMediator();

    void SetNetworkEngine(mxo::liblttcp::CLTThreadPerClientTCPEngine* engine);
    mxo::liblttcp::CLTThreadPerClientTCPEngine* NetworkEngine() const;

    void SetCurrentState(CLTLoginState* state);
    CLTLoginState* CurrentState() const;

    void SetAuthConnectionContextKey(void* contextKey);
    void SetMarginConnectionContextKey(void* contextKey);

    // Recovered config anchors:
    // - launcher `qsAuthServerDNSName` / `AuthServerPort`
    // - launcher `MarginServerDNSSuffix` / `MarginServerPort`
    // The replacement launcher should eventually populate these from the same launcher-owned
    // config path instead of treating connection setup as generic ad-hoc socket work.
    void SetAuthServerConfig(const char* dnsName, uint16_t portHostOrder, bool ignoreHostsFile = false);
    void SetMarginServerConfig(const char* dnsSuffix, uint16_t portHostOrder, bool ignoreHostsFile = false);

    const std::string& AuthServerDnsName() const;
    uint16_t AuthServerPortHostOrder() const;
    bool IgnoreHostsFileForAuth() const;

    const std::string& MarginServerDnsSuffix() const;
    uint16_t MarginServerPortHostOrder() const;
    bool IgnoreHostsFileForMargin() const;
    std::string ResolvedMarginHostName() const;

    const mxo::liblttcp::LTTCPEndpointKey& AuthEndpoint() const;
    const mxo::liblttcp::LTTCPEndpointKey& MarginEndpoint() const;

    mxo::liblttcp::CMessageConnection* AuthConnection() const;
    mxo::liblttcp::CMessageConnection* MarginConnection() const;

    void SetMarginRouteState(uint8_t currentCharacterOrRouteIndex, uint32_t pendingWorldId, int32_t currentWorldId);
    void SetMarginRouteHostPrefix(const char* routeHostPrefix);
    void SetExactMarginHostName(const char* exactMarginHostName);
    const MarginRouteState& CurrentMarginRouteState() const;

    const ConnectionHelperFamily& Helpers() const;
    const AuthBootstrapState680Sketch& AuthBootstrap680() const;

    // launcher.exe:0x43b300
    // Current best read:
    // - allocates / initializes a contiguous 15-slot launcher-global helper/state array
    //   rooted at `0x4f7868 .. 0x4f78a0`
    // - this happens immediately after `0x4f78b8 = esi`
    // - the slot index and each helper's vtable `+0x18` phase-code getter now match across
    //   the recovered table (`0..14`)
    // - exact class names for most helper objects are still being recovered
    void InitializeConnectionHelpers();

    // Current best auth-side connection-init path:
    // - launcher `0x43909f -> 0x41d170`
    // - copies auth DNS into owner `+0x4c`
    // - reads auth port from recovered config state
    // - builds endpoint into owner `+0x5c`
    // - constructs auth-side CMessageConnection child at owner `+0x18`
    // - then calls `connection->+0x1c(owner+0x5c)`
    // Current best method mapping still treats that virtual `+0x1c` as the connection-
    // oriented ensure-connected / engine-Connect wrapper.
    uint32_t BeginAuthConnection();

    // Current best post-connect status/result anchors:
    // - original engine `Connect` success path `0x4329b9..0x4329cc` builds `0x435050(0x7000001)`
    //   which is a type-2 work item enqueued as `(workItem, connection, 0)`
    // - auth-side derived connection family (`0x41d170`, vtable `0x4afef0`) later reaches
    //   owner-side packet handling through wrapper `0x449a70`
    // - margin-side derived connection family (`0x41e500`, vtable `0x4aff38`) later reaches
    //   owner-side packet handling through wrapper `0x44af60`
    // - newer helper-object review now narrows one important negative detail:
    //   - startup auth/margin derived objects come through `0x4417e0 -> 0x448b40(flag=0)`
    //   - so connection helper slots `+0x7c / +0x80` stay null on that path
    //   - type-2 connect-status completion therefore falls through `0x449a70 / 0x44af60`
    //     into the owner callback / fallback chain instead of being fully handled by those
    //     optional helper objects alone
    // - important auth-side fallback-chain narrowing from `0x449a70 -> owner +0x17c -> 0x448a60`:
    //   - owner `+0x17c` is now resolved as thunk `0x41f260`
    //   - `0x41f260` forwards to the owner's current helper/state object at `+0x10`, then jumps
    //     to helper vtable `+0x14`
    //   - so the concrete auth-side body depends on the current helper selected through the
    //     `0x4f7868` family via `0x41b450(...)`
    //   - important correction: `0x4401a0` is one **later helper-state** `+0x14` body
    //     (`0x4f7890` / vtable `0x4b512c`), not the generic owner `+0x17c` target itself
    //   - that later helper body only meaningfully handles raw auth code `0x0b` (`AS_AuthReply`)
    //   - on success it parses that reply via `0x43a330`, updates owner `+0x80`, appends a
    //     small owner record under `+0x684`, mirrors the current index to owner byte `+0xcc8`,
    //     then reaches `0x41b450(0x0b)` and string-backed `CLTLoginMediator::PostEvent(0x14)`
    //   - newer helper-side follow-up now also narrows the `0x41b450(0x0b)` continuation:
    //     - it selects helper `0x4f7894` (vtable `0x4b5154`)
    //     - that helper's current concrete `+0x8` path `0x43c020` prepares owner-side data and
    //       posts event `0x15`, but still does **not** show the missing first auth send
    //   - if the current helper `+0x14` body returns 0, `0x448a60` only logs
    //     `Got unhandled op of type %d with status %s`
    // - important current nuance: those later packet handlers are not themselves proof of the
    //   first outbound request after connect; current best read is now stronger than that:
    //   this owner/helper/fallback chain is **not** where the first faithful outbound auth
    //   request begins, even though it remains important later incoming-path evidence
    // - newer helper-family tracing now gives a stronger earlier bootstrap lead than only the
    //   later world-list sender:
    //   - `0x41b450(1)` selects helper `0x4f786c`
    //   - helper `+0x08 / 0x439090` starts auth connect through `0x41d170`
    //   - direct code xrefs to auth wrapper `0x41af60` still only tie down the later helper
    //     `0x4f78a0 +0x08 / 0x43b830`
    //   - that later auth-side wrapper path remains:
    //       - `0x41af60`
    //       - auth connection `+0x24 / 0x41cf30`
    //       - auth connection `+0x28 / 0x448cf0`
    //       - send helper `0x448a00`
    //       - connection `+0x20 / 0x449d20`
    //       - engine `+0x20` / current best `SendBuffer`
    //     with raw code `0x35` -> `AS_GetWorldListRequest`
    // - current strongest earlier credential/bootstrap auth lead is now helper
    //   `0x4f7870` selected through `0x41b450(2)`:
    //   - helper `+0x08 / 0x439210`
    //   - if auth is not connected yet, it falls back to `0x41b450(1)`
    //   - on the connected branch it gathers launcher-owned owner data through:
    //     - owner `+0x168`
    //     - owner `+0x20`
    //     - owner `+0x38`
    //   - then calls `0x448050`, which is currently only xref'd from `0x439210`
    //   - Ghidra-backed callsite layout now narrows the selected source object materially:
    //     - owner vtable `+0x38` getter `0x41f0a0` returns embedded owner block `this + 0x94`
    //     - owner vtable `+0x30` / `0x41ecd0` copies/consumes the same family via `0x41eb80`
    //     - source `+0x00 .. +0x1f` = first inline 32-byte NUL-terminated string
    //     - source `+0x20 .. +0x3f` = second inline 32-byte NUL-terminated string
    //     - source `+0x40 .. +0x4f` = first copied 16-byte block
    //     - source `+0x50 .. +0x5f` = second copied 16-byte block
    //     - source `+0x60 .. +0x68` = embedded `0x407dd0`-style small-string object
    //       whose first dword is passed into `0x448050` as the raw third-string pointer
    //     - source `+0x6c` = trailing byte/flag
    //   - `0x448050` then branches into two launcher-owned outbound packet builders that both
    //     send indirectly through a bootstrap object send-target at `object + 0x50`
    //     via virtual `+0x24`, rather than through another simple direct `0x41af60` callsite:
    //     - `0x447eb0`
    //       - builds/sends raw code `0x06`
    //       - strongest current `AS_GetPublicKeyRequest` candidate
    //     - `0x4474f0`
    //       - builds/sends raw code `0x08`
    //       - strongest current `AS_AuthRequest` candidate
    //       - also builds/sends a later auxiliary raw `0x1b` packet on that same indirect path
    //   - the branch there is now best read as a low-byte null test on dword helper field `+0xa0`
    // - important channel-specific correction from the latest packet-code pass:
    //   - do not confuse this auth-side bootstrap lead with margin-side wrapper traffic like
    //     `0x41af70`, where raw code `0x06` maps through the margin table to
    //     `MS_GetClientIPRequest`
    uint32_t HandleAuthConnectStatus(uint32_t workResultCode);
    uint32_t HandleMarginConnectStatus(uint32_t workResultCode);
    uint32_t BeginAuthHandshake();
    uint32_t BeginMarginHandshake();

    void SetAuthCredentials(const char* username, const char* password);
    void SetAuthBootstrapConfig(
        uint32_t launcherVersion,
        uint32_t currentPublicKeyId,
        uint8_t loginType,
        const std::vector<uint8_t>& keyConfigMd5,
        const std::vector<uint8_t>& uiConfigMd5);
    uint32_t HandleAuthPacketBytes(const uint8_t* packetBytes, size_t packetSize);

    const char* ExpectedAuthRequestName() const;
    const char* ExpectedMarginRequestName() const;

    static constexpr uint32_t kConnectStatusSuccess = 0x7000001u;

    // launcher.exe owner-vtable surfaces currently recovered from the margin-state dispatcher.
    // These names remain provisional, but keeping them in source is more useful than leaving
    // the recovered callsites as anonymous `+0xe0/+0xfc/+0x10c` notes in markdown.
    uint32_t ResolveMarginRouteFromCurrentCharacterSlot() const;   // current best anchor: owner vtable +0xe0
    uint32_t ResolveMarginRouteFromWorldId(uint32_t worldId) const; // current best anchor: owner vtable +0xfc
    uint32_t ResolveMarginRouteDescriptor() const;                  // current best anchor: owner vtable +0x10c

    // Current best margin-side connection-init dispatcher:
    // - launcher `0x439300`
    // - consults `[owner+4]` current state object through vtable `+0x18`
    // - dispatches several cases into `0x41e500`
    // - concrete currently recovered case inputs include owner state vtable surfaces
    //   `+0xe0 / +0xfc / +0x10c` and owner fields `+0xcc8 / +0x12c / +0x104`
    // - `0x41e500` then constructs the margin-side CMessageConnection child at owner `+0x1c`,
    //   builds endpoint state into owner `+0x6c`, and calls `connection->+0x1c(owner+0x6c)`
    uint32_t DispatchMarginConnectionByState();

    // Minimal placeholder accessors for recovered world/selection storage families.
    // These are not yet faithful data structures; they only preserve the currently recovered
    // slot-count / owner-shape in source instead of re-describing it in markdown.
    void* WorldSlot(uint32_t index) const;
    void* WorldPayloadSlot(uint32_t index) const;

private:
    uint32_t SendAuthFramedPacket(const mxo::auth::FramedPacket& packet, const char* stepLabel);
    uint32_t SendAuthGetPublicKeyRequest();
    uint32_t SendAuthRequestFromReply(const mxo::auth::GetPublicKeyReply& reply);
    uint32_t SendAuthChallengeResponse(const mxo::auth::AuthChallenge& challenge);
    void LogParsedAuthReply(const mxo::auth::AuthReply& reply) const;
    void SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();

    void BuildAuthEndpoint();
    void BuildMarginEndpoint();
    mxo::liblttcp::CMessageConnection* EnsureAuthConnectionObject();
    mxo::liblttcp::CMessageConnection* EnsureMarginConnectionObject();

    // Current best field sketch for the `0x4f78b8` owner object:
    // - `+0x10` = current state/helper object used heavily by owner-state dispatch paths
    // - `+0x18` = auth-side CMessageConnection child
    // - `+0x1c` = margin-side CMessageConnection child
    // - `+0x4c` = auth DNS / route string staging area
    // - `+0x5c` = auth endpoint block consumed by auth-side `connection->+0x1c(...)`
    // - `+0x6c` = margin endpoint block consumed by margin-side `connection->+0x1c(...)`
    // - `+0x94` = embedded station/launchpad-flavored phase-2 auth/bootstrap source block
    //   returned by owner vtable `+0x38` (`0x41f0a0`), copied/consumed by owner vtable `+0x30`
    //   (`0x41ecd0 -> 0x41eb80`), and also written through owner vtable `+0x150` (`0x41f270`)
    // - `+0x680` = extra heap child built during owner initialization; current best read is
    //   the phase-2 auth/bootstrap object sketched above (`0x41290` / `0x45500` family)
    // - `+0x688` = world-slot pointer table (100 entries)
    // - `+0x818` = world payload/range table family (100-entry shape still provisional)
    // - `+0xd84` = world/character record pointer table family
    mxo::liblttcp::CLTThreadPerClientTCPEngine* engine_;
    CLTLoginState* currentState_;

    mxo::liblttcp::CMessageConnection* authConnection_;
    mxo::liblttcp::CMessageConnection* marginConnection_;
    void* authConnectionContextKey_;
    void* marginConnectionContextKey_;

    ConnectionHelperFamily helpers_;
    MarginRouteState marginRouteState_;
    AuthBootstrapSelectedSource38Sketch authBootstrapSource38_;
    AuthBootstrapState680Sketch authBootstrap680_;

    std::string authServerDnsName_;
    uint16_t authServerPortHostOrder_;
    bool ignoreHostsFileForAuth_;

    std::string marginServerDnsSuffix_;
    uint16_t marginServerPortHostOrder_;
    bool ignoreHostsFileForMargin_;

    mxo::liblttcp::LTTCPEndpointKey authEndpoint_;
    mxo::liblttcp::LTTCPEndpointKey marginEndpoint_;

    std::string authUsername_;
    std::string authPassword_;
    uint32_t authLauncherVersion_;
    uint32_t authCurrentPublicKeyId_;
    uint8_t authLoginType_;
    std::vector<uint8_t> authKeyConfigMd5_;
    std::vector<uint8_t> authUiConfigMd5_;
    bool authGetPublicKeyRequestSent_;
    bool authRequestSent_;
    bool authChallengeResponseSent_;
    mxo::auth::GetPublicKeyReply lastAuthPublicKeyReply_;
    mxo::auth::AuthRequestBuildResult lastAuthRequestBuildResult_;
    mxo::auth::AuthChallenge lastAuthChallenge_;
    mxo::auth::AuthReply lastAuthReply_;

    uint32_t lastAuthConnectStatus_;
    uint32_t lastMarginConnectStatus_;
    uint32_t authConnectStatusCount_;
    uint32_t marginConnectStatusCount_;
    const char* expectedAuthRequestName_;
    const char* expectedMarginRequestName_;

    std::array<void*, kRecoveredWorldSlotCapacity> worldSlots_;
    std::array<void*, kRecoveredWorldSlotCapacity> worldPayloadSlots_;
};

}  // namespace mxo::ltlogin
