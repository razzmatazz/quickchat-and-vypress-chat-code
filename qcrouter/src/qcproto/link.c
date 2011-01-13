/**
 * qcs_link: Vypress/QChat protocol interface library
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
 * (c) Saulius Menkevicius 2001,2002
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>

#include "qcs_link.h"
#include "link.h"
#include "supp.h"
#include "p_vypress.h"
#include "p_qchat.h"

#define VALID_ID(id)	(id!=NULL)
#define ERRRET(err)	if(1){errno=(err);return(0);}
#define ACTIVE_LINK(l)	((l)->rx >=0)
#define INVALIDATE(l)	((l)->rx=-1)

/** static variables */
static int link_count = 0;

/** internal implementation routines	*/

/* setup_bcast_list:
 *	setups broadcast list					*/
static unsigned long *
	setup_bcast_list(const unsigned long * src_list,
			unsigned int * p_count)
{
	unsigned long * list;
	unsigned int sz;

	assert(p_count);

	if(src_list==NULL || *src_list==0UL) {
		/* assume INADDR_BROADCAST for "NULL"/empty list
		 */
		list = malloc(sizeof(unsigned long));
		if(!list) {
			errno = ENOMEM;
			return NULL;
		}
		*list = htonl(INADDR_BROADCAST);
		*p_count = 1;
		return list;
	}

	/* scan the size of the list */
	sz = 0;
	while(src_list[sz]!=0UL) sz++;

	*p_count = sz;

	/* alloc list */
	list = malloc(sizeof(unsigned long)*sz);
	if(!list) {
		errno = ENOMEM;
		return NULL;
	}

	/* fill in & return the list */
	for(sz = 0; src_list[sz]; sz++) {
		list[sz] = htonl(src_list[sz]);
	}

	return list;
}

/* bind_link:
 *	binds sockets to specified port on spec interface	*/
static int bind_link(
	link_data * link,
	unsigned short port )	/* port to bind to */
{
	struct sockaddr_in sa;

	assert( link->tx>=0 && link->rx>=0 && link);

	/* bind rx */
	sa.sin_family = PF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(port);
	if( bind( link->rx, (struct sockaddr*)&sa, sizeof(sa))==-1 ) {
		return 0;
	}

	link->port = port;

	/* succ */
	return 1;
}

/** API implementation			*/
qcs_link qcs_open(
	int proto_mode,
	const unsigned long * broadcasts,
	unsigned short port )
{
	const int broadcast_on = 1;
	link_data * link;
	int errbak;

	/* check params */
	if( proto_mode!=QCS_PROTO_VYPRESS && proto_mode!=QCS_PROTO_QCHAT ) {
		ERRRET(ENOSYS);
	}

	/* alloc link */
	link = malloc(sizeof(link_data));
	if(link==NULL) {
		ERRRET(ENOMEM);
	}

	/* setup broadcast list */
	link->broadcasts = setup_bcast_list(broadcasts, &link->broadcast_count);
	if(link->broadcasts == NULL) {
		errbak = errno;
		free(link);
		ERRRET(errbak);
	}

	/* alloc sockets */
	link->tx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if( link->tx < 0 ) {
		free(link);
		return 0;
	}

        /* switch tx to broadcast mode */
        if( setsockopt(link->tx, SOL_SOCKET, SO_BROADCAST,
                (void*)&broadcast_on, sizeof(broadcast_on)) != 0 )
        {
		errbak = errno;
		close(link->tx);
		free(link);
		ERRRET(errbak);
	}

	/* setup rx */
	link->rx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if( link->rx < 1 ) {
		errbak = errno;
		close(link->tx);
		free(link);
		ERRRET(errbak);
	}

	if(!port) {
		/* adjust port */
		port = 8167;
	}

	/* bind rx */
	if( !bind_link(link, port)) {
		errbak = errno;
		close(link->rx);
		close(link->tx);
		free(link);
		ERRRET(errbak);
	}

	/* set mode */
	link->mode = proto_mode;

	link_count ++;

	/* return success */
	return (qcs_link)link;
}

