#include "netif.h"
#include "util/mallocHelper.h"

#include <ws2tcpip.h> // AF_INET6, IN6_IS_ADDR_UNSPECIFIED
#include <iphlpapi.h>

bool ffNetifGetDefaultRouteImpl(char iface[IF_NAMESIZE + 1], uint32_t* ifIndex)
{
    PMIB_IPFORWARD_TABLE2 pIpForwardTable = NULL;

    if (!NETIO_SUCCESS(GetIpForwardTable2(AF_UNSPEC, &pIpForwardTable)))
        return false;

    bool foundDefault = false;
    uint32_t smallestMetric = UINT32_MAX;

    for (ULONG i = 0; i < pIpForwardTable->NumEntries; ++i)
    {
        MIB_IPFORWARD_ROW2* row = &pIpForwardTable->Table[i];

        if ((row->DestinationPrefix.PrefixLength == 0) &&
            ((row->DestinationPrefix.Prefix.Ipv4.sin_family == AF_INET &&
              row->DestinationPrefix.Prefix.Ipv4.sin_addr.S_un.S_addr == 0) ||
             (row->DestinationPrefix.Prefix.Ipv6.sin6_family == AF_INET6 &&
              IN6_IS_ADDR_UNSPECIFIED(&row->DestinationPrefix.Prefix.Ipv6.sin6_addr))))
        {
            MIB_IF_ROW2 ifRow = {
                .InterfaceIndex = row->InterfaceIndex,
            };
            if (NETIO_SUCCESS(GetIfEntry2(&ifRow)) &&
                ifRow.OperStatus == IfOperStatusUp)
            {
                MIB_IPINTERFACE_ROW ipInterfaceRow = {
                    .Family = (row->DestinationPrefix.Prefix.Ipv4.sin_family == AF_INET) ? AF_INET : AF_INET6,
                    .InterfaceIndex = row->InterfaceIndex,
                };

                uint32_t realMetric = row->Metric /* Metric offset */;

                if (NETIO_SUCCESS(GetIpInterfaceEntry(&ipInterfaceRow)))
                    realMetric += ipInterfaceRow.Metric /* Interface metric */;

                if (realMetric < smallestMetric)
                {
                    smallestMetric = realMetric;
                    *ifIndex = row->InterfaceIndex;
                    if (WideCharToMultiByte(CP_UTF8, 0, ifRow.Alias, -1, iface, IF_NAMESIZE, NULL, NULL) > 0)
                        iface[IF_NAMESIZE] = '\0';
                    foundDefault = true;
                }
            }
        }
    }

    FreeMibTable(pIpForwardTable);

    return foundDefault;
}
