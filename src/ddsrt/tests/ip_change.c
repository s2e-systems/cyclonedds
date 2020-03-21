#include "dds/ddsrt/ip_change.h"


#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/time.h"

#include "CUnit/Test.h"

#ifndef _WIN32
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#else
#include <windows.h>
#include <tchar.h>
#include <setupapi.h>
#include <regstr.h>
#include <infstr.h>
#include <cfgmgr32.h>
#include <malloc.h>
#include <newdev.h>
#include <objbase.h>
#include <strsafe.h>
#include <io.h>
#include <fcntl.h>

typedef BOOL(WINAPI* UpdateDriverForPlugAndPlayDevicesProto)(_In_opt_ HWND hwndParent,
    _In_ LPCTSTR HardwareId,
    _In_ LPCTSTR FullInfPath,
    _In_ DWORD InstallFlags,
    _Out_opt_ PBOOL bRebootRequired
    );

#ifdef _UNICODE
#define UPDATEDRIVERFORPLUGANDPLAYDEVICES "UpdateDriverForPlugAndPlayDevicesW"
#define SETUPUNINSTALLOEMINF "SetupUninstallOEMInfW"
#else
#define UPDATEDRIVERFORPLUGANDPLAYDEVICES "UpdateDriverForPlugAndPlayDevicesA"
#define SETUPUNINSTALLOEMINF "SetupUninstallOEMInfA"
#endif
#define SETUPSETNONINTERACTIVEMODE "SetupSetNonInteractiveMode"
#define SETUPVERIFYINFFILE "SetupVerifyInfFile"
#endif

static volatile sig_atomic_t gtermflag = 0;

static char *get_ip_str(const struct sockaddr *sa, char *s, socklen_t maxlen)
{
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    s, maxlen);
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                    s, maxlen);
            break;

        default:
            ddsrt_strlcpy(s, "Unknown AF", maxlen);
            return NULL;
    }

    return s;
}

static void printIPAddress()
{
  dds_return_t ret;
  ddsrt_ifaddrs_t *ifa_root, *ifa;
  const int afs[] = { AF_INET, DDSRT_AF_TERM };
  ret = ddsrt_getifaddrs(&ifa_root, afs);
  if (ret == DDS_RETCODE_OK)
  {
    for (ifa = ifa_root; ifa; ifa = ifa->next)
    {
        char ip_addr[AF_MAX];
        get_ip_str(ifa->addr, (char*)&ip_addr, AF_MAX);
      printf("if: %s (%s)\n", ifa->name, ip_addr);
    }
  }
  ddsrt_freeifaddrs(ifa_root);
}

static void callback(void* vdata)
{
  printIPAddress();
  int* data = (int*)vdata;
  *data = 1;
  gtermflag = 1;
}

#ifndef _WIN32
static void checkIoctlError(int e, int i)
{
  if (e == -1)
  {
    printf("At %d:", i);
    char* str = malloc(256);
    switch (errno)
    {
    case EBADF:
      str = "fd is not a valid file descriptor";
      break;
    case EFAULT:
      str = "argp references an inaccessible memory area.";
      break;
    case EINVAL:
      str = "request or argp is not valid.";
      break;
    case ENOTTY:
      str = "fd is not associated with a character special device. The specified request does not apply to the kind of object that the file descriptor fd references.";
          break;
    default:
      str = strerror(errno);
    }
    printf("ioctl error: %s\n", str);
  }
}


static void change_address(const char *ip)
{
  const char * name = "eth11";
  struct ifreq ifr;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

  strncpy(ifr.ifr_name, name, IFNAMSIZ);

  ifr.ifr_addr.sa_family = AF_INET;
  struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
  int r1 = inet_pton(AF_INET, ip, &addr->sin_addr);
  if (r1 != 1)
  {
    exit(-1);
  }
  int r2 = ioctl(fd, SIOCSIFADDR, &ifr);
  checkIoctlError(r2, 2);

  close(fd);
}

static void create_if()
{
  system("sudo ip link add eth11 type dummy");
}

static void delete_if()
{
  system("sudo ip link delete eth11");
}
#else

HDEVINFO DeviceInfoSet = INVALID_HANDLE_VALUE;
SP_DEVINFO_DATA DeviceInfoData;

