// net_wins.c

#include "net_sys.h"
#include "quakedef.h"
#include "huffman.h"
#if defined(DEBUG_BUILD) && !defined(_WIN32)
#define OutputDebugString(X) fprintf(stderr,"%s",(X))
#endif

//=============================================================================

//
// net driver vars
//

int LastCompMessageSize = 0;

netadr_t	net_local_adr;
netadr_t	net_from;
sizebuf_t	net_message;
int			net_socket;

#ifdef _WIN32
static WSADATA	winsockdata;
#endif

#define	MAX_UDP_PACKET	(MAX_MSGLEN+9)	// one more than msg + header
static byte	net_message_buffer[MAX_UDP_PACKET];


//=============================================================================

//
// net driver functions
//

static void NetadrToSockadr (netadr_t *a, struct sockaddr_in *s)
{
	memset (s, 0, sizeof(*s));
	s->sin_family = AF_INET;

	*(int *)&s->sin_addr = *(int *)&a->ip;
	s->sin_port = a->port;
}

static void SockadrToNetadr (struct sockaddr_in *s, netadr_t *a)
{
	*(int *)&a->ip = *(int *)&s->sin_addr;
	a->port = s->sin_port;
}

qboolean NET_CompareAdr (netadr_t a, netadr_t b)
{
	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port)
		return true;
	return false;
}

