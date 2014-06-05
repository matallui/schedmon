#!/bin/bash

#
# Check INFILE
#
if [ $# -lt 1 ]; then
	echo "[plot_prof.sh] ERROR: Too few arguments! Two required."
	exit 1
fi
INFILE=$1
if [ ! -e $INFILE ]; then
	echo "[plot_prof.sh] ERROR: File $INFILE does not exist."
	exit 1
fi

TMP=`mktemp`

#
# Global Variables
#
time_offset=`cat $INFILE | grep "INFO" | awk '{print $2}'`
tsc_frequency=`cat $INFILE | grep "INFO" | awk '{print $3}'`
sample_time=`cat $INFILE | grep "INFO" | awk '{print $6}'`
energy_units=`cat $INFILE | grep "INFO" | awk '{print $8}'`


cat $INFILE | grep "PMC" | awk -v tsc_freq=$tsc_frequency 'BEGIN{
	duration = 0;
}{
	duration += $4;
}END{
	duration = duration * 1.0 / tsc_freq;

	printf("\n\t-> Duration: \t\t%.6f s\n", duration);
}'

RAPL=`cat $INFILE | grep "RAPL" | wc -l`

if [ $RAPL -gt 0 ]; then
	cat $INFILE | grep "RAPL" | awk '{
		
	}'
fi

echo ""
