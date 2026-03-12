#include "cmessageconnection.h"

namespace mxo::liblttcp {

// Like the engine skeleton, this file currently exists to preserve recovered original
// class/method structure and to make future migration out of diagnostics.cpp easier.

CMessageConnection::CMessageConnection()
    : CLTTCPConnection(),
      engine_(nullptr) {}

CMessageConnection::CMessageConnection(CLTThreadPerClientTCPEngine* engine)
    : CLTTCPConnection(),
      engine_(engine) {}

CMessageConnection::~CMessageConnection() = default;

void CMessageConnection::SetEngine(CLTThreadPerClientTCPEngine* engine) {
    engine_ = engine;
}

CLTThreadPerClientTCPEngine* CMessageConnection::Engine() const {
    return engine_;
}

uint32_t CMessageConnection::EnsureConnected() {
    if (!engine_) {
        return 0;
    }
    return engine_->Connect(this);
}

uint32_t CMessageConnection::CloseConnection(bool graceful) {
    if (!engine_) {
        return 0;
    }
    return engine_->Close(this, graceful);
}

uint32_t CMessageConnection::SendPacket(const void* packetData, uint32_t packetByteCount, void* completionContext) {
    if (!engine_ || !packetData || packetByteCount == 0) {
        return 0;
    }

    // Current starter path deliberately routes through the recovered connection-object-based
    // engine surface instead of pretending this is already a faithful packet serializer.
    return engine_->SendBuffer(this, packetData, packetByteCount, completionContext);
}

uint32_t CMessageConnection::OnOperationCompleted(uint32_t workCode) {
    if (!engine_) {
        return 0;
    }

    // Placeholder only: original launcher.exe shows this family logging packet/stream
    // outcomes and then driving engine callbacks / queue submissions.
    (void)workCode;
    return 1;
}

}  // namespace mxo::liblttcp
