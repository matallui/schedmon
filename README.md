#########################################################################
#                                                                       #
#               SchedMon - A Scheduler-based Monitoring Tool            #
#                                                                       #
#########################################################################

 This file contains a quick guide for using the SchedMon monitoring tool.
To use the tool, the "smon" command is available in /usr/local/bin.
At the moment, the command-line interface includes 4 sub-commands:

	-> smon event  -  for defining new performance events
	-> smon evset  -  for defining new event-set configurations
	-> smon profile - for running a profile evaluation
	-> smon roof   -  for running a roofline evaluation

 Using each command without the required parameters will lead SchedMon to
print some help information.

IMPORTANT NOTE: This code has been specifically designed for Linux kernel
                3.14, so it might not be compatible with other versions.
                Run at your own risk!

## 1. EVENTS

 An event corresponds to the configuration of one and only one performance
counter (on Intel architectures, it corresponds to a PERFEVTSEL value).
By using the proper configuration values (provided by the architecture's
developers manual), it is possible to profile specific hardware events
provided by the system.

SchedMon comes with 26 pre-defined events, which can be listed by running

	smon event -l

 This will provide a list of name and configuration values for each event.
It is possible to add new events by using the `smon event -a` command.
Example:
	smon event -a tag=INSTRUCTIONS_RET,evsel=0xC0,umask=0x00,mode=3

 This would create a new event named INSTRUCTIONS_RET, which is defined by
evsel=0xC0 and umask=0x00. By using mode=3, the event will be taken into
account in both user and OS modes.

Note: At the moment, the `smon event -d` functionality is not implemented


## 2. EVENT-SETS

 An event-set corresponds to a set of events that can be configured into
the hardware performance interface at the same time. For instance, on
Intel Ivy Bridges it is possible to configure 4 global-purpose performance
counters simultaneously. Moreover, these architectures provide 3 additio-
nal fixed counters that are already configured with a pre-defined archi-
tectural event and can only be enabled.

 SchedMon comes with 2 pre-defined event-sets, that are used for the `roof`
sub-command, and can be listed by running:

	smon evset -l

 Similarly to the `smon event` command, one can create additional event-
sets by using `smon evset -a`.
Example:
	smon evset -a tag=EVSET_DBL_MEM,events=14:15:21:23,fixed=0x333

 This creates an event-set that monitors events 14, 15, 21 and 23 in the
global-purpose counters and enables the 3 available fixed-counters in
mode 3 (user + OS).
It is important to notice that event-sets can only be defined with pre-
viously defined events.

Note: At the moment, the `smon evset -d` functionality is not implemented


## 3. PROFILING

 SchedMon's profiling functionality allows profiling the required event-
set(s) in order to evaluate the performance of a target application.
 In addition to performance, SchedMon also allows obtaining several
scheduling informations (migrations, forks, CPU sched-in/out) and it
also allows obtaining power consumption information.
The help information of this functionality can be obtained by running:

	smon profile

 In the example below, the /usr/bin/ls program is evaluated by using the
event-set 0. By using the -r option, SchedMon also provides RAPL samples
(energy monitoring samples). Both performance and RAPL samples are taken
with sampling time interval of 1ms.

	smon profile -e 0 -r -t 1 /usr/bin/ls


### 3.1. OUTPUT INFORMATION

 The output information is saved in `smon.data` by default. This file con-
tains all the sampling information and the raw values obtained by the
tool.
 There are 6 types of samples:
	-> PMC       - performance information
	-> RAPL      - energy information
	-> FORK      - provided when a fork is detected
	-> MIGRATION - provides task migration (between CPUs) information
	-> SCHED     - provides information of when a target task is
	               scheduled in/out of a CPU
	-> INFO      - provides general information (first line of output)

 Each output sample type is defined in the first column between `[ ]`.
The following text described the information provided by each sample type.

 [PMC] TSC_START TSC_STOP TSC_DUR PID EVSET FX0 FX1 FX2 GP0 GP1 GP2 GP3

 -> TSC_START - TSC value corresponding to the beginning of the sample
 -> TSC_STOP  - TSC value corresponding to the end of the sample
 -> TSC_DUR   - Sample duration in TSC cycles (might be different than
                TSC_STOP - TSC_START.
 -> PID       - PID of the monitored task
 -> EVSET     - Monitored Event-Set
 -> FX0..2    - Fixed Counters
 -> GP0..3    - Global-purpose Counters


 [RAPL] TSC_START TSC_DUR PGK PP0 PP1

 -> PKG  - package counts
 -> PP0  - power-plane 0 counts
 -> PP1  - power-plane 1 counts


 [FORK] TSC PPID CPID

 -> TSC   - Time-stamp value
 -> PPID  - Parent PID
 -> CPID  - Child PID


 [MIGRATION] TSC PID OLD_CPU NEW_CPU

 -> OLD_CPU  - CPU where task was migrated from
 -> NEW_CPU  - CPU where task was migrated to


 [SCHED] TSC_IN TSC_OUT PID CPU

 -> TSC_IN   - TSC of when task was scheduled in into CPU
 -> TSC_OUT  - TSC of when task was scheduled out from CPU


 [INFO] TSC_START TSC_FREQ N_CPUS STIME STIME N_EVSETS EUNITS ...

 -> TSC_FREQ   - TSC clock frequency
 -> N_CPUS     - number of available CPUs
 -> STIME      - sampling time interval
 -> N_EVSETS   - number of event-sets that were used
 -> EUNITS     - Energy Units (used to convert RAPL values into power)


## 4. CACHE-AWARE ROOFLINE MODEL (CARM)

 SchedMon provides a pre-configured command `smon roof` that allows to
obtain the performance information required for a CARM evalutation.
 The configuration parameters of this command are exactly the same of
those for the `smon profile` command, with the sole difference that
`smon roof` does not provide event-set selection (-e option).

Example:
	smon roof -t 1 /usr/bin/ls

This provides the performance information (with 1ms granularity) necessary
to perform a CARM analysis. Like for profiling, the raw output samples are
kept in smon.data (by default).

### 4.1 ADDITIONAL OUTPUT

 In addition to the already described `smon.data` output file, this mode
provides an additional file `smon.data.roof` which contains the parsed
information from `smon.data`.
 Each line of the additional file is formatted in the following way:

 TIME DUR FLOPS BYTES GFLOPSS OI

 -> TIME    - Execution time (in seconds)
 -> DUR     - Duration of that sample/line (in seconds)
 -> FLOPS   - Total number of Flops
 -> BYTES   - Total number of bytes (correspoding to the obtained FLOPS)
 -> GFLOPSS - GFlops/s
 -> OI      - Operational Intensity


## 5. AUTHORS

<matallui@gmail.com>

## 6. LICENSE

 Please refer to the provided 'LICENSE.txt' file.


