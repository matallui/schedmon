#!/bin/bash

#
# Check INFILE
#
if [ $# -lt 2 ]; then
	echo "[plot_roof.sh] ERROR: Too few arguments! Two required."
	exit 1
fi
INFILE=$1
if [ ! -e $INFILE ]; then
	echo "[plot_roof.sh] ERROR: File $INFILE does not exist."
	exit 1
fi
OUTFILE=$2
if [ ! -e $INFILE ]; then
	echo "[plot_roof.sh] ERROR: File $OUTFILE does not exist."
	exit 1
fi

TMP=`mktemp`
#TMP="blabla.txt"

#
# Global Variables
#
time_offset=`cat $INFILE | grep "INFO" | awk '{print $2}'`
tsc_frequency=`cat $INFILE | grep "INFO" | awk '{print $3}'`
sample_time=`cat $INFILE | grep "INFO" | awk '{print $6}'`
energy_units=`cat $INFILE | grep "INFO" | awk '{print $8}'`

#
# Check number of Samples
#
N_SAMPLES=`cat $INFILE | grep "PMC" | wc -l`
if [ $N_SAMPLES -lt 2 ]; then
	echo ""
	echo "--> Too few samples. Try reducing the sample time by using the -t option."
	echo ""
	exit 1
fi

#
# Scale counts (2 event-sets)
#
# output: TSC DUR LD ST SCL_S SCL_D SSE_S SSE_D AVX_S AVX_D
cat $INFILE | grep "PMC" | awk -v time_offset=$time_offset -v tsc_freq=$tsc_frequency '{
	evset = $6;

	if (evset == 0) 
	{
		tsc_start = $2 - time_offset;
		tsc_dur = $4;
		ld = $10;
		st = $11;
		scl_s = $12;
		scl_d = $13;
	} 
	else if (evset == 1) 
	{
		tsc_dur_1 = $4;
		tsc_dur_total = tsc_dur + tsc_dur_1;

		ld = ld * (tsc_dur_total) * 1.0 / tsc_dur;
		st = st * (tsc_dur_total) * 1.0 / tsc_dur;
		scl_s = scl_s * (tsc_dur_total) * 1.0 / tsc_dur;
		scl_d = scl_d * (tsc_dur_total) * 1.0 / tsc_dur;

		sse_s = $10 * (tsc_dur_total) * 1.0 / tsc_dur_1;
		sse_d = $11 * (tsc_dur_total) * 1.0 / tsc_dur_1;
		avx_s = $12 * (tsc_dur_total) * 1.0 / tsc_dur_1;
		avx_d = $13 * (tsc_dur_total) * 1.0 / tsc_dur_1;

		tsc_start_sec = tsc_start * 1.0 / tsc_freq;
		tsc_dur_total_sec = tsc_dur_total / tsc_freq;

		printf ("%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\n", tsc_start_sec, tsc_dur_total_sec, ld, st, scl_s, scl_d, sse_s, sse_d, avx_s, avx_d);
	}
}' > $TMP

#
# Prepare File for Roofline
#
# output: TSC DUR FLOPS BYTES GFLOPS/s OI
#
cat $TMP | awk '{
	flop_sum = $5 + $6 + $7 + $8 + $9 + $10;
	if (flop_sum == 0)
	{
		dbl = 0;
		sse = 0;
		avx = 0;
	}
	else
	{
		dbl = ($5 + $6) / flop_sum;
		sse = ($7 + $8) / flop_sum;
		avx = ($9 +$ 10) / flop_sum;
	}

	type[1]=dbl*($3+$4)*8; type[2]=sse*($3+$4)*16; type[3]=avx*($3+$4)*32;
	type_max=type[1];  type_ind=1; 
	for ( i=2; i<4; i++ ) { if(type[i] > type_max) {type_max=type[i]; type_ind=i;} }

	final_type = type_ind;
	flops = 0.5*$5 + $6 + 2*($7 + $8) + 4*($9 + $10);
	bytes = dbl*($3+$4)*8 + sse*($3+$4)*16 + avx*($3+$4)*32;

	#if (flops != 0 && bytes != 0)

	if (bytes > 0)
		operational_intensity = flops/bytes;
	else
		operational_intensity = 0;
	gflopss = flops / 1000000000 / $2;
	printf("%f\t%f\t%f\t%f\t%f\t%f\n", $1, $2, flops, bytes, gflopss, operational_intensity);

}' > $OUTFILE

rm -f $TMP

cat $OUTFILE | awk 'BEGIN{
	duration = 0;
	flops = 0;
	bytes = 0;
	gflopss = 0;
	oi = 0;
}{
	duration += $2;
	flops += $3;
	bytes += $4;
}END{
	gflopss = flops * 1.0 / 1000000000 / duration;
	if (bytes > 0)
		oi = flops / bytes;

	printf("\n\t-> Duration: \t\t%.6f s\n", duration);
	printf("\n\t-> Total Counts:\n");
	printf("\t\tFlops    = \t%d\n", flops);
	printf("\t\tBytes    = \t%d\n", bytes);
	printf("\t\tGFlops/s = \t%.6f\n", gflopss);
	printf("\t\tOI       = \t%.6f\n\n", oi);
}'

