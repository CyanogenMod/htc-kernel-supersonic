
# core framebuffer
#
obj-y := msm_fb.o logo.o
ifeq ($(CONFIG_FB_MSM_LOGO),y)
obj-y += logo.o
endif

# MDP DMA/PPP engine
#
obj-y += mdp.o mdp_ppp.o
obj-$(CONFIG_FB_MSM_OVERLAY) += mdp4_util.o
obj-$(CONFIG_FB_MSM_OVERLAY) += mdp4_overlay.o
obj-$(CONFIG_FB_MSM_OVERLAY) += mdp4_overlay_mddi.o
obj-$(CONFIG_MSM_MDP40) += msm_rotator.o

obj-$(CONFIG_MSM_MDP22) += mdp_ppp22.o
obj-$(CONFIG_MSM_MDP30) += mdp_ppp22.o
obj-$(CONFIG_MSM_MDP302)+= mdp_ppp22.o
obj-$(CONFIG_MSM_MDP31) += mdp_ppp31.o

# MDDI interface
#
obj-y += mddi.o

# MDDI client/panel drivers
#
obj-y += mddi_client_dummy.o
obj-y += mddi_client_epson.o
obj-y += mddi_client_novb9f6_5582.o

# MDP LCD controller driver
obj-$(CONFIG_FB_MSM_LCDC) += mdp_lcdc.o
obj-$(CONFIG_FB_MSM_TVOUT) += tvenc.o tvfb.o

# Yamato GL driver
ifeq ($(CONFIG_ARCH_MSM7X30),y)
obj-$(CONFIG_GPU_MSM_KGSL) += gpu/kgsl_adreno205/
else
obj-$(CONFIG_GPU_MSM_KGSL) += gpu/kgsl/
endif

obj-$(CONFIG_MSM_HDMI) += hdmi/
