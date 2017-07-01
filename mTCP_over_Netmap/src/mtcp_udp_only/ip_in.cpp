#include <string.h>
#include <netinet/ip.h>

#include "ip_in.h"
#include "tcp_in.h"
#include "mtcp_api.h"
#include "ps.h"
#include "debug.h"
#include "icmp.h"
#include "udp_in.h"

#include <iostream>
using namespace std;

#define ETH_P_IP_FRAG   0xF800
#define ETH_P_IPV6_FRAG 0xF6DD

/*----------------------------------------------------------------------------*/
int
ProcessIPv4Packet(mtcp_manager_t mtcp, uint32_t cur_ts, 
				  const int ifidx, unsigned char* pkt_data, int len)
{
	/* check and process IPv4 packets */
	struct iphdr* iph = (struct iphdr *)(pkt_data);// + sizeof(struct ethhdr)); TODO: Hacked. J.
	int ip_len = ntohs(iph->tot_len);
	int rc = -1;

	//cout << "In IPv4" << endl;
	/* drop the packet shorter than ip header */
	if (ip_len < sizeof(struct iphdr))
		return ERROR;

	//cout << "In IPv4" << endl;

#ifndef DISABLE_HWCSUM
	if (mtcp->iom->dev_ioctl != NULL)
		rc = mtcp->iom->dev_ioctl(mtcp->ctx, ifidx, PKT_RX_IP_CSUM, iph);
	if (rc == -1 && ip_fast_csum(iph, iph->ihl))
		return ERROR;
#else
	UNUSED(rc);
	if (ip_fast_csum(iph, iph->ihl))
		return ERROR;
#endif

#if !PROMISCUOUS_MODE
	/* if not promiscuous mode, drop if the destination is not myself */
	if (iph->daddr != CONFIG.eths[ifidx].ip_addr)
		//DumpIPPacketToFile(stderr, iph, ip_len);
		return TRUE;
#endif

	//cout << "In IPv4" << endl;

	// see if the version is correct
	if (iph->version != 0x4 ) {
		mtcp->iom->release_pkt(mtcp->ctx, ifidx, pkt_data, len);
		return FALSE;
	}
	
	switch (iph->protocol) {
		case IPPROTO_TCP:
			cout << "TCP Packet detected" << endl;
			TRACE_INFO("TCP Packet detected.");
			return ProcessTCPPacket(mtcp, cur_ts, ifidx, iph, ip_len);
		case IPPROTO_ICMP:
			return ProcessICMPPacket(mtcp, iph, ip_len);
		case IPPROTO_UDP:
			//TRACE_INFO("UDP Packet detected.");
			return ProcessUDPPacket(mtcp, iph, ip_len);
		default:
			/* currently drop other protocols */
			return FALSE;
	}
	return FALSE;
}
/*----------------------------------------------------------------------------*/
