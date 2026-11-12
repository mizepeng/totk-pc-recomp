#pragma once
#include <stdint.h>

typedef struct {
	unsigned target, multiplier, candidate;
	uint64_t candidate_since, changed_at;
	int limited;
} FramegenAuto;

/*
 * 按 CPU、GPU 和每张生成帧的实际耗时选择能满足目标的最小倍率。
 * 增加倍率需持续 0.4 秒，降低倍率需持续 1.5 秒，切换后至少保持一秒。
 */
static inline unsigned framegen_auto_update(FramegenAuto *s, uint64_t now, unsigned target, uint64_t cpu, uint64_t gpu, uint64_t generated) {
	unsigned minimum = (target + 239) / 240;
	if (minimum < 1) minimum = 1;
	unsigned maximum = target / 30;
	if (maximum > 6) maximum = 6;
	if (s->target != target) {
		*s = (FramegenAuto){.target = target, .multiplier = minimum > 2 ? minimum : 2, .changed_at = now};
		if (s->multiplier > maximum) s->multiplier = maximum;
	}
	if (!cpu || !gpu || !generated) return s->multiplier;
	unsigned wanted;
	for (wanted = minimum; wanted <= maximum; wanted++) {
		uint64_t gpu_work = gpu + generated * (wanted - 1);
		uint64_t work = cpu > gpu_work ? cpu : gpu_work;
		uint64_t budget = 1000000000ull * wanted / target;
		if (work <= budget * 92 / 100) break;
	}
	s->limited = wanted > maximum;
	if (wanted > maximum) wanted = maximum;
	if (wanted == s->multiplier) {
		s->candidate = 0;
		return s->multiplier;
	}
	if (s->candidate != wanted) {
		s->candidate = wanted;
		s->candidate_since = now;
	}
	uint64_t hold = wanted > s->multiplier ? 400000000ull : 1500000000ull;
	if (now - s->candidate_since >= hold && now - s->changed_at >= 1000000000ull) {
		s->multiplier = wanted;
		s->changed_at = now;
		s->candidate = 0;
	}
	return s->multiplier;
}
