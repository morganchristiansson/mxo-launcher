#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// ws2_32.lib is linked via -lws2_32 in the Makefile

#include "../matrixstaging/runtime/src/libltcrypto/auth_crypto.h"

namespace {

struct Options {
    std::string username;
    std::string password;
    int timeoutMs = 5000;
};

static bool ParseArgs(int argc, char** argv, Options* options) {
    if (!options) return false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " --username <name> [options]\n\n";
            std::cout << "Core CLI options:\n";
            std::cout << "  --username <name>\n";
            std::cout << "  --password <text>\n";
            std::cout << "  --timeout-ms <ms>\n";
            return false;
        } else if (arg == "--username") {
            if (++i >= argc) return false;
            options->username = argv[i];
        } else if (arg == "--password") {
            if (++i >= argc) return false;
            options->password = argv[i];
        } else if (arg == "--timeout-ms") {
            if (++i >= argc) return false;
            char* end = NULL;
            const unsigned long parsed = std::strtoul(argv[i], &end, 0);
            if (end == argv[i]) return false;
            options->timeoutMs = static_cast<int>(parsed);
        } else {
            std::cerr << "error: unknown option '" << arg << "'\n";
            return false;
        }
    }

    if (options->username.empty()) {
        std::cerr << "error: --username is required\n";
        return false;
    }
    return true;
}

static bool ConnectTcp(const std::string& host, uint16_t port, int timeoutMs, SOCKET* outFd) {
    if (!outFd) return false;
    *outFd = INVALID_SOCKET;

    WSADATA wsaData;
    const int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        std::cerr << "WSAStartup failed: " << wsaResult << "\n";
        return false;
    }

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char portText[16] = {0};
    _snprintf_s(portText, sizeof(portText), _TRUNCATE, "%u", static_cast<unsigned>(port));

    struct addrinfo* results = NULL;
    const int gai = getaddrinfo(host.c_str(), portText, &hints, &results);
    if (gai != 0) {
        std::cerr << "getaddrinfo failed: " << gai_strerror(gai) << "\n";
        return false;
    }

    for (struct addrinfo* current = results; current; current = current->ai_next) {
        const SOCKET fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (fd == INVALID_SOCKET) continue;

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        struct timeval tv;
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        char timeout_buf[8];
        memcpy(timeout_buf, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, timeout_buf, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, timeout_buf, sizeof(tv));

        if (connect(fd, current->ai_addr, current->ai_addrlen) == 0) {
            *outFd = fd;
            freeaddrinfo(results);
            return true;
        }

        closesocket(fd);
    }

    freeaddrinfo(results);
    return false;
}

static bool SendAll(SOCKET fd, const std::vector<uint8_t>& bytes) {
    size_t offset = 0u;
    while (offset < bytes.size()) {
        const char* data_ptr = reinterpret_cast<const char*>(bytes.data() + offset);
        const ssize_t written = send(fd, data_ptr, static_cast<int>(bytes.size() - offset), 0);
        if (written < 0) {
            const int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT) continue;
            std::cerr << "send failed: " << err << "\n";
            return false;
        }
        offset += static_cast<size_t>(written);
    }
    return true;
}

static bool ReceiveExact(SOCKET fd, size_t byteCount, std::vector<uint8_t>* outBytes, bool* outTimedOut) {
    if (!outBytes || !outTimedOut) return false;
    *outTimedOut = false;
    outBytes->clear();

    while (outBytes->size() < byteCount) {
        uint8_t buffer[512];
        const size_t want = (byteCount - outBytes->size() < sizeof(buffer)) ? (byteCount - outBytes->size()) : sizeof(buffer);
        char* buffer_ptr = reinterpret_cast<char*>(buffer);
        const ssize_t got = recv(fd, buffer_ptr, static_cast<int>(want), 0);
        if (got == 0) return false;
        if (got < 0) {
            const int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT) {
                *outTimedOut = true;
                return false;
            }
            std::cerr << "recv failed: " << err << "\n";
            return false;
        }
        outBytes->insert(outBytes->end(), buffer, buffer + got);
    }
    return true;
}

