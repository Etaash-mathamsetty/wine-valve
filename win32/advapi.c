/*
 * Win32 advapi functions
 *
 * Copyright 1995 Sven Verdoolaege
 */

#include <unistd.h>
#include <time.h>
#include "windows.h"
#include "winerror.h"
#include "shell.h"
#include "heap.h"
#include "debug.h"

/***********************************************************************
 *           StartServiceCtrlDispatcherA   [ADVAPI32.196]
 */
BOOL32 WINAPI StartServiceCtrlDispatcher32A(LPSERVICE_TABLE_ENTRY32A servent)
{	LPSERVICE_TABLE_ENTRY32A ptr = servent;

	while (ptr->lpServiceName)
	{ FIXME(advapi,"%s at %p\n", ptr->lpServiceName, ptr);
	  ptr++;
	}
	return TRUE;
}

/***********************************************************************
 *           StartServiceCtrlDispatcherW   [ADVAPI32.197]
 */
BOOL32 WINAPI StartServiceCtrlDispatcher32W(LPSERVICE_TABLE_ENTRY32W servent)
{	LPSERVICE_TABLE_ENTRY32W ptr = servent;
	LPSERVICE_MAIN_FUNCTION32W fpMain;
	
	while (ptr->lpServiceName)
	{ FIXME(advapi,"%s at %p): STUB.\n", debugstr_w(ptr->lpServiceName),ptr);
	  fpMain = ptr->lpServiceProc;
	  fpMain(0,NULL);	/* try to start the service */
	  ptr++;
	}
	return TRUE;
}

typedef DWORD       SERVICE_STATUS_HANDLE; 
typedef VOID (WINAPI *LPHANDLER_FUNCTION)( DWORD    dwControl);
 /***********************************************************************
 *           RegisterServiceCtrlHandlerA   [ADVAPI32.176]
 */
SERVICE_STATUS_HANDLE WINAPI RegisterServiceCtrlHandlerA (LPSTR lpServiceName, LPHANDLER_FUNCTION lpfHandler)
{	FIXME(advapi,"%s %p\n", lpServiceName, lpfHandler);
	return 0xcacacafe;	
}
 /***********************************************************************
 *           RegisterServiceCtrlHandlerW   [ADVAPI32.177]
 */
SERVICE_STATUS_HANDLE WINAPI RegisterServiceCtrlHandlerW (LPWSTR lpServiceName, LPHANDLER_FUNCTION lpfHandler)
{	FIXME(advapi,"%s %p\n", debugstr_w(lpServiceName), lpfHandler);
	return 0xcacacafe;	
}
 /***********************************************************************
 *           SetServiceStatus   [ADVAPI32.192]
 */
BOOL32 WINAPI SetServiceStatus(SERVICE_STATUS_HANDLE hService, LPSERVICE_STATUS lpStatus)
{	FIXME(advapi,"%lx %p\n",hService, lpStatus);
	TRACE(advapi,"\tType:%lx\n",lpStatus->dwServiceType);
	TRACE(advapi,"\tState:%lx\n",lpStatus->dwCurrentState);
	TRACE(advapi,"\tControlAccepted:%lx\n",lpStatus->dwControlsAccepted);
	TRACE(advapi,"\tExitCode:%lx\n",lpStatus->dwWin32ExitCode);
	TRACE(advapi,"\tServiceExitCode:%lx\n",lpStatus->dwServiceSpecificExitCode);
	TRACE(advapi,"\tCheckPoint:%lx\n",lpStatus->dwCheckPoint);
	TRACE(advapi,"\tWaitHint:%lx\n",lpStatus->dwWaitHint);
	return TRUE;
}
/******************************************************************************
 * OpenProcessToken [ADVAPI32.109]
 * Opens the access token associated with a process
 *
 * PARAMS
 *    ProcessHandle [I] Handle to process
 *    DesiredAccess [I] Desired access to process
 *    TokenHandle   [O] Pointer to handle of open access token
 *
 * RETURNS STD
 */
