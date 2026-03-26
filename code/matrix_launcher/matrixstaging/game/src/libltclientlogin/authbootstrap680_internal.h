#pragma once

#include "loginmediator.h"

namespace mxo::ltlogin {

struct AuthBootstrap680Ops {
    static void EraseSidecar(const CLTLoginMediator* mediator);

    static void* BootstrapRaw08AuxHandle50(const CLTLoginMediator& mediator);
    static bool HasBootstrapRaw08AuxHandle54(const CLTLoginMediator& mediator);
    static uint8_t GetCrashReporterPromptForSecurId58(const CLTLoginMediator& mediator);

    static uint32_t SendAuthGetPublicKeyRequest(CLTLoginMediator& mediator);
    static uint32_t SendAuthRequestFromReply(
        CLTLoginMediator& mediator,
        const mxo::auth::GetPublicKeyReply& reply);
    static uint32_t SendAuthChallengeResponse(
        CLTLoginMediator& mediator,
        const mxo::auth::AuthChallenge& challenge);
    static void LogParsedAuthReply(
        const CLTLoginMediator& mediator,
        const mxo::auth::AuthReply& reply);

    static void SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig(CLTLoginMediator& mediator);
    static void ResetRecoveredAuthBootstrapDynamicStateScaffold(CLTLoginMediator& mediator);
    static void SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(
        CLTLoginMediator& mediator,
        const mxo::auth::GetPublicKeyReply& reply);
    static void SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold(
        CLTLoginMediator& mediator,
        const mxo::auth::AuthChallengeResponseBuildResult& buildResult);
    static void SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(
        CLTLoginMediator& mediator,
        const mxo::auth::AuthReply& reply);
};

}  // namespace mxo::ltlogin
