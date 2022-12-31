P := main
OBJECTS := $(P).o m_malloc.o
CC := gcc
CFLAGS := -I$(HOME)/local/include -Wall -Wextra
LDFLAGS := -L$(HOME)/local/lib
LDLIBS :=

ifeq ($(BUILD_PROFILE), release)
	CFLAGS := $(CFLAGS) -Werror -O3
else
	CFLAGS := $(CFLAGS) -g -Og -DCHECK_HEAP=1 -DPRINT_DEBUG_INFO=1
endif

$(P): $(OBJECTS)

clean:
	rm -rf $(P) *.o