BOOL32 WINAPI OpenProcessToken( HANDLE32 ProcessHandle, DWORD DesiredAccess,
                                HANDLE32 *TokenHandle )
{
    FIXME(advapi,"(%08x,%08lx,%p): stub\n",ProcessHandle,DesiredAccess,
          TokenHandle);
    return TRUE;
}


/***********************************************************************
 *           OpenThreadToken		[ADVAPI32.114]
 */
BOOL32 WINAPI OpenThreadToken( HANDLE32 thread,DWORD desiredaccess,
                               BOOL32 openasself,HANDLE32 *thandle )
{
	FIXME(advapi,"(%08x,%08lx,%d,%p): stub!\n",
	      thread,desiredaccess,openasself,thandle);
	return TRUE;
}


/******************************************************************************
 * LookupPrivilegeValue32A                                        [ADVAPI32.92]
 */
BOOL32 WINAPI LookupPrivilegeValue32A( LPCSTR lpSystemName, 
                                       LPCSTR lpName, LPVOID lpLuid)
{
    LPWSTR lpSystemNameW = HEAP_strdupAtoW(GetProcessHeap(), 0, lpSystemName);
    LPWSTR lpNameW = HEAP_strdupAtoW(GetProcessHeap(), 0, lpName);
    BOOL32 ret = LookupPrivilegeValue32W( lpSystemNameW, lpNameW, lpLuid);
    HeapFree(GetProcessHeap(), 0, lpNameW);
    HeapFree(GetProcessHeap(), 0, lpSystemNameW);
    return ret;
}


/******************************************************************************
 * LookupPrivilegeValue32W                                        [ADVAPI32.93]
 * Retrieves LUID used on a system to represent the privilege name.
 *
 * NOTES
 *    lpLuid should be PLUID
 *
 * PARAMS
 *    lpSystemName [I] Address of string specifying the system
 *    lpName       [I] Address of string specifying the privilege
 *    lpLuid       [I] Address of locally unique identifier
 *
 * RETURNS STD
 */
BOOL32 WINAPI LookupPrivilegeValue32W( LPCWSTR lpSystemName,
                                       LPCWSTR lpName, LPVOID lpLuid )
{
    FIXME(advapi,"(%s,%s,%p): stub\n",debugstr_w(lpSystemName), 
                  debugstr_w(lpName), lpLuid);
    return TRUE;
}


/***********************************************************************
 *           AdjustTokenPrivileges   [ADVAPI32.10]
 */
BOOL32 WINAPI AdjustTokenPrivileges(HANDLE32 TokenHandle,BOOL32 DisableAllPrivileges,
	LPVOID NewState,DWORD BufferLength,LPVOID PreviousState,
	LPDWORD ReturnLength )
{
	return TRUE;
}

/***********************************************************************
 *           GetTokenInformation	[ADVAPI32.66]
 */
BOOL32 WINAPI GetTokenInformation(
	HANDLE32 token,TOKEN_INFORMATION_CLASS tokeninfoclass,LPVOID tokeninfo,
	DWORD tokeninfolength,LPDWORD retlen
) {
        FIXME(advapi,"(%08x,%d,%p,%ld,%p): stub\n",
	      token,tokeninfoclass,tokeninfo,tokeninfolength,retlen);
	return TRUE;
}


/******************************************************************************
 * OpenSCManager32A [ADVAPI32.110]
 */
HANDLE32 WINAPI OpenSCManager32A( LPCSTR lpMachineName, LPCSTR lpDatabaseName,
                                  DWORD dwDesiredAccess )
{
    LPWSTR lpMachineNameW = HEAP_strdupAtoW(GetProcessHeap(),0,lpMachineName);
    LPWSTR lpDatabaseNameW = HEAP_strdupAtoW(GetProcessHeap(),0,lpDatabaseName);
    DWORD ret = OpenSCManager32W(lpMachineNameW,lpDatabaseNameW,
                                 dwDesiredAccess);
    HeapFree(GetProcessHeap(),0,lpDatabaseNameW);
    HeapFree(GetProcessHeap(),0,lpMachineNameW);
    return ret;
}


