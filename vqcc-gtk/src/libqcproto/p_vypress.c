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
 *	vypress chat protocol implementation
 *
 * (c) Saulius Menkevicius 2001-2004
 */

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "qcproto.h"
#include "supp.h"
#include "link.h"
#include "p_vypress.h"
#include "p_qchat.h"

#define ADDCHAR(ch) do{raw_msg[(raw_len)++]=(ch);}while(0)
#define ADDSTR(s) do{\
	if(s==NULL){free(raw_msg);errno=ENOMSG;return NULL;}	\
	memcpy(raw_msg+raw_len,(s),slen=strlen(s)+1);	\
	raw_len+=slen;				\
	}while(0)
#define ADDCHAN(s) do{ADDCHAR('#');ADDSTR(s);}while(0)

char * qcs__make_vypress_msg(
	const qcs_msg * msg,
	ssize_t * pmsg_len )
{
	ssize_t raw_len = 0, slen;
	char * msg_buf,
		* raw_msg = (char *)malloc(QCP_MAXUDPSIZE);
	
	if( raw_msg==NULL ) {
		errno = ENOMEM;
		return NULL;
	}

	switch(msg->msg) {
		/* process differences of qc16 and vypresschat10 */
	case QCS_MSG_TOPIC_CHANGE:
		ADDCHAR('B');
		ADDCHAN(msg->chan);
		ADDSTR(msg->text);
		break;
		
	case QCS_MSG_TOPIC_REPLY:
		ADDCHAR('C');
		ADDSTR(msg->dst);
		ADDCHAN(msg->chan);
		ADDSTR(msg->text);
		break;

	case QCS_MSG_INFO_REPLY:
		ADDCHAR('G');
		ADDSTR(msg->dst);
		ADDSTR(msg->src);
		ADDSTR(msg->text);
		ADDSTR("(Unknown)");
		ADDSTR("(Unknown)");
		ADDSTR(msg->chan);
		ADDSTR(msg->supp);
		break;
	default:
		/* fallback to qc16 message format */
		free(raw_msg);
		raw_msg = (char*)qcs__make_qchat_msg(msg, &raw_len);
		break;
	}

	/* allocate buffer for final version */
	msg_buf = malloc(QCP_MAXUDPSIZE+10);
	if( msg_buf==NULL ) {
		free(raw_msg);
		errno = ENOMEM;
		return NULL;
	}

	/* append msg id and register for duplicates */
	qcs__generate_signature( msg_buf );

	/* build up final "version" */
	memcpy(msg_buf+10, raw_msg, raw_len);
	if( pmsg_len ) {
		*pmsg_len = raw_len + 10;
	}

	free(raw_msg);

	return msg_buf;
}

#define GETCHAR(c) do {\
	if(!src_len){qcs__cleanupmsg(msg);errno=ENOMSG;return 0;}\
	(c)=*(src++);	\
	src_len--;	\
	}while(0)
#define GETSTR(sp) do {\
	if(((sp)=qcs__gatherstr(&src,&src_len))==NULL) {\
		errno=ENOMSG;		\
		qcs__cleanupmsg(msg);	\
		return 0;}	\
	}while(0)
#define GETCHAN(s) do{\
	GETCHAR(ch);if(ch!='#'){errno=ENOMSG;qcs__cleanupmsg(msg);return 0;}\
	GETSTR(s);} while(0)

int qcs__parse_vypress_msg(
	const char * src, ssize_t src_len,
	qcs_msg * msg )
{
	int parsed_ok;
	char ch;

	qcs__cleanupmsg(msg);

	/* check for signature consistency and duplicates */
	if( src_len < (QCS_SIGNATURE_LENGTH + 2) ) {
		/* not a vypress extension packet */
		errno = ENOMSG;
		return 0;
	}

	if(*src!='X') {
		/* every vypress chat packet begins with 'X' prefix */
		return 0;
	}
	
	if( qcs__known_dup_entry(src+1)) {
		/* duplicate encountered: ignore */
		return 1;
	} else {
		/* not seen before: register */
		qcs__insert_dup_entry(src+1);
	}

	/* skip signature: already parsed */
	src += QCS_SIGNATURE_LENGTH + 1;
	src_len -= QCS_SIGNATURE_LENGTH + 1;

	/* parse message contents */
	switch( *(src++) )
	{
	case 'B':
		msg->msg = QCS_MSG_TOPIC_CHANGE;
		GETCHAN(msg->chan);
		GETSTR(msg->text);
		break;
	case 'C':
		msg->msg = QCS_MSG_TOPIC_REPLY;
		GETSTR(msg->dst);
		GETCHAN(msg->chan);
		GETSTR(msg->text);
		break;
	case 'G':
		msg->msg = QCS_MSG_INFO_REPLY;
		GETSTR(msg->dst);
		GETSTR(msg->src);
		GETSTR(msg->text);
		GETSTR(msg->chan);free(msg->chan);
		GETSTR(msg->chan);free(msg->chan);
		GETSTR(msg->chan);
		GETSTR(msg->supp);
		break;
	default:
		/* where it doesn't diff: parse with qc parser */
		parsed_ok = qcs__parse_qchat_msg(src-1, src_len, msg);

		/* flush msg id cache if the user has left the net
		 * (seems like, vypress chat v1.0 repeats the same
		 * msg id, if restarted: unseeded rand() ??)
		 */
		if(parsed_ok && msg->msg==QCS_MSG_CHANNEL_LEAVE
			&& !strcasecmp(msg->chan, "main"))
		{
			qcs__cleanup_dup();
		}

		return parsed_ok;
	}

	return 1;
}
