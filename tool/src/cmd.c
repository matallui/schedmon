#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <argp.h>
#include "driver.h"
#include "cmd.h"

#define SMON_USAGE	"usage: smon [--help] COMMAND [ARGS]"
#define SMON_HELP	"\n usage: smon [--help] COMMAND [ARGS]\n\n \
List of SchedMon commands:\n   \
event   \tTool for event management.\n   \
evset   \tTool for even-set management.\n   \
envir   \tTool for environment management.\n   \
profile \tTool for profiling (sampling).\n   \
stat 	\tTool for profiling (total counts)\n   \
roof    \tTool for statistical counting.\n\n \
See 'smon COMMAND [help]' for more information on a specific command.\n"

#define EVENT_HELP	"\n usage: smon event --add|-a tag=TAG,evsel=EVSEL,umask=UMASK[,mode=MODE]\n        \
smon event --del|-d id=ID\n        \
smon event --list|-l \n\n \
List of 'event --add' parameters:\n   \
TAG   \tString to tag the new event.\n   \
MODE  \t2-bit value defining the running mode (user-1, kernel-2 or both-3).\n   \
EVSEL \t8-bit event selector value.\n   \
UMASK \t8-bit umask value.\n\n \
List of 'event del' configurations:\n   \
ID    \tEvent id. Run 'smon event -l' to check the available events.\n\n"

#define EVSET_HELP	"\n usage: smon evset --add|-a tag=TAG,events=EVID[:EVID[...]][,fixed=FIXED]\n        \
smon evset --del|-d id=ID\n        \
smon evset --list|-l \n\n \
List of 'evset --add' parameters:\n   \
TAG   \tString to tag the new event-set.\n   \
EVID    \tEvent ID. Check 'smon event -l' for a list of available events.\n   \
FIXED   \t12-bit number (4 bits for each fixed ctr [only 2 used]): 0-Disabled 1-OS 2-User 3-Both\n\n \
List of 'evset --del' configurations:\n   \
ID    \tEvent-set ID. Run 'smon evset -l' to check the available event-sets.\n\n"

#define PROFILE_HELP	"\n usage: smon profile -e ESID[:ESID[...]] [-c CPUMASK] [-t STIME] [[options]] PROG [ARGS...]\n\n \
List of 'profile' options:\n   \
-b BURST      \tBurst size, i.e., number of samples transfered at a time (default is 1000)\n   \
-c CPUMASK    \tBind task to specific logical CPUs (e.g., to bind to CPUs 0,1 & 6 -> CPUMASK=0x43)\n   \
-e ESID:[...] \tEventset(s) to monitor (if more than one specified, they will be time-multiplexed round-robin style)\n   \
-f            \tdeliver information about Forking for the monitored task(s)\n   \
-i            \tchildren (recursive) of monitored process will Inherit monitoring\n   \
-m            \tdeliver CPU Migration information\n   \
-o FILE       \tOutput file (default is 'smon.data')\n   \
-p MMAP_PAGES \tnumber {power of 2} of mmap Pages (default is 1024)\n   \
-r            \tdeliver RAPL information at the time granularity of STIME\n   \
-s            \tdeliver CPU Scheduling information (this might have big overhead)\n   \
-t TIME_MS    \tsample Time in miliseconds (default is 10)\n\n"

#define STAT_HELP	"\n usage: smon stat -e ESID [[options]] PROG [ARGS...]\n\n \
List of 'profile' options:\n   \
-c CPUMASK    \tBind task to specific logical CPUs (e.g., to bind to CPUs 0,1 & 6 -> CPUMASK=0x43)\n   \
-e ESID       \tEventset to monitor (only one allowed in this mode)\n   \
-i            \tchildren (recursive) of monitored process will Inherit monitoring\n   \
-o FILE       \tOutput file (default is 'smon.data')\n\n"

#define ROOF_HELP	"\n usage: smon roof [-c CPUMASK] [-t STIME] [[options]] PROG [ARGS...]\n\n \
List of 'roof' options:\n   \
-b BURST      \tBurst size, i.e., number of samples transfered at a time (default is 1000)\n   \
-c CPUMASK    \tBind task to specific logical CPUs (e.g., to bind to CPUs 0,1 & 6 -> CPUMASK=0x43)\n   \
-f            \tdeliver information about Forking for the monitored task(s)\n   \
-i            \tchildren (recursive) of monitored process will Inherit monitoring\n   \
-m            \tdeliver CPU Migration information\n   \
-o FILE       \tOutput file (default is 'smon.data')\n   \
-p MMAP_PAGES \tnumber {power of 2} of mmap Pages (default is 1024)\n   \
-r            \tdeliver RAPL information at the time granularity of STIME\n   \
-s            \tdeliver CPU Scheduling information (this might have big overhead)\n   \
-t TIME_MS    \tsample Time in miliseconds (default is 10)\n\n"


