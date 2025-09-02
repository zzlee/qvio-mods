# Verbose flag
ifeq ($(V), 1)
AT =
else
AT = @
endif

AS             ?= as
LD             ?= ld
CC             ?= gcc
CXX            ?= g++
AR             ?= ar
NM             ?= nm
STRIP          ?= strip
OBJCOPY        ?= objcopy
OBJDUMP        ?= objdump
NVCC           ?= /usr/local/cuda/bin/nvcc

COMMON_DIR := ../common

CFLAGS += \
-O3 \
-I${COMMON_DIR}

LDFLAGS += \
-pthread

# CUDA code generation flags
GENCODE_SM53=-gencode arch=compute_53,code=sm_53
GENCODE_SM62=-gencode arch=compute_62,code=sm_62
GENCODE_SM72=-gencode arch=compute_72,code=sm_72
GENCODE_SM87=-gencode arch=compute_87,code=sm_87
GENCODE_SM_PTX=-gencode arch=compute_72,code=compute_72
GENCODE_FLAGS=$(GENCODE_SM53) $(GENCODE_SM62) $(GENCODE_SM72) $(GENCODE_SM87) $(GENCODE_SM_PTX)

CUFLAGS += \
${GENCODE_FLAGS}

ifeq (${BUILD_WITH_CUDA},ON)

CFLAGS += \
-DBUILD_WITH_CUDA=1 \
-I/usr/local/cuda/include

LDFLAGS += \
-L/usr/local/cuda/lib64/ \
-Wl,-rpath-link=/usr/local/cuda/lib64/ \
-lcudart \
-lcuda

endif

ifeq (${BUILD_WITH_NVBUF},ON)

CFLAGS += \
-DBUILD_WITH_NVBUF=1 \
-I/usr/src/jetson_multimedia_api/include

LDFLAGS += \
-L/usr/lib/aarch64-linux-gnu/tegra/ \
-Wl,-rpath-link=/usr/lib/aarch64-linux-gnu/tegra/ \
-lnvbufsurface

endif

ifeq (${BUILD_WITH_NPP},ON)

CFLAGS += \
-DBUILD_WITH_NPP=1

LDFLAGS += \
-lnppc \
-lnppial \
-lnppicc \
-lnppidei \
-lnppif \
-lnppig \
-lnppim \
-lnppist \
-lnppisu \
-lnppitc \
-lnpps

endif

CXXFLAGS += \
-std=c++11 \
${CFLAGS}
