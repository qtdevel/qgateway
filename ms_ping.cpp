
#ifdef HAVE_MS_PING

#include <winsock2.h>
#include <Ipexport.h>
#include <Iphlpapi.h>
#include <Icmpapi.h>
#include <stdio.h>

#include "ms_ping.h"

#define BUFSIZE                      8192
#define DEFAULT_LEN                  32
#define DEFAULT_TTL                  64
#define MAX_ICMP_ECHO_REPLY_COUNT    32

HANDLE hICMP;
char achRequestData[BUFSIZE];
char achReplyData[sizeof(ICMP_ECHO_REPLY) * MAX_ICMP_ECHO_REPLY_COUNT + BUFSIZE];

HINSTANCE hIcmpLib;
HINSTANCE hIphlpapiLib;
typedef  HANDLE (WINAPI *PIcmpCreateFile)();
typedef  BOOL   (WINAPI *PIcmpCloseHandle)(HANDLE icmpHandle);
typedef  DWORD  (WINAPI *PIcmpSendEcho)(
                                        HANDLE IcmpHandle,
                                        IPAddr DestinationAddress,
                                        LPVOID RequestData,
                                        WORD RequestSize,
                                        PIP_OPTION_INFORMATION RequestOptions,
                                        LPVOID ReplyBuffer,
                                        DWORD ReplySize,
                                        DWORD Timeout
                                      );

PIcmpCreateFile pIcmpCreateFile;
PIcmpCloseHandle pIcmpCloseHandle;
PIcmpSendEcho pIcmpSendEcho;

/* IP Flags - 3 bits
 *
 *  bit 0: reserved
 *  bit 1: 0=May Fragment, 1=Don't Fragment
 *  bit 2: 0=Last Fragment, 1=More Fragments
 */
#define IPFLAG_DONT_FRAGMENT 0x02


int initMsIcmp()
{
	//	Init variables
	hIcmpLib = 0;
	pIcmpCreateFile = 0;
	pIcmpCloseHandle = 0;
	pIcmpSendEcho = 0;
	hICMP = 0;
	hIphlpapiLib = 0;

	hIphlpapiLib = LoadLibrary(TEXT("Iphlpapi.dll"));
	if (hIphlpapiLib) {
		//  IcmpCreateFile() - Opens the ping service
		pIcmpCreateFile = (PIcmpCreateFile) GetProcAddress(hIphlpapiLib, "IcmpCreateFile");

		//  IcmpCloseHandle() - Closes the ping service
		pIcmpCloseHandle = (PIcmpCloseHandle) GetProcAddress(hIphlpapiLib, "IcmpCloseHandle");

		//  IcmpSendEcho() - Sends an IPv4 ICMP echo request and returns any echo response replies
		pIcmpSendEcho = (PIcmpSendEcho) GetProcAddress(hIphlpapiLib, "IcmpSendEcho");
	}

	if ((hIphlpapiLib == NULL) || (pIcmpCreateFile == NULL) ||
		(pIcmpCloseHandle == NULL) || (pIcmpSendEcho == NULL)) {
		if (hIphlpapiLib) {
			FreeLibrary(hIphlpapiLib);
			hIphlpapiLib = 0;
		}
		hIcmpLib = LoadLibrary(TEXT("Icmp.dll"));
		if (hIcmpLib) {
			//  IcmpCreateFile() - Opens the ping service
			pIcmpCreateFile = (PIcmpCreateFile) GetProcAddress(hIcmpLib, "IcmpCreateFile");

			//  IcmpCloseHandle() - Closes the ping service
			pIcmpCloseHandle = (PIcmpCloseHandle) GetProcAddress(hIcmpLib, "IcmpCloseHandle");

			//  IcmpSendEcho() - Sends an IPv4 ICMP echo request and returns any echo response replies
			pIcmpSendEcho = (PIcmpSendEcho) GetProcAddress(hIcmpLib, "IcmpSendEcho");
		}
	}

	if ((pIcmpCreateFile == NULL) || (pIcmpCloseHandle == NULL) || (pIcmpSendEcho == NULL)) {
		if (hIcmpLib) {
			FreeLibrary(hIcmpLib);
			hIcmpLib = 0;
			pIcmpCreateFile = 0;
			pIcmpCloseHandle = 0;
			pIcmpSendEcho = 0;
			return -1;
		}
	}

	hICMP = pIcmpCreateFile();
	if (hICMP == INVALID_HANDLE_VALUE) {
		printf ("IcmpCreateFile() failed\n");
		return -1;
	}

	/*
	 * Init data buffer printable ASCII 
	 *  32 (space) through 126 (tilde)
	 */
	for (int j=0, i=32; j < DEFAULT_LEN; j++, i++) {
		if (i>=126) 
		i= 32;
		achRequestData[j] = i;
	}

	return 0;
}

