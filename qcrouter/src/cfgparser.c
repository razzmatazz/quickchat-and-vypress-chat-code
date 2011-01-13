/**
 * qcRouter links several QuickChat & VypressChat nets through internet
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

/*	qcrouter project
 *		cfgparser.c
 *			parses config file & cmd line arguments
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "msg.h"
#include "net.h"
#include "cfgparser.h"

#define MIN_AVAIL	1
#define MAX_AVAIL	1023

static void parse_params(int, const char **, struct config *);
static void parse_config_file(FILE *, struct config *);

static void add_net_entry(
		struct config *, enum qnet_type,
		void *, unsigned short);
static const unsigned long * parse_broadcast_opt(char * opt);

struct config * read_config(
	int argc, const char ** argv)
{
	struct config * cfg = xalloc(sizeof(struct config));
	FILE * cfg_file;

	debug("read_config...");

	/** preset defaults
	 */
	cfg->avail_id = (unsigned short)rand();
	cfg->cfg_file_name = NULL;
	cfg->allow_host = 0;
	cfg->daemonize = 0;
	strcpy(cfg->host_if, "");
	cfg->host_port = 0;
	cfg->net_head = cfg->net_tail = NULL;
	cfg->net_count = 0;
	cfg->local_refresh_timeout = 30;

	/** parse cmd-line params
	 */
	parse_params(argc, argv, cfg);

	/** parse config file
	 */
	if(cfg->cfg_file_name)
	{
		cfg_file = fopen(cfg->cfg_file_name, "r");
		if(cfg_file!=NULL) {
			parse_config_file(cfg_file, cfg);

			fclose(cfg_file);
		} else {
			log_a("Could not open config file \"");
			log_a(cfg->cfg_file_name);
			log("\": ignored");
		}
	}

	/** fin
	 */
	return cfg;
}

void delete_config(struct config * cfg)
{
	struct config_net_entry * cne, * next_cne;

	assert(cfg && cfg->cfg_file_name);

	/* delete net list */
	cne = cfg->net_head;
	while(cne) {
		next_cne = cne->next;

		xfree(cne);

		cne = next_cne;
	}

	if(cfg->cfg_file_name) {
		xfree(cfg->cfg_file_name);
	}
	xfree(cfg);
}

static void print_usage(const char * prog_name)
{
	printf("%s: [-c[onfig] <config_file>] [-d[aemon]]\n",
		prog_name);
}

static void parse_params(
	int argc, const char ** argv,
	struct config * cfg)
{
	int i;
	assert(cfg);

	i = 0;
	while(++i < argc)
	{
		if(!strcmp(argv[i], "-licence")) {
			puts(LICENCE);
			exit(EXIT_SUCCESS);
		}
		if(!strcmp(argv[i], "-d")
			|| !strcmp(argv[i], "-daemon"))
		{
			cfg->daemonize = 1;
			continue;
		}
		if(!strcmp(argv[i], "-c")
			|| !strcmp(argv[i], "-config")) 
		{
			i++;

			if(i >= argc) {
				print_usage(argv[0]);
				log("invalid parameter(s): exiting..");
				exit(1);
			}

			if(cfg->cfg_file_name) {
				xfree(cfg->cfg_file_name);
			}
			cfg->cfg_file_name = xalloc(strlen(argv[i])+1);
			strcpy(cfg->cfg_file_name, argv[i]);

			continue;
		}

		print_usage(argv[0]);
		log("invalid parameter(s): exiting");
		exit(1);
	}
}

static void strip_whitespaces(
		char * line)
{
	assert(line);

	while(*line) {
		switch(*line) {
		case '\n':
		case '#':
			*line = '\0';
			return;
		case '\t':
		case ' ':
			memmove(line, line+1, strlen(line+1)+1);
			break;
		}
		line ++;
	}
}

static char * extract_next(char * opt)
{
	char * next;

	assert(opt);

	next = strchr(opt, ',');
	if(!next) return NULL;

	*next = '\0';

	return next+1;
}

static int process_config_entry(
	struct config * cfg,
	char * name,
	char * opt)	/* in <opt1>','<opt2>','...','<optN>'\0' format */
{
	char * next_opt, * host;
	unsigned short port;
	enum qnet_type type;
	assert(name);

	if(!strcasecmp(name, "first_id")) {
		if(!opt) return 0;
		next_opt = extract_next(opt);
		if(next_opt) return 0;
		if(!sscanf(opt, "%hd", &cfg->avail_id))
			return 0;
		return 1;
	}
	if(!strcasecmp(name, "host")) {
		if(!opt) return 0;
		next_opt = extract_next(opt);
		if(!next_opt) return 0;
		strcpy(cfg->host_if, opt);
		opt = next_opt;
		next_opt = extract_next(opt);
		if(next_opt) return 0;
		if(!sscanf(opt, "%hd", &cfg->host_port)) return 0;

		cfg->allow_host = 1;
		
		return 1;
	}
	if(!strcasecmp(name, "local")) {
		if(!opt) return 0;
		next_opt = extract_next(opt);
		if(!next_opt) return 0;

		/* extract connection type */
		if(strcasecmp(opt, "VCHAT") && strcasecmp(opt, "QCHAT")) {
			/* unknown/invalid local connection type */
			log_a("process_config_entry: invalid local connection "
				"type \"");
			log_a(opt); log("\"");
			return 0;
		}
		type = !strcasecmp(opt, "VCHAT")
			? QNETTYPE_VYPRESS_CHAT: QNETTYPE_QUICK_CHAT;

		/* extract port */
		opt = next_opt;
		next_opt = extract_next(opt);
		if(!sscanf(opt, "%hu", &port)) return 0;

		/* extract broadcast addressess */
		add_net_entry(
			cfg, type,
			(void*)parse_broadcast_opt(next_opt),
			port
		);
		return 1;
	}

	if(!strcasecmp(name, "remote")) {
		if(!opt) return 0;
		host = opt;
		next_opt = extract_next(opt);
		if(!next_opt) return 0;
		opt = next_opt;
		next_opt = extract_next(opt);
		if(next_opt) return 0;
		if(!sscanf(opt, "%hd", &port)) return 0;

		/* insert this entry */
		add_net_entry(cfg, QNETTYPE_ROUTER, (void*)host, port);
		return 1;
	}
	if(!strcasecmp(name, "daemon")) {
		if(!opt) return 0;
		next_opt = extract_next(opt);
		if(next_opt) return 0;

		cfg->daemonize = atoi(opt);
		return 1;
	}
	if(!strcasecmp(name, "local_refresh")) {
		if(!opt) return 0;
		next_opt = extract_next(opt);
		if(next_opt) return 0;

		cfg->local_refresh_timeout = atoi(opt);
		return 1;
	}

	return 0;
}

