# DuckStation Switch 前端界面与功能说明

本文以当前源码为准，整理 Switch 构建使用的前端实现、界面流程、控制器操作和可见功能。Switch 版本没有单独的 Qt 窗口前端，采用 **NoGUI Host + FullscreenUI（ImGui）+ Deko3D** 的组合。

## 1. 实现架构

```text
libnx appletMainLoop()
        │
        ▼
SwitchNoGUIPlatform ── 窗口尺寸、消息循环、错误应用、Applet 模式变化
        │
        ▼
NoGUIHost::CPUThreadEntryPoint
  ├─ System::Internal::ProcessStartup()
  ├─ Host::CreateGPUDevice()       → Deko3DDevice
  ├─ FullscreenUI::Initialize()    → ImGui 全屏菜单/对话框
  ├─ Host::RefreshGameListAsync()
  └─ CPUThreadMainLoop()           → 执行模拟、轮询输入、PresentDisplay
```

| 模块 | 位置 | 职责 |
| --- | --- | --- |
| NoGUI 主机 | [`src/duckstation-nogui/nogui_host.cpp`](../src/duckstation-nogui/nogui_host.cpp) | 启动/关闭模拟系统，创建 GPU 和 FullscreenUI，驱动 CPU 线程与空闲帧循环。 |
| Switch 平台层 | [`src/duckstation-nogui/switch_nogui_platform.cpp`](../src/duckstation-nogui/switch_nogui_platform.cpp) | libnx Applet hook、窗口信息、消息队列、错误显示和生命周期。 |
| 全屏前端 | [`src/core/fullscreen_ui.cpp`](../src/core/fullscreen_ui.cpp) | 所有主菜单、游戏列表、暂停菜单、设置页、存档选择器和对话框。 |
| 输入源 | [`src/util/switch_input_source.cpp`](../src/util/switch_input_source.cpp) | 最多 4 个 Switch 控制器的按键/摇杆/震动事件，转换为 DuckStation 通用输入。 |
| 前端控制器接口 | [`src/util/switch_controller_interface.cpp`](../src/util/switch_controller_interface.cpp) | 前端导航键、模拟器按键绑定、摇杆死区和连接状态。 |
| 音频 | [`src/util/switch_audio_stream.cpp`](../src/util/switch_audio_stream.cpp) | libnx `audren/audrv` 输出、双缓冲音频线程、暂停静音和音量。 |
| 图形 | [`src/util/gpu_device.cpp`](../src/util/gpu_device.cpp)、`src/util/deko3d_*` | Switch 固定走 Deko3D；负责 ImGui 绘制、交换链和 shader。 |

### Switch 显示参数

- 掌机模式：1280×720；底座模式：1920×1080。
- 刷新率按 60 Hz 提供，UI surface scale 为 1.2。
- 窗口句柄来自 `nwindowGetDefault()`；Applet 模式切换时重新提交窗口尺寸。

## 2. 主界面和状态流转

`FullscreenUI::MainWindowType` 定义了 `Landing`、`StartGame`、`Exit`、`GameList`、`GameListSettings`、`Settings`、`PauseMenu`、`Achievements`、`Leaderboards` 等状态。模拟器未加载游戏时进入 Landing；游戏启动后主窗口隐藏，按暂停键再显示 Pause Menu。

### Landing（首页）

- **Game List**：进入游戏库。
- **Start Game**：从文件、光盘或 BIOS 启动。
- **Settings**：打开全局设置。
- **Exit**：返回、退出 DuckStation；桌面模式入口在 Switch 上没有实际实现。
- 快捷入口：About、Resume Last Session、Toggle Fullscreen（后两项在 Switch 上分别受存档和平台能力限制）。

### Start Game

- **Start File**：文件选择器选择游戏镜像/可启动文件。
- **Start Disc**：选择光盘设备或镜像。
- **Start BIOS**：直接启动 BIOS。
- **Back**：返回 Landing。

启动路径可以附带恢复存档，也可以选择默认启动、快速启动或慢速启动。

### Exit

- **Back**：回到 Landing。
- **Exit DuckStation**：结束程序。
- **Desktop Mode**：通用 FullscreenUI 选项，但 Switch 没有桌面模式实现，不能作为可用功能宣传。

## 3. 游戏列表

`DrawGameListWindow()` 提供两种视图：

- **Game Grid**：封面网格，显示标题。
- **Game List**：列表 + 封面 + 详细信息（标题、开发商、序列号、地区、类型、发行日期、兼容性、游玩时间、文件大小等）。

选中游戏后可直接启动；Launch Options 菜单提供：

