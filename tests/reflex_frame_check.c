#include <stdio.h>
#include "reflex_frame.h"

int main(void) {
	uint64_t sequence = 0;
	for (unsigned multiplier = 1; multiplier <= 6; multiplier++) {
		uint64_t frame = 100 + multiplier;
		for (unsigned i = 0; i < multiplier; i++) {
			int generated = i + 1 < multiplier;
			uint64_t before = sequence;
			int kind = generated ? REFLEX_PRESENT_GENERATED : multiplier > 1 ? REFLEX_PRESENT_REAL_WITH_GENERATED : REFLEX_PRESENT_REAL;
			ReflexPresentTag tag = reflex_present_tag(frame, kind, &sequence);
			if (tag.frame != frame || tag.present != before + 1 ||
				tag.start != (multiplier > 1 ? VK_LATENCY_MARKER_OUT_OF_BAND_PRESENT_START_NV : VK_LATENCY_MARKER_PRESENT_START_NV) ||
				tag.end != (multiplier > 1 ? VK_LATENCY_MARKER_OUT_OF_BAND_PRESENT_END_NV : VK_LATENCY_MARKER_PRESENT_END_NV) ||
				tag.standard_markers != (!generated && multiplier > 1)) return 1;
		}
	}
	if (reflex_interval_us(12500000) != 12500 || reflex_interval_us(8333333) != 8334 || reflex_interval_us(0) != 0) return 2;
	puts("Reflex frame attribution: passed (1x-6x, independent IDs, generated markers, dynamic frame intervals)");
	return 0;
}
