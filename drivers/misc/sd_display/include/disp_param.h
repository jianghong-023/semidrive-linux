#ifndef __DISP_DCPARAM__
#define __DISP_DCPARAM__

#include <linux/types.h>

struct dcparam {
	struct list_head list;
	int width  ; //img width
	int height; //img height
	int format; //img format
	int data_mode;//select see enum DATA_MODE(disp_common_def.h)
	int frm_buf_str_fmt;//select see enum FRM_BUF_STR_FMT(disp_common_def.h)
	/*input params*/
	void *buffer;
	int imgbase_offset_y; //img memory addr offset in buffer
	int imgbase_offset_u;
	int imgbase_offset_v;
	int stride_y; //img stride
	int stride_u;
	int stride_v;

	int offset_x; //img memory offset x
	int offset_y; // img memory offset y

	/*output params*/
	int glb_alpha_en;
	int alpha;
	int pos_x;  //display positon x
	int pos_y; //display positon y
	void *context_handle;
	void *priv_handle;
};
#endif
