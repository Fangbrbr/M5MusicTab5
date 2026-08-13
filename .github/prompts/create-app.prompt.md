---
description: "Create a new App following the app_base_t lifecycle pattern"
---

# 创建新 App

## 步骤

1. **创建目录结构**
   ```
   components/app_<name>/
   ├── CMakeLists.txt
   ├── include/
   │   └── app_<name>.h
   └── app_<name>.c
   ```

2. **定义 App 结构体**
   ```c
   typedef struct {
       app_base_t base;  // 必须是第一个字段
       // App 私有状态
   } app_<name>_t;
   ```

3. **实现生命周期回调**
   ```c
   static bool on_init(app_base_t *base) { /* 初始化 */ }
   static bool on_render(app_base_t *base) { /* 渲染 */ }
   static void on_update(app_base_t *base) { /* 每帧更新 */ }
   static void on_pause(app_base_t *base) { /* 挂起 */ }
   static void on_resume(app_base_t *base) { /* 恢复 */ }
   static void on_destroy(app_base_t *base) { /* 销毁 */ }
   static void on_input(app_base_t *base, const app_input_event_t *event) { /* 输入 */ }
   static void on_sysex(app_base_t *base, const uint8_t *data, size_t len) { /* SysEx */ }
   ```

4. **注册 App**
   在 `app_manager_register_all()` 中添加：
   ```c
   app_manager_register(&(app_base_t){
       .name = "app_<name>",
       .screen_name = "screen_<name>",
       .screen_ctx_size = sizeof(app_<name>_t),
       .widget_bindings = <name>_bindings,
       .on_init = on_init,
       .on_render = on_render,
       .on_update = on_update,
       .on_pause = on_pause,
       .on_resume = on_resume,
       .on_destroy = on_destroy,
       .on_input = on_input,
       .on_sysex = on_sysex,
   });
   ```

5. **定义控件绑定**
   ```c
   static const widget_binding_t <name>_bindings[] = {
       {"btn_play", offsetof(app_<name>_t, btn_play), LV_OBJ_TYPE_BTN},
       {NULL, 0, 0}  // 结束标记
   };
   ```

6. **构建验证**
   - 运行 `idf.py fullclean` + `idf.py build`
   - 确保零错误

## 注意事项

- App 不得直接调用 Service/BSP
- 发声统一走 MIDI 总线：`engine_midi_publish_*()`
- LVGL 事件注册后必须在 `on_destroy` 中移除
- `on_update` 受递归互斥锁保护，禁止调用阻塞操作
