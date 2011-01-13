/**
 * libqcproto: Vypress/QChat protocol interface library
 *
 *   This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * QCS: qChat 1.6/VypressChat link interface
 *
 *	link module
 *
 * (c) Saulius Menkevicius 2001-2004
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
/* Win32 system */
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
/* UNIX system */
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#endif

#include "qcproto.h"
#include "link.h"
#include "supp.h"
#include "p_vypress.h"
#include "p_qchat.h"

#define VALID_ID(id)	(id!=NULL)
#define ERRRET(err)	if(1){errno=(err);return(0);}
#define ACTIVE_LINK(l)	((l)->rx >=0)
#define INVALIDATE(l)	((l)->rx=-1)

/* static variables */
static int link_count = 0;

/* internal routines */

/* link_bind_rx:
 *	binds link->rx to link->port
 * returns:
 *	non-0 on error
 */
static int
link_bind_rx(link_data * link)
{
	struct sockaddr_in sa;

	sa.sin_family = PF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(link->port);

	return bind(link->rx, (struct sockaddr*)&sa, sizeof(sa))==-1;
}

/* link_setup_broadcast:
 *	setups link->tx to broadcast mode
 * returns:
 *	non-0 on error
 */
static int
link_setup_broadcast(link_data * link)
{
	const int int_true = 1;

        return setsockopt(link->tx, SOL_SOCKET, SO_BROADCAST,
			(void*)&int_true, sizeof(int_true)
		) == -1;
}

/* link_setup_multicast:
 *	setups link->tx to multicast mode
 * returns:
 *	non-0 on error
 */
static int
link_setup_multicast(link_data * link)
{
	struct ip_mreq mreq;
	unsigned char opt;

	/* set IP_MULTICAST_LOOP to 1, so our host receives
	 * the messages we send
	 */
   	opt = 1;
	if(setsockopt(	link->tx, IPPROTO_IP, IP_MULTICAST_LOOP,
			(void*)&opt, sizeof(opt)) != 0)
		return 1;
		
	/* set IP_MULTICAST_TTL to 32, that is the packets
	 * will go through 32 routers before getting scrapped
	 */
	opt = 32;
	if(setsockopt(	link->tx, IPPROTO_IP, IP_MULTICAST_TTL,
			(void*)&opt, sizeof(opt)) != 0)
		return 1;


	/* set our group membership */
	mreq.imr_multiaddr.s_addr = htonl(link->broadcast_addr);
	mreq.imr_interface.s_addr = INADDR_ANY;
	if(setsockopt(	link->rx, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			(void*)&mreq, sizeof(mreq)) != 0)
		return 1;

	return 0;	/* multicast setup */
}

/** API implementation			*/
qcs_link qcs_open(
	enum qcs_proto proto,
	enum qcs_proto_opt proto_opt,
	unsigned long broadcast_addr,
	unsigned short port)
{
	const int int_true = 1;
	int error;
	link_data * link;

	/* check params */
	if(proto >= QCS_PROTO_NUM)
		ERRRET(ENOSYS);

	/* alloc and setup `link_data' struct */
	link = malloc(sizeof(link_data));
	if(link==NULL)
		ERRRET(ENOMEM);

	link->proto = proto;
	link->port = port ? port: 8167;
	link->broadcast_addr = broadcast_addr;

	/* alloc sockets */
	link->tx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(link->tx < 0) {
		free(link);
#ifdef WIN32
		errno = EINVAL;
#endif
		return NULL;
	}

	/* setup rx */
	link->rx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(link->rx < 1) {
		close(link->tx);
		free(link);
#ifdef WIN32
		errno = EINVAL;
#endif
		return NULL;
	}

#ifdef WIN32
	/* on win32, we can setup the rx socket so, that we can use
	 * multiple clients on the same pc simultaneously
	 */
	setsockopt(link->rx, SOL_SOCKET, SO_REUSEADDR,
		(void*)&int_true, sizeof(int_true));
#endif
	/* bind the rx socket to the port specified
	 */
	if(link_bind_rx(link)) {
		close(link->rx);
		close(link->tx);
		free(link);
#ifdef WIN32
		errno = EINVAL;
#endif
		return NULL;
	}

	/* setup link to multicast/broadcast mode, as specified
	 */
	if(proto_opt & QCS_PROTO_OPT_MULTICAST)
		error = link_setup_multicast(link);
	else	error = link_setup_broadcast(link);
	if(error) {
		close(link->tx);
		close(link->rx);
		free(link);
#ifdef WIN32
		errno = ENOSYS;
#endif
		return NULL;
	}

	/* increase link count */
	link_count ++;

	/* return success */
	return (qcs_link)link;
}

int qcs_close(qcs_link link_id)
{
	link_data * link = (link_data *)link_id;

	/* check if link is valid */
	if(!VALID_ID(link_id)) ERRRET(EINVAL);
	if(!ACTIVE_LINK(link)) ERRRET(EINVAL);

	/* shutdown sockets and free the struct */
	close(link->rx);
	close(link->tx);
	free(link);

	link_count--;
	if( link_count==0 ) {
		/* clean up duplicate buffer */
		qcs__cleanup_dup();
	}

	return 1;
}

int qcs_rxsocket(
	qcs_link link_id,
	int * p_rxsocket )
{
	link_data * link = (link_data*)link_id;

	if(!VALID_ID(link_id)) ERRRET(EINVAL);
	if(!ACTIVE_LINK(link)) ERRRET(EINVAL);

	*p_rxsocket = link->rx;
	return 1;
}

