#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mxo::liblttcp {

class CLTThreadPerClientTCPEngine;

// Reimplementation note:
// This is still a starter original-name skeleton.
// Canonical RE references remain:
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// - docs/launcher.exe/VTABLES/0x004b8034.md

// State values recovered from original CLTThreadPerClientTCPEngine paths.
// Only the meanings marked in comments are evidence-backed so far.
enum class LTTCPEngineConnectionState : uint32_t {
    kUnknown = 0,
    kConnectActive = 1,     // written by Connect success path
    kUdpMonitorActive = 2,  // written by UDPMonitorPort success path
    kClosing = 4,           // provisional: written by low-level close path
    kClosed = 8,            // required by connection-wrapper and engine prechecks
};

struct LTTCPEndpointKey {
    uint16_t family = 2;          // AF_INET
    uint16_t portNetworkOrder = 0;
    uint32_t ipv4NetworkOrder = 0;
    uint32_t reserved0 = 0;
    uint32_t reserved1 = 0;
};

// Recovered parser input fragment prefix consumed by connection `+0x6c`
// (`CVariableLengthPrefixedTCPStreamParser::Parse`).
// Current high-confidence fields from `0x469bf0`:
// - `+0x08` = byte count
// - `+0x0c` = first payload byte
// - vtable `+0x04` / `+0x08` = retain/release-style lifetime hooks
struct CLTTCPConnection_ReadOperationFragmentScaffold {
    void** vtable;      // +0x00
    uint32_t field04;   // +0x04 unknown so far
    uint32_t byteCount; // +0x08
    uint8_t bytes0C[1]; // +0x0c variable-length fragment bytes begin here
};

// Recovered parser-emitted completed-packet work item family built via
// `CVariableLengthPrefixedTCPStreamParser_AllocatePacketBuffer -> CParsedPacketWorkItem_ctor`.
// Current high-confidence fields from the helper family around that object:
// - size = `0x2c`
// - `+0x04` = work type `3`
// - `+0x18` low byte = `1`
// - vtable = `0x4b3e08`
struct CLTTCPConnection_ParsedPacketWorkItemScaffold {
    void** vtable;                 // +0x00
    uint32_t workType;             // +0x04 = 3
    uint32_t field08;              // +0x08
    uint32_t field0C;              // +0x0c
    void* firstFragment10;         // +0x10 first retained fragment (`CParsedPacketWorkItem_BeginFragmentTraversal`)
    void* fragmentList14;          // +0x14 optional retained fragment-list root (`CParsedPacketWorkItem_AppendFragment`)
    uint8_t flag18;                // +0x18 low byte set to 1 by ctor
    uint8_t unknown19_1b[3];       // +0x19..+0x1b
    uint32_t traversalIndex1C;     // +0x1c traversal/reset state (`CParsedPacketWorkItem_GetNextFragment`)
    uint32_t field20;              // +0x20 not explicitly initialized in current ctor read
    uint32_t currentCursor24;      // +0x24 set/get by `CParsedPacketWorkItem_Set/GetCurrentCursor`
    uint32_t assembledByteCount28; // +0x28 set/get by `CParsedPacketWorkItem_Set/GetAssembledByteCount`
};

static_assert(sizeof(CLTTCPConnection_ParsedPacketWorkItemScaffold) == 0x2c, "parsed packet work item size mismatch");

// Source-owned abstraction over the recovered connection family.
// Important current limitation:
// - this C++ base keeps the recovered state/virtual relationships useful to the replacement
//   launcher, but it is not yet a byte-faithful class-layout reconstruction of the original
//   `CBaseConnection` / `CLTTCPConnection` hierarchy.
class CBaseConnection {
 public:
  // UNANCHORED: source-owned abstract base for the recovered connection family.
  virtual ~CBaseConnection() = default;

  // UNANCHORED: source-owned abstraction over the recovered receive entry surface.
  virtual uint32_t OnReceive(void* callbackContext) = 0;
  // UNANCHORED: source-owned abstraction over the recovered completion callback surface.
  virtual uint32_t OnOperationCompleted(void*) = 0;
  // UNANCHORED: source-owned abstraction over the recovered send callback surface.
  virtual uint32_t SendPacket(const void*, uint32_t, void*) = 0;

  // UNANCHORED: source-owned utility accessor over the recovered `+0x34` state field.
  virtual bool IsConnected() const;

  // UNANCHORED: source-owned accessor over the recovered `+0x34` state field.
  LTTCPEngineConnectionState State() const {
    return state_;
  }

  // UNANCHORED: source-owned base-field initializer for the recovered state slot.
  CBaseConnection(LTTCPEngineConnectionState initialState = LTTCPEngineConnectionState::kClosed);

 protected:
  LTTCPEngineConnectionState state_;
};

