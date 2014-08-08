#ifndef _TinyIP_H_
#define _TinyIP_H_

#include "Enc28j60.h"

// 精简IP类
class TinyIP
{
private:
    Enc28j60* _enc;

	byte* Buffer; // 缓冲区

	void ProcessICMP(byte* buf, uint len);
	void ProcessTcp(byte* buf, uint len);
	void ProcessUdp(byte* buf, uint len);
	void ShowIP(byte* ip);
	void ShowMac(byte* mac);

	byte seqnum;
	uint info_data_len;
	uint buf_len;
	uint info_hdr_len;
	uint tcp_d_len;

	byte eth_type_is_arp_and_my_ip(byte* buf, uint len);
	byte eth_type_is_ip_and_my_ip(byte* buf, uint len);
	void make_arp_answer_from_request(byte* buf);
	void make_echo_reply_from_request(byte* buf, uint len);
	void make_udp_reply_from_request(byte* buf,byte *data, uint datalen, uint port);
	void make_eth(byte* buf);
	void make_ip(byte* buf);
	void make_tcp_synack_from_syn(byte* buf);
	void init_len_info(byte* buf);
	uint get_tcp_data_pointer(void);
	void make_tcphead(byte* buf, uint rel_ack_num, byte mss, byte cp_seq);
	uint fill_tcp_data_p(byte* buf, uint pos, const byte* progmem_s);
	uint fill_tcp_data(byte* buf, uint pos, const byte* s);
	void make_tcp_ack_from_any(byte* buf);
	void make_tcp_ack_with_data(byte* buf, uint dlen);

	void fill_ip_hdr_checksum(byte* buf);
	uint checksum(byte* buf, uint len, byte type);

	uint dhcp_id;
	void dhcp_discover(byte* buf);
	int dhcp_offer(byte* buf);
	void dhcp_request(byte* buf);
	int dhcp_ack(byte* buf);
	void fill_data(byte* src, int src_begin, byte* dst, int dst_begin, int len);
	void search_list_data(byte* buf);
	void dhcp_fill_public_data(byte* buf);
	void DHCP_config(byte* buf);

public:
    byte IP[4];
	byte Mac[6];
	ushort BufferSize;	// 缓冲区大小
	bool UseDHCP;
	bool IPIsReady;
	byte DHCPServer[4];
	byte DNSServer[4];
	byte Gateway[4];
	byte Mask[4];

    TinyIP(Enc28j60* enc, byte ip[4], byte mac[6]);
    virtual ~TinyIP();

	void Start();

	byte RemoteIP[4];
	ushort RemotePort;

    void TcpSend(byte* packet, uint len);
    void TcpClose(byte* packet, uint maxlen);
};


// ******* ETH *******
#define ETH_HEADER_LEN	14
// values of certain bytes:
#define ETHTYPE_ARP_H_V 0x08
#define ETHTYPE_ARP_L_V 0x06
#define ETHTYPE_IP_H_V  0x08
#define ETHTYPE_IP_L_V  0x00
// byte positions in the ethernet frame:
//
// Ethernet type field (2bytes):
#define ETH_TYPE_H_P 12
#define ETH_TYPE_L_P 13
//
#define ETH_DST_MAC 0
#define ETH_SRC_MAC 6


// ******* ARP *******
#define ETH_ARP_OPCODE_REPLY_H_V 0x0
#define ETH_ARP_OPCODE_REPLY_L_V 0x02
//
#define ETHTYPE_ARP_L_V 0x06
// arp.dst.ip
#define ETH_ARP_DST_IP_P 0x26
// arp.opcode
#define ETH_ARP_OPCODE_H_P 0x14
#define ETH_ARP_OPCODE_L_P 0x15
// arp.src.mac
#define ETH_ARP_SRC_MAC_P 0x16
#define ETH_ARP_SRC_IP_P 0x1c
#define ETH_ARP_DST_MAC_P 0x20
#define ETH_ARP_DST_IP_P 0x26

