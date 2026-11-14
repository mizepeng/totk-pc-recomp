// Mouse wheel and keyboard gameplay tweaks with independent live settings.
// The game's native R + D-pad object rotation and D-pad distance controls remain authoritative.
#include "gameplay_tweaks.h"
#include "cfg.h"
#include "host.h"
#include "input.h"
#include "input_map.h"
#include "input_wsys.h"
#include "platform/platform.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define BIT(x) (1ull << (x))
#define HOLD_FRAMES 3
#define GAP_FRAMES 1
#define MAX_QUEUED 16
enum { NONE, ROTATE, ITEMS, WEAPONS };
enum { W_IDLE = 0, W_PRIME, W_HOLD, W_GAP, W_WAIT };
#define WEAPON_PRIME_MIN_NS  75000000ull
#define WEAPON_PRIME_MAX_NS 135000000ull
#define WEAPON_HOLD_NS       42000000ull
#define WEAPON_GAP_NS        36000000ull

static PlatMutex lock = PLAT_MUTEX_INIT;
static unsigned seen = UINT_MAX;
static struct {
    int available, rotate, c_axis, distance, wheel, weapon_wheel;
    double rotation_speed, distance_speed, item_speed, weapon_speed;
} options;
static struct {
    int kind, axis, direction, queued, hold, gap, primed, valid;
    double fraction;
    uint64_t frame, buttons;
    int32_t rx;
} pulse;
static struct {
    int active, phase, direction, scroll_dir, queued, polls_in_phase;
    // 按下了游戏自己会关闭快捷栏的键（B 返回、X 从袋子中取出）：松开前继续按住十字键右，松开后结束
    int closing;
    int confirm_pending;
    double fraction;
    uint64_t phase_until_ns, prime_start_ns, last_frame;
    int32_t rx;
} weapon;
// 究极手按住右键前后移动鼠标推拉物体：游戏原生的推拉是十字键上/下按住匀速移动，
// 所以把鼠标位移换算成按住时长。drag_ns 为还要按住的时长，符号是方向（负 = 上 = 推远）；drag_at 是上次扣减的时刻。
#define DRAG_NS_PER_COUNT 1000000.0
#define DRAG_MAX_NS 250000000.0
static double drag_ns;
static uint64_t drag_at;
static int consumed_c, consumed_right;
static int was_e_down, consumed_e;
static uint64_t physical_buttons;

static void clear_weapon_locked(void) { memset(&weapon, 0, sizeof weapon); }
static void clear_locked(void) { memset(&pulse, 0, sizeof pulse); clear_weapon_locked(); drag_ns = 0; }

static void refresh_locked(void) {
    unsigned version = cfg_version();
    if (seen == version) return;
    options.available = cfg_bool("TOTK_GAMEPLAY_TWEAKS_ENABLED");
    options.rotate = cfg_bool("TOTK_GAMEPLAY_ULTRAHAND_ROTATE");
    options.c_axis = cfg_bool("TOTK_GAMEPLAY_ULTRAHAND_C_AXIS");
    options.distance = cfg_bool("TOTK_GAMEPLAY_ULTRAHAND_DISTANCE");
    options.wheel = cfg_bool("TOTK_GAMEPLAY_ITEM_WHEEL");
    options.weapon_wheel = cfg_bool("TOTK_GAMEPLAY_WEAPON_WHEEL");
    options.rotation_speed = cfg_double("TOTK_GAMEPLAY_ROTATION_SENSITIVITY");
    options.distance_speed = cfg_double("TOTK_GAMEPLAY_DISTANCE_SENSITIVITY");
    options.item_speed = cfg_double("TOTK_GAMEPLAY_ITEM_SENSITIVITY");
    options.weapon_speed = cfg_double("TOTK_GAMEPLAY_WEAPON_SENSITIVITY");
    if (!isfinite(options.rotation_speed) || options.rotation_speed < .25 || options.rotation_speed > 4) options.rotation_speed = 1;
    if (!isfinite(options.distance_speed) || options.distance_speed < .25 || options.distance_speed > 4) options.distance_speed = 1;
    if (!isfinite(options.item_speed) || options.item_speed < .25 || options.item_speed > 4) options.item_speed = 1;
    if (!isfinite(options.weapon_speed) || options.weapon_speed < .25 || options.weapon_speed > 4) options.weapon_speed = 1;
    seen = version;
    clear_locked();
}

