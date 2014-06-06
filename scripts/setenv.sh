#!/bin/bash

SMON="../tool/bin/smon"

####################################################
#			EVENTS			   #
####################################################

#-> Architectural Events
$SMON event -a tag=UNHALTED_CORE_CYCLES,evsel=0x3C,umask=0x00,mode=3
$SMON event -a tag=UNHALTED_REF_CYCLES,evsel=0x3C,umask=0x01,mode=3
$SMON event -a tag=INSTRUCTIONS_RET,evsel=0xC0,umask=0x00,mode=3
$SMON event -a tag=LLC_REFERENCE,evsel=0x2E,umask=0x4F,mode=3
$SMON event -a tag=LLC_MISSES,evsel=0x2E,umask=0x41,mode=3
$SMON event -a tag=BRANCH_INST_RETIRED,evsel=0xC4,umask=0x00,mode=3
$SMON event -a tag=BRANCH_MISSES_RETIRED,evsel=0xC5,umask=0x00,mode=3

#-> Memory Events
$SMON event -a tag=L2_RQSTS_CODE_RD_MISS,evsel=0x24,umask=0x20,mode=3
$SMON event -a tag=L2_RQSTS_ALL_CODE,evsel=0x24,umask=0x30,mode=3
$SMON event -a tag=L2_L1D_WB_RQSTS_ALL,evsel=0x28,umask=0x0F,mode=3
$SMON event -a tag=L1D_REPLACEMENT,evsel=0x51,umask=0x01,mode=3
$SMON event -a tag=MEM_UOP_RETIRED_LOADS,evsel=0xD0,umask=0x01,mode=3
$SMON event -a tag=MEM_UOP_RETIRED_STORES,evsel=0xD0,umask=0x02,mode=3
$SMON event -a tag=MEM_UOP_RETIRED_ALL,evsel=0xD0,umask=0x80,mode=3
$SMON event -a tag=MEM_UOP_RETIRED_ALL_LOADS,evsel=0xD0,umask=0x81,mode=3
$SMON event -a tag=MEM_UOP_RETIRED_ALL_STORES,evsel=0xD0,umask=0x82,mode=3
$SMON event -a tag=MEM_LOAD_UOP_RETIRED_L1_HIT,evsel=0xD1,umask=0x01,mode=3
$SMON event -a tag=MEM_LOAD_UOP_RETIRED_LLC_MISS,evsel=0xD1,umask=0x20,mode=3
$SMON event -a tag=MEM_LOAD_UOP_LLC_MISS_RET_LOCAL_DRAM,evsel=0xD3,umask=0x01,mode=3

#-> FLOPs
$SMON event -a tag=FP_COMP_OPS_EXE_X87,evsel=0x10,umask=0x01,mode=3
$SMON event -a tag=FP_SSE_PACKED_DOUBLE,evsel=0x10,umask=0x10,mode=3
$SMON event -a tag=FP_SSE_SCALAR_SINGLE,evsel=0x10,umask=0x20,mode=3
$SMON event -a tag=FP_SSE_PACKED_SINGLE,evsel=0x10,umask=0x40,mode=3
$SMON event -a tag=FP_SSE_SCALAR_DOUBLE,evsel=0x10,umask=0x80,mode=3
$SMON event -a tag=FP_256_PACKED_SINGLE,evsel=0x11,umask=0x01,mode=3
$SMON event -a tag=FP_256_PACKED_DOUBLE,evsel=0x11,umask=0x02,mode=3


####################################################
#			EVSETS			   #
####################################################

$SMON evset -a tag=EVSET_DBL_MEM,events=14:15:21:23,fixed=0x333
$SMON evset -a tag=EVSET_SSE_AVX,events=22:20:24:25,fixed=0x333

$SMON evset -a tag=EVSET_OVH_1,events=5:6:14:15,fixed=0x333


# List All
$SMON event -l
$SMON evset -l
