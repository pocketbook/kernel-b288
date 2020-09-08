include $(obj)/de/Makefile

obj-$(CONFIG_DISP2_SUNXI) += disp.o

disp-objs :=  dev_disp.o dev_fb.o dev_disp_debugfs.o
disp-objs	+= disp_sys_intf.o
disp-objs	+= de/disp_display.o de/disp_features.o de/disp_device.o de/disp_lcd.o de/disp_manager.o de/disp_private.o \
						 de/disp_eink_manager.o de/eink_pipeline_manager.o de/axi.o de/disp_format_convert.o de/disp_index_manager.o  \
						 de/frame.o de/waveform.o de/sysfs.o de/amepd.o de/bootlogo.o de/neon.o platform/b288.o pmic/tps65185.o

disp-objs += lcd/panels.o lcd/lcd_source.o lcd/default_panel.o lcd/default_eink.o
disp-objs += $(obj_low)