#define DEFAULT_OFILE	"smon.data"

/*
 * Structures from ioctl communication
 */
struct smon_event	event;
struct smon_evset	evset;
struct smon_envir	envir;
// envir


/*
 * GetOpt Stuff
 */
const struct option all_options [] = {
	{ "add", required_argument, NULL, 'a' },
	{ "del", required_argument, NULL, 'd' },
	{ "list",no_argument, NULL, 'l' },
	{ 0, 0, 0, 0}
};

const struct option prof_options [] = {
	{"bind", required_argument, NULL, 'b'},
	{ "evsets", required_argument, NULL, 'e' },
	{ "inherit", no_argument, NULL, 'i' },
	{ "stime", required_argument, NULL, 't' },
	{ 0, 0, 0, 0}
};

/* Event */
char * event_add_opts [] = {
#define EVENT_ADD_TAG	0
	"tag",
#define EVENT_ADD_MODE	1
	"mode",
#define EVENT_ADD_EVSEL	2
	"evsel",
#define EVENT_ADD_UMASK	3
	"umask",
	NULL
};

char * event_del_opts [] = {
#define EVENT_DEL_ID	0
	"id",
	NULL
};

/* Event-Set */
char * evset_add_opts [] = {
#define EVSET_ADD_TAG		0
	"tag",
#define EVSET_ADD_EVENTS	1
	"events",
#define EVSET_ADD_FIXED		2
	"fixed",
	NULL
};

char * evset_del_otps [] = {
#define EVSET_DEL_ID	0
	"id",
	NULL
};


/*
 * Print Functions
 */
void print_help (void)
{
	printf ("%s\n", SMON_HELP);
}

void print_help_event (void)
{
	printf ("%s\n", EVENT_HELP);
}

void print_help_evset (void)
{
	printf("%s\n", EVSET_HELP);
}

void print_help_profile (void)
{
	printf("%s\n", PROFILE_HELP);
}

void print_help_stat (void)
{
	printf("%s\n", STAT_HELP);
}

void print_help_roof (void)
{
	printf("%s\n", ROOF_HELP);
}

/*
 * Parse Functions
 */
int parse_event (int argc, char **argv, struct smon_cmd *cmd)
{
	int			c;
	char			*optr, *val;
	extern char		*optarg;
	extern int		optind;
	int			done = 0;
	unsigned long int	tmp;
	int			err = 0;

	int			ftag=0, fevsel=0, fumask=0;

	/* clean event configuration */
	bzero ((void*)&event, sizeof(struct smon_event));

	while ((c=getopt(argc, argv, "a:l")) != -1 && !done)
	{
		switch (c) {
			case 'a':
				++done;
				cmd->subcmd = 'a';
				optr = optarg;
				while (*optr != '\0') {
					switch (getsubopt(&optr, event_add_opts, &val)) {
						case EVENT_ADD_TAG:
							strncpy (event.tag, val, 63);
							strcat (event.tag, "");
							ftag++;
							break;
						case EVENT_ADD_MODE:
							tmp = strtoul(val, 0, 0);
							tmp &= 0x03;
							event.perfevtsel |= (unsigned int)(tmp << 16);
							event.perfevtsel |= (unsigned int)(1 << 22);
							break;
						case EVENT_ADD_EVSEL:
							tmp = strtoul(val, 0, 0);
							tmp &= 0xff;
							event.perfevtsel |= (unsigned int)(tmp << 0);
							fevsel++;
							break;
						case EVENT_ADD_UMASK:
							tmp = strtoul(val, 0, 0);
							tmp &= 0xff;
							event.perfevtsel |= (unsigned int)(tmp << 8);
							fumask++;
							break;
						default:
							fprintf (stderr, "smon event -a: invalid parameter!\n");
							return ++err;
							break;
					}
				}
				if (!(ftag & fevsel & fumask))
				{
					fprintf (stderr, "smon event -a: tag, evsel and umask required!\n");
					return ++err;
				}
				if (!event.usr && !event.os)
					event.perfevtsel |= (3<<16);	/* user & os (default) */
				break;
			case 'l':
				++done;
				cmd->subcmd = 'l';
			default:
				break;
		}
	}

	if (!done) {
		err++;
		print_help_event();
	}

	return err;
}