// Recovered CLTTCPConnection-family wrapper surface.
// Current high-confidence anchors from launcher.exe vtable `0x004b8034`:
// - `0x00449ca0` = Close wrapper into engine slot `+0x1c`
// - `0x00449d40` = OnReceive
// - `0x00449fd0` = OnClose callback-forwarder
// - `0x00449cd0` = Connect wrapper that updates endpoint `+0x24` then calls engine slot `+0x18`
// - `0x00449d20` = SendBuffer wrapper into engine slot `+0x20`
class CLTTCPConnection : public CBaseConnection {
public:
    // UNANCHORED: source-owned convenience ctor for the current replacement-side connection model.
    CLTTCPConnection();
    // UNANCHORED: source-owned convenience ctor that seeds owner-context state.
    explicit CLTTCPConnection(void* ownerContext);
    // anchor: launcher.exe:0x44ac40
    ~CLTTCPConnection();

    // UNANCHORED: source-owned compatibility wrapper over the recovered connection `+0x10` engine field.
    void SetEngine(CLTThreadPerClientTCPEngine* engine);
    // UNANCHORED: source-owned compatibility accessor over the recovered connection `+0x10` engine field.
    CLTThreadPerClientTCPEngine* Engine() const;

    // UNANCHORED: source-owned owner-context setter used by the current scaffolds.
    void SetOwnerContext(void* ownerContext);
    // UNANCHORED: source-owned owner-context accessor used by the current scaffolds.
    void* OwnerContext() const;

    // UNANCHORED: source-owned socket-handle setter used by the current scaffolds.
    void SetSocketHandle(uint32_t socketHandle);
    // UNANCHORED: source-owned socket-handle accessor used by the current scaffolds.
    uint32_t SocketHandle() const;

    // UNANCHORED: source-owned connection-state setter used by the current scaffolds.
    void SetState(LTTCPEngineConnectionState state);
    // UNANCHORED: source-owned connection-state accessor used by the current scaffolds.
    LTTCPEngineConnectionState State() const;

    // UNANCHORED: source-owned endpoint setter over the recovered connection `+0x24` copy.
    void SetRemoteEndpoint(const LTTCPEndpointKey& endpoint);
    // UNANCHORED: source-owned endpoint accessor over the recovered connection `+0x24` copy.
    const LTTCPEndpointKey& RemoteEndpoint() const;

    // UNANCHORED: source-owned hostname setter used by the current resolver scaffold.
    void SetRemoteHostName(const char* hostName);
    // UNANCHORED: source-owned hostname accessor used by the current resolver scaffold.
    const std::string& RemoteHostName() const;

    // UNANCHORED: source-owned nonblocking socket poll helper used by the launcher bridge scaffolds.
    int PollReceiveNonBlocking();
    // UNANCHORED: source-owned diagnostic accessor over the buffered receive bytes.
    const std::vector<uint8_t>& ReceivedBytes() const;
    // UNANCHORED: source-owned buffered receive reset helper.
    void ClearReceivedBytes();
    // UNANCHORED: source-owned buffered receive prefix-consumption helper.
    void ConsumeReceivedBytesPrefix(size_t byteCount);

    // anchor: launcher.exe:0x449ca0
    // vtable: launcher.exe:0x004b8040
    uint32_t Close(bool graceful);

    // anchor: launcher.exe:0x449cd0
    // vtable: launcher.exe:0x004b8050
    uint32_t Connect(const LTTCPEndpointKey& endpoint);

    // anchor: launcher.exe:0x449d20
    // vtable: launcher.exe:0x004b8054
    uint32_t SendBuffer(const void* buffer, uint32_t byteCount, void* completionContext);

    // anchor: launcher.exe:0x449fd0
    // vtable: launcher.exe:0x004b804c
    // Current best read: tiny release/teardown forwarder for the read-operation fragment object.
    void OnClose(void* readOperationFragment);

    // anchor: launcher.exe:0x449d40
    // vtable: launcher.exe:0x004b8048
    // Current best read: consume one read-operation fragment through the parser at connection `+0x6c`
    // and enqueue each parser-emitted completed packet work item.
    uint32_t OnReceive(void* readOperationFragment) override;

    // UNANCHORED: low-level socket close helper used beneath the anchored Close wrapper.
    uint32_t CloseSocketTransportScaffold(bool graceful);
    // UNANCHORED: low-level raw-socket send helper used beneath the anchored SendBuffer wrapper.
    uint32_t SendRawSocketBufferScaffold(const void* buffer, uint32_t byteCount, void* completionContext);

    // UNANCHORED: source-owned mirror of the connection `+0x6c` parser call shape seen in `0x449d40`.
    // Current best original callee is `CVariableLengthPrefixedTCPStreamParser::Parse` (`0x469bf0`).
    uint32_t pollReceive(
        CLTTCPConnection_ReadOperationFragmentScaffold* readOperationFragment,
        CLTTCPConnection_ParsedPacketWorkItemScaffold** outWorkItem);
    // UNANCHORED: source-owned mirror of the queue-enqueue helper call shape seen in `0x449d40`.
    void pushCompletedOperation(
        CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
        void* context,
        bool useQueue34);

private:
    CLTThreadPerClientTCPEngine* engine_;
    void* ownerContext_;
    uint32_t socketHandle_;
    LTTCPEndpointKey remoteEndpoint_;
    std::string remoteHostName_;
    std::vector<uint8_t> receivedBytes_;
};

}  // namespace mxo::liblttcp
