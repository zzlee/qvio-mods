include ../Rules.mk

APP := 07_aximm-test0

SRCS := \
	main.cpp \
	$(wildcard $(COMMON_DIR)/*.cpp)

OBJS := $(SRCS:.cpp=.cpp.o)

.PHONY: all clean

all: $(APP)

clean:
	$(AT)rm -rf $(APP) $(OBJS)

$(APP): $(OBJS)
	@echo "Linking: $@"
	$(AT)$(CXX) -o $@ $(OBJS) $(CXXFLAGS) $(LDFLAGS)

include ../Targets.mk
