#ifndef _SMON_CMD_H_
#define _SMON_CMD_H_

#define CMD_INVAL	0
#define CMD_EVENT	1
#define	CMD_EVSET	2
#define CMD_ENVIR	3
#define CMD_STAT	4
#define CMD_PROFILE	5
#define CMD_ROOF	6

#define ARGSIZE		256

struct smon_cmd {
	int	type;
	void*	ptr;
	union {
		char		subcmd;
		unsigned int	argindex;
	};

	unsigned int count_mode;
	unsigned int cpumask;
	unsigned int mmap_pages;
	char out_file[512];

};

int smon_parse_input (int argc, char **argv, struct smon_cmd *cmd);

#endif
