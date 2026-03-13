#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "auth/auth_crypto.h"

namespace {

struct Options {
    std::string host = "auth.lith.thematrixonline.net";
    uint16_t port = 11000;
    std::string username;
    std::string password;
    std::string soePassword;
    uint32_t launcherVersion = 76005;
    uint32_t currentPublicKeyId = 0;
    uint8_t loginType = 1;
    int timeoutMs = 5000;
    mxo::auth::FrameMode frameMode = mxo::auth::kFrameModeAuto;

    std::vector<uint8_t> keyConfigMd5;
    std::vector<uint8_t> uiConfigMd5;

    bool haveTimestampOverride = false;
    uint32_t timestampOverride = 0;

    bool haveAuthPublicKeyIdOverride = false;
    uint32_t authPublicKeyIdOverride = 0;

    uint8_t blobLeadingByte = 0x00;
    uint32_t blobRsaMethod = 4;
    uint16_t blobSomeShort = 0x001b;
    std::vector<uint8_t> blobTwofishKey;
    bool blobIncludeUsernameNullTerminator = true;
    int blobUsernameLengthAdjust = 0;

    std::vector<uint8_t> authFixedHeaderBytes;
};

static const char* GetEnvOrNull(const char* name) {
    const char* value = std::getenv(name);
    return (value && value[0]) ? value : NULL;
}

static bool ParseUnsigned32(const char* text, uint32_t* outValue) {
    if (!text || !text[0] || !outValue) {
        return false;
    }
    char* end = NULL;
    const unsigned long parsed = std::strtoul(text, &end, 0);
    if (end == text || (end && *end != '\0')) {
        return false;
    }
    *outValue = static_cast<uint32_t>(parsed);
    return true;
}

static bool ParseSignedInt(const char* text, int* outValue) {
    if (!text || !text[0] || !outValue) {
        return false;
    }
    char* end = NULL;
    const long parsed = std::strtol(text, &end, 0);
    if (end == text || (end && *end != '\0')) {
        return false;
    }
    *outValue = static_cast<int>(parsed);
    return true;
}

static bool ParseFrameMode(const char* text, mxo::auth::FrameMode* outMode) {
    if (!text || !outMode) {
        return false;
    }
    if (std::strcmp(text, "auto") == 0) {
        *outMode = mxo::auth::kFrameModeAuto;
        return true;
    }
    if (std::strcmp(text, "one") == 0 || std::strcmp(text, "1") == 0) {
        *outMode = mxo::auth::kFrameModeForceOneByte;
        return true;
    }
    if (std::strcmp(text, "two") == 0 || std::strcmp(text, "2") == 0) {
        *outMode = mxo::auth::kFrameModeForceTwoByte;
        return true;
    }
    return false;
}

static const char* FrameModeName(mxo::auth::FrameMode mode) {
    switch (mode) {
        case mxo::auth::kFrameModeAuto: return "auto";
        case mxo::auth::kFrameModeForceOneByte: return "one-byte";
        case mxo::auth::kFrameModeForceTwoByte: return "two-byte";
        default: return "<unknown>";
    }
}

static void PrintUsage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " --username <name> [options]\n"
        << "\n"
        << "Fast auth-only probe for the launcher-owned Matrix Online auth path.\n"
        << "\n"
        << "Core CLI options:\n"
        << "  --host <host>\n"
        << "  --port <port>\n"
        << "  --username <name>\n"
        << "  --password <text>\n"
        << "  --launcher-version <n>\n"
        << "  --current-public-key-id <n>\n"
        << "  --login-type <n>\n"
        << "  --keyconfig-md5 <32 hex>\n"
        << "  --uiconfig-md5 <32 hex>\n"
        << "  --timestamp <unix-seconds>\n"
        << "  --timeout-ms <ms>\n"
        << "  --frame-mode <auto|one|two>\n"
        << "\n"
        << "Advanced shaping is env-driven to keep the probe small:\n"
        << "  MXO_AUTH_PROBE_AUTH_PUBLIC_KEY_ID\n"
        << "  MXO_AUTH_PROBE_BLOB_LEADING_BYTE\n"
        << "  MXO_AUTH_PROBE_BLOB_RSA_METHOD\n"
        << "  MXO_AUTH_PROBE_BLOB_SOME_SHORT\n"
        << "  MXO_AUTH_PROBE_TWOFISH_KEY_HEX\n"
        << "  MXO_AUTH_PROBE_USERNAME_LENGTH_ADJUST\n"
        << "  MXO_AUTH_PROBE_USERNAME_NO_NUL=1\n"
        << "  MXO_AUTH_PROBE_AUTH_FIXED_HEADER35_HEX\n";
}

