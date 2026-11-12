#include <math.h>
#include <stdio.h>
#include "framegen_input.h"

static int failed;
#define CHECK(ok, message) do { if (!(ok)) { fprintf(stderr, "FAIL: %s\n", message); failed++; } } while (0)

int main(void) {
	FramegenInput a, b;
	CHECK(framegen_input(2560, 1440, 42, 41, 0, &a), "accept valid scene");
	CHECK(fabsf((0.55f - 128.0f * a.scale_x) - 0.5f) < 0.000001f,
		"camera pan reprojects the stationary surface to its previous horizontal screen position");
	CHECK(fabsf((0.45f + 72.0f * a.scale_y) - 0.5f) < 0.000001f,
		"camera tilt reprojects the stationary surface to its previous vertical screen position");
	CHECK(framegen_input(1920, 1080, 43, 42, 0, &b), "accept another scene size");
	CHECK(fabsf(128.0f * a.scale_x - 96.0f * b.scale_x) < 0.000001f,
		"the same camera displacement is independent of render resolution");
	CHECK(a.invalid_motion != 0.0f && a.invalid_motion > 65504.0f,
		"zero motion remains valid and the sentinel cannot occur in the half-float vector image");
	CHECK(!a.reset && !b.reset, "consecutive real frames keep their history");
	framegen_input(2560, 1440, 46, 42, 0, &a);
	CHECK(a.reset, "a skipped scene or menu invalidates interpolation history");
	framegen_input(2560, 1440, 1, 46, 0, &a);
	CHECK(a.reset, "a new frame sequence resets history");
	framegen_input(2560, 1440, 42, 41, 1, &a);
	CHECK(a.reset, "a camera reset is preserved");
	framegen_input(2560, 1440, 42, 0, 0, &a);
	CHECK(a.reset, "the first available frame resets history");
	CHECK(!framegen_input(0, 1440, 42, 41, 0, &a), "reject missing width");
	CHECK(!framegen_input(2560, 0, 42, 41, 0, &a), "reject missing height");
	CHECK(!framegen_input(2560, 1440, 0, 41, 0, &a), "reject missing frame identity");
	if (!failed) puts("framegen input: passed (camera reprojection, render sizes, zero motion, skipped frames, resets)");
	return failed ? 1 : 0;
}
