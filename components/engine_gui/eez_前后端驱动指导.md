# EEZ Studio + LVGL 前后端链路搭建参考

> 适用场景：在 ESP-IDF / LVGL 项目中使用 EEZ Studio 作为可视化前端，后端为 C/C++ 业务代码。
> 目标：建立一套**单向、解耦、线程安全**的前后端通信链路，便于复用到其他项目。
> 本文只描述实现思路与操作方法，不透露具体业务界面与功能细节。

---

## 1. 核心原则：单向调用

| 方向 | 通道 | 说明 |
|------|------|------|
| 前端 → 后端 | EEZ Native Action + User Properties | UI 事件触发后端行为，后端不感知具体控件 |
| 后端 → 前端 | Flow Global Variables / Native Variables | 后端通过变量刷新 UI 状态，不直接操作界面元素 |
| 后端内部 | 事件总线 / 消息队列 | 后端模块间通信，不依赖 UI |

**禁止**：后端直接调用前端控件 API、直接切换页面、直接读取 UI 内部状态。所有交互必须通过 EEZ Flow 提供的官方接口完成。

---

## 2. 前端 → 后端：Native Action 参数传递

### 2.1 机制说明

EEZ Studio 生成的代码会在 `src/ui/actions.h` 中声明若干 Native Action 函数，例如：

```c
extern void action_<name>(lv_event_t *e);
```

后端需要在一个独立的 C/C++ 文件中提供这些函数的实现。**注意**：参数 `lv_event_t *e` 并不是真实的 LVGL 事件对象，不能用于获取触发控件或事件坐标，所有输入参数必须通过 EEZ User Properties 读取。

### 2.2 读取 User Properties

每个 Native Action 在 EEZ Studio 中可以定义若干 User Properties。EEZ 生成代码时会为每个属性分配一个枚举索引，集中在 `actions.h` 中：

```c
enum Action_<Name>Properties {
    ACTION_<NAME>_PROPERTY_<PARAM_A>,
    ACTION_<NAME>_PROPERTY_<PARAM_B>,
    ...
};
```

后端通过 `eez::flow::getUserProperty()` 读取：

```cpp
#include "eez-flow.h"
#include "actions.h"

extern "C" void action_<name>(lv_event_t *e)
{
    (void)e;  // 不使用该参数

    int param_a = eez::flow::getUserProperty(ACTION_<NAME>_PROPERTY_<PARAM_A>).getInt32();
    bool param_b = eez::flow::getUserProperty(ACTION_<NAME>_PROPERTY_<PARAM_B>).getBoolean();

    // 调用后端业务接口
}
```

### 2.3 实现要点

1. **C/C++ 混合**：EEZ Flow API 是 C++ 命名空间，后端实现文件通常需要是 `.cpp`，但函数声明使用 `extern "C"`，以便与生成的前端 C 代码链接。
2. **属性顺序敏感**：`actions.h` 中属性枚举顺序由 EEZ Studio 中定义顺序决定。调整顺序后，后端的索引必须同步更新。
3. **类型一致**：读取时使用与 EEZ Studio 中定义一致的类型，如 `getInt32()`、`getBoolean()`、`getString()` 等。
4. **事件参数无用**：所有控件相关数据都应作为 User Property 显式传入，不要从 `lv_event_t` 解析。

---

## 3. 后端 → 前端：全局变量与状态反馈

### 3.1 Flow Global Variables

在 EEZ Studio 中定义 Global Variable 后，后端可通过索引写入：

```cpp
#include "vars.h"

// 写入整型变量
eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_<NAME>,
                              eez::Value(value, eez::VALUE_TYPE_INT32));
```

适用于：版本号、连接状态、运行模式等离散状态。

### 3.2 Assignable User Properties

部分 Native Action 的属性可勾选为 Assignable，后端可以在 Action 内部写回：

```cpp
eez::flow::setUserProperty(ACTION_<NAME>_PROPERTY_<PARAM>,
                            eez::Value(value, eez::VALUE_TYPE_INT32));
```

适用于：需要立即回显给用户的临时反馈值。

### 3.3 Native Variables

对于需要持续刷新的数据（如时间、电量、实时数值），推荐在 EEZ Studio 中使用 **Native Variable**：

- 前端只负责绑定变量到显示控件。
- 后端提供 `get_var_<name>()` 回调，EEZ Flow 会在刷新周期自动调用。
- 避免后端主动轮询写入，降低跨任务同步复杂度。

---

## 4. 线程安全

