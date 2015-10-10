#include "Time.h"
#include "Stream.h"
#include "ITransport.h"

#include "DNS.h"
#include "Ethernet.h"

#define NET_DEBUG DEBUG

#define	MAX_DNS_BUF_SIZE	256		///< maximum size of DNS buffer. */
/*
 * @brief Maxium length of your queried Domain name
 * @todo SHOULD BE defined it equal as or greater than your Domain name lenght + null character(1)
 * @note SHOULD BE careful to stack overflow because it is allocated 1.5 times as MAX_DOMAIN_NAME in stack.
 */
#define  MAX_DOMAIN_NAME   16       // for example "www.google.com"

#define	MAX_DNS_RETRY     2        ///< Requery Count
#define	DNS_WAIT_TIME     3        ///< Wait response time. unit 1s.

#define	IPPORT_DOMAIN     53       ///< DNS server port number

#define	INITRTT		2000L	/* Initial smoothed response time */
#define	MAXCNAME	   (MAX_DOMAIN_NAME + (MAX_DOMAIN_NAME>>1))	   /* Maximum amount of cname recursion */

#define	TYPE_A		1	   /* Host address */
#define	TYPE_NS		2	   /* Name server */
#define	TYPE_MD		3	   /* Mail destination (obsolete) */
#define	TYPE_MF		4	   /* Mail forwarder (obsolete) */
#define	TYPE_CNAME	5	   /* Canonical name */
#define	TYPE_SOA	   6	   /* Start of Authority */
#define	TYPE_MB		7	   /* Mailbox name (experimental) */
#define	TYPE_MG		8	   /* Mail group member (experimental) */
#define	TYPE_MR		9	   /* Mail rename name (experimental) */
#define	TYPE_NULL	10	   /* Null (experimental) */
#define	TYPE_WKS	   11	   /* Well-known sockets */
#define	TYPE_PTR	   12	   /* Pointer record */
#define	TYPE_HINFO	13	   /* Host information */
#define	TYPE_MINFO	14	   /* Mailbox information (experimental)*/
#define	TYPE_MX		15	   /* Mail exchanger */
#define	TYPE_TXT	   16	   /* Text strings */
#define	TYPE_ANY	   255	/* Matches any type */

#define	CLASS_IN	   1	   /* The ARPA Internet */

/* Round trip timing parameters */
#define	AGAIN	      8     /* Average RTT gain = 1/8 */
#define	LAGAIN      3     /* Log2(AGAIN) */
#define	DGAIN       4     /* Mean deviation gain = 1/4 */
#define	LDGAIN      2     /* log2(DGAIN) */

/* Header for all domain messages */
typedef struct dhdr
{
	ushort id;   /* Identification */
	byte	qr;      /* Query/Response */
#define	QUERY    0
#define	RESPONSE 1
	byte	opcode;
#define	IQUERY   1
	byte	aa;      /* Authoratative answer */
	byte	tc;      /* Truncation */
	byte	rd;      /* Recursion desired */
	byte	ra;      /* Recursion available */
	byte	rcode;   /* Response code */
#define	NO_ERROR       0
#define	FORMAT_ERROR   1
#define	SERVER_FAIL    2
#define	NAME_ERROR     3
#define	NOT_IMPL       4
#define	REFUSED        5
	ushort qdcount;	/* Question count */
	ushort ancount;	/* Answer count */
	ushort nscount;	/* Authority (name server) count */
	ushort arcount;	/* Additional record count */
} TDNS;

DNS::DNS(ISocket* socket)
{
	Socket	= socket;

	socket->Remote.Port		= 53;
	socket->Remote.Address	= socket->Host->DNSServer;

	ITransport* port = dynamic_cast<ITransport*>(Socket);
	port->Register(OnReceive, this);
}

DNS::~DNS()
{
}

