
PROGRAM=timekeeping_sntp_run

# Test the use of extras/timekeeping with LWIP SNTP

EXTRA_COMPONENTS = extras/timekeeping

# Can work for broadcast or poll
# (assuming you have NTP broadcast already configured on the network)

# Broadcast / poll and hosts for poll set in timekeeping_sntp_run.c
# SNTP parameters set at the top of lwipopts.h


# To set then only log time difference (rather than correct the clock)
# define TIMEKEEPING_SET_AND_MEASURE_ONLY

# PROGRAM_CFLAGS = $(CFLAGS) -DTIMEKEEPING_SET_AND_MEASURE_ONLY

include ../../../../common.mk