// ******* IP *******
#define IP_HEADER_LEN	20
// ip.src
#define IP_SRC_P 0x1a
#define IP_DST_P 0x1e
#define IP_HEADER_LEN_VER_P 0xe
#define IP_CHECKSUM_P 0x18
#define IP_TTL_P 0x16
#define IP_FLAGS_P 0x14
#define IP_P 0xe
#define IP_TOTLEN_H_P 0x10
#define IP_TOTLEN_L_P 0x11

#define IP_PROTO_P 0x17  

#define IP_PROTO_ICMP_V 1
#define IP_PROTO_TCP_V 6
// 17=0x11
#define IP_PROTO_UDP_V 17
// ******* ICMP *******
#define ICMP_TYPE_ECHOREPLY_V 0
#define ICMP_TYPE_ECHOREQUEST_V 8
//
#define ICMP_TYPE_P 0x22
#define ICMP_CHECKSUM_P 0x24

// ******* UDP *******
//UDP首部长度
#define UDP_HEADER_LEN	8
//源端口位置
#define UDP_SRC_PORT_H_P 0x22
#define UDP_SRC_PORT_L_P 0x23
//目标端口位置
#define UDP_DST_PORT_H_P 0x24
#define UDP_DST_PORT_L_P 0x25
//UDP数据长度位置
#define UDP_LEN_H_P 0x26
#define UDP_LEN_L_P 0x27
//UDP校验和位置
#define UDP_CHECKSUM_H_P 0x28
#define UDP_CHECKSUM_L_P 0x29
//UDP数据其实位置
#define UDP_DATA_P 0x2a

// ******* TCP *******
//源端口位置
#define TCP_SRC_PORT_H_P 0x22
#define TCP_SRC_PORT_L_P 0x23
//目的端口位置
#define TCP_DST_PORT_H_P 0x24
#define TCP_DST_PORT_L_P 0x25
// the tcp seq number is 4 bytes 0x26-0x29
//32位序列号
#define TCP_SEQ_H_P 0x26
//32位确认序列号
#define TCP_SEQACK_H_P 0x2a

// flags: SYN=2
//flags位置,最高两位保留
#define TCP_FLAGS_P 0x2f
//SYN连接请求标志位。为1表示发起连接的请求数据包
#define TCP_FLAGS_SYN_V 2
//FIN结束连接请求标志位。为1表示结束连接请求数据包
#define TCP_FLAGS_FIN_V 1
//PUSH标志位，为1表示此数据包应立即进行传递
#define TCP_FLAGS_PUSH_V 8
//SYN+ACK
#define TCP_FLAGS_SYNACK_V 0x12
//ACK应答标志位，为1表示确认，数据包为应答数据包
#define TCP_FLAGS_ACK_V 0x10
//PUSH+ACK
#define TCP_FLAGS_PSHACK_V 0x18

//  plain len without the options:
//TCP首部长度
#define TCP_HEADER_LEN_PLAIN 20
//4位首部长度
#define TCP_HEADER_LEN_P 0x2e
//校验和位置
#define TCP_CHECKSUM_H_P 0x32
#define TCP_CHECKSUM_L_P 0x33
//选项起始位置
#define TCP_OPTIONS_P 0x36

//DHCP ID
#define DHCP_ID_H 0x2e
#define MY_IP_H 0x3a
#define dhcp_option_type_h 0x11a
//option 内容
#define dhcp_option_mask 0x01
#define dhcp_option_dns 0x06
#define dhcp_option_router 0x03
#define dhcp_option_server_id 0x36
#define dhcp_option_end 0xff
//头部
#define dhcp_eth_det_h 0x00
#define dhcp_eth_src_h 0x06
//DHCP协议头部
#define dhcp_protocol_h 0x116
#define dhcp_protocol 0x63825363

#endif