static bool ApplyEnvDefaults(Options* options) {
    if (!options) {
        return false;
    }

    if (const char* value = GetEnvOrNull("MXO_AUTH_PROBE_HOST")) {
        options->host = value;
    }
    uint32_t parsedU32 = 0;
    int parsedInt = 0;
    if (ParseUnsigned32(GetEnvOrNull("MXO_AUTH_PROBE_PORT"), &parsedU32)) {
        options->port = static_cast<uint16_t>(parsedU32 & 0xffffu);
    }
    if (const char* value = GetEnvOrNull("MXO_AUTH_PROBE_USERNAME")) {
        options->username = value;
    } else if (const char* value = GetEnvOrNull("MXO_USER")) {
        options->username = value;
    }
    if (const char* value = GetEnvOrNull("MXO_AUTH_PROBE_PASSWORD")) {
        options->password = value;
    } else if (const char* value = GetEnvOrNull("MXO_PASS")) {
        options->password = value;
    }
    if (const char* value = GetEnvOrNull("MXO_AUTH_PROBE_SOE_PASSWORD")) {
        options->soePassword = value;
    }
    if (ParseUnsigned32(GetEnvOrNull("MXO_AUTH_PROBE_LAUNCHER_VERSION"), &parsedU32)) {
        options->launcherVersion = parsedU32;
    }
    if (ParseUnsigned32(GetEnvOrNull("MXO_AUTH_PROBE_CURRENT_PUBLIC_KEY_ID"), &parsedU32)) {
        options->currentPublicKeyId = parsedU32;
    }
    if (ParseUnsigned32(GetEnvOrNull("MXO_AUTH_PROBE_LOGIN_TYPE"), &parsedU32)) {
        options->loginType = static_cast<uint8_t>(parsedU32 & 0xffu);
    }
    if (mxo::auth::HexDecode(
            GetEnvOrNull("MXO_AUTH_PROBE_KEYCONFIG_MD5") ? GetEnvOrNull("MXO_AUTH_PROBE_KEYCONFIG_MD5") : "",
            &options->keyConfigMd5) &&
        !options->keyConfigMd5.empty() &&
        options->keyConfigMd5.size() != 16u) {
        std::cerr << "error: MXO_AUTH_PROBE_KEYCONFIG_MD5 must decode to 16 bytes\n";
        return false;
    }
    if (mxo::auth::HexDecode(
            GetEnvOrNull("MXO_AUTH_PROBE_UICONFIG_MD5") ? GetEnvOrNull("MXO_AUTH_PROBE_UICONFIG_MD5") : "",
            &options->uiConfigMd5) &&
        !options->uiConfigMd5.empty() &&
        options->uiConfigMd5.size() != 16u) {
        std::cerr << "error: MXO_AUTH_PROBE_UICONFIG_MD5 must decode to 16 bytes\n";
        return false;
    }
    if (ParseUnsigned32(GetEnvOrNull("MXO_AUTH_PROBE_TIMESTAMP"), &parsedU32)) {
        options->haveTimestampOverride = true;
        options->timestampOverride = parsedU32;
    }
    if (ParseSignedInt(GetEnvOrNull("MXO_AUTH_PROBE_TIMEOUT_MS"), &parsedInt) && parsedInt >= 0) {
        options->timeoutMs = parsedInt;
    }
    if (const char* value = GetEnvOrNull("MXO_AUTH_PROBE_FRAME_MODE")) {
        if (!ParseFrameMode(value, &options->frameMode)) {
            std::cerr << "error: MXO_AUTH_PROBE_FRAME_MODE must be auto|one|two\n";
            return false;
        }
    }
    if (ParseUnsigned32(GetEnvOrNull("MXO_AUTH_PROBE_AUTH_PUBLIC_KEY_ID"), &parsedU32)) {
        options->haveAuthPublicKeyIdOverride = true;
        options->authPublicKeyIdOverride = parsedU32;
    }
    if (ParseUnsigned32(GetEnvOrNull("MXO_AUTH_PROBE_BLOB_LEADING_BYTE"), &parsedU32)) {
        options->blobLeadingByte = static_cast<uint8_t>(parsedU32 & 0xffu);
    }
    if (ParseUnsigned32(GetEnvOrNull("MXO_AUTH_PROBE_BLOB_RSA_METHOD"), &parsedU32)) {
        options->blobRsaMethod = parsedU32;
    }
    if (ParseUnsigned32(GetEnvOrNull("MXO_AUTH_PROBE_BLOB_SOME_SHORT"), &parsedU32)) {
        options->blobSomeShort = static_cast<uint16_t>(parsedU32 & 0xffffu);
    }
    if (const char* value = GetEnvOrNull("MXO_AUTH_PROBE_TWOFISH_KEY_HEX")) {
        if (!mxo::auth::HexDecode(value, &options->blobTwofishKey) ||
            options->blobTwofishKey.size() != 16u) {
            std::cerr << "error: MXO_AUTH_PROBE_TWOFISH_KEY_HEX must decode to 16 bytes\n";
            return false;
        }
    }
    if (ParseSignedInt(GetEnvOrNull("MXO_AUTH_PROBE_USERNAME_LENGTH_ADJUST"), &parsedInt)) {
        options->blobUsernameLengthAdjust = parsedInt;
    }
    if (GetEnvOrNull("MXO_AUTH_PROBE_USERNAME_NO_NUL")) {
        options->blobIncludeUsernameNullTerminator = false;
    }
    if (const char* value = GetEnvOrNull("MXO_AUTH_PROBE_AUTH_FIXED_HEADER35_HEX")) {
        if (!mxo::auth::HexDecode(value, &options->authFixedHeaderBytes) ||
            options->authFixedHeaderBytes.size() != 35u) {
            std::cerr << "error: MXO_AUTH_PROBE_AUTH_FIXED_HEADER35_HEX must decode to 35 bytes\n";
            return false;
        }
    }

    return true;
}

