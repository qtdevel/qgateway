
#ifndef __MS_PING_H__
#define __MS_PING_H__

#ifdef HAVE_MS_PING

int initMsIcmp();
bool canUseMsIcmp();
BOOL shutdownMsIcmp();

int ms_ping(const char *pDest, unsigned int nTimeout);

#endif // HAVE_MS_PING

#endif // __MS_PING_H__