// 转换域名为可读格式
int parse_name(Stream& ms, char* buf, short len)
{
	ushort slen;		/* Length of current segment */
	int clen = 0;		/* Total length of compressed name */
	int indirect = 0;	/* Set if indirection encountered */
	int nseg = 0;		/* Total number of segments in name */

	byte* msg = ms.GetBuffer();
	byte* cp = ms.Current();

	for (;;)
	{
		slen = *cp++;	/* Length of this segment */

		if (!indirect) clen++;

		if ((slen & 0xc0) == 0xc0)
		{
			if (!indirect)
				clen++;
			indirect = 1;
			/* Follow indirection */
			cp = &msg[((slen & 0x3f)<<8) + *cp];
			slen = *cp++;
		}

		if (slen == 0)	/* zero length == all done */
			break;

		len -= slen + 1;

		if (len < 0) return -1;

		if (!indirect) clen += slen;

		while (slen-- != 0) *buf++ = (char)*cp++;
		*buf++ = '.';
		nseg++;
	}

	if (nseg == 0)
	{
		/* Root name; represent as single dot */
		*buf++ = '.';
		len--;
	}

	*buf++ = '\0';
	len--;

	ms.Seek(clen);

	return clen;	/* Length of compressed message */
}

// 分析请求段
bool dns_question(Stream& ms)
{
	char name[MAXCNAME];

	int len = parse_name(ms, name, MAXCNAME);
	if (len == -1) return false;

	//cp += len;
	//cp += 2;		/* type */
	//cp += 2;		/* class */
	ms.Seek(2 + 2);

	return true;
}

// 分析应答记录
bool dns_answer(Stream& ms, byte* ip_from_dns)
{
	char name[MAXCNAME];

	int len = parse_name(ms, name, MAXCNAME);
	if (len == -1) return 0;

	int type = ms.ReadUInt16();
	//cp += 2;		/* type */
	//cp += 2;		/* class */
	//cp += 4;		/* ttl */
	//cp += 2;		/* len */
	ms.Seek(2 + 2 + 4 + 2);

	switch (type)
	{
	case TYPE_A:
		/* Just read the address directly into the structure */
		//!!! 不知道为什么偏移了2个字节，这里临时后退
		ms.Seek(-2);
		ms.Read(ip_from_dns, 0, 4);
		debug_printf("DNA::A %08X \r\n", *(uint*)ip_from_dns);
		break;
	case TYPE_CNAME:
	case TYPE_MB:
	case TYPE_MG:
	case TYPE_MR:
	case TYPE_NS:
	case TYPE_PTR:
		/* These types all consist of a single domain name */
		/* convert it to ascii format */
		len = parse_name(ms, name, MAXCNAME);
		if (len == -1) return 0;
		debug_printf("DNA::PTR %s \r\n", name);

		break;
	case TYPE_HINFO:
		len = ms.ReadByte();
		ms.Seek(len);
		len = ms.ReadByte();
		ms.Seek(len);

		break;
	case TYPE_MX:
		ms.Seek(2);
		/* Get domain name of exchanger */
		len = parse_name(ms, name, MAXCNAME);
		if (len == -1) return 0;
		debug_printf("DNA::MX %s \r\n", name);

		break;
	case TYPE_SOA:
		/* Get domain name of name server */
		len = parse_name(ms, name, MAXCNAME);
		if (len == -1) return 0;
		debug_printf("DNA::SOA %s \r\n", name);

		/* Get domain name of responsible person */
		len = parse_name(ms, name, MAXCNAME);
		if (len == -1) return 0;
		debug_printf("DNA::SOA %s \r\n", name);

		ms.Seek(4 + 4 + 4 + 4 + 4);

		break;
	case TYPE_TXT:
		/* Just stash */
		break;
	default:
		debug_printf("DNA::ANS type=0x%02X \r\n", type);
		/* Ignore */
		break;
	}

	return true;
}