static bool ReadNextValue(int argc, char** argv, int* index, std::string* outValue) {
    if (!index || !outValue || *index + 1 >= argc) {
        return false;
    }
    *outValue = argv[++(*index)];
    return true;
}

static bool ParseArgs(int argc, char** argv, Options* options) {
    if (!options) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        std::string value;
        uint32_t parsedU32 = 0;
        int parsedInt = 0;

        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return false;
        } else if (arg == "--host") {
            if (!ReadNextValue(argc, argv, &i, &value)) return false;
            options->host = value;
        } else if (arg == "--port") {
            if (!ReadNextValue(argc, argv, &i, &value) || !ParseUnsigned32(value.c_str(), &parsedU32)) return false;
            options->port = static_cast<uint16_t>(parsedU32 & 0xffffu);
        } else if (arg == "--username") {
            if (!ReadNextValue(argc, argv, &i, &value)) return false;
            options->username = value;
        } else if (arg == "--password") {
            if (!ReadNextValue(argc, argv, &i, &value)) return false;
            options->password = value;
        } else if (arg == "--launcher-version") {
            if (!ReadNextValue(argc, argv, &i, &value) || !ParseUnsigned32(value.c_str(), &parsedU32)) return false;
            options->launcherVersion = parsedU32;
        } else if (arg == "--current-public-key-id") {
            if (!ReadNextValue(argc, argv, &i, &value) || !ParseUnsigned32(value.c_str(), &parsedU32)) return false;
            options->currentPublicKeyId = parsedU32;
        } else if (arg == "--login-type") {
            if (!ReadNextValue(argc, argv, &i, &value) || !ParseUnsigned32(value.c_str(), &parsedU32)) return false;
            options->loginType = static_cast<uint8_t>(parsedU32 & 0xffu);
        } else if (arg == "--keyconfig-md5") {
            if (!ReadNextValue(argc, argv, &i, &value) || !mxo::auth::HexDecode(value, &options->keyConfigMd5) || options->keyConfigMd5.size() != 16u) return false;
        } else if (arg == "--uiconfig-md5") {
            if (!ReadNextValue(argc, argv, &i, &value) || !mxo::auth::HexDecode(value, &options->uiConfigMd5) || options->uiConfigMd5.size() != 16u) return false;
        } else if (arg == "--timestamp") {
            if (!ReadNextValue(argc, argv, &i, &value) || !ParseUnsigned32(value.c_str(), &parsedU32)) return false;
            options->haveTimestampOverride = true;
            options->timestampOverride = parsedU32;
        } else if (arg == "--timeout-ms") {
            if (!ReadNextValue(argc, argv, &i, &value) || !ParseSignedInt(value.c_str(), &parsedInt) || parsedInt < 0) return false;
            options->timeoutMs = parsedInt;
        } else if (arg == "--frame-mode") {
            if (!ReadNextValue(argc, argv, &i, &value) || !ParseFrameMode(value.c_str(), &options->frameMode)) return false;
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

static void LogHex(const char* label, const std::vector<uint8_t>& bytes) {
    std::cout << label << mxo::auth::HexEncode(bytes) << "\n";
}

static bool ConnectTcp(const std::string& host, uint16_t port, int timeoutMs, int* outFd) {
    if (!outFd) {
        return false;
    }
    *outFd = -1;

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char portText[16] = {0};
    std::snprintf(portText, sizeof(portText), "%u", static_cast<unsigned>(port));

    struct addrinfo* results = NULL;
    const int gai = getaddrinfo(host.c_str(), portText, &hints, &results);
    if (gai != 0) {
        std::cerr << "getaddrinfo failed: " << gai_strerror(gai) << "\n";
        return false;
    }

    for (struct addrinfo* current = results; current; current = current->ai_next) {
        const int fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (fd < 0) {
            continue;
        }

        struct timeval tv;
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (connect(fd, current->ai_addr, current->ai_addrlen) == 0) {
            *outFd = fd;
            freeaddrinfo(results);
            return true;
        }

        close(fd);
    }

    freeaddrinfo(results);
    return false;
}

static bool SendAll(int fd, const std::vector<uint8_t>& bytes) {
    size_t offset = 0u;
    while (offset < bytes.size()) {
        const ssize_t written = send(fd, bytes.data() + offset, bytes.size() - offset, 0);
        if (written < 0) {
            std::cerr << "send failed: " << std::strerror(errno) << "\n";
            return false;
        }
        offset += static_cast<size_t>(written);
    }
    return true;
}

static bool ReceiveExact(int fd, size_t byteCount, std::vector<uint8_t>* outBytes, bool* outTimedOut) {
    if (!outBytes || !outTimedOut) {
        return false;
    }
    *outTimedOut = false;
    outBytes->clear();
    outBytes->reserve(byteCount);

    while (outBytes->size() < byteCount) {
        uint8_t buffer[512];
        const size_t want = (byteCount - outBytes->size() < sizeof(buffer)) ? (byteCount - outBytes->size()) : sizeof(buffer);
        const ssize_t got = recv(fd, buffer, want, 0);
        if (got == 0) {
            return false;
        }
        if (got < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                *outTimedOut = true;
                return false;
            }
            std::cerr << "recv failed: " << std::strerror(errno) << "\n";
            return false;
        }
        outBytes->insert(outBytes->end(), buffer, buffer + got);
    }
    return true;
}