static bool ReceivePacket(SOCKET fd, mxo::auth::FramedPacket* outPacket, bool* outTimedOut) {
    if (!outPacket || !outTimedOut) return false;

    std::vector<uint8_t> first;
    if (!ReceiveExact(fd, 1u, &first, outTimedOut)) return false;

    outPacket->headerBytes = first;
    size_t payloadLen = first[0];
    if (first[0] & 0x80u) {
        std::vector<uint8_t> second;
        if (!ReceiveExact(fd, 1u, &second, outTimedOut)) return false;
        outPacket->headerBytes.push_back(second[0]);
        payloadLen = ((static_cast<size_t>(first[0] & 0x7fu)) << 8u) | static_cast<size_t>(second[0]);
    }

    if (!ReceiveExact(fd, payloadLen, &outPacket->payloadBytes, outTimedOut)) return false;

    outPacket->bytes = outPacket->headerBytes;
    outPacket->bytes.insert(outPacket->bytes.end(), outPacket->payloadBytes.begin(), outPacket->payloadBytes.end());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!ParseArgs(argc, argv, &options)) return 2;

    std::cout << "auth_probe config:\n";
    std::cout << "  host='auth.lith.thematrixonline.net'\n";
    std::cout << "  port=11000\n";
    std::cout << "  username='" << options.username << "'\n";
    std::cout << "  passwordProvided=" << (options.password.empty() ? 0 : 1) << "\n";
    std::cout << "  timeoutMs=" << options.timeoutMs << "\n\n";

    SOCKET fd = INVALID_SOCKET;
    if (!ConnectTcp("auth.lith.thematrixonline.net", 11000, options.timeoutMs, &fd)) {
        std::cerr << "error: connect failed\n";
        return 1;
    }
    std::cout << "connected\n\n";

    mxo::auth::FramedPacket getPublicKeyRequest;
    if (!mxo::auth::BuildGetPublicKeyRequestPacket(
            76005,  // launcher version
            0,       // current public key ID
            mxo::auth::kFrameModeAuto,
            &getPublicKeyRequest)) {
        std::cerr << "error: failed to build AS_GetPublicKeyRequest\n";
        closesocket(fd);
        return 1;
    }

    std::cout << "send 0x06 AS_GetPublicKeyRequest\n";
    if (!SendAll(fd, getPublicKeyRequest.bytes)) {
        closesocket(fd);
        return 1;
    }

    mxo::auth::FramedPacket getPublicKeyReplyPacket;
    bool timedOut = false;
    if (!ReceivePacket(fd, &getPublicKeyReplyPacket, &timedOut)) {
        std::cerr << "error: failed waiting for 0x07 reply" << (timedOut ? " (timeout)" : "") << "\n";
        closesocket(fd);
        return timedOut ? 0 : 1;
    }

    std::cout << "recv after 0x06\n\n";

    mxo::auth::GetPublicKeyReply publicKeyReply;
    if (!mxo::auth::ParseGetPublicKeyReplyPacket(
            getPublicKeyReplyPacket.bytes.data(),
            getPublicKeyReplyPacket.bytes.size(),
            &publicKeyReply)) {
        std::cerr << "error: failed to parse AS_GetPublicKeyReply\n";
        closesocket(fd);
        return 1;
    }

    std::cout
        << "parsed 0x07 fields:\n"
        << "  status=" << publicKeyReply.status << "\n"
        << "  currentTime=" << publicKeyReply.currentTime << "\n"
        << "  publicKeyId=" << publicKeyReply.publicKeyId << "\n"
        << "  keySize=" << static_cast<unsigned>(publicKeyReply.keySize) << "\n"
        << "  hasEmbeddedPublicKey=" << (publicKeyReply.hasEmbeddedPublicKey ? 1 : 0) << "\n\n";

    mxo::auth::AuthBlobLayout blobLayout;
    blobLayout.embeddedTime = static_cast<uint32_t>(std::time(NULL));
    blobLayout.twofishKey.clear();

    mxo::auth::AuthRequestLayout requestLayout;
    requestLayout.publicKeyId = publicKeyReply.publicKeyId;
    requestLayout.loginType = 1;
    requestLayout.keyConfigMd5.clear();
    requestLayout.uiConfigMd5.clear();

    mxo::auth::AuthRequestBuildResult authRequest;
    if (!mxo::auth::BuildAuthRequestPacket(
            options.username,
            blobLayout,
            requestLayout,
            mxo::auth::kFrameModeAuto,
            &authRequest)) {
        std::cerr << "error: failed to build AS_AuthRequest\n";
        closesocket(fd);
        return 1;
    }

    std::cout
        << "send 0x08 AS_AuthRequest\n"
        << "  authPublicKeyId=" << requestLayout.publicKeyId << "\n"
        << "  headerLen=" << authRequest.packet.headerBytes.size() << "\n"
        << "  payloadLen=" << authRequest.packet.payloadBytes.size() << "\n"
        << "  blobLen=" << authRequest.blobCiphertextBytes.size() << "\n\n";

    if (!SendAll(fd, authRequest.packet.bytes)) {
        closesocket(fd);
        return 1;
    }

    mxo::auth::FramedPacket postAuthReply;
    timedOut = false;
    if (!ReceivePacket(fd, &postAuthReply, &timedOut)) {
        if (timedOut) {
            std::cout << "recv after 0x08: timeout\n";
            closesocket(fd);
            return 0;
        }
        std::cout << "recv after 0x08: connection closed\n";
        closesocket(fd);
        return 1;
    }

    std::cout << "recv after 0x08\n\n";

    const uint8_t postAuthRawCode = postAuthReply.payloadBytes.empty() ? 0u : postAuthReply.payloadBytes[0];
    if (postAuthRawCode == 0x09u) {
        mxo::auth::AuthChallenge authChallenge;
        if (!mxo::auth::ParseAuthChallengePacket(
                postAuthReply.bytes.data(),
                postAuthReply.bytes.size(),
                &authChallenge)) {
            std::cerr << "error: failed to parse AS_AuthChallenge\n";
            closesocket(fd);
            return 1;
        }

        std::cout << "recv 0x09 AS_AuthChallenge\n";
        if (!options.password.empty()) {
            mxo::auth::AuthChallengeResponseLayout challengeResponseLayout;
            mxo::auth::AuthChallengeResponseBuildResult challengeResponse;
            if (!mxo::auth::BuildAuthChallengeResponsePacket(
                    authChallenge.encryptedChallengeBytes,
                    authRequest.twofishKeyBytes,
                    options.password,
                    "",  // soePassword (legacy, ignored)
                    challengeResponseLayout,
                    mxo::auth::kFrameModeAuto,
                    &challengeResponse)) {
                std::cerr << "error: failed to build AS_AuthChallengeResponse\n";
                closesocket(fd);
                return 1;
            }

            std::cout << "send 0x0A AS_AuthChallengeResponse\n";
            if (!SendAll(fd, challengeResponse.packet.bytes)) {
                closesocket(fd);
                return 1;
            }

            mxo::auth::FramedPacket authReplyPacket;
            timedOut = false;
            if (!ReceivePacket(fd, &authReplyPacket, &timedOut)) {
                std::cout << "recv after 0x0A: timeout\n";
                closesocket(fd);
                return timedOut ? 0 : 1;
            }

            std::cout << "recv after 0x0A\n\n";

            if (!authReplyPacket.payloadBytes.empty() && authReplyPacket.payloadBytes[0] == 0x0bu) {
                mxo::auth::AuthReply authReply;
                if (!mxo::auth::ParseAuthReplyPacket(
                        authReplyPacket.bytes.data(),
                        authReplyPacket.bytes.size(),
                        &authReply)) {
                    std::cerr << "error: failed to parse AS_AuthReply\n";
                    closesocket(fd);
                    return 1;
                }

                if (authReply.isErrorReply) {
                    std::cout
                        << "parsed 0x0B auth error:\n"
                        << "  errorCode=0x" << std::hex << authReply.errorCode
                        << " zeroDword=0x" << authReply.zeroDword
                        << " trailingWord=0x" << authReply.trailingWord
                        << std::dec << "\n";
                } else {
                    std::cout
                        << "parsed 0x0B auth success:\n"
                        << "  characterCount=" << authReply.characterCount << "\n"
                        << "  worldCount=" << authReply.worldCount << "\n"
                        << "  username='" << authReply.username.text << "'\n"
                        << "  offsetAuthData=0x" << std::hex << authReply.offsetAuthData
                        << " offsetEncryptedData=0x" << authReply.offsetEncryptedData
                        << " offsetCharData=0x" << authReply.offsetCharData
                        << " offsetServerData=0x" << authReply.offsetServerData
                        << std::dec << "\n";
                }
            }
        } else {
            std::cout << "challenge received, but no password provided\n";
        }
    } else if (postAuthRawCode == 0x0bu) {
        mxo::auth::AuthReply authReply;
        if (!mxo::auth::ParseAuthReplyPacket(
                postAuthReply.bytes.data(),
                postAuthReply.bytes.size(),
                &authReply)) {
            std::cerr << "error: failed to parse AS_AuthReply\n";
            closesocket(fd);
            return 1;
        }

        if (authReply.isErrorReply) {
            std::cout
                << "parsed 0x0B auth error:\n"
                << "  errorCode=0x" << std::hex << authReply.errorCode
                << " zeroDword=0x" << authReply.zeroDword
                << " trailingWord=0x" << authReply.trailingWord
                << std::dec << "\n";
        } else {
            std::cout
                << "parsed 0x0B auth success:\n"
                << "  characterCount=" << authReply.characterCount << "\n"
                << "  worldCount=" << authReply.worldCount << "\n"
                << "  username='" << authReply.username.text << "'\n";
        }
    }

    std::cout << "\n";
    closesocket(fd);
    return 0;
}
