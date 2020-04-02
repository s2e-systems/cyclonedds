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

#ifdef _WIN32
    #include <tchar.h>
    #include <setupapi.h>
    #include <cfgmgr32.h>
    #include <strsafe.h>
    #include <Iphlpapi.h>
    #include <newdev.h>
    #include <shlwapi.h>
#else
    #include <sys/ioctl.h>
    #include <arpa/inet.h>
    #include <net/if.h>
    #include <ifaddrs.h>
#endif


struct if_info {
#ifdef _WIN32
  HDEVINFO DeviceInfoSet;
  SP_DEVINFO_DATA DeviceInfoData;
#endif  
  char if_name[256];
};

struct callback_data {
  int result;
  volatile sig_atomic_t termflag;
};

static void callback(void* vdata)
{
  struct callback_data* data = (struct callback_data*)vdata;
  data->result = 1;
  data->termflag = 1;
}



static void change_address(const char* if_name, const char* ip)
{
#ifdef _WIN32
  size_t num_char;
  WCHAR IfNameW[256];
  NET_LUID IfLuid;
  DWORD IfIndex = 0;
  DWORD ret;
  ULONG NTEContext = 0;
  ULONG NTEInstance = 0;
  mbstowcs_s(&num_char, IfNameW, 256, if_name, 256);
  ConvertInterfaceAliasToLuid(IfNameW, &IfLuid);
  ConvertInterfaceLuidToIndex(&IfLuid, &IfIndex);

  struct sockaddr_in ip_addr;
  struct sockaddr_in netmask_addr;
  ddsrt_sockaddrfromstr(AF_INET, ip, &ip_addr);
  ddsrt_sockaddrfromstr(AF_INET, "255.255.0.0", &netmask_addr);
  ret = AddIPAddress(ip_addr.sin_addr.S_un.S_addr /*Address*/, netmask_addr.sin_addr.S_un.S_addr /*IpMask*/, IfIndex, &NTEContext, &NTEInstance);
  if (ret != NO_ERROR)
  {
    printf("Error adding ip address %lu\n", ret);
  }

  //IPAddr ip_new = inet_addr("10.11.12.14");
  //ULONG NTEContext_new = 0;
  //ULONG NTEInstance_new = 0;
  //ret = AddIPAddress(ip_new /*Address*/, netmask /*IpMask*/, IfIndex, &NTEContext_new, &NTEInstance_new);
  //if (ret != NO_ERROR)
  //{
  //	printf("Error adding ip address %d\n", ret);
  //}

  //DeleteIPAddress(NTEContext);

#else
  char buf[512];
  sprintf(buf, "sudo ifconfig %s %s", if_name, ip);
  int ret = system(buf);
  if (ret != 0)
  {
    CU_FAIL("Changing IP address of interface failed");
  }

#endif
}

#ifdef _WIN32
void NETIOAPI_API_ interface_change_cb(_In_ PVOID CallerContext, _In_ PMIB_IPINTERFACE_ROW Row OPTIONAL, _In_ MIB_NOTIFICATION_TYPE NotificationType)
{
    PDWORD index = (PDWORD)CallerContext;
	switch (NotificationType) {
	case MibParameterNotification:
		break;
	case MibAddInstance:
		*index = Row->InterfaceIndex;
		break;
	case MibDeleteInstance:
		break;
	case MibInitialNotification:
		break;
	default:
		;
	}

}
#endif

