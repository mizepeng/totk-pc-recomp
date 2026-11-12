#pragma once
#include <float.h>
#include <stdint.h>

typedef struct {
	float scale_x, scale_y, invalid_motion;
	int reset;
} FramegenInput;

/*
 * NVIDIA DLSS 的 nvsdk_ngx_defs_dlssg.h 要求 MvecScale 把位移归一化。
 * 帧号来自真实呈现序列；中断后不能把跨多帧的颜色与相邻帧运动矢量混用。
 */
static inline int framegen_input(uint32_t width, uint32_t height, uint64_t frame, uint64_t previous, int reset, FramegenInput *out) {
	if (!width || !height || !frame) return 0;
	*out = (FramegenInput){1.0f / (float)width, 1.0f / (float)height, FLT_MAX,
		reset || !previous || frame != previous + 1};
	return 1;
}
