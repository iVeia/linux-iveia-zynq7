zap-objs := main.o pool.o dma.o
zap-objs += $(if $(CONFIG_ARCH_OMAP),omap_zap_dma.o)
zap-objs += $(if $(IVEIA_TITANV5),v5fxt_zap_dma.o)
zap-objs += $(if $(CONFIG_MACH_IV_ATLAS_I_Z7E),dma_zynq.o)
zap-objs += $(if $(CONFIG_ARCH_IV_ATLAS_II_Z8),dma_zynq.o)
obj-$(CONFIG_IVEIA_ZAP)	:= zap.o