static int create_if()
{
    
    TCHAR InfPath[MAX_PATH];
    LPCSTR inf = "C:\\Windows\\inf\\netloop.inf\0";
    LPCTSTR hwid = "*msloop";
    GUID ClassGUID;
    TCHAR ClassName[MAX_CLASS_NAME_LEN];
    
    TCHAR hwIdList[LINE_LEN + 4];

    HMODULE newdevMod = NULL;
    UpdateDriverForPlugAndPlayDevicesProto UpdateFn;
    DWORD flags = 0;
    BOOL reboot = FALSE;

    //
    // Inf must be a full pathname
    //
    if (GetFullPathName(inf, MAX_PATH, InfPath, NULL) >= MAX_PATH) {
        //
        // inf pathname too long
        //
        printf("Pathname too long");
        return -1;
    }

    //
    // List of hardware ID's must be double zero-terminated
    //
    ZeroMemory(hwIdList, sizeof(hwIdList));
    if (FAILED(StringCchCopy(hwIdList, LINE_LEN, hwid))) {
        goto final;
    }

    if (!SetupDiGetINFClass(InfPath, &ClassGUID, ClassName, sizeof(ClassName) / sizeof(ClassName[0]), 0))
    {
        printf("Error getting INF class");
        return -1;
    }

    DeviceInfoSet = SetupDiCreateDeviceInfoList(&ClassGUID, 0);
    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
        goto final;
    }

    //
    // Now create the element.
    // Use the Class GUID and Name from the INF file.
    //
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    if (!SetupDiCreateDeviceInfo(DeviceInfoSet,
        ClassName,
        &ClassGUID,
        NULL,
        0,
        DICD_GENERATE_ID,
        &DeviceInfoData))
    {
        DWORD error = GetLastError();
        printf("Error: %u", error);
        goto final;
    }

    //
    // Add the HardwareID to the Device's HardwareID property.
    //
    if (!SetupDiSetDeviceRegistryProperty(DeviceInfoSet,
        &DeviceInfoData,
        SPDRP_HARDWAREID,
        (LPBYTE)hwIdList,
        ((DWORD)_tcslen(hwIdList) + 1 + 1) * sizeof(TCHAR)))
    {
        printf("Failed to add hwid\n");
        goto final;
    }

    //
   // Transform the registry element into an actual devnode
   // in the PnP HW tree.
   //
    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE,
        DeviceInfoSet,
        &DeviceInfoData))
    {
        printf("Failed SetupDiCallClassInstaller\n");
        DWORD error = GetLastError();
        printf("Error: %u\n", error);
        goto final;
    }

    //
    // make use of UpdateDriverForPlugAndPlayDevices
    //
    newdevMod = LoadLibrary(TEXT("newdev.dll"));
    if (!newdevMod) {
        goto final;
    }
    UpdateFn = (UpdateDriverForPlugAndPlayDevicesProto)GetProcAddress(newdevMod, UPDATEDRIVERFORPLUGANDPLAYDEVICES);
    if (!UpdateFn)
    {
        goto final;
    }

    //FormatToStream(stdout, inf ? MSG_UPDATE_INF : MSG_UPDATE, hwid, inf);

    if (!UpdateFn(NULL, hwid, inf, flags, &reboot)) {
        goto final;
    }

    return 1;

    final:

    //printf("Error: %s", GetLastError());
    if (DeviceInfoSet != INVALID_HANDLE_VALUE) {
        SetupDiDestroyDeviceInfoList(DeviceInfoSet);
    }

    return -1;
}

static void delete_if()
{
    SP_REMOVEDEVICE_PARAMS rmdParams;
    LPCTSTR action = NULL;
    
    rmdParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    rmdParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
    rmdParams.Scope = DI_REMOVEDEVICE_GLOBAL;
    rmdParams.HwProfile = 0;
    if (!SetupDiSetClassInstallParams(DeviceInfoSet, &DeviceInfoData, &rmdParams.ClassInstallHeader, sizeof(rmdParams)) ||
        !SetupDiCallClassInstaller(DIF_REMOVE, DeviceInfoSet, &DeviceInfoData)) {
        //
        // failed to invoke DIF_REMOVE
        //
        printf("Error removing");
    }


}

static void change_address(const char *ip)
{
    DDSRT_UNUSED_ARG(ip);
}

#endif

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
  create_if();

  dds_sleepfor(10000000000);

  const char *ip_before = "10.12.0.1";
  change_address(ip_before);

  printIPAddress();

  const int expected = 1;
  int result = 0;

  struct ddsrt_ip_change_notify_data* icnd = ddsrt_ip_change_notify_new(&callback, &result);

  const char *ip_after = "10.12.0.2";
  change_address(ip_after);

  while(!gtermflag)
  {
    dds_sleepfor(10);
  }
  ddsrt_ip_change_notify_free(icnd);

  delete_if();

  CU_ASSERT_EQUAL(expected, result);

}