1. Game Properties（单游戏设置）；
2. Open Containing Directory；
3. Resume Game（读取自动恢复存档）；
4. Load State；
5. Default Boot；
6. Fast Boot；
7. Slow Boot；
8. Reset Play Time；
9. Close Menu。

### Game List Settings

- 添加搜索目录；每个目录可打开文件浏览器、切换是否递归扫描、从列表移除。
- Default View：默认网格/列表。
- Sort By：类型、序列号、标题、文件标题、游玩时间、最后游玩时间、文件大小、未压缩大小。
- Sort Reversed：反向排序。
- Covers Directory：封面目录；Download Covers：按 URL 模板下载封面。
- Scan For New Games、Rescan All Games：增量扫描或完整重扫。

## 4. 游戏运行中的暂停菜单

打开暂停菜单会暂停 CPU（若之前未暂停），关闭时恢复原状态。菜单背景还显示当前游戏封面、标题、序列号、Rich Presence、当前时间、本次会话时间和累计游玩时间。

### PauseSubMenu::None

- Resume Game
- Toggle Fast Forward
- Load State / Save State
- Cheat List（启用作弊且游戏有序列号时可用）
- Toggle Analog
- Game Properties
- Achievements（含 Leaderboards 子菜单）
- Save Screenshot
- Change Disc
- Settings
- Close Game（进入退出子菜单）

### Exit 子菜单

- Back To Pause Menu
- Reset System
- Exit And Save State
- Exit Without Saving

### Achievements 子菜单

- Back To Pause Menu
- Achievements
- Leaderboards

## 5. 存档和恢复

- Save State / Load State 以网格显示槽位、预览图、时间和摘要。
- 支持 Quick Save、Game Slot 和 Global Slot。
- 对单个槽位可执行加载/保存、Delete Save、Close Menu。
- 从游戏列表启动时若发现自动恢复存档，会弹出 **Load Resume State**：Load State、Clean Boot、Delete State、Cancel。
- 存档加载/保存通过 CPU 线程执行，失败以异步错误消息报告。

## 6. 设置界面

全局设置和单游戏设置共用同一套页面渲染器。单游戏设置使用独立 INI 层，支持“使用全局值/覆盖全局值”的三态行为；Summary 页面还提供 Copy Settings 和 Clear Settings。

| 页面 | 功能范围 |
| --- | --- |
| Summary（仅单游戏） | 标题、序列号、类型、地区、兼容性、路径；复制或清除该游戏覆盖配置。 |
| Interface | 启动暂停、失焦暂停、关机确认、退出自动存档、启动全屏、光标/屏保行为、应用单游戏设置、明暗主题、确认/返回键交换、UI 语言、Discord Presence。 |
| On-Screen Display（Interface 内） | OSD 缩放、OSD 消息、速度、FPS、GPU/CPU/延迟、帧时间、分辨率、控制器输入显示。 |
| Console | 主机区域、8 MB RAM、关闭增强、作弊；CPU 执行模式、超频、Recompiler ICache；CD-ROM 读速/寻道加速、预读、镜像预加载、PPF 补丁。 |
| Emulation | 模拟速度、快进速度、Turbo 速度；VSync、匹配主机刷新率、最佳帧 pacing、降低输入延迟；Rewind 和 Runahead。 |
| BIOS | 各主机区域 BIOS 路径、自动检测、BIOS 目录、Fast Boot 补丁、TTY 日志。 |
| Display / Graphics | GPU Renderer、Adapter、全屏分辨率；内部分辨率、纹理过滤、线检测、真彩色/去色带、宽屏、PGXP；比例、去隔行、裁剪、对齐、下采样、缩放；截图尺寸/格式/质量；抖动、24-bit 色彩、纹理替换。 |
| Post-Processing | 启用/禁用、Reload Shaders、Add Shader、Clear Shaders；阶段删除、上移、下移；每个 shader 的布尔/整数/浮点参数。 |
| Audio | 输出音量、快进音量、全局静音、CD 音频静音；Audio Backend、Stretch Mode、Buffer Size、最小/指定输出延迟。 |
| Controller | 单游戏控制器配置、复制全局设置、重置、保存/加载输入 profile；输入源开关、Multitap、端口控制器类型、自动映射、宏和死区。 |
| Hotkey | 枚举所有已注册热键并进入输入绑定对话框。 |
| Memory Cards | 存档目录、恢复默认目录、多光盘共用卡、Port 1/2 卡类型和共享卡文件。 |
| Achievements | 启用 RetroAchievements、Hardcore、成就/排行榜通知、音效、游戏内覆盖、Encore、Spectator、非官方成就；登录、注销、当前游戏和 Rich Presence。 |
| Advanced | 日志级别/输出位置、调试 GPU、无 SBI 启动、存档备份、从存档加载设备；状态图标、增强信息、FPS 限制、垂直拉伸、线框；PGXP 高级阈值；纹理 dump；Recompiler ICache/异常/区块链接/Fastmem；CD 区域检查。 |

