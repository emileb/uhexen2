/*
	net_sys.h
	common network system header

	$Id: net_sys.h,v 1.3 2007-03-14 21:04:23 sezero Exp $
*/

#ifndef __NET_SYS_H__
#define __NET_SYS_H__

#include <sys/types.h>
#include <errno.h>

// unix includes and compatibility macros
#if defined(PLATFORM_UNIX)

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#if defined(SUNOS)
#include <sys/filio.h>
#endif

#if defined(__MORPHOS__)
#include <proto/socket.h>
#endif
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifndef INADDR_NONE
#define INADDR_NONE	((in_addr_t) 0xffffffff)
#endif
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK	((in_addr_t) 0x7f000001)	/* 127.0.0.1	*/
#endif

#define SOCKETERRNO errno

#if defined(__MORPHOS__)
//#if !defined(ixemul)
#undef SOCKETERRNO
#define SOCKETERRNO Errno()
//#endif
#define socklen_t int
#define ioctlsocket IoctlSocket
#define closesocket CloseSocket
#else
#define ioctlsocket ioctl
#define closesocket close
#endif

#endif	// end of unix stuff

// windows includes and compatibility macros
#if defined(_WIN32)

#include <windows.h>
#include <winsock.h>

#if !( defined(_WS2TCPIP_H) || defined(_WS2TCPIP_H_) )
// on win32, socklen_t seems to be a winsock2 thing
typedef int	socklen_t;
#endif

#define SOCKETERRNO WSAGetLastError()
#define EWOULDBLOCK	WSAEWOULDBLOCK
#define ECONNREFUSED	WSAECONNREFUSED

#endif	// end of windows stuff


#endif	// __NET_SYS_H__

