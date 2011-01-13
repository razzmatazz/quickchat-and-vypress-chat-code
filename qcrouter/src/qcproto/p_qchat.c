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
 *	qchat 1.6 protocol implementation
 *
 * (c) Saulius Menkevicius 2001,2002,2003
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "qcs_link.h"
#include "supp.h"
#include "link.h"
#include "p_qchat.h"

#define ADDCHAR(ch) do{msg_buf[(*pmsg_len)++]=(ch);}while(0)
#define ADDSTR(s) do{\
	if(s==NULL){free(msg_buf);errno=ENOMSG;return NULL;}	\
	memcpy(msg_buf+*pmsg_len,(s),slen=strlen(s)+1);	\
	*pmsg_len+=slen;				\
	}while(0)
#define ADDCHAN(s) do{ADDCHAR('#');ADDSTR(s);}while(0)

const char * qcs__make_qchat_msg(
	const qcs_msg * msg,
	int * pmsg_len )
{
	char * msg_buf;
	int slen;

	assert(msg && pmsg_len);

	msg_buf = (char*) malloc(QCP_MAXUDPSIZE);
	*pmsg_len = 0;
	if( msg_buf==NULL ) {
		errno = ENOMEM;
		return NULL;
	}

	switch(msg->msg) {
	case QCS_MSG_REFRESH_REQUEST:
		ADDCHAR('0');
		ADDSTR(msg->src);
		break;
	case QCS_MSG_REFRESH_ACK:
		ADDCHAR('1');
		ADDSTR(msg->dst);
		ADDSTR(msg->src);
		ADDCHAR(qcs__net_qcmode(msg->mode));
		ADDCHAR(qcs__net_qcwatch(msg->mode));
		break;
	case QCS_MSG_CHANNEL_BROADCAST:
		ADDCHAR('2');
		ADDCHAN(msg->chan);
		ADDSTR(msg->src);
		ADDSTR(msg->text);
		break;
	case QCS_MSG_CHANNEL_JOIN:
		ADDCHAR('4');
		ADDSTR(msg->src);
		ADDCHAN(msg->chan);
		ADDCHAR(qcs__net_qcmode(msg->mode));
		ADDCHAR('0');		/* unknown purpose !! */
		break;
	case QCS_MSG_CHANNEL_LEAVE:
		ADDCHAR('5');
		ADDSTR(msg->src);
		ADDCHAN(msg->chan);
		ADDCHAR('0');		/* unknown */
		break;
	case QCS_MSG_CHANNEL_ME:
		ADDCHAR('A');
		ADDCHAN(msg->chan);
		ADDSTR(msg->src);
		ADDSTR(msg->text);
		break;
	case QCS_MSG_MESSAGE_ACK:
		ADDCHAR('7');
		ADDCHAR(qcs__net_qcmode(msg->mode));
		ADDSTR(msg->dst);
		ADDSTR(msg->src);
		ADDCHAR('0');		/* unknown purpose */
		ADDSTR(msg->text);
		break;
	case QCS_MSG_MESSAGE_MASS:
		ADDCHAR('E');
		ADDSTR(msg->src);
		ADDSTR(msg->dst);
		ADDSTR(msg->text);
		break;
	case QCS_MSG_MESSAGE_SEND:
		ADDCHAR('6');
		ADDSTR(msg->src);
		ADDSTR(msg->dst);
		ADDSTR(msg->text);
		break;
	case QCS_MSG_RENAME:
		ADDCHAR('3');
		ADDSTR(msg->src);
		ADDSTR(msg->text);
		ADDCHAR('0');		/* unknown */
		break;
	case QCS_MSG_MODE_CHANGE:
		ADDCHAR('D');
		ADDSTR(msg->src);
		ADDCHAR(qcs__net_qcmode(msg->mode));
		ADDCHAR('0');		/* unknown */
		break;
	case QCS_MSG_WATCH_CHANGE:
		ADDCHAR('M');
		ADDSTR(msg->src);
		ADDCHAR(qcs__net_qcwatch(msg->mode));
		break;
	case QCS_MSG_TOPIC_REPLY:
		ADDCHAR('C');
		ADDSTR(msg->dst);
		ADDSTR(msg->text);
		if(msg->chan==NULL
			|| strcasecmp(msg->chan, "Main"))
		{
			free(msg_buf);
			errno = ENOMSG;
			return NULL;
		}
		break;
	case QCS_MSG_TOPIC_CHANGE:
		/* check that the channel is "Main",
		 * in QC1.x topic is the same for all channels
		 * (while each channel has it's topic in Vypress Chat)
		 */
		if(msg->chan==NULL
			|| strcasecmp(msg->chan, "Main"))
		{
			free(msg_buf);
			errno = ENOMSG;
			return NULL;
		}

		ADDCHAR('B');
		ADDSTR(msg->text);
		break;
	case QCS_MSG_INFO_REPLY:
		ADDCHAR('G');
		ADDSTR(msg->dst);
		ADDSTR(msg->src);
		ADDSTR(msg->text);
		ADDSTR("QcProto1.6");
		ADDSTR("0 %");
		ADDSTR("0 Kb");
		ADDSTR(msg->chan);
		ADDSTR(msg->supp);
		break;
	case QCS_MSG_INFO_REQUEST:
		ADDCHAR('F');
		ADDSTR(msg->dst);
		ADDSTR(msg->src);
		break;
	case QCS_MSG_CHANMEMBER_REPLY:
		ADDCHAR('K');
		ADDSTR(msg->dst);
		ADDSTR(msg->chan);
		ADDSTR(msg->src);
		break;
	case QCS_MSG_CHANMEMBER_REQUEST:
		ADDCHAR('L');
		ADDSTR(msg->src);
		break;
	case QCS_MSG_CHANLIST_REPLY:
		ADDCHAR('O');
		ADDSTR(msg->dst);
		ADDSTR(msg->chan);
		break;
	case QCS_MSG_CHANLIST_REQUEST:
		ADDCHAR('N');
		ADDSTR(msg->src);
		break;
	case QCS_MSG_BEEP_SEND:
	case QCS_MSG_BEEP_ACK:
		ADDCHAR('H');
		ADDCHAR((msg->msg==QCS_MSG_BEEP_ACK)?'1':'0');
		ADDSTR(msg->dst);
		ADDSTR(msg->src);
		if(msg->msg==QCS_MSG_BEEP_ACK) {
			ADDCHAR('0');	/* dummy */
		}
		break;
	case QCS_MSG_PRIVATE_ME:
	case QCS_MSG_PRIVATE_TEXT:
		ADDCHAR('J');
		ADDCHAR(msg->msg==QCS_MSG_PRIVATE_ME ? '3': '2');
		ADDSTR(msg->src);
		ADDSTR(msg->dst);
		ADDSTR(msg->text);
		break;
	case QCS_MSG_PRIVATE_OPEN:
	case QCS_MSG_PRIVATE_CLOSE:
		ADDCHAR('J');
		ADDCHAR(msg->msg==QCS_MSG_PRIVATE_CLOSE ? '1': '0');
		ADDSTR(msg->src);
		ADDSTR(msg->dst);
		ADDCHAR('0');		/* dummy/unknown */
		break;
	default:
		errno = ENOMSG;
		free(msg_buf);
		return NULL;
	}

	return msg_buf;
}

