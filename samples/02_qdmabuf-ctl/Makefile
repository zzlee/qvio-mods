include ../Rules.mk

APP := 02_qdmabuf-ctl

SRCS := \
	main.cpp \
	$(wildcard $(COMMON_DIR)/*.cpp)

OBJS := $(SRCS:.cpp=.cpp.o)
OBJS := $(OBJS:.cu=.cu.o)

all: $(APP)

clean:
	$(AT)rm -rf $(APP) $(OBJS)

$(APP): $(OBJS)
	@echo "Linking: $@"
	$(AT)$(CXX) -o $@ $(OBJS) $(CXXFLAGS) $(LDFLAGS)

include ../Targets.mk
