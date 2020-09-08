//*****************************************************************************
//  All Winner Micro, All Right Reserved. 2006-2015 Copyright (c)
//
//  File name   :        de_eink.h
//
//  Description :  DE2.0 Write-Back Controller  interface functions
//  History     :
//				   2015/07/17	initial version
//	Code by		:  zengqi
//******************************************************************************
#ifndef __DE_EINK_H__
#define __DE_EINK_H__

#include "include.h"
#include "disp_debug.h"

int eink_config(unsigned char in_mode, unsigned int out_mode);
//int eink_start_idx(struct ee_img *last_img, struct ee_img *curr_img, unsigned char flash_mode, unsigned char win_en,
//					phys_addr_t last_idx_addr, phys_addr_t curr_idx_addr, struct area_info *info);
int eink_set_base(unsigned long reg_base);
int eink_irq_enable(void);
int eink_irq_disable(void);
int eink_irq_query(void);
int eink_set_mode(unsigned char in_mode, unsigned char out_mode);
//int eink_get_updata_area(struct area_info *info);
//int eink_pipe_enable(unsigned int pipe_no);
//int eink_pipe_disable(unsigned int pipe_no);
//int eink_pipe_config(struct area_info *info, unsigned int pipe_no);
//int eink_pipe_config_wavefile(phys_addr_t wav_file_addr, unsigned int pipe_no);
//int eink_edma_init(int bus_width);
//int eink_edma_en(unsigned char en);
//int eink_dbuf_rdy(void);
//int eink_set_wb(unsigned char wb_en, phys_addr_t wb_addr);
//int eink_decoder_start(phys_addr_t new_idx_addr, phys_addr_t wav_data_addr, struct epdc_init_param *para);
//int eink_edma_cfg(phys_addr_t wav_addr, struct epdc_init_param *para);
//int eink_edma_cfg_addr(phys_addr_t wav_addr);
int eink_index_finish(void);


#endif
