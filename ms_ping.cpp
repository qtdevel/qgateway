
#ifdef HAVE_MS_PING

#include <winsock2.h>
#include <Ipexport.h>
#include <Icmpapi.h>
#include <Iphlpapi.h>
#include <stdio.h>

#include "ms_ping.h"

#define BUFSIZE     8192
#define DEFAULT_LEN 32
#define LOOPLIMIT   4
#define TIMEOUT     5000
#define DEFAULT_TTL 64

HANDLE hICMP;
char achReqData[BUFSIZE];
char achRepData[sizeof(ICMP_ECHO_REPLY) + BUFSIZE];

/* IP Flags - 3 bits
 *
 *  bit 0: reserved
 *  bit 1: 0=May Fragment, 1=Don't Fragment
 *  bit 2: 0=Last Fragment, 1=More Fragments
 */
#define IPFLAG_DONT_FRAGMENT 0x02


int initMsIcmp()
{
	/*
	 * IcmpCreateFile() - Open the ping service
	 */
	hICMP = IcmpCreateFile();
	if (hICMP == INVALID_HANDLE_VALUE) {
		printf ("IcmpCreateFile() failed, err: ");
		return -1;
	}

	/*
	 * Init data buffer printable ASCII 
	 *  32 (space) through 126 (tilde)
	 */
	for (int j=0, i=32; j < DEFAULT_LEN; j++, i++) {
		if (i>=126) 
		i= 32;
		achReqData[j] = i;
	}

	return 0;
}

bool canUseMsIcmp()
{
	return (hICMP != 0);
}

BOOL shutdownMsIcmp()
{
	BOOL ret = IcmpCloseHandle(hICMP);
	hICMP = 0;
	return ret;
}

int ms_ping(const char *pDest, unsigned int nTimeout)
{
	if (hICMP == 0) {
		printf("ms_ping(): ICMP handle is NULL.");
		return -1;
	}

	LPHOSTENT lpstHost = NULL;
	IN_ADDR stDestAddr;
	IP_OPTION_INFORMATION stIPInfo, *lpstIPInfo;

	if (pDest == NULL) {
		printf("ms_ping(): *pDest is NULL.");
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


	/*
	 * IcmpSendEcho() - Send the ICMP Echo Request
	 *                  and read the Reply
	 */
	DWORD dwReplyCount = IcmpSendEcho(hICMP, 
										stDestAddr.s_addr,
										achReqData,
										DEFAULT_LEN,
										lpstIPInfo,
										achRepData, 
										sizeof(achRepData), 
										nTimeout);
	if (dwReplyCount <= 0)
		return -1;

	stDestAddr.s_addr = *(u_long *)achRepData;
	u_long ms = *(u_long *) &(achRepData[8]);
	if (ms > nTimeout)
		return -1;
	return ms;
}

#endif // HAVE_MS_PING
