include ../Rules.mk

SRCS := $(wildcard *.cpp)

OBJS := $(SRCS:.cpp=.o)
OBJS := $(OBJS:.cu=.o)

all: $(OBJS)

clean:
	$(AT)rm -rf $(OBJS)

include ../Targets.mk
