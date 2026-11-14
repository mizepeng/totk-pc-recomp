// Mouse wheel and keyboard gameplay tweaks: Ultrahand rotation and distance, item and weapon wheel, map zoom.
#pragma once
#include <stdint.h>

int gameplay_tweaks_available(void);
int gameplay_tweaks_capture_needed(void);
int gameplay_tweaks_strip_wheel(uint64_t buttons);
// 滚轮打开的武器快捷栏正开着（等待 E 确认）；菜单覆盖层据此显示按键提示。任意线程。
int gameplay_tweaks_weapon_open(void);
void gameplay_tweaks_init(void);
void gameplay_tweaks_reset(void);
void gameplay_tweaks_wheel(double delta, int c_down, int right_down);
// 直接鼠标模式下究极手抓着物体时的鼠标纵向位移（原始计数）。按住右键且推拉功能开启时返回 1 并收下位移，
// 调用方不再用它上下移动物体；否则返回 0。
int gameplay_tweaks_hand_drag(int32_t dy);
void gameplay_tweaks_prepare_keys(uint8_t *down, uint64_t pad_buttons);
void gameplay_tweaks_apply(uint64_t frame, int active, uint64_t *buttons,
                           int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry);
