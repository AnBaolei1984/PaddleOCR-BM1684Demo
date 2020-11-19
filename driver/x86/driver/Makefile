SOC_MODE ?= 0
PLATFORM ?= asic
PCIE_MODE_ENABLE_CPU ?= 1

ifeq ($(PLATFORM), asic)
ccflags-y += -DPLATFORM_ASIC
endif
ifeq ($(PLATFORM), palladium)
ccflags-y += -DPLATFORM_PALLADIUM
endif
ifeq ($(PLATFORM), fpga)
ccflags-y += -DPLATFORM_FPGA
endif

ifneq ($(SOC_MODE), 1)
ifeq ($(PCIE_MODE_ENABLE_CPU), 1)
ccflags-y += -DPCIE_MODE_ENABLE_CPU
endif
endif

PWD := $(shell pwd)
BM1682_DIR ?= bm1682
ifeq ($(SOC_MODE), 1)
else
BM1682_OBJ += bm1682_pcie.o bm1682_irq.o
endif
BM1682_OBJ += bm1682_ddr.o bm1682_gmem.o bm1682_cdma.o bm1682_msgfifo.o bm1682_smmu.o bm1682_card.o
BM1682_OBJ := $(addprefix $(BM1682_DIR)/, $(BM1682_OBJ))


BM1684_DIR ?= bm1684
ifeq ($(SOC_MODE), 1)
else
BM1684_OBJ += bm1684_pcie.o bm1684_vpp.o bm1684_irq.o bm1684_ddr.o bm1684_lpddr4.o \
			  bm1684_pld_ddr.o bm1684_base64.o bm1684_jpu.o bm1684_smbus.o
endif
BM1684_OBJ += bm1684_gmem.o bm1684_cdma.o bm1684_smmu.o bm1684_clkrst.o bm1684_card.o \
			  bm1684_msgfifo.o bm1684_perf.o bm1684_flash.o
BM1684_OBJ := $(addprefix $(BM1684_DIR)/, $(BM1684_OBJ))

BM_KERNEL_DRIVER_OUT_DIR ?= $(PWD)/../out/$(TARGET_PROJECT)
ifeq ($(SOC_MODE), 1)
  BM_MODULE_NAME = bmtpu
else
  BM_MODULE_NAME = bmsophon
endif

ccflags-y += -g
ccflags-y += -I$(src) -I$(src)/bm1682 -I$(src)/bm1684
ifeq ($(SOC_MODE), 1)
  ccflags-y += -DSOC_MODE
  cflags-y += -DSOC_MODE
endif

FW_SIMPLE ?= 0
ifeq ($(FW_SIMPLE),1)
  ccflags-y += -DFW_SIMPLE
endif

SYNC_API_INT_MODE ?= 1
ifeq ($(SYNC_API_INT_MODE),1)
  ccflags-y += -DSYNC_API_INT_MODE=1
else
  ccflags-y += -DSYNC_API_INT_MODE=0
endif

obj-m += $(BM_MODULE_NAME).o
ifeq ($(SOC_MODE), 1)
	$(BM_MODULE_NAME)-y += bm_soc_drv.o
else
	$(BM_MODULE_NAME)-y += bm_pcie_drv.o spi.o vpu/vpu.o bm_wdt.o \
	bm_boot_info.o
endif
$(BM_MODULE_NAME)-y += bm_drv.o bm_fops.o bm_irq.o i2c.o pwm.o  \
	bm_memcpy.o bm_ctl.o bm_fw.o bm_cdma.o \
	bm_msgfifo.o bm_thread.o bm_api.o \
	bm_timer.o bm_io.o bm_trace.o bm_attr.o bm_gmem.o \
	bm_bgm.o bm_genalloc.o gpio.o uart.o bm_debug.o bm_card.o

$(BM_MODULE_NAME)-y += $(BM1682_OBJ)
$(BM_MODULE_NAME)-y += $(BM1684_OBJ)

ifeq ($(SOC_MODE), 1)
  ifndef SOC_LINUX_DIR
      $(error "subtype of soc kernel to build not set!")
  endif
  LINUX_SRC := $(SOC_LINUX_DIR)
  KBUILD_CFLAGS += -DSOC_MODE
else
  ifndef CROSS_COMPILE
    KERNEL_VERSION = `uname -v`
    ifeq ($(findstring Debian, $(KERNEL_VERSION)),Debian)
      LINUX_SRC = /usr/src/linux-headers-`uname -r`
    else ifeq ($(findstring Ubuntu,$(KERNEL_VERSION)),Ubuntu)
      LINUX_SRC = /lib/modules/$(shell uname -r)/build
    else
      LINUX_SRC = /lib/modules/$(shell uname -r)/build
      CENTOS_KERNEL = $(shell uname -r |cut -d - -f1)
      CENTOS_RELEASE =  $(shell uname -r |cut -d - -f 2 |cut -d . -f 1-3)
      CENTOS_RELEASE_FIX = $(shell echo $(CENTOS_RELEASE)|cut -d . -f1)
      ifeq ($(CENTOS_KERNEL),3.10.0)
        cflags-y += -DCENTOS_KERNEL_FIX=$(CENTOS_RELEASE_FIX)
        ccflags-y += -DCENTOS_KERNEL_FIX=$(CENTOS_RELEASE_FIX)
      endif
    endif
  else
    ifndef LINUX_SRC
      $(error "LINUX_SRC not set!")
    endif
    ifndef ARCH
      $(error "ARCH not set!")
    endif
  endif
endif

all: clean
ifeq ($(SOC_MODE), 1)
	mkdir -p $(BM_KERNEL_DRIVER_OUT_DIR)
	make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C $(LINUX_SRC) M=$(PWD) modules
	cp -u $(BM_MODULE_NAME).ko $(BM_KERNEL_DRIVER_OUT_DIR)
	@make -C $(LINUX_SRC) M=$(PWD) clean
else
ifndef CROSS_COMPILE
	make -C $(LINUX_SRC) M=$(PWD) modules
else
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(LINUX_SRC) M=$(PWD) modules
endif
ifneq ($(TARGET_PROJECT),)
	mkdir -p $(BM_KERNEL_DRIVER_OUT_DIR)
	mv *.ko $(BM_KERNEL_DRIVER_OUT_DIR)/
else
	mv *.ko $(PWD)/../
endif
	@make -C $(LINUX_SRC) M=$(PWD) clean
endif

clean:
ifeq ($(SOC_MODE), 1)
	make -C $(LINUX_SRC) M=$(PWD) clean
else
	make -C $(LINUX_SRC) M=$(PWD) clean
endif

install:
	cp -u $(BM_MODULE_NAME).ko $(BM_KERNEL_DRIVER_OUT_DIR)
