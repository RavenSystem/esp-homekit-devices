GZIP_COMPRESSION ?= no
USE_HEATSHRINK ?= yes

CFLAGS=-I../../lib/heatshrink -I../../include/libesphttpd -I.. -std=gnu99
ifeq ("$(GZIP_COMPRESSION)","yes")
CFLAGS		+= -DESPFS_GZIP
endif

ifeq ("$(USE_HEATSHRINK)","yes")
CFLAGS		+= -DESPFS_HEATSHRINK
endif

TARGET_DIR = $(BUILD_DIR)mkespfsimage/
TARGET=$(TARGET_DIR)mkespfsimage
OBJS=main.o heatshrink_encoder.o

$(TARGET): $(OBJS)
ifeq ("$(GZIP_COMPRESSION)","yes")
	$(CC) -o $@ $^ -lz
else
	$(CC) -o $@ $^
endif

clean:
	rm -f $(TARGET) $(OBJS)
