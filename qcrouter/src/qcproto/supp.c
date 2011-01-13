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
 *	supplementary routines
 *
 * (c) Saulius Menkevicius 2001,2002
 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "qcs_link.h"
#include "supp.h"
#include "link.h"

int qcs__local_qcmode(int net)
{
	switch(net) {
	case '0': return QCS_UMODE_NORMAL;
	case '1': return QCS_UMODE_DND;
	case '2': return QCS_UMODE_AWAY;
	case '3': return QCS_UMODE_OFFLINE;
	}
	return QCS_UMODE_INVALID;
}

int qcs__net_qcmode(int local)
{
	switch(local) {
	case QCS_UMODE_NORMAL:	return '0';
	case QCS_UMODE_DND:	return '1';
	case QCS_UMODE_AWAY:	return '2';
	case QCS_UMODE_OFFLINE:	return '3';
	}
	return '0';	/* let's assume we didn't see this :) */
}

void qcs__cleanupmsg(
	qcs_msg * msg )
{
	assert(msg);
	msg->msg=QCS_MSG_INVALID;
	if(msg->text){free(msg->text);msg->text=NULL;}
	if(msg->supp){free(msg->supp);msg->supp=NULL;}
	if(msg->src){free(msg->src);msg->src=NULL;}
	if(msg->dst){free(msg->dst);msg->dst=NULL;}
	if(msg->chan){free(msg->chan);msg->chan=NULL;}
}
char * qcs__gatherstr(
	const char ** str, int * len)
{
	char *retstr;
	const char * t;
	int ln;

	assert(str && *str && len);

	if(*len<0) return NULL;

	t = *str;
	ln = *len;

	while(ln && *t!='\0') {
		ln--; t++;
	}
	if(ln==0) return NULL;
	ln = *len - ln + 1;

	retstr = malloc(ln);
	if(retstr==NULL) {
		errno = ENOMEM;
		return NULL;
	}
	memcpy(retstr, *str, ln);

	*len -= ln;
	*str += ln;

	return retstr;
}

/** signature checking stuff ***
  ****************************/

#define QCS_SIGNATURE_MAX_DUP	(0x20)

/* struct dup_entry:
 *	defines list node for duplicated check
 */
struct dup_entry {
	char 	signature[ QCS_SIGNATURE_LENGTH ];
	struct dup_entry
		* next, * prev;
};
#define EQ_SIGNATURE(s1,s2)	(memcmp((s1),(s2),QCS_SIGNATURE_LENGTH)==0)
#define CP_SIGNATURE(d,s)	memcpy((d),(s),QCS_SIGNATURE_LENGTH)

static struct dup_entry * dup_head = NULL, * dup_tail = NULL;
static int dup_count = 0;

int qcs__known_dup_entry(
	const char *signature )
{
	struct dup_entry * e;

	/* search for specified signature */
	for( e = dup_head; e; e = e->next ) {
		if( EQ_SIGNATURE(e->signature, signature) ) {
			return 1;
		}
	}
	return 0;
}

void qcs__remove_last_entry()
{
	struct dup_entry * dead;

	assert(dup_count!=0);

	/* adjust list references */
	dead = dup_tail;
	if( dup_head==dup_tail ) {
		dup_head = dup_tail = NULL;
	} else {
		dup_tail = dup_tail->prev;
		dup_tail->next = NULL;
	}
	dup_count --;

	/* free entry */
	free( dead );
}

int qcs__insert_dup_entry(
	const char *signature )
{
	struct dup_entry * e;

	assert(signature != NULL);

	/* memalloc can fail here, before it does any harm */
	e = malloc(sizeof(struct dup_entry));
	if( e==NULL ) {
		errno = ENOMEM;
		return 0;
	}

	/* check for overflow */
	if( dup_count==QCS_SIGNATURE_MAX_DUP ) {
		qcs__remove_last_entry();
	}

	/* setup and insert new signature */
	e->prev = NULL;
	e->next = dup_head;
	CP_SIGNATURE(e->signature, signature);

	if(!dup_count) {
		dup_tail = e;
	} else {
		dup_head->prev = e;
	}
	dup_head = e;

	dup_count ++;

	return 1;
}

void qcs__generate_signature(char * buf)
{
	int i;

	*buf = 'X';
	for(i=1; i < (1+QCS_SIGNATURE_LENGTH); i++) {
		*(buf+i) = (unsigned char)('a'+rand()%('z'-'a'+1));
	}
}

int qcs__cleanup_dup()
{
	/* free any dup entries allocated */
	struct dup_entry * cur, * dead;
	cur = dup_head;
	while( cur != NULL ) {
		dead = cur;
		cur = cur->next;

		free( dead );
	}
	dup_head = dup_tail = NULL;
	dup_count = 0;

	return 1;
}


