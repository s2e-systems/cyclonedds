#include "dds/ddsrt/ip_change.h"

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/ifaddrs.h"

#include "CUnit/Test.h"

#include <Winsock2.h>
#include <iphlpapi.h>

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

static void printIPAddress(void)
{
    // The following code is from the ifaddr tests,
    // It does crash though if the address of the interface
    // is changed. This is because there is a short moment
    // when the interface has no address before it gests the
    // new one. This losing of the address causes a trigger
    // of the "IP address changed" as well.

    //    dds_return_t ret;
    //    ddsrt_ifaddrs_t *ifa_root, *ifa;
    //    const int afs[] = { AF_INET, DDSRT_AF_TERM };

    //    ret = ddsrt_getifaddrs(&ifa_root, afs);
    //    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    //    for (ifa = ifa_root; ifa; ifa = ifa->next) {
    //      CU_ASSERT_PTR_NOT_EQUAL_FATAL(ifa->addr, NULL);
    //      CU_ASSERT_EQUAL(ifa->addr->sa_family, AF_INET);
    //      if (ifa->addr->sa_family == AF_INET) {
    //        if (ifa->flags & IFF_LOOPBACK) {
    //          CU_ASSERT(ddsrt_sockaddr_isloopback(ifa->addr));
    //        } else {
    //          printf("IP adress: %s\n", ifa->addr->sa_data);
    //          CU_ASSERT(!ddsrt_sockaddr_isloopback(ifa->addr));
    //        }
    //      }
    //    }

    //    ddsrt_freeifaddrs(ifa_root);



    PMIB_IPADDRTABLE pIPAddrTable;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    IN_ADDR IPAddr;

    pIPAddrTable = (MIB_IPADDRTABLE *) MALLOC(sizeof (MIB_IPADDRTABLE));
    if (pIPAddrTable == NULL) {
        CU_FAIL("");
    }
    else
    {
        dwSize = 0;
        // Make an initial call to GetIpAddrTable to get the
        // necessary size into the dwSize variable
        if (GetIpAddrTable(pIPAddrTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER)
        {
            FREE(pIPAddrTable);
            pIPAddrTable = (MIB_IPADDRTABLE *) MALLOC(dwSize);

        }
        if (pIPAddrTable == NULL)
        {
            CU_FAIL("");
        }
    }
    // Make a second call to GetIpAddrTable to get the
    // actual data we want
    dwRetVal = GetIpAddrTable(pIPAddrTable, &dwSize, 0);
    if (dwRetVal == NO_ERROR)
    {
        // Save the interface index to use for adding an IP address
        for (DWORD i = 0; i < pIPAddrTable->dwNumEntries; i++)
        {
            struct _MIB_IPADDRROW_XP table = pIPAddrTable->table[i];
            IPAddr.S_un.S_addr = (u_long)table.dwAddr;
            printf("\tIP Address:       \t%s (%lu)\n", inet_ntoa(IPAddr), table.dwAddr);
            IPAddr.S_un.S_addr = (u_long)table.dwMask;
            printf("\tSubnet Mask:      \t%s (%lu)\n", inet_ntoa(IPAddr), table.dwMask);
            IPAddr.S_un.S_addr = (u_long)table.dwBCastAddr;
            printf("\tBroadCast Address:\t%s (%lu)\n\n", inet_ntoa(IPAddr), table.dwBCastAddr);
        }
    }
    else
    {
        printf("Call to GetIpAddrTable failed with error %lu.\n", dwRetVal);
        if (pIPAddrTable)
        {
            FREE(pIPAddrTable);
        }
        CU_FAIL("");
    }

    if (pIPAddrTable)
    {
        FREE(pIPAddrTable);
        pIPAddrTable = NULL;
    }
}


CU_Init(ddsrt_dhcp)
{
    ddsrt_init();
    return 0;
}

CU_Clean(ddsrt_dhcp)
{
    ddsrt_fini();
    return 0;
}


CU_Test(ddsrt_ip_change_notify, ipv4)
{
    struct ddsrt_ip_change_notify_data* icnd = ddsrt_ip_change_notify_new(&printIPAddress, NULL);



    ddsrt_ip_change_notify_free(icnd);

}

