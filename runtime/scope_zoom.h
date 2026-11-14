#pragma once
#include <math.h>
#include <stdint.h>

/*
 * 每四格把望远镜视野角减半，最多八格；退出时恢复原视野角。
 */
static inline int scope_zoom_steps(int current, int active, int notches) {
	if (!active) return 0;
	int64_t next = (int64_t)current + notches;
	return next < 0 ? 0 : next > 8 ? 8 : (int)next;
}

static inline float scope_zoom_fov(int steps) {
	return exp2f(-0.25f * (float)steps);
}
