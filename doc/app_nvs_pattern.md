# App 级 NVS 参数存取样板

本文档定义项目内所有 App 持久化自身参数时应遵循的一致模式。目标：

- 读、写、校验逻辑清晰可复用
- UI 控件与本地状态保持单一可信源
- 避免参数丢失，同时不滥用 NVS Flash 写入

## 前置条件

- 已在 `service_nvs.h` 中定义好 App 专用的参数结构体（如 `service_nvs_piano_t`）
- `service_nvs` 已为该结构体提供 `service_nvs_get_xxx()` / `service_nvs_set_xxx()` API
- 若 `service_nvs` 中尚无对应分组，请先按项目规范新增 NVS 分组

## 样板代码

### 1. 状态结构体

```c
typedef struct {
    /* 需要持久化的参数 */
    uint8_t param_a;
    uint8_t param_b;

    /* 运行时状态 */
    bool    running;
    /* ... */
} app_state_t;

static app_state_t s_state = {0};
```

### 2. 参数加载

```c
static void app_load_params(void)
{
    service_nvs_xxx_t params;
    service_nvs_get_xxx(&params);

    s_state.param_a = params.param_a;
    if (s_state.param_a > PARAM_A_MAX) {
        s_state.param_a = PARAM_A_DEFAULT;
    }

    s_state.param_b = params.param_b;
    if (s_state.param_b > PARAM_B_MAX) {
        s_state.param_b = PARAM_B_DEFAULT;
    }
}
```

### 3. 参数保存

```c
static void app_save_params(void)
{
    service_nvs_xxx_t params = {
        .param_a = s_state.param_a,
        .param_b = s_state.param_b,
    };
    service_nvs_set_xxx(&params);
}
```

### 4. on_init：清零 → 默认值 → 读 NVS → 校验 → 应用 UI 与引擎

```c
static bool app_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    (void)screen_ctx;

    memset(&s_state, 0, sizeof(s_state));
    s_state.param_a = PARAM_A_DEFAULT;
    s_state.param_b = PARAM_B_DEFAULT;

    app_load_params();

    lvgl_port_lock(portMAX_DELAY);
    if (ui->dropdown_a != NULL) {
        lv_dropdown_set_selected(ui->dropdown_a, s_state.param_a);
    }
    if (ui->dropdown_b != NULL) {
        lv_dropdown_set_selected(ui->dropdown_b, s_state.param_b);
    }
    /* 应用到引擎/硬件 */
    app_apply_params();
    lvgl_port_unlock();

    return true;
}
```

### 5. on_update：轮询 UI，任一参数变化即更新并保存

```c
static void app_on_update(app_base_t *self)
{
    (void)self;

    uint32_t a = lv_dropdown_get_selected(ui->dropdown_a);
    uint32_t b = lv_dropdown_get_selected(ui->dropdown_b);

    bool changed = false;
    if (a != s_state.param_a) {
        s_state.param_a = (uint8_t)a;
        changed = true;
    }
    if (b != s_state.param_b) {
        s_state.param_b = (uint8_t)b;
        changed = true;
    }

    if (changed) {
        app_apply_params();
        app_save_params();
    }
}
```

### 6. on_pause：兜底保存

```c
static void app_on_pause(app_base_t *self)
{
    (void)self;

    /* 兜底保存，防止用户修改后未触发 on_update 保存就退出 */
    app_save_params();
}
```

## 关键原则

1. **本地状态是单一可信源**：`on_init` 把 NVS 读到 `s_state`，再把 `s_state` 同步到 UI；`on_update` 从 UI 读到 `s_state` 再保存。不要直接在 UI 和 NVS 之间穿梭。
2. **范围校验在 `load_params` 完成**：NVS 里可能残留异常值（例如 App 升级后枚举范围变化），读取时立即 clamp 到合法范围。
3. **参数变化即时保存**：`service_nvs_set_xxx()` 只标记脏位，`service_nvs_commit()` 由 `task_app` 每秒批量调用，不会每次 set 都刷 Flash。
4. **退出时兜底保存**：`on_pause` 中调用 `app_save_params()`，防止 `on_update` 漏保存。
5. **避免死代码**：不要把加载逻辑写成未调用的独立函数；`load_params` 必须被 `on_init` 调用。

## 完整示例：小钢琴