#define GETMODE(c) do {\
	GETCHAR((c));\
	(c)=qcs__local_qcmode(c);\
	if((c)==QCS_UMODE_INVALID){\
		qcs__cleanupmsg(msg);\
		errno=ENOMSG;return 0;}\
	}while(0)
#define GETCHAR(c) do {\
	if(!pmsg_len){qcs__cleanupmsg(msg);errno=ENOMSG; return 0;}	\
	(c)=*(pmsg++);pmsg_len--;\
	}while(0)
#define GETSTR(sp) do {\
	if(((sp)=qcs__gatherstr(&pmsg,&pmsg_len))==NULL) {\
		errno=ENOMSG;\
		qcs__cleanupmsg(msg);return 0;}\
	}while(0)
#define GETCHAN(s) do{\
	GETCHAR(ch);if(ch!='#'){errno=ENOMSG;return 0;}\
	GETSTR(s);} while(0)

int qcs__parse_qchat_msg(
	const char * pmsg, int pmsg_len,
	qcs_msg * msg )
{
	char ch;

	assert( pmsg && pmsg_len && msg );

	qcs__cleanupmsg(msg);

	GETCHAR(ch);
	switch(ch) {
	case '0':
		msg->msg = QCS_MSG_REFRESH_REQUEST;
		GETSTR(msg->src);
		break;
	case '1':
		msg->msg = QCS_MSG_REFRESH_ACK;
		GETSTR(msg->dst);
		GETSTR(msg->src);
		GETMODE(msg->mode);
		if( pmsg_len ) { /* check WATCH in qc16 */
			GETCHAR(ch);
			msg->mode |= (ch=='1') ? QCS_UMODE_WATCH: 0;
		}
		break;
	case '2':
		msg->msg = QCS_MSG_CHANNEL_BROADCAST;
		GETCHAN(msg->chan);
		GETSTR(msg->src);
		GETSTR(msg->text);
		break;
	case '4':
		msg->msg = QCS_MSG_CHANNEL_JOIN;
		GETSTR(msg->src);
		GETCHAN(msg->chan);
		GETMODE(msg->mode);
		/*dummy byte skipped*/
		break;
	case '5':
		msg->msg = QCS_MSG_CHANNEL_LEAVE;
		GETSTR(msg->src);
		GETCHAN(msg->chan);
		/*dummy*/
		break;
	case 'A':
		msg->msg = QCS_MSG_CHANNEL_ME;
		GETCHAN(msg->chan);
		GETSTR(msg->src);
		GETSTR(msg->text);
		break;
	case '7':
		msg->msg = QCS_MSG_MESSAGE_ACK;
		GETMODE(msg->mode);
		GETSTR(msg->dst);
		GETSTR(msg->src);
		GETCHAR(ch);	/* dummy */
		GETSTR(msg->text);
		break;
	case 'E':
		msg->msg = QCS_MSG_MESSAGE_MASS;
		GETSTR(msg->src);
		GETSTR(msg->dst);
		GETSTR(msg->text);
		break;
	case '6':
		msg->msg = QCS_MSG_MESSAGE_SEND;
		GETSTR(msg->src);
		GETSTR(msg->dst);
		GETSTR(msg->text);
		break;
	case '3':
		msg->msg = QCS_MSG_RENAME;
		GETSTR(msg->src);
		GETSTR(msg->text);
		/*dummy*/
		break;
	case 'D':
		msg->msg = QCS_MSG_MODE_CHANGE;
		GETSTR(msg->src);
		GETMODE(msg->mode);
		/* dummy byte passed by */
		break;
	case 'M':
		msg->msg = QCS_MSG_WATCH_CHANGE;
		GETSTR(msg->src);
		GETCHAR(ch);
		msg->mode = ch=='1' ? QCS_UMODE_WATCH:0;
		break;
	case 'C':
		msg->msg = QCS_MSG_TOPIC_REPLY;
		GETSTR(msg->dst);
		GETSTR(msg->text);
		
		/* add "Main" as msg->chan */
		msg->chan = malloc(5);
		if(!msg->chan) {
			qcs__cleanupmsg(msg);
			errno = ENOMEM;
			return 0;
		}
		memcpy(msg->chan, "Main", 5);
		break;
	case 'B':
		msg->msg = QCS_MSG_TOPIC_CHANGE;
		GETSTR(msg->text);
		
		/* fill in "Main": this is unsupported in qc,
		 * thus we need to fill it in */
		msg->chan = malloc(5);
		if(!msg->chan) {
			qcs__cleanupmsg(msg);
			errno = ENOMEM;
			return 0;
		}
		memcpy(msg->chan, "Main", 5);
		break;
	case 'G':
		msg->msg = QCS_MSG_INFO_REPLY;
		GETSTR(msg->dst);
		GETSTR(msg->src);
		GETSTR(msg->text);
		/* skip 3 strings - not used */
		GETSTR(msg->chan);free(msg->chan);
		GETSTR(msg->chan);free(msg->chan);
		GETSTR(msg->chan);free(msg->chan);
		GETSTR(msg->chan);
		GETSTR(msg->supp);
		break;
	case 'F':
		msg->msg = QCS_MSG_INFO_REQUEST;
		GETSTR(msg->dst);
		GETSTR(msg->src);
		break;
	case 'K':
		msg->msg = QCS_MSG_CHANMEMBER_REPLY;
		GETSTR(msg->dst);
		GETSTR(msg->chan);
		GETSTR(msg->src);
		break;
	case 'L':
		msg->msg = QCS_MSG_CHANMEMBER_REQUEST;
		GETSTR(msg->src);
		break;
	case 'O':
		msg->msg = QCS_MSG_CHANLIST_REPLY;
		GETSTR(msg->dst);
		GETSTR(msg->chan);
		break;
	case 'N':
		msg->msg = QCS_MSG_CHANLIST_REQUEST;
		GETSTR(msg->src);
		break;
	case 'H':
		GETCHAR(ch);
		switch(ch) {
		case '1': msg->msg = QCS_MSG_BEEP_ACK;	break;
		case '0': msg->msg = QCS_MSG_BEEP_SEND;	break;
		default: errno = ENOMSG;
			 return 0;
		}
		GETSTR(msg->dst);
		GETSTR(msg->src);
		/* dummy byte when 'H1' */
		break;
	case 'J':
		GETCHAR(ch);
		switch(ch) {
		case '0':msg->msg = QCS_MSG_PRIVATE_OPEN;	break;
		case '1':msg->msg = QCS_MSG_PRIVATE_CLOSE;	break;
		case '2':msg->msg = QCS_MSG_PRIVATE_TEXT;	break;
		case '3':msg->msg = QCS_MSG_PRIVATE_ME;		break;
		default: errno = ENOMSG;
			 return 0;
		}
		GETSTR(msg->src);
		GETSTR(msg->dst);
		if(ch=='2'||ch=='3') {
			GETSTR(msg->text);
		}
		break;
	default:
		errno = ENOMSG;
		return 0;
	}
	return 1;
}