static int item_menu(uint64_t buttons) {
    if (buttons & BIT(B_L)) return 0;
    const uint64_t held = BIT(B_LEFT) | BIT(B_UP) | BIT(B_RIGHT) | BIT(B_DOWN);
    return input_camera_context() == INPUT_CAM_MENU_WHEEL || (buttons & held) != 0;
}

static int object_context(uint64_t buttons) {
    const uint64_t menus = BIT(B_L) | BIT(B_LEFT) | BIT(B_UP) | BIT(B_RIGHT) | BIT(B_DOWN) |
                           BIT(B_B) | BIT(B_PLUS) | BIT(B_MINUS);
    return input_camera_context() == INPUT_CAM_ULTRAHAND_OBJECT && !(buttons & menus);
}

static int weapon_wheel_allowed(uint64_t buttons, int already_active) {
    if (!options.available || !options.weapon_wheel) return 0;
    if (!ikb_captured() || menu_cursor_map() || menu_cursor_pointer()) return 0;
    const uint64_t forbidden = BIT(B_L) | BIT(B_R) | BIT(B_ZL) | BIT(B_ZR) |
                               BIT(B_LEFT) | BIT(B_UP) | BIT(B_RIGHT) | BIT(B_DOWN) |
                               BIT(B_PLUS) | BIT(B_MINUS);
    // 这些按键只阻止打开。快捷栏打开后它们不再关闭它（游戏里关闭即选中，等于误确认），
    // 而是被 gameplay_tweaks_apply 吞掉，只有 E 确认。按住 ZL（默认鼠标右键，举盾）时也可以打开。
    if (!already_active && (buttons & forbidden & ~BIT(B_ZL))) return 0;

    if (!already_active) {
        if (menu_cursor_shortcut() == 1 || menu_cursor_strip() || menu_cursor_camera() || menu_cursor_menu())
            return 0;
    }

    int ctx = input_camera_context();
    if (!already_active) {
        // Opening needs a camera the mouse_direct hooks saw this moment: an unclassified one is also a paused menu,
        // 1.2.1's menus or any screen menu_cursor.c does not read, where D-pad right and the right stick would move
        // the menu instead.
        if (ctx != INPUT_CAM_FREE_ROAM && ctx != INPUT_CAM_LOCK_ON && ctx != INPUT_CAM_VEHICLE)
            return 0;
    } else {
        if (ctx != INPUT_CAM_UNCLASSIFIED && ctx != INPUT_CAM_MENU_WHEEL &&
            ctx != INPUT_CAM_FREE_ROAM && ctx != INPUT_CAM_LOCK_ON && ctx != INPUT_CAM_VEHICLE)
            return 0;
    }
    return 1;
}

int gameplay_tweaks_available(void) {
    plat_mutex_lock(&lock);
    refresh_locked();
    int available = options.available;
    plat_mutex_unlock(&lock);
    return available;
}

int gameplay_tweaks_capture_needed(void) {
    plat_mutex_lock(&lock);
    refresh_locked();
    int active = options.available && (options.rotate || options.distance || options.wheel ||
                                      options.weapon_wheel);
    plat_mutex_unlock(&lock);
    return active;
}

int gameplay_tweaks_strip_wheel(uint64_t buttons) {
    plat_mutex_lock(&lock);
    refresh_locked();
    int active = options.available && (weapon.active || (options.wheel && item_menu(buttons)));
    plat_mutex_unlock(&lock);
    return active;
}

int gameplay_tweaks_weapon_open(void) {
    plat_mutex_lock(&lock);
    int open = weapon.active;
    plat_mutex_unlock(&lock);
    return open;
}

void gameplay_tweaks_init(void) {
    gameplay_tweaks_available();
}

void gameplay_tweaks_reset(void) {
    plat_mutex_lock(&lock);
    clear_locked();
    physical_buttons = 0;
    was_e_down = consumed_e = 0;
    plat_mutex_unlock(&lock);
}