int parse_evset (int argc, char **argv, struct smon_cmd *cmd)
{
	int			c, ntok, evids[MAX_GP_CTRS];
	char			*optr, *val, *token;
	extern char		*optarg;
	extern int		optind;
	int			done = 0;
	unsigned int		tmp;
	int			err = 0;

	int			ftag=0, fevents=0;

	/* clean event configuration */
	bzero ((void*)&evset, sizeof(struct smon_evset));

	while ((c=getopt(argc, argv, "a:l")) != -1 && !done)
	{
		switch (c) {
			case 'a':
				++done;
				cmd->subcmd = 'a';
				optr = optarg;
				while (*optr != '\0') {
					switch (getsubopt(&optr, evset_add_opts, &val)) {
						case EVSET_ADD_TAG:
							strncpy (evset.tag, val, 63);
							strcat (evset.tag, "");
							ftag++;
							break;
						case EVSET_ADD_EVENTS:
							tmp = 0;
							ntok = 0;
							token = strtok (val, ":");
							if (!token) {
								fprintf (stderr, " smon evset -a: No events specified!\n");
								return ++err;
							}
							do {
								if (ntok == MAX_GP_CTRS) {
									fprintf (stderr, " smon evset -a: Too many events specified! (max is %d)\n", MAX_GP_CTRS);
									return ++err;
								}
								evids[ntok] = strtoul(token, 0, 0);
								if (evids[ntok++] >= MAX_EVENTS) {
									fprintf (stderr, " smon evset -a: EVID out of range! (evid[%d]=%d)\n", ntok-1, evids[ntok-1]);
									return ++err;
								}

							}while ((token=strtok(NULL, ":")));
							tmp = ntok;
							for (; ntok; ntok--)
								evset.evids[ntok-1] = (int)evids[ntok-1];
							for (; tmp < MAX_GP_CTRS; tmp++)
								evset.evids[tmp] = (int)-1;
							for (tmp = 0; tmp < MAX_GP_CTRS; tmp++) {
								if (evset.evids[tmp] < 0)
									continue;
								evset.global_ctrl |= ((long long)1 << tmp);
							}
							fevents++;
							break;
						case EVSET_ADD_FIXED:
							tmp = strtoul(val, 0, 0);
							tmp &= 0x0333;
							evset.fixed_ctrl |= ((long long)tmp << 0);
							if ( tmp & 0x003 )
								evset.global_ctrl |= ((long long)1 << 32);
							if ( tmp & 0x030 )
								evset.global_ctrl |= ((long long)1 << 33);
							if ( tmp & 0x300 )
								evset.global_ctrl |= ((long long)1 << 34);
							break;
						default:
							fprintf (stderr, " smon evset -a: invalid parameter! (%s)\n", val);
							return ++err;
							break;
					}
				}
				if (!(ftag & fevents))
				{
					fprintf (stderr, " smon evset -a: parameters 'tag' and 'events' required!\n");
					return ++err;
				}
				break;
			case 'l':
				++done;
				cmd->subcmd = 'l';
			default:
				break;
		}
	}

	if (!done) {
		err++;
		print_help_evset();
	}

	return err;
}

