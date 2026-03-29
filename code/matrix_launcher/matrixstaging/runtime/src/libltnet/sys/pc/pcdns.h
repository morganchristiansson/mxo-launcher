#pragma once

#include <cstdint>
#include <vector>

namespace mxo::libltnet {

static constexpr uint8_t kDNSResolveNameToIPListFlagNoHostsFile = 0x04;

// Current best static read from launcher.exe `pcdns.cpp`:
// - `0x0046e910 = DNSResolveNameToIPList`
// - optional dnsapi.dll path using `DnsQuery_A` / `DnsRecordListFree`
// - when bit `0x04` is set, original passes `DNS_QUERY_NO_HOSTS_FILE`
// - result appends IPv4 A-records into the caller-supplied list object
bool DNSResolveNameToIPList(
    const char* hostName,
    std::vector<uint32_t>* outIpv4NetworkOrderList,
    uint8_t flags);

}  // namespace mxo::libltnet
