# explicitly use gcc as in xtensa build environment it might be set to
# cross compiler
CC = gcc

SOURCES := spiffs_hydrogen.c
SOURCES += spiffs_cache.c
SOURCES += spiffs_gc.c
SOURCES += spiffs_check.c
SOURCES += spiffs_nucleus.c
SOURCES += mkspiffs.c

OBJECTS := $(SOURCES:.c=.o)

VPATH = ../spiffs/src

CFLAGS += -I..
CFLAGS += -DSPIFFS_SINGLETON=0

all: mkspiffs

$(OBJECTS): $(SOURCES)

$(OBJECTS): ../spiffs_config.h

mkspiffs: $(OBJECTS)

clean:
	@rm -f mkspiffs
	@rm -f *.o

.PHONY: all clean