static bool ReceivePacket(int fd, mxo::auth::FramedPacket* outPacket, bool* outTimedOut) {
    if (!outPacket || !outTimedOut) {
        return false;
    }

    std::vector<uint8_t> first;
    if (!ReceiveExact(fd, 1u, &first, outTimedOut)) {
        return false;
    }

    outPacket->headerBytes = first;
    size_t payloadLen = first[0];
    if (first[0] & 0x80u) {
        std::vector<uint8_t> second;
        if (!ReceiveExact(fd, 1u, &second, outTimedOut)) {
            return false;
        }
        outPacket->headerBytes.push_back(second[0]);
        payloadLen = ((static_cast<size_t>(first[0] & 0x7fu)) << 8u) | static_cast<size_t>(second[0]);
    }

    if (!ReceiveExact(fd, payloadLen, &outPacket->payloadBytes, outTimedOut)) {
        return false;
    }

    outPacket->bytes = outPacket->headerBytes;
    outPacket->bytes.insert(outPacket->bytes.end(), outPacket->payloadBytes.begin(), outPacket->payloadBytes.end());
    return true;
}

static void LogPacketSummary(const char* label, const mxo::auth::FramedPacket& packet) {
    const uint8_t rawCode = packet.payloadBytes.empty() ? 0u : packet.payloadBytes[0];
    std::cout
        << label
        << " headerLen=" << packet.headerBytes.size()
        << " payloadLen=" << packet.payloadBytes.size()
        << " byteCount=" << packet.bytes.size();
    if (!packet.payloadBytes.empty()) {
        std::cout
            << " rawCode=0x" << std::hex << static_cast<unsigned>(rawCode)
            << std::dec << " message='" << mxo::auth::AuthOpcodeName(rawCode) << "'";
    }
    std::cout << "\n";
    LogHex("  headerHex=", packet.headerBytes);
    LogHex("  payloadHex=", packet.payloadBytes);
    LogHex("  packetHex=", packet.bytes);
}