/******************************************************************************
 * OpenSCManager32W [ADVAPI32.111]
 * Establishes a connection to the service control manager and opens database
 *
 * NOTES
 *    This should return a SC_HANDLE
 *
 * PARAMS
 *    lpMachineName   [I] Pointer to machine name string
 *    lpDatabaseName  [I] Pointer to database name string
 *    dwDesiredAccess [I] Type of access
 *
 * RETURNS
 *    Success: Handle to service control manager database
 *    Failure: NULL
 */
HANDLE32 WINAPI OpenSCManager32W( LPCWSTR lpMachineName, LPCWSTR lpDatabaseName,
                                  DWORD dwDesiredAccess )
{
    FIXME(advapi,"(%s,%s,0x%08lx): stub\n", debugstr_w(lpMachineName), 
          debugstr_w(lpDatabaseName), dwDesiredAccess);
    return 1;
}


BOOL32 WINAPI AllocateLocallyUniqueId(LPLUID lpluid) {
	lpluid->LowPart = time(NULL);
	lpluid->HighPart = 0;
	return TRUE;
}


/******************************************************************************
 * ControlService [ADVAPI32.23]
 * Sends a control code to a Win32-based service.
 *
 * NOTES
 *    hService should be SC_HANDLE
 *
 * RETURNS STD
 */
BOOL32 WINAPI ControlService( HANDLE32 hService, DWORD dwControl, 
                              LPSERVICE_STATUS lpServiceStatus )
{
    FIXME(advapi, "(%d,%ld,%p): stub\n",hService,dwControl,lpServiceStatus);
    return TRUE;
}


/******************************************************************************
 * CloseServiceHandle [ADVAPI32.22]
 * Close handle to service or service control manager
 *
 * PARAMS
 *    hSCObject [I] Handle to service or service control manager database
 *
 * NOTES
 *    hSCObject should be SC_HANDLE
 *
 * RETURNS STD
 */
BOOL32 WINAPI CloseServiceHandle( HANDLE32 hSCObject )
{
    FIXME(advapi, "(%d): stub\n", hSCObject);
    return TRUE;
}


/******************************************************************************
 * GetFileSecurityA [32.45]
 * Obtains Specified information about the security of a file or directory
 * The information obtained is constrained by the callers acces rights and
 * priviliges
 */

BOOL32 WINAPI GetFileSecurity32A( LPCSTR lpFileName,
                                SECURITY_INFORMATION RequestedInformation,
                                LPSECURITY_DESCRIPTOR pSecurityDescriptor,
                                DWORD nLength,
                                LPDWORD lpnLengthNeeded)
{
  FIXME(advapi, "(%s) : stub\n", debugstr_a(lpFileName));
  return TRUE;
}

/******************************************************************************
 * GetFileSecurityiW [32.46]
 * Obtains Specified information about the security of a file or directory
 * The information obtained is constrained by the callers acces rights and
 * priviliges
 */

BOOL32 WINAPI GetFileSecurity32W( LPCWSTR lpFileName,
                                SECURITY_INFORMATION RequestedInformation,
                                LPSECURITY_DESCRIPTOR pSecurityDescriptor,
                                DWORD nLength,
                                LPDWORD lpnLengthNeeded)
{
  FIXME(advapi, "(%s) : stub\n", debugstr_w(lpFileName) ); 
  return TRUE;
}

/******************************************************************************
 * SetFileSecurityA [32.182]
 * Sets the security of a file or directory
 */

BOOL32 WINAPI SetFileSecurity32A( LPCSTR lpFileName,
                                SECURITY_INFORMATION RequestedInformation,
                                LPSECURITY_DESCRIPTOR pSecurityDescriptor)
{
  FIXME(advapi, "(%s) : stub\n", debugstr_a(lpFileName));
  return TRUE;
}

/******************************************************************************
 * SetFileSecurityW [32.183]
 * Sets the security of a file or directory
 */

BOOL32 WINAPI SetFileSecurity32W( LPCWSTR lpFileName,
                                SECURITY_INFORMATION RequestedInformation,
                                LPSECURITY_DESCRIPTOR pSecurityDescriptor)
{
  FIXME(advapi, "(%s) : stub\n", debugstr_w(lpFileName) ); 
  return TRUE;
}

/******************************************************************************
 * OpenService32A [ADVAPI32.112]
 */
