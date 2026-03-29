#include "pcdns.h"

#include <winsock2.h>
#include <windows.h>
#include <windns.h>

#include <cstring>
#include <spdlog/spdlog.h>

namespace mxo::libltnet {
namespace {

using DnsQueryAFn = DNS_STATUS (WINAPI*)(PCSTR, WORD, DWORD, PVOID, PDNS_RECORD*, PVOID*);
using DnsRecordListFreeFn = VOID (WINAPI*)(PDNS_RECORD, DNS_FREE_TYPE);

}  // namespace

// anchor: launcher.exe:0x0046e910
bool DNSResolveNameToIPList(
    const char* hostName,
    std::vector<uint32_t>* outIpv4NetworkOrderList,
    uint8_t flags) {
    if (!hostName || !hostName[0] || !outIpv4NetworkOrderList) {
        return false;
    }

    HMODULE dnsApiModule = LoadLibraryA("dnsapi.dll");
    DnsQueryAFn dnsQueryA = nullptr;
    DnsRecordListFreeFn dnsRecordListFree = nullptr;
    if (dnsApiModule) {
        FARPROC rawDnsQueryA = GetProcAddress(dnsApiModule, "DnsQuery_A");
        FARPROC rawDnsRecordListFree = GetProcAddress(dnsApiModule, "DnsRecordListFree");
        static_assert(sizeof(dnsQueryA) == sizeof(rawDnsQueryA));
        static_assert(sizeof(dnsRecordListFree) == sizeof(rawDnsRecordListFree));
        std::memcpy(&dnsQueryA, &rawDnsQueryA, sizeof(dnsQueryA));
        std::memcpy(&dnsRecordListFree, &rawDnsRecordListFree, sizeof(dnsRecordListFree));
    }

    bool resolvedAny = false;
    if (!dnsQueryA || !dnsRecordListFree) {
        spdlog::warn(
            "DNSResolveNameToIPList scaffold: FLAG_NOHOSTSFILE requested but dnsapi.dll DnsQuery_A/DnsRecordListFree unavailable");
    } else {
        DWORD queryFlags = 0;
        if ((flags & kDNSResolveNameToIPListFlagNoHostsFile) != 0u) {
            queryFlags = DNS_QUERY_NO_HOSTS_FILE;
        }

        PDNS_RECORD records = nullptr;
        const DNS_STATUS status = dnsQueryA(hostName, DNS_TYPE_A, queryFlags, nullptr, &records, nullptr);
        if (status == ERROR_SUCCESS && records) {
            resolvedAny = true;
            for (PDNS_RECORD record = records; record; record = record->pNext) {
                const uint32_t section = record->Flags.DW & 0x3u;
                if (record->wType != DNS_TYPE_A || (section != 0u && section != 1u)) {
                    continue;
                }
                outIpv4NetworkOrderList->push_back(record->Data.A.IpAddress);
            }
            dnsRecordListFree(records, DnsFreeRecordList);
        }
    }

    if (dnsApiModule) {
        FreeLibrary(dnsApiModule);
    }
    return resolvedAny;
}

}  // namespace mxo::libltnet