int qcs_waitinput(
	qcs_link link_id,
	int timeout_ms )
{
	link_data * link = (link_data *) link_id;
	struct timeval tv;
	fd_set fds;

	// check if link is valid
	if(!VALID_ID(link_id)) ERRRET(EINVAL);
	if(!ACTIVE_LINK(link)) ERRRET(EINVAL);

	// fill structs & select()
	if(timeout_ms < 0) {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	} else {
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;
	}

	FD_ZERO(&fds);
	FD_SET(link->rx, &fds);

	return select(FD_SETSIZE, &fds, NULL, NULL, &tv);
}

int qcs_send(
	qcs_link link_id,
	const qcs_msg * msg )
{
	char * datagram;
	ssize_t datagram_len, datagram_sent;
	struct sockaddr_in sab;
	link_data * link = (link_data *)link_id;

	// check link
	if(!VALID_ID(link_id)) ERRRET(EINVAL);
	if(!ACTIVE_LINK(link)) ERRRET(EINVAL);

	// build proto message
	switch(link->proto) {
	case QCS_PROTO_VYPRESS:
		datagram = qcs__make_vypress_msg(msg, &datagram_len);
		break;
	case QCS_PROTO_QCHAT:
		datagram = qcs__make_qchat_msg(msg, &datagram_len);
		break;
	default:
		ERRRET(ENOSYS);
		break;
	}
	if(datagram==NULL)
		ERRRET(EINVAL);

	sab.sin_family = PF_INET;
	sab.sin_port = htons(link->port);
	sab.sin_addr.s_addr = htonl(link->broadcast_addr);
	datagram_sent = sendto(
		link->tx, datagram, datagram_len, 0,
		(struct sockaddr*)&sab, sizeof(sab));

	free(datagram);

	if(datagram_sent != datagram_len)
		ERRRET(ENETUNREACH);

	return 1;
}

int qcs_recv(
	qcs_link link_id,
	qcs_msg * msg )
{
	link_data * link = (link_data *)link_id;
	char * buff;

	struct sockaddr_in sa;
	socklen_t sa_len;
	ssize_t dgram_size;
	int parse_ok;

	if(msg==NULL) ERRRET(EINVAL);

	if(!VALID_ID(link_id)) ERRRET(EINVAL);
	if(!ACTIVE_LINK(link)) ERRRET(EINVAL);

	buff = (char*) malloc(QCP_MAXUDPSIZE+0x10);
	if(buff==NULL) ERRRET(ENOMEM);

	/* receive data of the message */
	sa_len = sizeof(sa);
	dgram_size = recvfrom(
		link->rx, (void*)buff, QCP_MAXUDPSIZE, 0,
		(struct sockaddr*)&sa, &sa_len
	);
	
	/* fill in message source address */
	msg->src_ip = ntohl(sa.sin_addr.s_addr);
	
	/* check if msg is too long for us to process:
	 * just strip it down. we guess this was the last
	 * ascii field, in proto msg, that was soo long.
	 */
	if(dgram_size > QCP_MAXUDPSIZE)
		*(char*)(buff+QCP_MAXUDPSIZE-1) = '\0';

	/* failure */
	if(dgram_size < 0) {
		free(buff);
		return 0;
	}

	/* parse the message */
	switch(link->proto) {
	case QCS_PROTO_QCHAT:
		parse_ok = qcs__parse_qchat_msg(buff, dgram_size, msg);
		break;
	case QCS_PROTO_VYPRESS:
		parse_ok = qcs__parse_vypress_msg(buff, dgram_size, msg);
		break;
	default: break;
	}

	/* cleanup */
	free((void*)buff);
	return parse_ok;
}

qcs_msg * qcs_newmsg()
{
	qcs_msg * msg = malloc(sizeof(qcs_msg));
	if(msg==NULL)
		return NULL;

	msg->msg = QCS_MSG_INVALID;
	msg->umode = QCS_UMODE_INVALID;
	msg->uactive = 1;
	msg->src = msg->dst = msg->text = msg->supp = msg->chan = NULL;

	return msg;
}

void qcs_deletemsg( qcs_msg * msg )
{
	if(msg==NULL) return;

	if( msg->src ) free( msg->src );
	if( msg->dst ) free( msg->dst );
	if( msg->text ) free( msg->text );
	if( msg->supp ) free( msg->supp );
	if( msg->chan ) free( msg->chan );

	free( msg );
}

int qcs_msgset(
	qcs_msg * msg,
	enum qcs_textid which,
	const char * new_text )
{
	char ** ptext, * new_p;
	size_t slen;

	if(!msg) {
		errno = EINVAL;
		return 0;
	}

	switch(which) {
	case QCS_SRC: ptext = &msg->src; break;
	case QCS_DST: ptext = &msg->dst; break;
	case QCS_TEXT: ptext = &msg->text; break;
	case QCS_SUPP: ptext = &msg->supp; break;
	case QCS_CHAN: ptext = &msg->chan; break;
	default:
		errno = EINVAL;
		return 0;
	}

	if(*ptext==NULL && new_text==NULL) {
		/* we have nothing to set here */
		return 1;
	}

	/* try to allocate new space, if we have to
	 * before we damage something
	 */
	if(new_text) {
		slen = strlen(new_text);
		new_p = malloc(slen + 1);
		if(new_p==NULL) {
			/* return -- out of mem */
			errno = ENOMEM;
			return 0;
		}
		memcpy((void*)new_p, (void*)new_text, slen + 1);
	}

	/* clear previous text */
	if(*ptext!=NULL) {
		free((void*)*ptext);
		*ptext = NULL;

		/* return if we have nothing to set */
		if(new_text==NULL) {
			return 1;
		}
	}

	/* set new text */
	*ptext = new_p;

	return 1;
}