void gameplay_tweaks_wheel(double delta, int c_down, int right_down) {
    if (!isfinite(delta) || delta == 0) return;
    if (delta > MAX_QUEUED) delta = MAX_QUEUED;
    if (delta < -MAX_QUEUED) delta = -MAX_QUEUED;
    plat_mutex_lock(&lock);
    refresh_locked();
    if (!options.available) {
        clear_locked();
        plat_mutex_unlock(&lock);
        return;
    }
    int kind = NONE, axis = 0;
    double sensitivity = 1;
    if (weapon.active && weapon_wheel_allowed(physical_buttons, 1)) {
        // 已由滚轮打开的武器栏优先：此时按着的方向键等已被吞掉，不应把这次滚动判给别的功能而关掉它
        kind = WEAPONS;
    } else if (object_context(physical_buttons)) {
        // 按住右键时右键在推拉物体（gameplay_tweaks_hand_drag），滚轮不再旋转，以免两种十字键输入互相干扰
        if (options.rotate && !(right_down && options.distance)) {
            kind = ROTATE;
            axis = options.c_axis && c_down;
            sensitivity = options.rotation_speed;
        }
    } else if ((physical_buttons & (BIT(B_LEFT) | BIT(B_UP) | BIT(B_RIGHT) | BIT(B_DOWN))) &&
               options.wheel && item_menu(physical_buttons)) {
        kind = ITEMS;
        sensitivity = options.item_speed;
    } else if (weapon_wheel_allowed(physical_buttons, weapon.active)) {
        kind = WEAPONS;
    }
    if (!kind) {
        clear_locked();
        plat_mutex_unlock(&lock);
        return;
    }
    if (kind == WEAPONS) {
        uint64_t now = plat_now_ns();
        int new_dir = delta > 0 ? 1 : -1;
        if (weapon.scroll_dir != 0 && new_dir != weapon.scroll_dir) {
            weapon.fraction = 0;
            weapon.queued = 0;
            if (weapon.active && weapon.phase == W_HOLD) {
                weapon.phase = W_GAP;
                weapon.rx = 0;
                weapon.polls_in_phase = 0;
                weapon.phase_until_ns = now + WEAPON_GAP_NS;
            }
        }
        weapon.scroll_dir = new_dir;
        weapon.fraction += delta * options.weapon_speed;
        int steps = (int)trunc(weapon.fraction);
        if (steps != 0) {
            weapon.confirm_pending = 0;
            weapon.fraction -= steps;
            weapon.direction = steps > 0 ? 1 : -1;
            weapon.queued += (steps > 0 ? steps : -steps);
            if (weapon.queued > MAX_QUEUED) weapon.queued = MAX_QUEUED;
            if (!weapon.active) {
                weapon.active = 1;
                weapon.phase = W_PRIME;
                weapon.polls_in_phase = 0;
                weapon.prime_start_ns = now;
                weapon.phase_until_ns = now + WEAPON_PRIME_MAX_NS;
                weapon.rx = 0;
            } else if (weapon.phase == W_WAIT) {
                weapon.phase = W_HOLD;
                weapon.polls_in_phase = 0;
                weapon.phase_until_ns = now + WEAPON_HOLD_NS;
                weapon.rx = 32767 * weapon.direction;
                if (weapon.queued > 0) weapon.queued--;
            }
        }
        plat_mutex_unlock(&lock);
        return;
    }
    if (weapon.active) clear_weapon_locked();
    if (kind != pulse.kind || axis != pulse.axis ||
        (delta > 0 && pulse.direction < 0) || (delta < 0 && pulse.direction > 0)) {
        clear_locked();
    }
    pulse.kind = kind;
    pulse.axis = axis;
    pulse.fraction += delta * sensitivity;
    int steps = (int)trunc(pulse.fraction);
    if (steps) {
        pulse.fraction -= steps;
        pulse.direction = steps > 0 ? 1 : -1;
        if (!pulse.queued && !pulse.hold && !pulse.gap) pulse.valid = 0;
        pulse.queued += steps > 0 ? steps : -steps;
        if (pulse.queued > MAX_QUEUED) pulse.queued = MAX_QUEUED;
    }
    plat_mutex_unlock(&lock);
}

