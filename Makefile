P := main
OBJECTS := $(P).o m_malloc.o
CC := gcc
CFLAGS := -I$(HOME)/local/include -Wall -Wextra -Werror
LDFLAGS := -L$(HOME)/local/lib
LDLIBS :=


ifeq ($(ENABLE_PROFILING), 1)
	CFLAGS := $(CFLAGS) -pg
	LDFLAGS := $(LDFLAGS) -pg
endif


ifeq ($(BUILD_PROFILE), release)
	CFLAGS := $(CFLAGS)  -O3
else
	CFLAGS := $(CFLAGS) -g -Og -DCHECK_HEAP=1 -DPRINT_DEBUG_INFO=1
	LDFLAGS := $(LDFLAGS)
endif


$(P): $(OBJECTS)

clean:
	rm -rf $(P) *.o