int parse_profile (int argc, char **argv, struct smon_cmd *cmd)
{
	int			c, ntok, esids[MUX_EVSETS];
	char			*token;
	extern char		*optarg;
	extern int		optind;
	unsigned int		tmp;
	int			err = 0;

	int			eflag = 0, oflag = 0, tflag = 0;

	/* clean envir configuration */
	bzero ((void*)&envir, sizeof(struct smon_envir));

	envir.options |= ENVOP_EXE;

	// Defaults
	envir.batch_size = DEFAULT_BURST;
	envir.sample_time = DEFAULT_SAMPLE_TIME;
	sprintf (cmd->out_file, "%s", DEFAULT_OFILE);
	cmd->mmap_pages = DEFAULT_MMAP_PAGES;
	cmd->cpumask = 0;


	while ((c=getopt(argc, argv, "b:c:e:fimo:p:rst:")) != -1)
	{
		switch (c) {
			case 'b':
				tmp = strtoul(optarg, 0, 0);
				if (tmp > 0)
					envir.batch_size = tmp;
				else
					envir.batch_size = DEFAULT_BURST;
				break;
			case 'c':
				tmp = strtoul(optarg, 0, 0);
				cmd->cpumask = tmp;
				break;
			case 'e':
				tmp = 0;
				ntok = 0;
				token = strtok (optarg, ":");
				if (!token) {
					fprintf (stderr, " smon-profile: No event-sets specified!\n");
					return ++err;
				}
				do {
					if (ntok == MUX_EVSETS) {
						fprintf (stderr, " smon-profile: Too many event-sets specified! (max is %d)\n", MUX_EVSETS);
						return ++err;
					}
					esids[ntok] = strtoul(token, 0, 0);
					printf("-> evset[%d] = %d\n", ntok, esids[ntok]);

					if (esids[ntok] >= MAX_EVSETS) {
						fprintf (stderr, " smon-profile: ESID out of range! (%d)\n", esids[ntok]);
						return ++err;
					}
					if (esids[ntok] >= 0) ntok++;

				}while ((token=strtok(NULL, ":")));

				envir.n_esids = ntok;
				printf("n_evsets = %d\n", envir.n_esids);
				tmp = ntok;
				for (; ntok; ntok--)
					envir.esids[ntok-1] = (int)esids[ntok-1];
				for (; tmp < MUX_EVSETS; tmp++)
					envir.esids[tmp] = (int)-1;

				eflag++;
				break;
			case 'f':
				envir.options |= ENVOP_FORK;
				break;
			case 'i':
				envir.options |= ENVOP_INH;
				break;
			case 'm':
				envir.options |= ENVOP_CPU;
				break;
			case 'o':
				sprintf(cmd->out_file, "%s", optarg);
				oflag++;
				break;
			case 'p':
				tmp = strtoul(optarg, 0, 0);
				switch (tmp) {
					case 2:
					case 4:
					case 8:
					case 16:
					case 32:
					case 64:
					case 128:
					case 256:
					case 512:
					case 1024:
					case 2048:
					case 4096:
					case 8192:
						cmd->mmap_pages = tmp;
						break;
					default:
						cmd->mmap_pages = DEFAULT_MMAP_PAGES;
						break;
				}
				break;
			case 'r':
				envir.options |= ENVOP_RAP;
				break;
			case 's':
				envir.options |= ENVOP_SCHED;
				break;
			case 't':
				tmp = strtoul(optarg, 0, 0);
				if (tmp < 0)
					fprintf (stderr, "smon-profile: Invalid STIME. (0 defined)\n");
				else
					envir.sample_time = tmp;
				tflag++;
				break;
			default:
				break;
		}
	}


	if (optind >= argc) {
		if (eflag)
			fprintf (stderr, "smon-profile: argument PROG required for profiling\n");
		print_help_profile();
		return ++err;
	}
	cmd->argindex = optind+1;

	if (!eflag) {
		fprintf (stderr, "smon-profile: -e argument required\n");
		print_help_profile();
		err++;
	}

	return err;
}

int parse_stat (int argc, char **argv, struct smon_cmd *cmd)
{
	int			c, ntok, esids[MUX_EVSETS];
	char			*token;
	extern char		*optarg;
	extern int		optind;
	unsigned int		tmp;
	int			err = 0;

	int			eflag = 0, oflag = 0;

	/* clean envir configuration */
	bzero ((void*)&envir, sizeof(struct smon_envir));

	envir.options |= ENVOP_EXE;
	envir.options |= ENVOP_STAT;

	// Defaults
	envir.batch_size = 1;
	envir.sample_time = DEFAULT_SAMPLE_TIME;
	sprintf (cmd->out_file, "%s", DEFAULT_OFILE);
	cmd->mmap_pages = 8;


	while ((c=getopt(argc, argv, "c:e:io:")) != -1)
	{
		switch (c) {
			case 'c':
				tmp = strtoul(optarg, 0, 0);
				cmd->cpumask = tmp;
				break;
			case 'e':
				tmp = 0;
				ntok = 0;
				token = strtok (optarg, ":");
				if (!token) {
					fprintf (stderr, " smon-stat: No event-sets specified!\n");
					return ++err;
				}
				do {
					if (ntok == MUX_EVSETS) {
						fprintf (stderr, " smon-stat: Too many event-sets specified! (max is %d)\n", MUX_EVSETS);
						return ++err;
					}
					esids[ntok] = strtoul(token, 0, 0);

					if (esids[ntok] >= MAX_EVSETS) {
						fprintf (stderr, " smon-stat: ESID out of range! (%d)\n", esids[ntok]);
						return ++err;
					}
					if (esids[ntok] >= 0) ntok++;

				}while ((token=strtok(NULL, ":")));

				envir.n_esids = ntok;
				tmp = ntok;
				for (; ntok; ntok--)
					envir.esids[ntok-1] = (int)esids[ntok-1];
				for (; tmp < MUX_EVSETS; tmp++)
					envir.esids[tmp] = (int)-1;

				envir.n_esids = 1;
				for (ntok = 1; ntok < MUX_EVSETS; ntok++)
					envir.esids[ntok] = -1;

				eflag++;
				break;
			case 'i':
				envir.options |= ENVOP_INH;
				break;
			case 'o':
				sprintf(cmd->out_file, "%s", optarg);
				oflag++;
				break;
			default:
				break;
		}
	}


	if (optind >= argc) {
		if (eflag)
			fprintf (stderr, "smon-stat: argument PROG required for profiling\n");
		print_help_stat();
		return ++err;
	}
	cmd->argindex = optind+1;

	if (!eflag) {
		fprintf (stderr, "smon-stat: -e argument required\n");
		print_help_stat();
		err++;
	}

	return err;
}