bool canUseMsIcmp()
{
	return (hICMP != 0);
}

BOOL shutdownMsIcmp()
{
	BOOL ret = FALSE;
	if (pIcmpCloseHandle != NULL) {
		ret = pIcmpCloseHandle(hICMP);
		hICMP = 0;
	}
	if (hIphlpapiLib) {
		FreeLibrary(hIphlpapiLib);
		hIphlpapiLib = 0;
	}
	if (hIcmpLib) {
		FreeLibrary(hIcmpLib);
		hIcmpLib = 0;
	}
	pIcmpCreateFile = 0;
	pIcmpCloseHandle = 0;
	pIcmpSendEcho = 0;
	hICMP = 0;
	return ret;
}

int ms_ping(const char *pDest, unsigned int nTimeout)
{
	if (hICMP == 0) {
		printf("ms_ping(): ICMP handle is NULL.\n");
		return -1;
	}

	if (pIcmpSendEcho == 0) {
		printf("ms_ping(): IcmpSendEcho() function is not found in DLL.\n");
		return -1;
	}

	LPHOSTENT lpstHost = NULL;
	IN_ADDR stDestAddr;
	IP_OPTION_INFORMATION stIPInfo, *lpstIPInfo;

	if (pDest == NULL) {
		printf("ms_ping(): *pDest is NULL.\n");
		return -1;
	}

	/* Is the string an address? */
	u_long lAddr = inet_addr(pDest);
	if (!lAddr) {
		/* Or, is it a hostname? */
		lpstHost = gethostbyname(pDest);
		if (!lpstHost) {
				printf ("%s is an invalid address or hostname (err: %d)\n", 
					pDest, WSAGetLastError());
				return -1;
		} else {
			/* Hostname was resolved, save the address */
			stDestAddr.s_addr = *(u_long*)lpstHost->h_addr;
		}
	} else {
			/* Address is ok */
		stDestAddr.s_addr = lAddr;
	}

	/*
	 * Init IPInfo structure
	 */
	lpstIPInfo = &stIPInfo;

	stIPInfo.Ttl      = DEFAULT_TTL;

	/* Set IP Type of Service field value: 
	 *  bits
	 * 0-2: Precedence bits = 000
	 *   3: Delay bit       = 0
	 *   4: Throughput bit  = 1 (to maximize throughput)
	 *   5: Reliability bit = 0
	 *   6: Cost bit        = 0
	 *   7: <reserved>      = 0
	 *
	 * See RFCs 1340, and 1349 for appropriate settings
	 */
	stIPInfo.Tos      = 0;

	stIPInfo.Flags    = IPFLAG_DONT_FRAGMENT;
	stIPInfo.OptionsSize = 0;
	stIPInfo.OptionsData = NULL;

	memset(achReplyData, 0, sizeof(achReplyData));
	/*
	 * IcmpSendEcho() - Send the ICMP Echo Request
	 *                  and read the Reply
	 */
	DWORD dwReplyCount = pIcmpSendEcho(hICMP, 
										stDestAddr.s_addr,
										achRequestData,
										DEFAULT_LEN,
										lpstIPInfo,
										achReplyData, 
										sizeof(achReplyData), 
										nTimeout);
	if (dwReplyCount <= 0)
		return -1;

	int idx = (dwReplyCount - 1) * sizeof(ICMP_ECHO_REPLY);
	PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)(achReplyData + idx);

	u_long ms = pEchoReply->RoundTripTime;
	if (ms > nTimeout)
		return -1;

	if (pEchoReply->Status != IP_SUCCESS)
		return -1;

	return ms;
}

#endif // HAVE_MS_PING
