#
# Copyright (C) 2015 MediaTek Inc.
#

ifneq ($(KERNELRELEASE),)
    subdir-ccflags-y += -I$(srctree)/
    subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
    subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/clkbuf/src
    subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/clkbuf_v1/$(MTK_PLATFORM)
    subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
    subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ifeq ($(CONFIG_MTK_PMIC_CHIP_MT6359),y)
    subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6359
endif
ifeq ($(CONFIG_MTK_PMIC_NEW_ARCH),y)
    subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
endif
    subdir-ccflags-y += -I$(srctree)/drivers/mmc/core
    subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
    subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/
    subdir-ccflags-y += -I$(srctree)/drivers/clk/mediatek/
    subdir-ccflags-y += -I$(srctree)/drivers/pinctrl/mediatek/
    subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/power_throttling/

    # Core connectivity infrastructure
    MODULE_NAME := connadp
    obj-$(CONFIG_MTK_COMBO) += $(MODULE_NAME).o

    $(MODULE_NAME)-objs += common/connectivity_build_in_adapter.o
    $(MODULE_NAME)-objs += common/wmt_build_in_adapter.o
    $(MODULE_NAME)-objs += power_throttling/adapter.o
    $(MODULE_NAME)-objs += power_throttling/core.o
    ifeq ($(CONFIG_CONN_PWR_DEBUG),y)
        $(MODULE_NAME)-objs += power_throttling/test.o
    endif

    ifeq ($(CONFIG_MTK_COMBO), y)
        ccflags-y += -D CFG_CONNADP_BUILD_IN
    endif

    # Merged Hardware Parameters (Handles MT6765/Redmi A1 & MT6768/Redmi 12C)
    export CONFIG_MTK_COMBO_WIFI_HIF=axi
    export BT_PLATFORM=connac1x
    export MTK_COMBO_CHIP=CONNAC
    export MTK_ANDROID_WMT=y
    export MTK_ANDROID_EMI=y
    export WIFI_IP_SET=1
    export WIFI_ECO_VER=1
    export MTK_WLAN_SERVICE=yes

    # Dynamically select WLAN_CHIP_ID based on your Defconfig selection
    ifneq ($(CONFIG_MTK_COMBO_CHIP_CONSYS_6765),)
        export WLAN_CHIP_ID=6765
    else
        export WLAN_CHIP_ID=6768
    endif

    # Handle Built-In Driver Flags
    ifeq ($(CONFIG_WLAN_DRV_BUILD_IN),y)
        subdir-ccflags-y += -D MTK_WCN_REMOVE_KERNEL_MODULE
        subdir-ccflags-y += -D MTK_WCN_BUILT_IN_DRIVER
    endif

    # Direct Compilation Paths (No Symlinks Required)
    obj-y += common/
    obj-$(CONFIG_MTK_EXTRA_CONN_DRV_BUILD_IN) += connfem/
    obj-$(CONFIG_WLAN_DRV_BUILD_IN) += wlan/adaptor/
    obj-$(CONFIG_WLAN_DRV_BUILD_IN) += wlan/core/gen4m/
    obj-$(CONFIG_MTK_EXTRA_CONN_DRV_BUILD_IN) += bt/mt66xx/wmt/
    obj-$(CONFIG_MTK_EXTRA_CONN_DRV_BUILD_IN) += fmradio/
    obj-$(CONFIG_MTK_EXTRA_CONN_DRV_BUILD_IN) += gps/

else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
