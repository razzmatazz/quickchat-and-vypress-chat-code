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
 *      supplementary routines
 *
 * (c) Saulius Menkevicius 2001,2002
 */

#ifndef SUPP_H
#define SUPP_H

int qcs__local_qcmode(int);
#define qcs__local_qcwatch(m) (((m)=='1')?QCS_UMODE_WATCH:0)
int qcs__net_qcmode(int);
#define qcs__net_qcwatch(m) (((m)&QCS_UMODE_WATCH)?'1':'2')
void qcs__cleanupmsg(qcs_msg *);
char * qcs__gatherstr(const char **, int *);

/* vypress chat protocol signature stuff */
#define QCS_SIGNATURE_LENGTH	(9)

int qcs__known_dup_entry(const char *);
void qcs__remove_last_entry();
int qcs__insert_dup_entry(const char *);
void qcs__generate_signature(char *);
int qcs__cleanup_dup();

#endif	/* SUPP_H */