设置控件统一使用 FullscreenUI 的 Toggle、枚举列表、整数/浮点范围、旋钮、文件夹选择和输入绑定组件，修改后标记设置层并按需要应用或提示重启。

## 7. Switch 控制器和导航

`SwitchControllerInterface` 将以下按键作为前端导航：

| Switch 输入 | 前端行为 |
| --- | --- |
| A | Activate / Select |
| B | Cancel / Back |
| D-pad | 上下左右导航；设置页左右切换分类 |
| L / R | LeftShoulder / RightShoulder，常用于页签或范围调整 |
| Plus、Minus、X、Y、L3、R3、ZL、ZR | 可绑定为模拟器按键；未映射为前端导航 |

`SwitchInputSource` 支持 P0–P3、双摇杆轴（LeftX/LeftY/RightX/RightY）、按键字符串解析和大小/小型震动马达。前端底部 footer 会根据输入源显示 D-pad、确认、返回、删除、设置等帮助。

源码中还保留桌面键盘兼容快捷键：F1/F2/F3/F11 分别用于部分 About、列表视图/设置、启动选项/恢复和全屏入口；Switch 实际使用时以控制器导航为主。

## 8. 对话框与辅助功能

- Choice Dialog：单选列表（设备、BIOS、渲染器、存档操作等）。
- Confirm Message Dialog：清除 shader、删除或退出等确认场景。
- File Selector：文件、目录、光盘镜像和 BIOS 选择。
- Input String Dialog：输入 profile 名称等文本。
- Input Binding Dialog：等待按键/摇杆输入，默认绑定等待时间为 5 秒。
- Toast：操作成功/失败、重载 shader、存档删除等短消息。
- About、Achievements、Leaderboards、Rich Presence 覆盖层。

## 9. Switch 平台已知限制

这些是当前实现的事实，不应在界面说明中写成已支持功能：

- `ConfirmMessage()` 直接返回 `true`，没有真正调用 Switch 确认弹窗；涉及确认的调用路径会被直接接受。
- `SetFullscreen()`、`RequestRenderWindowSize()`、`OpenURL()`、`CopyTextToClipboard()` 在 Switch 平台为空操作或返回失败。
- `ConvertHostKeyboardStringToCode()` / `ConvertHostKeyboardCodeToString()` 返回空，Switch 没有桌面键盘码转换。
- `SwitchControllerInterface::GetControllerRumbleMotorCount()` 返回 0，`SetControllerRumbleStrength()` 为空；通用控制器接口层没有震动能力。底层 `SwitchInputSource` 仍实现了 HID 震动发送，因此实际震动是否可用取决于调用路径。
- 窗口由 libnx 提供，不能像桌面版那样创建/销毁任意窗口；渲染后端固定为 Deko3D，Qt、OpenGL、Vulkan、Cubeb 在 Switch 构建中关闭。
- 没有独立的桌面模式、URL 打开器、剪贴板和运行时改变窗口大小能力。

## 10. 构建与产物核对

按 [`README_Switch.md`](../README_Switch.md) 的 Switch CMake/Ninja 配置完成构建后，当前工作区已有：

- [`build-switch-ninja/GBAStationDuckStationStub.nro`](../build-switch-ninja/GBAStationDuckStationStub.nro)
- 大小：27,010,817 字节
- 文件头：`NRO0`
- 增量 `ninja`：`no work to do`

这证明 NRO 已成功生成；尚未把“能编译”扩展为 Switch 真机或模拟器运行验证，硬件输入、音频、Deko3D shader 和在线 RetroAchievements 仍需独立运行测试。

## 11. 维护建议

1. 新增主界面时同步更新 `MainWindowType`、`Render()` 分发、返回逻辑和 footer 帮助。
2. 新增设置页时同时加入全局/单游戏页签数组、标题、图标和对应 `Draw*SettingsPage()`。
3. 任何桌面专属功能在 Switch 上必须先检查 `switch_nogui_platform.cpp` 的实际实现，再决定是否显示或标注不可用。
4. 修改输入映射时同时更新 `switch_input_source.cpp` 的名称表、通用映射、解析和反向格式化，避免配置文件无法往返。
5. 变更 UI 后至少执行一次 Switch 增量构建，并把真机/模拟器运行结果与编译结果分开记录。
