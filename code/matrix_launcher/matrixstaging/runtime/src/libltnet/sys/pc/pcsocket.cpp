#include "pcsocket.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <spdlog/spdlog.h>

namespace {

static std::atomic<bool> g_socketLayerLoggedOnce{false};

static uint32_t QueryMaxUdpPacketSizeScaffold() {
    SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == INVALID_SOCKET) {
        return 0;
    }

    unsigned long maxUdpPacketSize = 0;
    int optionSize = sizeof(maxUdpPacketSize);
    if (getsockopt(
            socketHandle,
            SOL_SOCKET,
            SO_MAX_MSG_SIZE,
            reinterpret_cast<char*>(&maxUdpPacketSize),
            &optionSize) == SOCKET_ERROR) {
        closesocket(socketHandle);
        return 0;
    }

    closesocket(socketHandle);
    return static_cast<uint32_t>(maxUdpPacketSize);
}

static uint32_t QueryMaxUserPortScaffold() {
    uint32_t maxUserPort = 0x1388;
    HKEY tcpipParametersKey = nullptr;
    if (RegOpenKeyA(
            HKEY_LOCAL_MACHINE,
            "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
            &tcpipParametersKey) == ERROR_SUCCESS) {
        DWORD valueType = 0;
        DWORD valueSize = sizeof(maxUserPort);
        (void)RegQueryValueExA(
            tcpipParametersKey,
            "MaxUserPort",
            nullptr,
            &valueType,
            reinterpret_cast<LPBYTE>(&maxUserPort),
            &valueSize);
        RegCloseKey(tcpipParametersKey);
    }
    return maxUserPort;
}

}  // namespace

// anchor: launcher.exe:0x00452e00
bool CLTSocketLayer::Init() {
    WSADATA wsaData = {};
    const int startupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (startupResult != 0) {
        spdlog::warn(
            "CLTSocketLayer::Init scaffold: WSAStartup failed error={}",
            startupResult);
        return false;
    }

    if (!g_socketLayerLoggedOnce.exchange(true)) {
        const uint32_t maxUdpPacketSize = QueryMaxUdpPacketSizeScaffold();
        spdlog::info(
            "CLTSocketLayer::Init scaffold: Winsock initialized ver={}.{} highest={}.{} desc='{}' status='{}'",
            LOBYTE(wsaData.wVersion),
            HIBYTE(wsaData.wVersion),
            LOBYTE(wsaData.wHighVersion),
            HIBYTE(wsaData.wHighVersion),
            wsaData.szDescription,
            wsaData.szSystemStatus);

        if (HIBYTE(wsaData.wVersion) > 1) {
            const uint32_t maxUserPort = QueryMaxUserPortScaffold();
            spdlog::info(
                "CLTSocketLayer::Init scaffold: MaxUserPort={} maxUdpPacketSize={} bytes",
                maxUserPort,
                maxUdpPacketSize);
        } else {
            spdlog::info(
                "CLTSocketLayer::Init scaffold: MaxSockets={} maxUdpPacketSize={} bytes",
                wsaData.iMaxSockets,
                maxUdpPacketSize);
        }
    }

    return true;
}