HANDLE32 WINAPI OpenService32A( HANDLE32 hSCManager, LPCSTR lpServiceName,
                                DWORD dwDesiredAccess )
{
    LPWSTR lpServiceNameW = HEAP_strdupAtoW(GetProcessHeap(),0,lpServiceName);
    DWORD ret = OpenService32W( hSCManager, lpServiceNameW, dwDesiredAccess);
    HeapFree(GetProcessHeap(),0,lpServiceNameW);
    return ret;
}


/******************************************************************************
 * OpenService32W [ADVAPI32.113]
 * Opens a handle to an existing service
 *
 * NOTES
 *    The return value should be SC_HANDLE
 *    hSCManager should be SC_HANDLE
 *
 * RETURNS
 *    Success: Handle to the service
 *    Failure: NULL
 */
HANDLE32 WINAPI OpenService32W( HANDLE32 hSCManager, LPCWSTR lpServiceName,
                                DWORD dwDesiredAccess )
{
    FIXME(advapi, "(%d,%p,%ld): stub\n",hSCManager, lpServiceName,
          dwDesiredAccess);
    return 1;
}


/******************************************************************************
 * CreateServiceA [ADVAPI32.28]
 */
DWORD WINAPI CreateServiceA( DWORD hSCManager, LPCSTR lpServiceName,
                             LPCSTR lpDisplayName, DWORD dwDesiredAccess, 
                             DWORD dwServiceType, DWORD dwStartType, 
                             DWORD dwErrorControl, LPCSTR lpBinaryPathName,
                             LPCSTR lpLoadOrderGroup, LPDWORD lpdwTagId, 
                             LPCSTR lpDependencies, LPCSTR lpServiceStartName, 
                             LPCSTR lpPassword )
{
    FIXME(advapi, "(%ld,%s,%s,...): stub\n", 
          hSCManager, debugstr_a(lpServiceName), debugstr_a(lpDisplayName));
    return 1;
}


/******************************************************************************
 * DeleteService [ADVAPI32.31]
 *
 * PARAMS
 *    hService [I] Handle to service
 *
 * RETURNS STD
 *
 * NOTES
 *    hService should be SC_HANDLE
 */
BOOL32 WINAPI DeleteService( HANDLE32 hService )
{
    FIXME(advapi, "(%d): stub\n",hService);
    return TRUE;
}


/******************************************************************************
 * StartService32A [ADVAPI32.195]
 *
 * NOTES
 *    How do we convert lpServiceArgVectors to use the 32W version?
 */
BOOL32 WINAPI StartService32A( HANDLE32 hService, DWORD dwNumServiceArgs,
                               LPCSTR *lpServiceArgVectors )
{
    FIXME(advapi, "(%d,%ld,%p): stub\n",hService,dwNumServiceArgs,lpServiceArgVectors);
    return TRUE;
}


/******************************************************************************
 * StartService32W [ADVAPI32.198]
 * Starts a service
 *
 * PARAMS
 *    hService            [I] Handle of service
 *    dwNumServiceArgs    [I] Number of arguments
 *    lpServiceArgVectors [I] Address of array of argument string pointers
 *
 * RETURNS STD
 *
 * NOTES
 *    hService should be SC_HANDLE
 */
BOOL32 WINAPI StartService32W( HANDLE32 hService, DWORD dwNumServiceArgs,
                               LPCWSTR *lpServiceArgVectors )
{
    FIXME(advapi, "(%d,%ld,%p): stub\n",hService,dwNumServiceArgs,
          lpServiceArgVectors);
    return TRUE;
}


/******************************************************************************
 * DeregisterEventSource [ADVAPI32.32]
 * Closes a handle to the specified event log
 *
 * PARAMS
 *    hEventLog [I] Handle to event log
 *
 * RETURNS STD
 */
BOOL32 WINAPI DeregisterEventSource( HANDLE32 hEventLog )
{
    FIXME(advapi, "(%d): stub\n",hEventLog);
    return TRUE;
}


/******************************************************************************
 * RegisterEventSource32A [ADVAPI32.174]
 */
