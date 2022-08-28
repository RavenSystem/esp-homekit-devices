# Component makefile for "open Espressif libs"

INC_DIRS += $(open_esplibs_ROOT)include

$(eval $(call component_compile_rules,open_esplibs))

# args for passing into compile rule generation
open_esplibs_libmain_ROOT = $(open_esplibs_ROOT)libmain
open_esplibs_libmain_INC_DIR = 
open_esplibs_libmain_SRC_DIR = $(open_esplibs_libmain_ROOT)
open_esplibs_libmain_EXTRA_SRC_FILES = 
open_esplibs_libmain_CFLAGS = $(CFLAGS)
open_esplibs_libmain_WHOLE_ARCHIVE = yes

$(eval $(call component_compile_rules,open_esplibs_libmain))

open_esplibs_libnet80211_ROOT = $(open_esplibs_ROOT)libnet80211
open_esplibs_libnet80211_INC_DIR = 
open_esplibs_libnet80211_SRC_DIR = $(open_esplibs_libnet80211_ROOT)
open_esplibs_libnet80211_EXTRA_SRC_FILES = 
open_esplibs_libnet80211_CFLAGS = $(CFLAGS)
open_esplibs_libnet80211_WHOLE_ARCHIVE = yes

$(eval $(call component_compile_rules,open_esplibs_libnet80211))

open_esplibs_libphy_ROOT = $(open_esplibs_ROOT)libphy
open_esplibs_libphy_INC_DIR = 
open_esplibs_libphy_SRC_DIR = $(open_esplibs_libphy_ROOT)
open_esplibs_libphy_EXTRA_SRC_FILES = 
open_esplibs_libphy_CFLAGS = $(CFLAGS)
open_esplibs_libphy_WHOLE_ARCHIVE = yes

$(eval $(call component_compile_rules,open_esplibs_libphy))

open_esplibs_libpp_ROOT = $(open_esplibs_ROOT)libpp
open_esplibs_libpp_INC_DIR = 
open_esplibs_libpp_SRC_DIR = $(open_esplibs_libpp_ROOT)
open_esplibs_libpp_EXTRA_SRC_FILES = 
open_esplibs_libpp_CFLAGS = $(CFLAGS)
open_esplibs_libpp_WHOLE_ARCHIVE = yes

$(eval $(call component_compile_rules,open_esplibs_libpp))

open_esplibs_libwpa_ROOT = $(open_esplibs_ROOT)libwpa
open_esplibs_libwpa_INC_DIR = 
open_esplibs_libwpa_SRC_DIR = $(open_esplibs_libwpa_ROOT)
open_esplibs_libwpa_EXTRA_SRC_FILES = 
open_esplibs_libwpa_CFLAGS = $(CFLAGS)
open_esplibs_libwpa_WHOLE_ARCHIVE = yes

$(eval $(call component_compile_rules,open_esplibs_libwpa))