static int create_if(struct if_info* info)
{
#ifdef _WIN32
  DWORD dwRet;
  TCHAR InfPath[MAX_PATH];
  LPCSTR infFileName = "inf\\netloop.inf\0";
  LPCTSTR hwid = "*msloop";
  GUID ClassGUID;
  TCHAR ClassName[MAX_CLASS_NAME_LEN];
  TCHAR hwIdList[LINE_LEN + 4];
  DWORD flags = INSTALLFLAG_READONLY | INSTALLFLAG_NONINTERACTIVE;
  BOOL reboot = FALSE;
  LPTSTR windir = (LPTSTR)malloc(MAX_PATH*sizeof(TCHAR));
  HANDLE callbackChangeHandle;
  DWORD IfIndex = 0;

  dwRet = GetEnvironmentVariable(TEXT("WINDIR"), windir, MAX_PATH);
  if(dwRet == 0) {
      CU_FAIL_FATAL("WINDIR environment variable not found")
  }
  PathCombine((char *)InfPath, windir, infFileName);
  // List of hardware ID's must be double zero-terminated
  ZeroMemory(hwIdList, sizeof(hwIdList));
  if (FAILED(StringCchCopy(hwIdList, LINE_LEN, hwid))) {
    CU_FAIL_FATAL("StringCchCopy failed");
  }
  // Get the ClassName from the InfFile
  if (!SetupDiGetINFClass(InfPath, &ClassGUID, ClassName, sizeof(ClassName) / sizeof(ClassName[0]), 0)) {
    CU_FAIL_FATAL("Error getting INF class");
  }
  info->DeviceInfoSet = SetupDiCreateDeviceInfoList(&ClassGUID, 0);
  if (info->DeviceInfoSet == INVALID_HANDLE_VALUE) {
    CU_FAIL_FATAL("DeviceInfoSet is INVALID");
  }
  info->DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
  if (!SetupDiCreateDeviceInfo(info->DeviceInfoSet,
    ClassName, &ClassGUID, NULL /*DeviceDescription*/,
    0 /*hwndParent*/, DICD_GENERATE_ID, &info->DeviceInfoData)) {
    DWORD error = GetLastError();
    SetupDiDestroyDeviceInfoList(info->DeviceInfoSet);
    switch (error) {
        case ERROR_ACCESS_DENIED: CU_FAIL_FATAL("SetupDiCreateDeviceInfo failed with ERROR_ACCESS_DENIED"); break;
        default: CU_FAIL_FATAL("SetupDiCreateDeviceInfo failed");
    }
  }
  // Add the HardwareID to the Device's HardwareID property.
  if (!SetupDiSetDeviceRegistryProperty(info->DeviceInfoSet,
    &info->DeviceInfoData,
    SPDRP_HARDWAREID,
    (LPBYTE)hwIdList,
    ((DWORD)_tcslen(hwIdList) + 1 + 1) * sizeof(TCHAR))) {
    SetupDiDestroyDeviceInfoList(info->DeviceInfoSet);
    CU_FAIL_FATAL("SetupDiSetDeviceRegistryProperty failed");
  }

   // Transform the registry element into an actual devnode in the PnP HW tree.
  if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE,
    info->DeviceInfoSet,
    &info->DeviceInfoData))
  {
    DWORD error = GetLastError();
    SetupDiDestroyDeviceInfoList(info->DeviceInfoSet);
    switch (error) {
        case ERROR_IN_WOW64:
            CU_PASS("SetupDiCreateDeviceInfo failed. Skipping test");
            return -2;
        default: CU_FAIL_FATAL("SetupDiCallClassInstaller failed");
    }
  }

  NotifyIpInterfaceChange(AF_UNSPEC /*Family*/, interface_change_cb /*PIPINTERFACE_CHANGE_CALLBACK Callback*/,
    (void*)&IfIndex /*CallerContext*/, FALSE /*InitialNotification*/, &callbackChangeHandle /*NotificationHandle*/);

  if (!UpdateDriverForPlugAndPlayDevicesA(NULL, hwid, InfPath, flags, &reboot)) {
    SetupDiDestroyDeviceInfoList(info->DeviceInfoSet);
    CU_FAIL_FATAL("UpdateDriverForPlugAndPlayDevices failed");
  }

  // Wait until the interface is ready
  while (IfIndex == 0) {
    dds_sleepfor(DDS_USECS(10));
  }

  CancelMibChangeNotify2(callbackChangeHandle);

  // Get the interface name alias
  NET_LUID IfLuid;
  WCHAR IfNameW[256];
  size_t return_value;
  ConvertInterfaceIndexToLuid(IfIndex, &IfLuid);
  ConvertInterfaceLuidToAlias(&IfLuid, IfNameW, 256);
  wcstombs_s(&return_value, info->if_name, 256, IfNameW, _TRUNCATE);

  return 1;

#else
  char buf[512];
  sprintf(buf, "sudo ip link add %s type dummy", info->if_name);
  int ret = system(buf);
  if (ret != 0)
  {
    CU_FAIL("Creating interface failed");
  }
  return 1;
#endif
}

static void delete_if(struct if_info* info)
{
#ifdef _WIN32
  SP_REMOVEDEVICE_PARAMS rmdParams;

  rmdParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
  rmdParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
  rmdParams.Scope = DI_REMOVEDEVICE_GLOBAL;
  rmdParams.HwProfile = 0;
  if (!SetupDiSetClassInstallParams(info->DeviceInfoSet, &info->DeviceInfoData, &rmdParams.ClassInstallHeader, sizeof(rmdParams)) ||
    !SetupDiCallClassInstaller(DIF_REMOVE, info->DeviceInfoSet, &info->DeviceInfoData)) {
    CU_FAIL_FATAL("Error removing interface");
  }

  memset(info, 0, sizeof(*info));
#else
  char buf[512];
  sprintf(buf, "sudo ip link delete %s", info->if_name);
  int ret = system(buf);
  if (ret != 0)
  {
    CU_FAIL("Deleting interface failed");
  }
#endif
}

