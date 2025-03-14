BUILD_WITH_CUDA:=ON
BUILD_WITH_NPP:=ON
BUILD_WITH_NVBUF:=ON

include ../Rules.mk

APP := 03_qvio-ctl

SRCS := \
	main.cpp \
	$(wildcard $(COMMON_DIR)/*.cpp) \
	$(wildcard $(COMMON_DIR)/*.cu)

OBJS := $(SRCS:.cpp=.cpp.o)
OBJS := $(OBJS:.cu=.cu.o)

.PHONY: all clean

all: $(APP)

clean:
	$(AT)rm -rf $(APP) $(OBJS)

$(APP): $(OBJS)
	@echo "Linking: $@"
	$(AT)$(CXX) -o $@ $(OBJS) $(CXXFLAGS) $(LDFLAGS)

include ../Targets.mk