```c
static void piano_load_params(void)
{
    service_nvs_piano_t params;
    service_nvs_get_piano(&params);

    s_piano.display = params.display;
    if (s_piano.display > 1) {
        s_piano.display = 0;
    }

    s_piano.scale = params.scale;
    if (s_piano.scale >= PIANO_SCALE_COUNT) {
        s_piano.scale = 0;
    }

    s_piano.root_oct = params.root_oct;
    if (s_piano.root_oct > 6) {
        s_piano.root_oct = 3;
    }

    s_piano.sound_type = params.sound_type;
    if (s_piano.sound_type > 15) {
        s_piano.sound_type = 0;
    }
}

static void piano_save_params(void)
{
    service_nvs_piano_t params = {
        .display = s_piano.display,
        .scale = s_piano.scale,
        .root_oct = s_piano.root_oct,
        .sound_type = s_piano.sound_type,
    };
    service_nvs_set_piano(&params);
}

static bool app_tiny_piano_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    (void)screen_ctx;

    memset(&s_piano, 0, sizeof(s_piano));
    s_piano.display = 0;
    s_piano.scale = 0;
    s_piano.root_oct = 3;
    s_piano.sound_type = 0;

    piano_load_params();

    lvgl_port_lock(portMAX_DELAY);
    if (s_piano_ui.display_type != NULL) {
        lv_dropdown_set_selected(s_piano_ui.display_type, s_piano.display);
    }
    if (s_piano_ui.scale_type != NULL) {
        lv_dropdown_set_selected(s_piano_ui.scale_type, s_piano.scale);
    }
    if (s_piano_ui.root_v != NULL) {
        lv_roller_set_selected(s_piano_ui.root_v, s_piano.root_oct, LV_ANIM_OFF);
    }
    if (s_piano_ui.sound_type != NULL) {
        lv_dropdown_set_selected(s_piano_ui.sound_type, s_piano.sound_type);
    }
    /* ... 注册事件回调 ... */
    lvgl_port_unlock();

    piano_apply_display();
    piano_refresh_pads();
    piano_set_sound_type(s_piano.sound_type);
    return true;
}

static void app_tiny_piano_on_update(app_base_t *self)
{
    (void)self;

    /* 已在 UI lock 内读取 disp / scale / root / sound_type */
    bool changed = false;
    if (disp != s_piano.display) {
        s_piano.display = (uint8_t)disp;
        piano_apply_display();
        changed = true;
    }
    if (scale != s_piano.scale && scale < PIANO_SCALE_COUNT) {
        s_piano.scale = (uint8_t)scale;
        piano_refresh_pads();
        changed = true;
    }
    if (root != s_piano.root_oct) {
        s_piano.root_oct = (uint8_t)root;
        changed = true;
    }
    if (sound_type != s_piano.sound_type && sound_type <= 15) {
        s_piano.sound_type = (uint8_t)sound_type;
        piano_set_sound_type(s_piano.sound_type);
        changed = true;
    }

    if (changed) {
        piano_save_params();
    }
}

static void app_tiny_piano_on_pause(app_base_t *self)
{
    (void)self;
    /* ... 停止发声/录音 ... */
    piano_save_params();
}
```

## 完整示例：节拍器

```c
static void metron_load_params(void)
{
    service_nvs_metronome_t params;
    service_nvs_get_metronome(&params);

    s_metron.bpm = params.bpm;
    if (s_metron.bpm < METRON_BPM_MIN || s_metron.bpm > METRON_BPM_MAX) {
        s_metron.bpm = 120;
    }

    s_metron.sig_top = params.sig_top;
    if (s_metron.sig_top < 1 || s_metron.sig_top > 16) {
        s_metron.sig_top = 4;
    }

    s_metron.sig_bot = params.sig_bot;
    if (s_metron.sig_bot > 4) {
        s_metron.sig_bot = 0;
    }

    s_metron.sound = params.sound;
    if (s_metron.sound >= METRON_SOUND_COUNT) {
        s_metron.sound = 0;
    }
}

static void metron_save_params(void)
{
    service_nvs_metronome_t params = {
        .bpm = s_metron.bpm,
        .sig_top = s_metron.sig_top,
        .sig_bot = s_metron.sig_bot,
        .sound = s_metron.sound,
        .reserved = 0,
    };
    service_nvs_set_metronome(&params);
}
```

`on_init` / `on_update` / `on_pause` 的结构与小钢琴完全一致。

## 常见反模式

- ❌ 把加载函数写成死代码，实际 `on_init` 里用默认值
- ❌ 只在某个特定操作（如点击播放）才保存，导致其它参数修改丢失
- ❌ 读取 NVS 后不回写 UI 控件，用户再次进入看到默认选项
- ❌ 读取 NVS 后不应用到引擎/硬件，实际运行还是默认值
- ❌ 不做范围校验，导致 NVS 里的旧数据破坏 App 状态

## 何时扩展 service_nvs 分组

如果新 App 的参数无法归入已有分组（`piano`、`metronome`、`clock`、`ear_trainer` 等），请按以下步骤扩展：

1. 在 `components/service_nvs/include/service_nvs.h` 中定义 App 参数结构体
2. 在 `struct s_system_parameters` 中新增一个字段
3. 在 `components/service_nvs/service_nvs.c` 中新增：
   - 一个 NVS key
   - 一个脏位
   - `load_xxx()` / `commit_xxx()` 处理
   - `service_nvs_get_xxx()` / `service_nvs_set_xxx()` 公共 API
4. 在 App 中按本文档样板读写
