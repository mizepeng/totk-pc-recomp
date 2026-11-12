#pragma once
#include <stdint.h>
#include <vulkan/vulkan_core.h>

enum { REFLEX_PRESENT_REAL, REFLEX_PRESENT_GENERATED, REFLEX_PRESENT_REAL_WITH_GENERATED };

typedef struct {
	uint64_t frame, present;
	VkLatencyMarkerNV start, end;
	int standard_markers;
} ReflexPresentTag;

/*
 * Vulkan 的 Frame Attribution 区分应用真实帧和交换链呈现编号。
 * 生成帧使用额外呈现标记，并与所属真实帧共享 Reflex 编号。
 */
static inline ReflexPresentTag reflex_present_tag(uint64_t frame, int kind, uint64_t *sequence) {
	return (ReflexPresentTag){frame, ++*sequence,
		kind != REFLEX_PRESENT_REAL ? VK_LATENCY_MARKER_OUT_OF_BAND_PRESENT_START_NV : VK_LATENCY_MARKER_PRESENT_START_NV,
		kind != REFLEX_PRESENT_REAL ? VK_LATENCY_MARKER_OUT_OF_BAND_PRESENT_END_NV : VK_LATENCY_MARKER_PRESENT_END_NV,
		kind == REFLEX_PRESENT_REAL_WITH_GENERATED};
}

static inline uint32_t reflex_interval_us(uint64_t period_ns) {
	return (uint32_t)((period_ns + 999) / 1000);
}