HANDLE32 WINAPI RegisterEventSource32A( LPCSTR lpUNCServerName, 
                                        LPCSTR lpSourceName )
{
    LPWSTR lpUNCServerNameW = HEAP_strdupAtoW(GetProcessHeap(),0,lpUNCServerName);
    LPWSTR lpSourceNameW = HEAP_strdupAtoW(GetProcessHeap(),0,lpSourceName);
    HANDLE32 ret = RegisterEventSource32W(lpUNCServerNameW,lpSourceNameW);
    HeapFree(GetProcessHeap(),0,lpSourceNameW);
    HeapFree(GetProcessHeap(),0,lpUNCServerNameW);
    return ret;
}


/******************************************************************************
 * RegisterEventSource32W [ADVAPI32.175]
 * Returns a registered handle to an event log
 *
 * PARAMS
 *    lpUNCServerName [I] Server name for source
 *    lpSourceName    [I] Source name for registered handle
 *
 * RETURNS
 *    Success: Handle
 *    Failure: NULL
 */
HANDLE32 WINAPI RegisterEventSource32W( LPCWSTR lpUNCServerName, 
                                        LPCWSTR lpSourceName )
{
    FIXME(advapi, "(%s,%s): stub\n", debugstr_w(lpUNCServerName),
          debugstr_w(lpSourceName));
    return 1;
}

/******************************************************************************
 * QueryServiceStatus [ADVAPI32]
 */
BOOL32 WINAPI QueryServiceStatus(/*SC_HANDLE*/HANDLE32 hService,/*LPSERVICE_STATUS*/LPVOID lpservicestatus) {
	FIXME(advapi,"(%d,%p),stub!\n",hService,lpservicestatus);
	return TRUE;
}

/******************************************************************************
 * IsValidSecurityDescriptor [ADVAPI32]
 */
BOOL32 WINAPI IsValidSecurityDescriptor(LPSECURITY_DESCRIPTOR lpsecdesc) {
	FIXME(advapi,"(%p),stub!\n",lpsecdesc);
	return TRUE;
}

/******************************************************************************
 * MakeSelfRelativeSD [ADVAPI32]
 */
BOOL32 WINAPI MakeSelfRelativeSD(
	LPSECURITY_DESCRIPTOR lpabssecdesc,
	LPSECURITY_DESCRIPTOR lpselfsecdesc,LPDWORD lpbuflen
) {
	FIXME(advapi,"(%p,%p,%p),stub!\n",lpabssecdesc,lpselfsecdesc,lpbuflen);
	return TRUE;
}

/******************************************************************************
 * QueryWindows31FilesMigration [ADVAPI32]
 */
BOOL32 WINAPI QueryWindows31FilesMigration(DWORD x1) {
	FIXME(advapi,"(%ld),stub!\n",x1);
	return TRUE;
}

/******************************************************************************
 * SynchronizeWindows31FilesAndWindowsNTRegistry [ADVAPI32]
 */
BOOL32 WINAPI SynchronizeWindows31FilesAndWindowsNTRegistry(DWORD x1,DWORD x2,DWORD x3,DWORD x4) {
	FIXME(advapi,"(0x%08lx,0x%08lx,0x%08lx,0x%08lx),stub!\n",x1,x2,x3,x4);
	return TRUE;
}

/******************************************************************************
 * LsaOpenPolicy [ADVAPI32]
 */
BOOL32 WINAPI LsaOpenPolicy(DWORD x1,DWORD x2,DWORD x3,DWORD x4) {
	FIXME(advapi,"(0x%08lx,0x%08lx,0x%08lx,0x%08lx),stub!\n",x1,x2,x3,x4);
	return 0xc0000000; /* generic error */
}

/******************************************************************************
 * NotifyBootConfigStatus [ADVAPI32]
 */
BOOL32 WINAPI NotifyBootConfigStatus(DWORD x1) {
	FIXME(advapi,"(0x%08lx),stub!\n",x1);
	return 1;
}

/******************************************************************************
 * OpenEventLogA [ADVAPI32]
 */
HANDLE32 WINAPI OpenEventLog32A(LPCSTR uncname,LPCSTR source) {
	FIXME(advapi,"(%s,%s),stub!\n",uncname,source);
	return 0xcafe4242;
}

