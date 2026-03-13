#pragma once

#include <array>
#include <cstdint>
#include <string>

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
    static constexpr const char* kMessageMsConnectReply = "MS_ConnectReply";
    static constexpr const char* kMessageMsLoadCharacterReply = "MS_LoadCharacterReply";

    struct ConnectionHelperFamily {
        // launcher.exe:0x43b300 currently initializes this small helper/object family
        // immediately after `0x4f78b8 = esi`.
        void* helper7868 = nullptr;  // global `0x4f7868`
        void* helper786C = nullptr;  // global `0x4f786c`
        void* helper7870 = nullptr;  // global `0x4f7870`
        void* helper78A0 = nullptr;  // global `0x4f78a0`
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

    // launcher.exe:0x43b300
    // Current best read:
    // - allocates / initializes a family of small launcher-global helper/state objects
    //   currently rooted at `0x4f7868 / 0x4f786c / 0x4f7870 / 0x4f78a0`
    // - this happens immediately after `0x4f78b8 = esi`
    // - exact class names for those helper objects are still being recovered
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
    // - important current nuance: those later packet handlers are not themselves proof of the
    //   first outbound request after connect, but they do show that raw connect success alone is
    //   not the whole launcher-owned auth/margin path
    uint32_t HandleAuthConnectStatus(uint32_t workResultCode);
    uint32_t HandleMarginConnectStatus(uint32_t workResultCode);
    uint32_t BeginAuthHandshake();
    uint32_t BeginMarginHandshake();

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
    // - `+0x680` = extra heap child built during owner construction
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

    std::string authServerDnsName_;
    uint16_t authServerPortHostOrder_;
    bool ignoreHostsFileForAuth_;

    std::string marginServerDnsSuffix_;
    uint16_t marginServerPortHostOrder_;
    bool ignoreHostsFileForMargin_;

    mxo::liblttcp::LTTCPEndpointKey authEndpoint_;
    mxo::liblttcp::LTTCPEndpointKey marginEndpoint_;

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