static void LogAuthReplyDetails(
    const mxo::auth::AuthReply& authReply,
    const std::vector<uint8_t>& twofishKeyBytes,
    const std::vector<uint8_t>& challengeIvBytes) {
    if (authReply.isErrorReply) {
        std::cout
            << "parsed 0x0B auth error"
            << " errorCode=0x" << std::hex << authReply.errorCode
            << " zeroDword=0x" << authReply.zeroDword
            << " trailingWord=0x" << authReply.trailingWord
            << std::dec << "\n";
        return;
    }

    std::cout
        << "parsed 0x0B auth success"
        << " characterCount=" << authReply.characterCount
        << " worldCount=" << authReply.worldCount
        << " usernameLength=" << authReply.username.length
        << " username='" << authReply.username.text << "'"
        << " offsetAuthData=0x" << std::hex << authReply.offsetAuthData
        << " offsetEncryptedData=0x" << authReply.offsetEncryptedData
        << " offsetCharData=0x" << authReply.offsetCharData
        << " offsetServerData=0x" << authReply.offsetServerData
        << " offsetUsername=0x" << authReply.offsetUsername
        << std::dec << "\n";

    std::cout
        << "  auth data: marker=0x" << std::hex << authReply.authDataMarker
        << std::dec
        << " signatureLen=" << authReply.authSignatureBytes.size()
        << " encryptedPrivateExponentLength=" << authReply.encryptedPrivateExponentLength
        << "\n";
    LogHex("  authSignatureHex=", authReply.authSignatureBytes);
    LogHex("  encryptedPrivateExponentHex=", authReply.encryptedPrivateExponentBytes);

    if (authReply.signedData.valid) {
        std::cout
            << "  signedData:"
            << " unknownByte=" << static_cast<unsigned>(authReply.signedData.unknownByte)
            << " userId1=" << authReply.signedData.userId1
            << " userName='" << authReply.signedData.userName << "'"
            << " unknownShort=" << authReply.signedData.unknownShort
            << " expiryTime=" << authReply.signedData.expiryTime
            << " publicExponent=" << authReply.signedData.publicExponent
            << " timeCreated=" << authReply.signedData.timeCreated
            << " modulusLen=" << authReply.signedData.modulusBytes.size()
            << "\n";
        LogHex("  signedDataModulusHex=", authReply.signedData.modulusBytes);
    }

    for (size_t i = 0; i < authReply.characters.size(); ++i) {
        const mxo::auth::AuthCharacterEntry& entry = authReply.characters[i];
        std::cout
            << "  character[" << i << "]"
            << " unknownByte=" << static_cast<unsigned>(entry.unknownByte)
            << " handleStringOffset=0x" << std::hex << entry.handleStringOffset
            << " characterId=0x" << entry.characterId
            << std::dec
            << " status=" << static_cast<unsigned>(entry.status)
            << " worldId=" << entry.worldId
            << " handle='" << entry.handle.text << "'"
            << " handleLength=" << entry.handle.length
            << "\n";
    }

    for (size_t i = 0; i < authReply.worlds.size(); ++i) {
        const mxo::auth::AuthWorldEntry& world = authReply.worlds[i];
        std::cout
            << "  world[" << i << "]"
            << " unknownByte=" << static_cast<unsigned>(world.unknownByte)
            << " worldId=" << world.worldId
            << " name='" << world.worldName << "'"
            << " status=" << static_cast<unsigned>(world.status)
            << " type=" << static_cast<unsigned>(world.type)
            << " clientVersion=" << world.clientVersion
            << " unknown4=" << world.unknown4
            << " load='" << static_cast<char>(world.load) << "'"
            << "\n";
    }

    std::vector<uint8_t> decryptedPrivateExponentBytes;
    if (mxo::auth::DecryptAuthReplyPrivateExponent(
            authReply,
            twofishKeyBytes,
            challengeIvBytes,
            &decryptedPrivateExponentBytes)) {
        std::cout
            << "  decryptedPrivateExponentLen=" << decryptedPrivateExponentBytes.size()
            << "\n";
        LogHex("  decryptedPrivateExponentHex=", decryptedPrivateExponentBytes);
    }
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!ApplyEnvDefaults(&options)) {
        return 2;
    }
    if (!ParseArgs(argc, argv, &options)) {
        return 2;
    }

    std::cout
        << "auth_probe config:"
        << " host='" << options.host << "'"
        << " port=" << options.port
        << " username='" << options.username << "'"
        << " passwordProvided=" << (options.password.empty() ? 0 : 1)
        << " launcherVersion=" << options.launcherVersion
        << " currentPublicKeyId=" << options.currentPublicKeyId
        << " loginType=" << static_cast<unsigned>(options.loginType)
        << " frameMode=" << FrameModeName(options.frameMode)
        << " timeoutMs=" << options.timeoutMs
        << "\n";

    int fd = -1;
    if (!ConnectTcp(options.host, options.port, options.timeoutMs, &fd)) {
        std::cerr << "error: connect failed\n";
        return 1;
    }
    std::cout << "connected\n";

    mxo::auth::FramedPacket getPublicKeyRequest;
    if (!mxo::auth::BuildGetPublicKeyRequestPacket(
            options.launcherVersion,
            options.currentPublicKeyId,
            options.frameMode,
            &getPublicKeyRequest)) {
        std::cerr << "error: failed to build AS_GetPublicKeyRequest\n";
        close(fd);
        return 1;
    }

    std::cout << "send 0x06 AS_GetPublicKeyRequest\n";
    LogPacketSummary("  outbound", getPublicKeyRequest);
    if (!SendAll(fd, getPublicKeyRequest.bytes)) {
        close(fd);
        return 1;
    }

    mxo::auth::FramedPacket getPublicKeyReplyPacket;
    bool timedOut = false;
    if (!ReceivePacket(fd, &getPublicKeyReplyPacket, &timedOut)) {
        std::cerr << "error: failed waiting for 0x07 reply" << (timedOut ? " (timeout)" : "") << "\n";
        close(fd);
        return 1;
    }

    std::cout << "recv after 0x06\n";
    LogPacketSummary("  inbound ", getPublicKeyReplyPacket);

    mxo::auth::GetPublicKeyReply publicKeyReply;
    if (!mxo::auth::ParseGetPublicKeyReplyPacket(
            getPublicKeyReplyPacket.bytes.data(),
            getPublicKeyReplyPacket.bytes.size(),
            &publicKeyReply)) {
        std::cerr << "error: failed to parse AS_GetPublicKeyReply\n";
        close(fd);
        return 1;
    }

    std::cout
        << "parsed 0x07 fields:"
        << " status=" << publicKeyReply.status
        << " currentTime=" << publicKeyReply.currentTime
        << " publicKeyId=" << publicKeyReply.publicKeyId
        << " keySize=" << static_cast<unsigned>(publicKeyReply.keySize)
        << " payloadLength=" << publicKeyReply.payloadLength
        << " hasEmbeddedPublicKey=" << (publicKeyReply.hasEmbeddedPublicKey ? 1 : 0)
        << " reservedByte=0x" << std::hex << static_cast<unsigned>(publicKeyReply.reservedByte)
        << " publicExponentByte=0x" << static_cast<unsigned>(publicKeyReply.publicExponentByte)
        << " unknownWord=0x" << static_cast<unsigned>(publicKeyReply.unknownWord)
        << std::dec
        << " modulusLength=" << publicKeyReply.modulusLength
        << " signatureLength=" << publicKeyReply.signatureLength
        << "\n";
    LogHex("  replyTailHex=", publicKeyReply.tailBytes);
    LogHex("  replyModulusHex=", publicKeyReply.modulusBytes);
    LogHex("  replySignatureHex=", publicKeyReply.signatureBytes);

    mxo::auth::AuthBlobLayout blobLayout;
    blobLayout.leadingByte = options.blobLeadingByte;
    blobLayout.rsaMethod = options.blobRsaMethod;
    blobLayout.someShort = options.blobSomeShort;
    blobLayout.embeddedTime = options.haveTimestampOverride
        ? options.timestampOverride
        : static_cast<uint32_t>(std::time(NULL));
    blobLayout.twofishKey = options.blobTwofishKey;
    blobLayout.includeUsernameNullTerminator = options.blobIncludeUsernameNullTerminator;
    blobLayout.usernameLengthAdjust = options.blobUsernameLengthAdjust;

    mxo::auth::AuthRequestLayout requestLayout;
    requestLayout.publicKeyId = options.haveAuthPublicKeyIdOverride
        ? options.authPublicKeyIdOverride
        : publicKeyReply.publicKeyId;
    requestLayout.loginType = options.loginType;
    requestLayout.keyConfigMd5 = options.keyConfigMd5;
    requestLayout.uiConfigMd5 = options.uiConfigMd5;
    requestLayout.fixedHeaderBytes = options.authFixedHeaderBytes;
    if (publicKeyReply.hasEmbeddedPublicKey) {
        requestLayout.rsaModulusBytes = publicKeyReply.modulusBytes;
        requestLayout.rsaExponentBytes.assign(1u, publicKeyReply.publicExponentByte);
    }

    mxo::auth::AuthRequestBuildResult authRequest;
    if (!mxo::auth::BuildAuthRequestPacket(
            options.username,
            blobLayout,
            requestLayout,
            options.frameMode,
            &authRequest)) {
        std::cerr << "error: failed to build AS_AuthRequest\n";
        close(fd);
        return 1;
    }

    std::cout
        << "send 0x08 AS_AuthRequest"
        << " authPublicKeyId=" << requestLayout.publicKeyId
        << " headerLen=" << authRequest.packet.headerBytes.size()
        << " payloadLen=" << authRequest.packet.payloadBytes.size()
        << " byteCount=" << authRequest.packet.bytes.size()
        << " blobLen=" << authRequest.blobCiphertextBytes.size()
        << " usernameLengthField=" << authRequest.usernameLengthField
        << " includeUsernameNul=" << (authRequest.includedUsernameNullTerminator ? 1 : 0)
        << " fixedHeaderOverride=" << (authRequest.usedFixedHeaderOverride ? 1 : 0)
        << " usedReplyPublicKey=" << (authRequest.usedProvidedPublicKey ? 1 : 0)
        << "\n";
    LogHex("  fixedHeader35Hex=", authRequest.authHeaderBytes);
    LogHex("  keyConfigMd5Hex=", authRequest.keyConfigMd5Bytes);
    LogHex("  uiConfigMd5Hex=", authRequest.uiConfigMd5Bytes);
    LogHex("  blobTwofishKeyHex=", authRequest.twofishKeyBytes);
    LogHex("  blobPlaintextHex=", authRequest.blobPlaintextBytes);
    LogHex("  blobCiphertextHex=", authRequest.blobCiphertextBytes);
    LogPacketSummary("  outbound", authRequest.packet);

    if (!SendAll(fd, authRequest.packet.bytes)) {
        close(fd);
        return 1;
    }

    mxo::auth::FramedPacket postAuthReply;
    timedOut = false;
    if (!ReceivePacket(fd, &postAuthReply, &timedOut)) {
        if (timedOut) {
            std::cout << "recv after 0x08: timeout after " << options.timeoutMs << " ms\n";
            close(fd);
            return 0;
        }
        std::cout << "recv after 0x08: connection closed or recv failed\n";
        close(fd);
        return 0;
    }

    std::cout << "recv after 0x08\n";
    LogPacketSummary("  inbound ", postAuthReply);

    const uint8_t postAuthRawCode = postAuthReply.payloadBytes.empty() ? 0u : postAuthReply.payloadBytes[0];
    if (postAuthRawCode == 0x09u) {
        mxo::auth::AuthChallenge authChallenge;
        if (!mxo::auth::ParseAuthChallengePacket(
                postAuthReply.bytes.data(),
                postAuthReply.bytes.size(),
                &authChallenge)) {
            std::cerr << "error: failed to parse AS_AuthChallenge\n";
            close(fd);
            return 1;
        }

        std::cout << "parsed 0x09 AS_AuthChallenge\n";
        LogHex("  encryptedChallengeHex=", authChallenge.encryptedChallengeBytes);

        if (options.password.empty()) {
            std::cout << "challenge received, but no password was provided; stopping before 0x0A\n";
            close(fd);
            return 0;
        }

        mxo::auth::AuthChallengeResponseLayout challengeResponseLayout;
        mxo::auth::AuthChallengeResponseBuildResult challengeResponse;
        const std::string effectiveSoePassword =
            options.soePassword.empty() ? options.password : options.soePassword;
        if (!mxo::auth::BuildAuthChallengeResponsePacket(
                authChallenge.encryptedChallengeBytes,
                authRequest.twofishKeyBytes,
                options.password,
                effectiveSoePassword,
                challengeResponseLayout,
                options.frameMode,
                &challengeResponse)) {
            std::cerr << "error: failed to build AS_AuthChallengeResponse\n";
            close(fd);
            return 1;
        }

        std::cout
            << "send 0x0A AS_AuthChallengeResponse"
            << " packetSomeShort=0x" << std::hex << challengeResponseLayout.packetSomeShort
            << std::dec
            << " passwordLengthField=" << challengeResponse.passwordLengthField
            << " soePasswordLengthField=" << challengeResponse.soePasswordLengthField
            << " paddingLengthField=" << challengeResponse.paddingLengthField
            << " plaintextLen=" << challengeResponse.plaintextBytes.size()
            << " ciphertextLen=" << challengeResponse.ciphertextBytes.size()
            << "\n";
        LogHex("  decryptedChallengeHex=", challengeResponse.decryptedChallengeBytes);
        LogHex("  processedChallengeMd5Hex=", challengeResponse.processedChallengeMd5Bytes);
        LogHex("  challengeResponsePlaintextHex=", challengeResponse.plaintextBytes);
        LogHex("  challengeResponseCiphertextHex=", challengeResponse.ciphertextBytes);
        LogPacketSummary("  outbound", challengeResponse.packet);

        if (!SendAll(fd, challengeResponse.packet.bytes)) {
            close(fd);
            return 1;
        }

        mxo::auth::FramedPacket authReplyPacket;
        timedOut = false;
        if (!ReceivePacket(fd, &authReplyPacket, &timedOut)) {
            if (timedOut) {
                std::cout << "recv after 0x0A: timeout after " << options.timeoutMs << " ms\n";
                close(fd);
                return 0;
            }
            std::cout << "recv after 0x0A: connection closed or recv failed\n";
            close(fd);
            return 0;
        }

        std::cout << "recv after 0x0A\n";
        LogPacketSummary("  inbound ", authReplyPacket);

        if (!authReplyPacket.payloadBytes.empty() && authReplyPacket.payloadBytes[0] == 0x0bu) {
            mxo::auth::AuthReply authReply;
            if (!mxo::auth::ParseAuthReplyPacket(
                    authReplyPacket.bytes.data(),
                    authReplyPacket.bytes.size(),
                    &authReply)) {
                std::cerr << "error: failed to parse AS_AuthReply\n";
                close(fd);
                return 1;
            }

            LogAuthReplyDetails(
                authReply,
                authRequest.twofishKeyBytes,
                authChallenge.encryptedChallengeBytes);
        }
    } else if (postAuthRawCode == 0x0bu) {
        mxo::auth::AuthReply authReply;
        if (mxo::auth::ParseAuthReplyPacket(
                postAuthReply.bytes.data(),
                postAuthReply.bytes.size(),
                &authReply)) {
            LogAuthReplyDetails(
                authReply,
                authRequest.twofishKeyBytes,
                std::vector<uint8_t>());
        }
    }

    close(fd);
    return 0;
}
