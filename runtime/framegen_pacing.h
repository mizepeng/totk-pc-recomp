#pragma once
#include <stdint.h>

typedef struct {
	uint64_t first, real, next;
} FramegenPacing;

/*
 * 将生成帧排在当前可用时间之后，真实帧延后不足一个游戏帧。
 * earliest 来自上一组输出的计划结束时间，避免用实际等待时间反推帧率。
 */
static inline FramegenPacing framegen_pacing(uint64_t now, uint64_t target, uint64_t period, unsigned generated, uint64_t earliest) {
	uint64_t first = target > now ? target : now;
	if (earliest > first) first = earliest;
	return (FramegenPacing){first, first + period * generated / (generated + 1), first + period};
}
