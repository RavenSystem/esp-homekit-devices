# Makefile for tsfotuart example
PROGRAM=tsoftuart
EXTRA_COMPONENTS=extras/dhcpserver extras/wificfg extras/mactimer extras/tsoftuart

# For the mDNS responder included with lwip:
EXTRA_CFLAGS += -DLWIP_MDNS_RESPONDER=1 -DLWIP_NUM_NETIF_CLIENT_DATA=1 -DLWIP_NETIF_EXT_STATUS_CALLBACK=1

# Avoid writing the wifi state to flash when using wificfg.
EXTRA_CFLAGS += -DWIFI_PARAM_SAVE=0

EXTRA_CFLAGS += -DWIFICFG_CLIENT_TASK=1 -DWIFICFG_IRAM_TEST=1

include ../../common.mk