EEZ Flow 的 `ui_tick()` 与所有 Native Action 回调都运行在 GUI 任务中。因此：

| 调用位置 | 是否可直接调用 `setGlobalVariable` | 说明 |
|----------|------------------------------------|------|
| GUI 任务（`ui_tick` / Native Action） | ✅ 可以 | 与 EEZ Flow 同一线程 |
| 其他任务 / 中断 | ❌ 不建议 | 需通过队列 / SysEx / 消息投递到 GUI 任务后再写变量 |

### 4.1 跨任务刷新流程

当非 GUI 任务需要更新 UI 状态时：

1. 生产者任务打包状态数据。
2. 通过 FreeRTOS 队列、环形缓冲区或内部事件总线投递到 GUI 任务。
3. GUI 任务在 `ui_tick()` 或专用处理函数中取出数据，再调用 `setGlobalVariable`。

这样可以避免 EEZ Flow 内部数据结构被多任务并发访问。

---

## 5. 项目接入操作步骤

1. **在 EEZ Studio 中完成界面设计**
   - 定义需要触发的 Native Action。
   - 为每个 Action 定义 User Properties，并确定数据类型。
   - 定义 Global Variables / Native Variables 用于后端回显状态。

2. **导出生成代码**
   - 生成代码输出到 `components/<gui_component>/src/ui/`。
   - 将 `src/ui/` 设为只读，禁止手动修改其中的文件。

3. **创建后端实现文件**
   - 在 GUI 组件目录下新建 `eez_backend.cpp`（或类似名称）。
   - 实现 `src/ui/actions.h` 中声明的所有 Native Action。
   - 所有函数声明为 `extern "C"`。

4. **修改组件 CMakeLists.txt**
   - 使用 `file(GLOB ...)` 自动收集 `src/ui/` 下的生成源文件。
   - 将后端实现文件（`.cpp`）显式加入 `SRCS`。
   - 将 `src/ui/` 加入 `INCLUDE_DIRS`。

   示例片段（占位符）：

   ```cmake
   set(UI_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/ui")
   file(GLOB UI_C_SOURCES "${UI_SRC_DIR}/*.c")
   file(GLOB UI_CXX_SOURCES "${UI_SRC_DIR}/*.cpp")
   set(UI_INCLUDES "${UI_SRC_DIR}")

   idf_component_register(
       SRCS "engine_gui.c"
            "eez_backend.cpp"
            ${UI_C_SOURCES}
            ${UI_CXX_SOURCES}
       INCLUDE_DIRS "include" ${UI_INCLUDES}
       REQUIRES ...
   )
   ```

5. **初始化与 tick**
   - 在 GUI 初始化函数中调用 `ui_init()`。
   - 在 GUI 任务循环中调用 `ui_tick()`。
   - 由 EEZ Flow 自动管理页面切换与 Native Action 调用。

6. **验证参数传递**
   - 在每个 Native Action 开头打印所有 User Property 值。
   - 确认日志与前端预期一致后，再接入真实业务逻辑。

---

## 6. 常见问题与避坑指南

| 问题 | 原因 | 解决方法 |
|------|------|----------|
| `actions.h` 中枚举值变了，后端行为异常 | 调整 EEZ 中属性顺序后枚举索引改变 | 每次修改 User Properties 后，同步检查后端的属性索引 |
| `VALUE_TYPE_INT32` 未定义 | 缺少命名空间 | 使用 `eez::VALUE_TYPE_INT32`，并包含 `eez-flow.h` |
| 后端无法链接到前端生成的 Action | 函数签名不匹配 | 确保 `extern "C"`、函数名与 `actions.h` 完全一致 |
| 非 GUI 任务调用 `setGlobalVariable` 导致崩溃 | 线程不安全 | 通过队列把更新投递到 GUI task |
| 手动修改 `src/ui/` 后重新生成被覆盖 | 该目录为生成代码 | 所有手写逻辑放在后端实现文件中 |

---

## 7. 设计心得

- **最小接口原则**：每个 Native Action 只干一件事，参数尽量精简，避免把业务状态塞进 Action。
- **前后端解耦**：前端只负责“用户操作发生了什么”，后端决定“系统该怎么做”。
- **状态回显走变量**：不要把后端状态通过日志或副作用传回前端，统一用 Flow 变量。
- **先验证参数再写业务**：Native Action 最容易出错的点是属性索引与类型不匹配，早期加日志可以节省大量调试时间。

---

**维护责任**：AI Agent / 开发者  
**最后更新**：2026-06-14
