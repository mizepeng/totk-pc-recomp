#include <assert.h>
#include <stdio.h>
#include "framegen_pacing.h"

int main(void) {
	const uint64_t period = 16666668;
	for (unsigned generated = 1; generated <= 5; generated++) {
		FramegenPacing p = framegen_pacing(1000000000, 1000000000, period, generated, 0);
		assert(p.first == 1000000000);
		assert(p.real == p.first + period * generated / (generated + 1));
		assert(p.next == p.first + period);
		uint64_t previous = p.first;
		for (unsigned i = 1; i <= generated; i++) {
			uint64_t at = p.first + period * i / (generated + 1);
			assert(at > previous);
			previous = at;
		}
		assert(previous == p.real);
	}
	FramegenPacing late = framegen_pacing(2000000000, 1000000000, period, 1, 1500000000);
	assert(late.first == 2000000000);
	assert(late.real > 2000000000);
	FramegenPacing early = framegen_pacing(1000000000, 1005000000, period, 1, 1010000000);
	assert(early.first == 1010000000);
	FramegenPacing next = framegen_pacing(early.real, 1006000000, period, 1, early.next);
	assert(next.first - early.real == period / 2);
	FramegenPacing future = framegen_pacing(1000000000, 1020000000, period, 1, 0);
	assert(future.first == 1020000000);
	puts("framegen pacing: passed (2x-6x, late frames, continuity, future targets)");
	return 0;
}