char *NET_AdrToString (netadr_t a)
{
	static	char	s[64];

	sprintf (s, "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));

	return s;
}

char *NET_BaseAdrToString (netadr_t a)
{
	static	char	s[64];

	sprintf (s, "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);

	return s;
}

/*
=============
NET_StringToAdr

idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
qboolean NET_StringToAdr (char *s, netadr_t *a)
{
	struct hostent		*h;
	struct sockaddr_in	sadr;
	char	*colon;
	char	copy[128];

	memset (&sadr, 0, sizeof(sadr));
	sadr.sin_family = AF_INET;

	sadr.sin_port = 0;

	strcpy (copy, s);
	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++)
	{
		if (*colon == ':')
		{
			*colon = 0;
			sadr.sin_port = htons((short)atoi(colon+1));
		}
	}

	if (copy[0] >= '0' && copy[0] <= '9')
	{
		*(int *)&sadr.sin_addr = inet_addr(copy);
	}
	else
	{
		h = gethostbyname (copy);
		if (!h)
			return 0;
		*(int *)&sadr.sin_addr = *(int *)h->h_addr_list[0];
	}

	SockadrToNetadr (&sadr, a);

	return true;
}


//=============================================================================

static unsigned char huffbuff[65536];

qboolean NET_GetPacket (void)
{
	int	ret;
	struct sockaddr_in	from;
	socklen_t		fromlen;

	fromlen = sizeof(from);
	ret = recvfrom (net_socket,(char *) huffbuff, sizeof(net_message_buffer), 0, (struct sockaddr *)&from, &fromlen);
	if (ret == -1)
	{
		int err = SOCKETERRNO;
		if (err == EWOULDBLOCK)
			return false;
		if (err == ECONNREFUSED)
		{
			Con_Printf ("%s: Connection refused\n", __FUNCTION__);
			return false;
		}
#	ifdef _WIN32
		if (err == WSAEMSGSIZE)
		{
			Con_Printf ("Oversize packet from %s\n", NET_AdrToString (net_from));
			return false;
		}
#	endif
		Sys_Error ("%s: %s", __FUNCTION__, strerror(err));
	}

	SockadrToNetadr (&from, &net_from);

	if (ret == sizeof(net_message_buffer) )
	{
		Con_Printf ("Oversize packet from %s\n", NET_AdrToString (net_from));
		return false;
	}

	LastCompMessageSize += ret;//keep track of bytes actually received for debugging

	HuffDecode(huffbuff, net_message_buffer, ret, &ret, sizeof(net_message_buffer));
	if (ret > sizeof(net_message_buffer))
	{
		Con_Printf ("Oversize compressed data from %s\n", NET_AdrToString (net_from));
		return false;
	}
	net_message.cursize = ret;

	return ret;
}


//=============================================================================

void NET_SendPacket (int length, void *data, netadr_t to)
{
	int	ret, outlen;
	struct sockaddr_in	addr;
#ifdef DEBUG_BUILD
	char	string[120];
#endif

	NetadrToSockadr (&to, &addr);
	HuffEncode((unsigned char *)data, huffbuff, length, &outlen);

#ifdef DEBUG_BUILD
	sprintf(string,"in: %d  out: %d  ratio: %f\n", HuffIn, HuffOut, 1-(float)HuffOut/(float)HuffIn);
	OutputDebugString(string);
	CalcFreq((unsigned char *)data, length);
#endif	// DEBUG_BUILD

	ret = sendto (net_socket, (char *) huffbuff, outlen, 0, (struct sockaddr *)&addr, sizeof(addr) );
	if (ret == -1)
	{
		int err = SOCKETERRNO;
		if (err == EWOULDBLOCK)
			return;
		if (err == ECONNREFUSED)
		{
			Con_Printf ("%s: Connection refused\n", __FUNCTION__);
			return;
		}
		Con_Printf ("%s ERROR: %i\n", __FUNCTION__, errno);
	}
}


//=============================================================================

static int UDP_OpenSocket (int port)
{
	int	i, newsocket;
	struct sockaddr_in	address;
	unsigned long _true = true;

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		Sys_Error ("%s: socket: %s", __FUNCTION__, strerror(errno));

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("%s: ioctl FIONBIO: %s", __FUNCTION__, strerror(errno));

	address.sin_family = AF_INET;
	//ZOID -- check for interface binding option
	i = COM_CheckParm("-ip");
	if (i && i < com_argc-1)
	{
		address.sin_addr.s_addr = inet_addr(com_argv[i+1]);
		if (address.sin_addr.s_addr == INADDR_NONE)
			Sys_Error ("%s is not a valid IP address", com_argv[i+1]);
		Con_Printf("Binding to IP Interface Address of %s\n", inet_ntoa(address.sin_addr));
	}
	else
	{
		address.sin_addr.s_addr = INADDR_ANY;
	}

	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons((short)port);

	if ( bind(newsocket, (void *)&address, sizeof(address)) == -1 )
		Sys_Error ("%s: bind: %s", __FUNCTION__, strerror(errno));

	return newsocket;
}

static void NET_GetLocalAddress (void)
{
	char	buff[512];
	struct sockaddr_in	address;
	socklen_t		namelen;

	if (gethostname(buff, 512) == -1)
		Sys_Error ("%s: gethostname: %s", __FUNCTION__, strerror(errno));
	buff[512-1] = 0;

	NET_StringToAdr (buff, &net_local_adr);

	namelen = sizeof(address);
	if (getsockname (net_socket, (struct sockaddr *)&address, &namelen) == -1)
		Sys_Error ("%s: getsockname: %s", __FUNCTION__, strerror(errno));
	net_local_adr.port = address.sin_port;

	Con_Printf("IP address %s\n", NET_AdrToString (net_local_adr) );
}

/*
====================
NET_Init
====================
*/
void NET_Init (int port)
{
#ifdef _WIN32
	WORD	wVersionRequested;
	int		r;
#endif

#ifdef DEBUG_BUILD
	ZeroFreq();
#endif

	HuffInit();

#ifdef _WIN32
	wVersionRequested = MAKEWORD(1, 1);
	r = WSAStartup (MAKEWORD(1, 1), &winsockdata);
	if (r)
		Sys_Error ("Winsock initialization failed.");
#endif

	//
	// open the single socket to be used for all communications
	//
	net_socket = UDP_OpenSocket (port);

	//
	// init the message buffer
	//
	SZ_Init (&net_message, net_message_buffer, sizeof(net_message_buffer));

	//
	// determine my name & address
	//
	NET_GetLocalAddress ();

	Con_Printf("UDP Initialized\n");
}

/*
====================
NET_Shutdown
====================
*/
void	NET_Shutdown (void)
{
	closesocket (net_socket);
#ifdef _WIN32
	WSACleanup ();
#endif
#ifdef DEBUG_BUILD
	PrintFreqs();
#endif
}