int parse_roof (int argc, char **argv, struct smon_cmd *cmd)
{
	int			c;
	extern char		*optarg;
	extern int		optind;
	unsigned int		tmp;
	int			err = 0;

	int			oflag = 0, tflag = 0;

	/* clean envir configuration */
	bzero ((void*)&envir, sizeof(struct smon_envir));

	envir.options |= ENVOP_EXE;

	// Defaults
	envir.batch_size = DEFAULT_BURST;
	envir.sample_time = DEFAULT_SAMPLE_TIME;
	sprintf (cmd->out_file, "%s", DEFAULT_OFILE);
	cmd->mmap_pages = DEFAULT_MMAP_PAGES;
	cmd->count_mode = 0;

	// Event-sets for Roofline
	envir.n_esids = 2;
	envir.esids[0] = 0;
	envir.esids[1] = 1;
	envir.esids[2] = -1;


	while ((c=getopt(argc, argv, "b:c:fimo:p:rst:")) != -1)
	{
		switch (c) {
			case 'b':
				tmp = strtoul(optarg, 0, 0);
				if (tmp > 0)
					envir.batch_size = tmp;
				else
					envir.batch_size = DEFAULT_BURST;
				break;
			case 'c':
				tmp = strtoul(optarg, 0, 0);
				cmd->cpumask = tmp;
				break;
			case 'f':
				envir.options |= ENVOP_FORK;
				break;
			case 'i':
				envir.options |= ENVOP_INH;
				break;
			case 'm':
				envir.options |= ENVOP_CPU;
				break;
			case 'o':
				sprintf(cmd->out_file, "%s", optarg);
				oflag++;
				break;
			case 'p':
				tmp = strtoul(optarg, 0, 0);
				switch (tmp) {
					case 2:
					case 4:
					case 8:
					case 16:
					case 32:
					case 64:
					case 128:
					case 256:
					case 512:
					case 1024:
					case 2048:
					case 4096:
					case 8192:
						cmd->mmap_pages = tmp;
						break;
					default:
						cmd->mmap_pages = DEFAULT_MMAP_PAGES;
						break;
				}
				break;
			case 'r':
				envir.options |= ENVOP_RAP;
				break;
			case 's':
				envir.options |= ENVOP_SCHED;
				break;
			case 't':
				tmp = strtoul(optarg, 0, 0);
				if (tmp <= 0)
					cmd->count_mode = 1;
				else
					envir.sample_time = tmp;
				tflag++;
				break;
			default:
				break;
		}
	}


	if (optind >= argc) {
		print_help_roof();
		return ++err;
	}
	cmd->argindex = optind+1;
	
	return err;
}

int smon_parse_input (int argc, char **argv, struct smon_cmd *cmd)
{
	int err = 0;;

	if (argc < 2)
	{
		print_help();
		return ++err;
	}

	if (!strcmp(argv[1], "event")) {
		cmd->type = CMD_EVENT;
		cmd->ptr = (void*)&event;
		err = parse_event(argc, argv, cmd);

	} else if (!strcmp(argv[1], "evset")) {
		cmd->type = CMD_EVSET;
		cmd->ptr = (void*)&evset;
		err = parse_evset(argc, argv, cmd);

	} else if (!strcmp(argv[1], "profile")) {
		cmd->type = CMD_PROFILE;
		cmd->ptr = (void*)&envir;
		err = parse_profile(--argc, ++argv, cmd);

	} else if (!strcmp(argv[1], "stat")) {
		cmd->type = CMD_STAT;
		cmd->ptr = (void*)&envir;
		err = parse_stat(--argc, ++argv, cmd);

	} else if (!strcmp(argv[1], "roof")) {
		cmd->type = CMD_ROOF;
		cmd->ptr = (void*)&envir;
		err = parse_roof(--argc, ++argv, cmd);
	} else if (!strcmp(argv[1], "help") || !strcmp(argv[1], "--help")) {
		print_help();
		err++;
	} else {
		fprintf (stderr, "smon: '%s' is not a smon-command. See 'smon --help' for more information.\n", argv[1]);
		err++;
	}

	return err;
}