int qcs_close(qcs_link link_id)
{
	link_data * link = (link_data *)link_id;

	/* check if link is valid */
	if( !VALID_ID(link_id)) {
		ERRRET(EINVAL);
	}

	if(!ACTIVE_LINK(link)) {
		ERRRET(EINVAL);
	}

	/* shutdown sockets */
	close(link->rx);
	close(link->tx);

	/* delete broadcast ip list */
	free(link->broadcasts);

	/* delete link entry */
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
	const char * proto_msg;
	int proto_len, retval;
	struct sockaddr_in sab;
	link_data * link = (link_data *)link_id;
	int bcast, succ = 0;

	// check link
	if(!VALID_ID(link_id)) ERRRET(EINVAL);
	if(!ACTIVE_LINK(link)) ERRRET(EINVAL);

	// build proto message
	switch(link->mode) {
	case QCS_PROTO_VYPRESS:
		proto_msg = qcs__make_vypress_msg(msg, &proto_len);
		break;
	case QCS_PROTO_QCHAT:
		proto_msg = qcs__make_qchat_msg(msg, &proto_len);
		break;
	}
	if( proto_msg==NULL ) {
		// failed to build msg
		ERRRET(EINVAL);
	}

	sab.sin_family = PF_INET;
	sab.sin_port = htons(link->port);

	/* send msg to every network in bcast list */
	for(bcast=0; bcast < link->broadcast_count; bcast++)
	{
		sab.sin_addr.s_addr = link->broadcasts[bcast];

		retval = sendto(
			link->tx, proto_msg, proto_len, 0,
			(struct sockaddr*)&sab, sizeof(sab));

		/* we return success if we managed to 
		 * send to at least one broadcast address */
		succ |= retval==proto_len;
	}
	free((void*)proto_msg);

	if(!succ) errno = ENETUNREACH;
	return succ;
}

int qcs_recv(
	qcs_link link_id,
	qcs_msg * msg )
{
	link_data * link = (link_data *)link_id;
	char * buff;

	struct sockaddr_in sa;
	socklen_t sa_len;
	int retval;

	if(msg==NULL) ERRRET(EINVAL);

	if(!VALID_ID(link_id)) ERRRET(EINVAL);
	if(!ACTIVE_LINK(link)) ERRRET(EINVAL);

	buff = (char*) malloc(QCP_MAXUDPSIZE+0x10);
	if(buff==NULL) ERRRET(ENOMEM);

	// recv the data
	sa_len = sizeof(sa);
	retval = recvfrom(
		link->rx, (void*)buff, QCP_MAXUDPSIZE, 0,
		(struct sockaddr*)&sa, &sa_len
	);
	
	/* check if msg is too long for us to process:
	 * just strip it down. we guess this was the last
	 * ascii field, in proto msg, that was soo long.
	 */
	if(retval==QCP_MAXUDPSIZE) {
		*(char*)(buff+QCP_MAXUDPSIZE-1)='\0';
	}

	/* failure */
	if(retval < 0) {
		free(buff);
		/* errno left from recvfrom() */
		return 0;
	}

	/* parse the message */
	switch(link->mode)
	{
	case QCS_PROTO_QCHAT:
		retval = qcs__parse_qchat_msg(buff, retval, msg);
		break;
	case QCS_PROTO_VYPRESS:
		retval = qcs__parse_vypress_msg(buff, retval, msg);
		break;
	}

	/* cleanup */
	free((void*)buff);
	return retval;
}

qcs_msg * qcs_newmsg()
{
	qcs_msg * msg = malloc( sizeof(qcs_msg));
	if( msg==NULL ) {
		// out of memory
		return NULL;
	}

	msg->msg = QCS_MSG_INVALID;
	msg->mode = QCS_UMODE_INVALID;
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
	int slen;

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
