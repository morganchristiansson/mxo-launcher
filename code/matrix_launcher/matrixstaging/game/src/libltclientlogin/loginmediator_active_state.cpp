/**
 * CLTLoginMediator active-state-source / character-view helpers.
 *
 * Keep this TU narrow:
 * - generic registration for whichever mediator instance currently owns the live character/load state
 * - controller-adjacent character-view accessors used by diagnostics and wrapper-facing code
 *
 * Intentionally avoid answering the broader original-binary ownership split here.
 * The current runtime still lets diagnostics register a separate live controller instance, but the
 * interface stays generic so that ownership can move later without a broad rewrite.
 */

#include "loginmediator.h"

#include <string>

namespace mxo::ltlogin {
namespace {

static const char* NonEmptyOrNull(const char* value) {
    return (value && value[0]) ? value : nullptr;
}

static const char* PreferNonEmpty(const char* primary, const char* fallback) {
    return NonEmptyOrNull(primary) ? primary : NonEmptyOrNull(fallback);
}

static bool IsLikelyMiddleInitialOnly(const char* value) {
    return value != nullptr && std::char_traits<char>::length(value) == 1u;
}

}  // namespace

CLTLoginMediator* g_CurrentLoginMediator = nullptr;

// Stub globals for faithful Initialize implementation.
// These are populated by the launcher.exe path; replacement uses SetAuthServerConfig.
// anchor: launcher.exe:0x4d6304
void* g_pThreadPerClientTCPEngine = nullptr;
// anchor: launcher.exe:0x4f7b14
const char* g_qsAuthServerDNSName = "";
// anchor: launcher.exe:0x4d6780
uint32_t g_IgnoreHostsFileForAuth = 0;
// anchor: launcher.exe:0x4f7a50 - auth server port used by BeginAuthConnection endpoint builder
uint16_t g_AuthServerPort = 11000;
// Server RSA public key (base64 encoded) - from selected server config
// Initialized by ApplySelectedServerConfigToMediator()
const char* g_ServerPublicModulusB64 = nullptr;
const char* g_ServerPublicExponentB64 = nullptr;
// Skip AS_GetPublicKeyReply embedded key validation (for non-standard key sizes like 2048-bit)
// Initialized by ApplySelectedServerConfigToMediator() from server config
uint32_t g_SkipAuthPublicKeyReplyValidation = 0u;

// Margin server globals - analogous to auth globals, for faithful server-selection flow.
// anchor: launcher.exe:0x4f7b14 / similar to auth
const char* g_marginServerDNSName = "";
// anchor: launcher.exe:0x4f7a50 / similar to auth
uint16_t g_marginServerPort = 10000;

}  // namespace mxo::ltlogin
