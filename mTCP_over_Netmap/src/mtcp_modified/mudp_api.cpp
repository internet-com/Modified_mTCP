
#include "mtcp.h"
#include "mtcp_api.h"
#include "mudp_api.h"
#include <unordered_map>
#include <iostream>
using namespace std;

ssize_t mudp_sendto(mctx_t mctx,int s, const void *buf, size_t len,int flags, const struct sockaddr *to,socklen_t tolen){

	//get socket from internal socket list
	mtcp_manager_t mtcp;
	//struct sockaddr_in *addr_in;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		return -1;
	}

	struct socket_map *socket = &mtcp->smap[s];

	//Add to UDP send list. mTCP thread will send it afterwards.
	//TODO: Note for Trishal and Priyanka. You can add locks here.
	/* Uncomment this for non-blocking.
	int allocated_memeory_space;
	char *allocated = udp_allocate_send(mtcp->udp_send_buffer,len,allocated_memeory_space);
	if(allocated == NULL){
		return -1;
	}
	*/
	struct mudp_send_list *list_elem = TAILQ_FIRST(&mtcp->udp_free_send_list);
	if(!list_elem){
		return -1;
	}
	TAILQ_REMOVE(&mtcp->udp_free_send_list, list_elem, free_send_list_link);

	//Init internal info structure
	list_elem->info.from = &socket->saddr;
	list_elem->info.to = (struct sockaddr_in *)to;
	list_elem->info.len = len;
	list_elem->info.buf = NULL; //list_elem->info.buf = allocated; (For non-blocking)
	list_elem->info.socket = socket;

	//Here you can put usleep or yield if you don't want infinite looping.
	//Comment out this loop for non-blocking.
	while(list_elem->info.buf == NULL){
		list_elem->info.buf = udp_allocate_send(mtcp->udp_send_buffer,len,list_elem->info.len);
	}
	memcpy(list_elem->info.buf,buf,len);
	INIT_LIST_HEAD(&list_elem->list_member);

	//TODO: Note for Trishal and Priyanka. You can add locks here.
	//Add to tail of send_list
	list_add_tail(&list_elem->list_member, &mtcp->udp_send_list);
	return 0;
}

ssize_t mudp_recvfrom(mctx_t mctx,int sockfd, void *buf, size_t len, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen){

	mtcp_manager_t mtcp;
	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		return -1;
	}

	struct socket_map *socket = &mtcp->smap[sockfd];

	if(list_empty(&socket->recv_queue)){
		return 0;
	}

	struct udp_receive_queue *message = list_first_entry(&socket->recv_queue, struct udp_receive_queue, recv_queue);
	int message_len = (len < message->size) ? len: message->size;
	memcpy(buf,message->data,message_len);
	struct sockaddr_in *from = (struct sockaddr_in *)src_addr;
	from->sin_addr = message->from.sin_addr;
	from->sin_port = message->from.sin_port;
	from->sin_family = AF_INET;

	/*delete from list */
	list_del(&message->recv_queue);

	udp_update_read(mtcp->udp_receive_buffer,message_len);

	TAILQ_INSERT_TAIL(&mtcp->udp_free_receive_list, message, free_receive_list_link);

	return message_len;
}
