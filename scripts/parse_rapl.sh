#!/bin/bash

#
# Check INFILE
#
if [ $# -lt 1 ]; then
	echo "[parse_rapl.sh] ERROR: Too few arguments! One required."
	exit 1
fi
INFILE=$1
if [ ! -e $INFILE ]; then
	echo "[parse_rapl.sh] ERROR: File $INFILE does not exist."
	exit 1
fi

#
# File Paths
#
TMP_FILE="rapl_tmp.data"
PLOT_FILE="smon_plot_rapl.pdf"

#
# Global Variables
#
time_offset=`cat $INFILE | grep "INFO" | awk '{print $2}'`
tsc_frequency=`cat $INFILE | grep "INFO" | awk '{print $3}'`
sample_time=`cat $INFILE | grep "INFO" | awk '{print $6}'`
energy_units=`cat $INFILE | grep "INFO" | awk '{print $8}'`
power_units=`cat $INFILE | grep "INFO" | awk '{print $9}'`
time_units=`cat $INFILE | grep "INFO" | awk '{print $10}'`

#
# Prepare TMP_FILE for Gnuplot
#
# output TSC PKG PP0 PP1
cat $INFILE | grep "RAPL" | awk -v time_off=$time_offset -v freq=$tsc_frequency -v stime=$sample_time -v eunits=$energy_units '{
		
	timestamp = ($2 - time_off) * 1.0 / freq;
	duration = $3 * 1.0 / freq;

	energy_pkg = $4 * 1.0 / eunits;
	power_pkg = energy_pkg / duration;
	
	energy_pp0 = $5 * 1.0 / eunits;
	power_pp0 = energy_pp0 / duration;

	energy_pp1 = $6 * 1.0 / eunits;
	power_pp1 = energy_pp1 / duration;
	
	printf("%f\t%f\t%f\t%f\n", timestamp, power_pkg, power_pp0, power_pp1); 

}' > "lalala.txt"


sed -i "" -e 's/\,/\./g' $TMP_FILE

#
# Create Graphs with Gnuplot
#
echo -n "Generating RAPL plots... "
/opt/local/bin/gnuplot <<xxxEOFxxx

reset
set   autoscale                        # scale axes automatically
unset log                              # remove any log-scaling
unset label                            # remove any previous labels
set xtic auto                          # set xtics automatically
set ytic auto                          # set ytics automatically

#set title "Power Consumption"
set xlabel "Time [s]" offset 0,0.5
set ylabel "Power [W]" offset 1
set xrange [0:215]
set yrange [20:30]
set cbrange[1:3]
set cbtics('DBL' 1, 'SSE' 2, 'AVX' 3)
set term pdfcairo enhanced font "Helvetica,16"
set output "$PLOT_FILE"

set palette maxcolors 3
set palette defined (1 "#73A8FF", 2 "#258F01", 3 "#670002")
#unset colorbox
set colorbox horiz user origin 0.69,0.25 size 0.2,0.04 front
set key left
set samples 10000

plot "$TMP_FILE" u (column(1)):(column(2)):(column(5)) with points pt 5 ps 0.2 palette t""

xxxEOFxxx

#rm -f $TMP_FILE
