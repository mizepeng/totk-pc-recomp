#include <limits.h>
#include <math.h>
#include <stdio.h>
#include "scope_zoom.h"

int main(void) {
	int n = scope_zoom_steps(0, 1, 4);
	if (n != 4 || fabsf(scope_zoom_fov(n) - 0.5f) > 0.00001f) return 1;
	n = scope_zoom_steps(n, 1, INT_MAX);
	if (n != 8 || fabsf(scope_zoom_fov(n) - 0.25f) > 0.00001f) return 2;
	n = scope_zoom_steps(n, 1, INT_MIN);
	if (n != 0 || scope_zoom_fov(n) != 1.0f) return 3;
	n = scope_zoom_steps(6, 0, 2);
	if (n != 0 || fabsf(1.1f * scope_zoom_fov(n) - 1.1f) > 0.00001f) return 4;
	if (scope_zoom_steps(0, 0, INT_MAX) != 0 || scope_zoom_steps(3, 1, 0) != 3) return 5;
	puts("scope zoom: passed (zoom direction, bounds, large wheel deltas, exit restoration)");
	return 0;
}