int gameplay_tweaks_hand_drag(int32_t dy) {
    plat_mutex_lock(&lock);
    refresh_locked();
    int take = options.available && options.distance && ikb_down(KEY_MOUSE_R) && object_context(physical_buttons);
    if (take && dy) {
        // 反向移动立即改向，不先消耗完上一方向的余量
        if ((dy < 0) != (drag_ns < 0)) drag_ns = 0;
        drag_ns += dy * DRAG_NS_PER_COUNT * options.distance_speed;
        if (drag_ns > DRAG_MAX_NS) drag_ns = DRAG_MAX_NS;
        if (drag_ns < -DRAG_MAX_NS) drag_ns = -DRAG_MAX_NS;
    }
    plat_mutex_unlock(&lock);
    return take;
}

void gameplay_tweaks_prepare_keys(uint8_t *down, uint64_t pad_buttons) {
    plat_mutex_lock(&lock);
    refresh_locked();
    int c = down[KEY_C], right = down[KEY_MOUSE_R];
    int e = down[KEY_E];
    int e_pressed = e && !was_e_down;
    was_e_down = e;
    if (!e) consumed_e = 0;
    if (!c) consumed_c = 0;
    if (!right) consumed_right = 0;
    if (options.available) {
        if (input_camera_context() == INPUT_CAM_ULTRAHAND_OBJECT) {
            if (options.rotate && options.c_axis && !(options.distance && right) && c) consumed_c = 1;
            if (options.distance && right) consumed_right = 1;
            if (pulse.kind == ROTATE && pulse.axis != (options.c_axis && c)) clear_locked();
        }
        if (weapon.active && e_pressed) {
            consumed_e = 1;
            weapon.confirm_pending = 1;
        }
        if (consumed_e) down[KEY_E] = 0;
        if (consumed_c) down[KEY_C] = 0;
        if (consumed_right) down[KEY_MOUSE_R] = 0;
    } else {
        consumed_c = consumed_right = consumed_e = 0;
    }
    plat_mutex_unlock(&lock);
}

static void sample_locked(uint64_t frame) {
    if (pulse.valid && pulse.frame == frame) return;
    pulse.buttons = 0;
    pulse.rx = 0;
    if (pulse.queued || pulse.hold || pulse.gap) {
        if (pulse.kind == ROTATE) pulse.buttons = BIT(B_R);
        if (pulse.kind == ROTATE && !pulse.primed) pulse.primed = 1;
        else if (pulse.gap) pulse.gap--;
        else {
            if (!pulse.hold && pulse.queued) {
                pulse.queued--;
                pulse.hold = HOLD_FRAMES;
            }
            if (pulse.hold) {
                if (pulse.kind == ROTATE)
                    pulse.buttons |= pulse.axis ? (pulse.direction > 0 ? BIT(B_DOWN) : BIT(B_UP)) :
                                                 (pulse.direction > 0 ? BIT(B_RIGHT) : BIT(B_LEFT));
                else pulse.rx = 32767 * pulse.direction;
                if (!--pulse.hold) pulse.gap = GAP_FRAMES;
            }
        }
    } else pulse.primed = 0;
    pulse.valid = 1;
    pulse.frame = frame;
}