CU_Init(ddsrt_ip_change_notify)
{
  ddsrt_init();
  return 0;
}

CU_Clean(ddsrt_ip_change_notify)
{
  ddsrt_fini();
  return 0;
}

CU_Test(ddsrt_ip_change_notify, ipv4_multiple_interfaces, .timeout = 60)
{
  const int expected = 1;

  const char* ip_if_two = "10.13.0.1";
  const char* ip_before = "10.12.0.1";
  const char* ip_after = "10.12.0.2";
  struct if_info info_one = { 0 };
  struct if_info info_two = { 0 };

  #ifdef __linux__
    sprintf(info_one.if_name, "eth11");
    sprintf(info_two.if_name, "eth12");
  #endif

  int ret = create_if(&info_one);
  if (ret == -2)
  {
      return;
  }
  create_if(&info_two);


  change_address(info_one.if_name, ip_before);
  change_address(info_two.if_name, ip_if_two);
  struct callback_data data = {.result = 0, .termflag = 0};


  struct ddsrt_ip_change_notify_data* icnd = ddsrt_ip_change_notify_new(&callback, info_one.if_name, &data);
  // Wait before changing the address so that the monitoring thread can get started
  dds_sleepfor(1000000000);
  change_address(info_one.if_name, ip_after);
  while (!data.termflag)
  {
    dds_sleepfor(10);
  }

  CU_ASSERT_EQUAL(expected, data.result);

  ddsrt_ip_change_notify_free(icnd);
  delete_if(&info_one);
  delete_if(&info_two);
}


CU_Test(ddsrt_ip_change_notify, ipv4_correct_interface, .timeout = 60)
{
  const int expected_if_one = 1;
  const int expected_if_two = 0;

  const char* ip_if_two = "21.13.0.1";
  const char* ip_before = "21.12.0.1";
  const char* ip_after = "21.12.0.2";

  struct if_info info_one = { 0 };
  struct if_info info_two = { 0 };

  struct callback_data data_one = {.result = 0, .termflag = 0};
  struct callback_data data_two = {.result = 0, .termflag = 0};

#ifdef __linux__
  sprintf(info_one.if_name, "eth13");
  sprintf(info_two.if_name, "eth14");
#endif
  int ret = create_if(&info_one);
  if (ret == -2)
  {
      return;
  }
  create_if(&info_two);

  change_address(info_one.if_name, ip_before);
  change_address(info_two.if_name, ip_if_two);


  struct ddsrt_ip_change_notify_data* icnd_one = ddsrt_ip_change_notify_new(&callback, info_one.if_name, &data_one);
  struct ddsrt_ip_change_notify_data* icnd_two = ddsrt_ip_change_notify_new(&callback, info_two.if_name, &data_two);

  // Wait before changing the address so that the monitoring thread can get started
  dds_sleepfor(DDS_SECS(1));
  change_address(info_one.if_name, ip_after);

  while (!data_one.termflag)
  {
    dds_sleepfor(DDS_USECS(10));
  }
  // Wait for one second and afterwards check no changes were triggered
  dds_sleepfor(DDS_SECS(1));

  CU_ASSERT_EQUAL(expected_if_one, data_one.result);
  CU_ASSERT_EQUAL(expected_if_two, data_two.result);

  ddsrt_ip_change_notify_free(icnd_one);
  ddsrt_ip_change_notify_free(icnd_two);
  delete_if(&info_one);
  delete_if(&info_two);
}


CU_Test(ddsrt_ip_change_notify, create_and_free, .timeout = 50)
{
  const int expected = 1;

  const char* ip_before = "30.12.0.1";
  const char* ip_after = "30.12.0.2";
  struct ddsrt_ip_change_notify_data* icnd;

  struct if_info info_one = { 0 };
  struct callback_data data = {.result = 0, .termflag = 0};

#ifdef __linux__
  sprintf(info_one.if_name, "eth16");
#endif
  int ret = create_if(&info_one);
  if (ret == -2)
  {
      return;
  }
  change_address(info_one.if_name, ip_before);

  icnd = ddsrt_ip_change_notify_new(NULL, info_one.if_name, NULL);
  ddsrt_ip_change_notify_free(icnd);

  icnd = ddsrt_ip_change_notify_new(&callback, info_one.if_name, &data);
  // Wait before changing the address so that the monitoring thread can get started
  dds_sleepfor(DDS_MSECS(1000));
  change_address(info_one.if_name, ip_after);
  while (!data.termflag)
  {
    dds_sleepfor(DDS_USECS(10));
  }
  CU_ASSERT_EQUAL(expected, data.result);

  ddsrt_ip_change_notify_free(icnd);
  delete_if(&info_one);
}

