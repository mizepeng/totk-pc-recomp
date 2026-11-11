#include <assert.h>
#include <stdio.h>
#include "framegen_auto.h"

int main(void) {
	FramegenAuto s = {0};
	uint64_t now = 1000000000;
	assert(framegen_auto_update(&s, now, 240, 6000000, 3000000, 1000000) == 2);
	assert(framegen_auto_update(&s, now + 100000000, 240, 20000000, 10000000, 1000000) == 2);
	assert(framegen_auto_update(&s, now + 1100000000, 240, 20000000, 10000000, 1000000) == 6);
	assert(framegen_auto_update(&s, now + 1200000000, 240, 4000000, 2000000, 500000) == 6);
	assert(framegen_auto_update(&s, now + 2800000000, 240, 4000000, 2000000, 500000) == 2);
	assert(framegen_auto_update(&s, now + 2900000000, 240, 100000000, 100000000, 1000000) == 2);
	assert(s.limited);
	assert(framegen_auto_update(&s, now + 4000000000, 240, 100000000, 100000000, 1000000) == 6);
	assert(framegen_auto_update(&s, now + 5000000000, 1000, 1000000, 1000000, 500000) >= 5);
	framegen_auto_update(&s, now + 6000000000, 60, 5000000, 3000000, 1000000);
	assert(framegen_auto_update(&s, now + 8000000000, 60, 5000000, 3000000, 1000000) == 1);
	FramegenAuto jitter = {0};
	for (unsigned i = 0; i < 100; i++) {
		unsigned m = framegen_auto_update(&jitter, now + i * 100000000ull, 240,
			(i & 1) ? 9000000 : 6000000, 3000000, 1000000);
		assert(m == 2);
	}
	puts("framegen auto: passed (increase, decrease, hysteresis, limits, target changes, 1x bypass)");
	return 0;
}