void gameplay_tweaks_apply(uint64_t frame, int active, uint64_t *buttons,
                           int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry) {
    (void)lx;
    (void)ly;
    plat_mutex_lock(&lock);
    refresh_locked();
    physical_buttons = *buttons;
    if (!active || !options.available) {
        clear_locked();
        plat_mutex_unlock(&lock);
        return;
    }
    int right = ikb_down(KEY_MOUSE_R);

    if (weapon.active) {
        if (!weapon_wheel_allowed(physical_buttons, 1)) {
            clear_weapon_locked();
        } else if (physical_buttons & (BIT(B_B) | BIT(B_X))) {
            // 先让游戏处理返回/取出并关闭快捷栏，这期间不松开十字键右（松开即选中当前武器）
            weapon.closing = 1;
        } else if (weapon.closing) {
            clear_weapon_locked();
        }
    }

    if (weapon.active) {
        uint64_t now = plat_now_ns();
        if (weapon.last_frame != frame) {
            weapon.last_frame = frame;
            weapon.polls_in_phase++;
        }

        switch (weapon.phase) {
        case W_PRIME: {
            uint64_t elapsed = now - weapon.prime_start_ns;
            int shortcut = (menu_cursor_shortcut() == 1);
            int ready = (elapsed >= WEAPON_PRIME_MIN_NS && weapon.polls_in_phase >= 3 && shortcut) ||
                        (elapsed >= WEAPON_PRIME_MAX_NS && weapon.polls_in_phase >= 4);
            if (ready) {
                if (weapon.queued > 0) {
                    weapon.phase = W_HOLD;
                    weapon.polls_in_phase = 0;
                    weapon.phase_until_ns = now + WEAPON_HOLD_NS;
                    weapon.rx = 32767 * weapon.direction;
                    weapon.queued--;
                } else if (weapon.confirm_pending) {
                    clear_weapon_locked();
                } else {
                    weapon.phase = W_WAIT;
                    weapon.polls_in_phase = 0;
                    weapon.rx = 0;
                }
            }
            break;
        }
        case W_HOLD: {
            if (now >= weapon.phase_until_ns && weapon.polls_in_phase >= 2) {
                weapon.phase = W_GAP;
                weapon.polls_in_phase = 0;
                weapon.phase_until_ns = now + WEAPON_GAP_NS;
                weapon.rx = 0;
            }
            break;
        }
        case W_GAP: {
            if (now >= weapon.phase_until_ns && weapon.polls_in_phase >= 2) {
                if (weapon.queued > 0) {
                    weapon.phase = W_HOLD;
                    weapon.polls_in_phase = 0;
                    weapon.phase_until_ns = now + WEAPON_HOLD_NS;
                    weapon.rx = 32767 * weapon.direction;
                    weapon.queued--;
                } else if (weapon.confirm_pending) {
                    clear_weapon_locked();
                } else {
                    weapon.phase = W_WAIT;
                    weapon.polls_in_phase = 0;
                    weapon.rx = 0;
                }
            }
            break;
        }
        case W_WAIT: {
            if (weapon.confirm_pending) {
                clear_weapon_locked();
            }
            break;
        }
        default:
            clear_weapon_locked();
            break;
        }
    }

    if (weapon.active) {
        // 快捷栏由本模块按住十字键右维持，E 确认、B 返回、X 从袋子中取出交给游戏；
        // 其余会误关快捷栏（等于选中当前武器）的按键不传给游戏
        *buttons &= ~(BIT(B_L) | BIT(B_R) | BIT(B_ZL) | BIT(B_ZR) | BIT(B_LEFT) | BIT(B_UP) | BIT(B_DOWN) |
                      BIT(B_PLUS) | BIT(B_MINUS));
        *buttons |= BIT(B_RIGHT);
        // 滚轮步进的脉冲优先；空闲时保留左右移动鼠标的选择（input.c 的快捷栏鼠标选择写入的右摇杆）
        if (weapon.rx) *rx = weapon.rx;
        *ry = 0;
    } else {
        int object = object_context(*buttons), items = item_menu(*buttons);
        if ((pulse.kind == ROTATE && (!object || !options.rotate || (right && options.distance))) ||
            (pulse.kind == ITEMS && (!items || !options.wheel))) {
            clear_locked();
        }
        sample_locked(frame);
        uint64_t drag_now = plat_now_ns();
        if (!object || !right || !options.distance) drag_ns = 0;
        else if (drag_ns != 0) {
            *buttons |= drag_ns < 0 ? BIT(B_UP) : BIT(B_DOWN);
            double spent = drag_at ? (double)(drag_now - drag_at) : 0;
            if (spent >= fabs(drag_ns)) drag_ns = 0;
            else drag_ns += drag_ns < 0 ? spent : -spent;
        }
        drag_at = drag_now;
        if (pulse.kind == ROTATE) {
            *buttons |= pulse.buttons;
            // R rotation owns the look stick, because the game's rotation mode would otherwise rotate both axes.
            if (pulse.buttons) *rx = *ry = 0;
        } else if (pulse.kind == ITEMS && (pulse.queued || pulse.hold || pulse.gap || pulse.rx)) {
            *rx = pulse.rx;
            *ry = 0;
        }
    }
    plat_mutex_unlock(&lock);
}