/** parse_broadcast_opt;
  *	parses network broadcast IPs in <ip1>,<ip2>,...<ipN>'\0' format
  *
  *	returns:
  *		NULL, if the list is empty
  *		0UL - terminated list of parsed ip's
  */
static const unsigned long *
parse_broadcast_opt(char * opt)
{
	struct in_addr in;
	char * next_opt;
	unsigned long * iplist = NULL;
	int alloced = 0;

	while(opt) {
		/* get next one */
		next_opt = extract_next(opt);

		if(inet_aton(opt, &in)) {
			/* insert parsed address into the list */
			if((alloced % 16)==0) {
				/* alloc in chunks of 16 IPs
				 * (plus add 1 (long) for NULL at the end) */
				iplist = xrealloc(iplist,
						sizeof(long)*(alloced+16+1));
			}
			iplist[alloced++] = ntohl(in.s_addr);
		} else {
			log_a("parse_broadcast_opt: invalid broadcast "
				"address: \"");
			log_a(opt); log("\": skipping");
		}

		/* skip to next ip */
		opt = next_opt;
	}
	/* finish list with 0UL */
	iplist[alloced] = 0UL;

	return iplist;
}

/** split_config_line:
  *	finds '=' char in configuration line and replaces it with '\0'
  *	thus you get config option in `line' and config params in *`p_opt'
  *
  *	during parse of boolean option (without params) *`p_opt' is set to NULL
  *
  * returns:
  *	0 on failure (config line is invalid/unparseable
  *			e.g. contains ',' in config_option)
  *	non-0 on success
  * modifies: *p_opt, line[]
  */
static int split_config_line(
	char * line, char ** p_opt)
{
	char * eq_sign;

	assert(line && p_opt);

	eq_sign = strchr(line, '=');
	if(eq_sign) {
		*(eq_sign++) = '\0';

		if(!strlen(eq_sign) || strchr(eq_sign, '=')) {
			return 0;
		}
		*p_opt = eq_sign;
	} else {
		*p_opt = NULL;
	}

	if(strchr(line, ',')) {
		return 0;
	}

	return 1;
}

static void parse_config_file(
	FILE * cfg_file,
	struct config * cfg)
{
	char * line = xalloc(512);
	char * backup = xalloc(512);
	char * cfg_options;

	assert(cfg_file && cfg);

	/* scan entire file on line basis */
	while(!feof(cfg_file))
	{
		/* get new line and make backup
		 * (for logging purposes)	*/
		fgets(line, 512, cfg_file);
		strcpy(backup, line);
		if(backup[strlen(backup)-1]=='\n') {
			backup[strlen(backup)-1] = '\0';
		}

		/* remove whitespaces & comments
		 * and skip to next line if this one is empty */
		strip_whitespaces(line);
		if(strlen(line)==0)
			continue;

		/* split config line to <config_name>'='<config_options> */
		if(!split_config_line(line, &cfg_options)) {
			log_a("config:\tinvalid config line \"");
			log_a(backup); log("\"");
			continue;
		}
	
		/* parse config entry */
		if(!process_config_entry(cfg, line, cfg_options)) {
			log_a("config:\tinvalid config line \"");
			log_a(backup); log("\"");
		}
	}

	xfree(line);
	xfree(backup);
}

static void add_net_entry(
	struct config * cfg,
	enum qnet_type type,
	void * addr,	/* can be `const char*' for remote nets (hostname) */
			/* or `long []' for local nets (broadcast ip) */
	unsigned short port)
{
	int hostname_len;
	struct config_net_entry * cne;

	/* hostname must be specified for ROUTER connections */
	assert(cfg && ((type==QNETTYPE_ROUTER && addr)||type!=QNETTYPE_ROUTER));

	/* setup new config entry */
	cne = xalloc(sizeof(struct config_net_entry));
	cne->type = type;
	cne->port = port;
	cne->next = NULL;

	switch(type) {
	case QNETTYPE_ROUTER:
		/* fill in hostname */
		hostname_len = strlen((const char*)addr);
		if(hostname_len>CONFIG_MAX_HOSTNAME)
			panic("hostname for remote net too long");
		memcpy(cne->hostname, (const char*)addr, hostname_len+1);
		break;
	case QNETTYPE_QUICK_CHAT:
	case QNETTYPE_VYPRESS_CHAT:
		/* fill in broadcast list */
		cne->broadcasts = (unsigned long*)addr;
		break;
	default:break;
	}

	/* insert into the list */
	if(cfg->net_count==0) {
		cfg->net_head = cfg->net_tail = cne;
	} else {
		cfg->net_tail->next = cne;
		cfg->net_tail = cne;
	}

	++ cfg->net_count;
}