// 分析响应
bool parseDNSMSG(TDNS* hdr, const ByteArray& bs, byte* ip_from_dns)
{
	Stream ms(bs);
	ms.Little = false;

	memset(hdr, 0, sizeof(hdr));

	hdr->id = ms.ReadUInt16();
	ushort tmp = ms.ReadUInt16();
	if (tmp & 0x8000) hdr->qr = 1;

	hdr->opcode = (tmp >> 11) & 0xf;

	if (tmp & 0x0400) hdr->aa = 1;
	if (tmp & 0x0200) hdr->tc = 1;
	if (tmp & 0x0100) hdr->rd = 1;
	if (tmp & 0x0080) hdr->ra = 1;

	hdr->rcode = tmp & 0xf;
	hdr->qdcount = ms.ReadUInt16();
	hdr->ancount = ms.ReadUInt16();
	hdr->nscount = ms.ReadUInt16();
	hdr->arcount = ms.ReadUInt16();

	// 开始分析变长部分

	/* Question section */
	for (int i = 0; i < hdr->qdcount; i++)
	{
		// 域名太长
		if(!dns_question(ms)) return false;
	}

	/* Answer section */
	for (int i = 0; i < hdr->ancount; i++)
	{
		if(!dns_answer(ms, ip_from_dns)) return -1;
	}

	/* Name server (authority) section */
	//for (i = 0; i < hdr->nscount; i++);

	/* Additional section */
	//for (i = 0; i < hdr->arcount; i++);

	return hdr->rcode == 0;
}

// DNS查询消息
short dns_makequery(short op, const String& name, ByteArray& bs)
{
	Stream ms(bs);
	ms.Little = false;

	ms.Write((ushort)Time.Seconds);
	ms.Write((ushort)((op << 11) | 0x0100));	// Recursion desired
	ms.Write((ushort)1);
	ms.Write((ushort)0);
	ms.Write((ushort)0);
	ms.Write((ushort)0);

	const char* dname = name.GetBuffer();
	ushort dlen = strlen(dname);
	for (;;)
	{
		// 查找下一个小圆点
		const char* cp1 = strchr(dname, '.');

		int len = 0;
		if (cp1 != NULL)
			len = cp1 - dname;	/* More to come */
		else
			len = dlen;			/* Last component */

		//*cp++ = len;				/* Write length of component */
		// 写长度
		ms.Write((byte)len);
		if (len == 0) break;

		/* Copy component up to (but not including) dot */
		//strncpy((char *)cp, dname, len);
		//cp += len;
		ms.Write((const byte*)dname, 0, len);
		if (cp1 == NULL)
		{
			//*cp++ = 0;			/* Last one; write null and finish */
			// 最后一个，写空，完成
			ms.Write((byte)0);
			break;
		}
		dname += len+1;
		dlen -= len+1;
	}

	ms.Write((ushort)0x0001);				/* type */
	ms.Write((ushort)0x0001);				/* class */

	bs.SetLength(ms.Position());

	return ms.Position();
}

IPAddress DNS::Query(const String& domain, int msTimeout)
{
#if NET_DEBUG
	IPAddress& server = Socket->Host->DNSServer;
	debug_printf("DNS::Query %s DNS Server : ", domain.GetBuffer());
	server.Show(true);
#endif

	byte buf[1024];
	ByteArray bs(buf, ArrayLength(buf));
	ByteArray rs(buf, ArrayLength(buf));
	// 同时作为响应缓冲区，别浪费了
	rs.SetLength(0);
	_Buffer = &rs;

	dns_makequery(0, domain, bs);
	Socket->Send(bs);

	IPAddress ip;
	TimeWheel tw(0, msTimeout);
	tw.Sleep = 100;
	while(!tw.Expired())
	{
		if(rs.Length() > 0)
		{
			debug_printf("DNS::Receive len = %d\r\n", rs.Length());

			TDNS dns;
			parseDNSMSG(&dns, rs, (byte*)&ip.Value);
			break;
		}
	}
	_Buffer = NULL;

	return ip;
}

uint DNS::OnReceive(ITransport* port, ByteArray& bs, void* param, void* param2)
{
	((DNS*)param)->Process(bs, *(const IPEndPoint*)param2);

	return 0;
}

void DNS::Process(ByteArray& bs, const IPEndPoint& server)
{
	// 只要来自服务器的
	if(server.Address != Socket->Host->DNSServer) return;

	if(_Buffer)
		_Buffer->Copy(bs);
	else
	{
		debug_printf("DNS::Process \r\n");
		server.Show(true);
	}
}
