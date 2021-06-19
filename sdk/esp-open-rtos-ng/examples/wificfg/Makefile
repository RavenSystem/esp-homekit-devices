# Makefile for wificfg example
PROGRAM=wificfg
EXTRA_COMPONENTS=extras/dhcpserver extras/wificfg

# For the mDNS responder included under extras:
# EXTRA_COMPONENTS += extras/mdnsresponder
# EXTRA_CFLAGS += -DEXTRAS_MDNS_RESPONDER

# For the mDNS responder included with lwip:
EXTRA_CFLAGS += -DLWIP_MDNS_RESPONDER=1 -DLWIP_NUM_NETIF_CLIENT_DATA=1 -DLWIP_NETIF_EXT_STATUS_CALLBACK=1

# Avoid writing the wifi state to flash when using wificfg.
EXTRA_CFLAGS += -DWIFI_PARAM_SAVE=0

include ../../common.mk
