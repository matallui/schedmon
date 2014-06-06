#define SMON_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "cmd.h"
#include "driver.h"
#include "profile.h"

int main (int argc, char **argv)
{
	struct smon_cmd		cmd;
	struct smon_event	*pevent;
	struct smon_evset	*pevset;
	struct smon_envir	*penvir;

	int err = 0, ret = 0;

	if (smon_load_driver()) {
		fprintf (stderr, "smon: error: Loading the driver. Check if module is running.\n");
		return -1;
	}

	err = smon_parse_input (argc, argv, &cmd);

	if (!err) {

		switch (cmd.type) {
				
			case CMD_EVENT:
				pevent = (struct smon_event *)cmd.ptr;
				switch (cmd.subcmd) {
					case 'a':
						ret = smon_set_event (pevent);
						if (ret)
							fprintf (stderr, "smon event -a: ioctl error: check if you reached the max events\n");
						break;
					case 'd':
						printf ("\nsmon event: -d option not available...\n\n");
						break;
					case 'l':
						smon_list_events();
						break;
					default:
						break;
				}
				break;
			case CMD_EVSET:
				pevset = (struct smon_evset *)cmd.ptr;

				switch (cmd.subcmd) {
					case 'a':
						if ( (ret = smon_check_events(pevset) ) != 0) {
							fprintf (stderr, "smon evset -a: error: you're trying to add events which aren't available\n");
							break;
						}
						ret = smon_set_evset (pevset);
						if (ret)
							fprintf (stderr, "smon evset -a: ioctl error: check if you're adding unavailable events\n");
						break;
					case 'd':
						printf ("\nsmon evset: -d option not available...\n\n");
						break;
					case 'l':
						smon_list_evsets();
						break;
					default:
						break;
				}
				break;
			case CMD_PROFILE:
			case CMD_STAT:
				penvir = (struct smon_envir *)cmd.ptr;

				if ( (ret = smon_check_evsets(penvir) ) != 0) {
					fprintf (stderr, "smon profile/stat: error: you're trying to add event-sets which aren't available\n");
					break;
				}
				ret = start_profile (argv[cmd.argindex], &argv[cmd.argindex], &cmd);
				break;
			case CMD_ROOF:
				penvir = (struct smon_envir *)cmd.ptr;
				ret = start_profile (argv[cmd.argindex], &argv[cmd.argindex], &cmd);
				break;
			default:
				break;
		}
		
	}

	smon_unload_driver();

	return ret;
}
