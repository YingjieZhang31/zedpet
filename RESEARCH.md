# Pet 实现调研：ClawPuter vs raising-hell-cardputer

## 一、整体定位

| 维度 | ClawPuter | raising-hell-cardputer |
|------|-----------|----------------------|
| **宠物类型** | 像素龙虾（单一物种） | 恶魔 / 邪神（2种可选） |
| **核心玩法** | 桌面陪伴 + 天气模拟 + 湿度养护 | 电子宠物养成 + 等级进化 + 资源管理 |
| **跨平台** | ESP32 固件 + macOS 桌面应用（双端同步） | 纯 ESP32 固件（M5Cardputer 单设备） |
| **代码规模** | ~10 个核心文件 | ~30+ 个核心文件 |

## 二、架构对比

### ClawPuter：双端 UDP 同步架构

```
ESP32 M5Cardputer (固件)                    macOS 桌面应用 (Swift/AppKit)
=======================                     =============================
Companion 类                                PetBehavior 类
  ├── 状态机 (IDLE/HAPPY/SLEEP/TALK/...)      ├── 同步模式 (跟随固件状态)
  ├── 精灵渲染 (16x16, 3倍放大)               ├── 独立模式 (光标跟随 + lerp)
  ├── 天气系统 (天空/粒子/天体)                └── 状态映射
  ├── 湿度系统 (0-4级)
  ├── 自动行为 (伸展/张望)              PetView 类
  └── 配件 (墨镜/雨伞/雪帽/口罩)          ├── 跟随模式 (透明浮动精灵)
        │                                 └── 场景模式 (360x200 天气场景)
        │ UDP 广播 (19820端口, 每200ms)
        │ JSON: {s,f,m,x,y,d,w,t,h,rh}   AppDelegate
        ▼                                   ├── 双窗口管理
       UDPListener 接收                       ├── 菜单栏菜单
        │                                    └── UDP 调度
        ▼
       TCPSender → UDP 端口 19822 (反向命令)
```

### raising-hell-cardputer：实体-组件 + 状态机架构

```
[Choose Pet] → [Hatching] → [Name Pet] → [Pet Screen (主界面)]
                                              │
                    ┌─────────────────────────┼─────────────────────────┐
                    ▼                         ▼                         ▼
              [PET 标签页]             [STATS 标签页]           [FEED / PLAY]
              (动画/漫游)              (属性/等级)              (交互操作)
                    │
                    ▼
              [Sleep Screen] ←→ [Autonomy 引擎]
              (睡眠/昏倒)       (外卖/恶作剧/自动入睡)
```

## 三、状态系统对比

### ClawPuter — 简单行为状态机

```
IDLE ←→ HAPPY / SLEEP / TALK / STRETCH / LOOK
```

- 6 种状态，自动过期回到 IDLE
- 受湿度水平影响的随机自主行为
- 帧率：IDLE 500ms, HAPPY 200ms, SLEEP 1000ms, TALK 250ms, STRETCH 400ms, LOOK 300ms

### raising-hell-cardputer — 情绪 + 生命阶段双维度

**6 种情绪**（按优先级排列）：
```
SICK > TIRED > HUNGRY > MAD > BORED > HAPPY
```
- 由饥饿度/快乐度/能量/生命值的阈值决定
- 情绪影响被动经验获取率（10%-150%）
- 情绪影响漫游行为触发条件

**4 个进化阶段**：
```
Baby (0) → Teen (1) → Adult (2) → Elder (3)
```
- 进化需要达到最低等级 (10/20/30) + 使用特定物品
- 不同阶段有不同的精灵图和背景
- 阶段描述符：Devil — Hellspawn/Infernal Youth/Tormented Adult/Elder Demon；Eldritch — Voidspawn/Warped Youth/Cult Horror/Outer Horror

## 四、属性系统对比

| 属性 | ClawPuter | raising-hell-cardputer |
|------|-----------|----------------------|
| 饥饿度 | 无 | hunger (0-100, 随时间衰减) |
| 快乐度 | 无 | happiness (0-100) |
| 能量 | 无 | energy (0-100) |
| 生命值 | 无 | health (0-100) |
| 湿度 | 0-4 级 (影响移速+行为) | 无 |
| 货币 | 无 | INF (商店购物) |
| 等级/经验 | 无 | XP + Level (公式: 120+20×(L-1)+12×(L-1)²) |
| 天气 | 雨/雪/雾 + 温度模拟 | 无 |
| 年龄 | 无 | 精确到分钟 (从出生时间戳计算) |
| 衰减速度 | 无 | 6 档 (超慢→疯狂) |

## 五、渲染系统对比

### ClawPuter
- 16×16 像素精灵，RGB565 数组硬编码在 `sprites.h`
- 3× 放大至 48px
- 场景：天空渐变 + 天气粒子 + 地面 + 时钟
- 配件系统：墨镜/雨伞/雪帽/口罩（根据天气自动切换）
- 睡眠 Z 动画、低湿度闪烁提示

### raising-hell-cardputer
- 96×96 PNG 精灵图，从 SD 卡按路径加载
- 路径格式：`/{type}/{stage}/{anim}/{direction}_{frame}.png`
- 场景：背景图分层缓存（M5Canvas）+ 动画帧合成 + HUD 叠加
- 漫游系统：6 种状态（HOME_IDLE → MOVING_TO_SIDE_A → PAUSE_AWAY_1 → MOVING_TO_SIDE_B → PAUSE_AWAY_2 → RETURNING_HOME），55px 范围
- 入场动画：从屏幕左侧走到 home 位置 (3px/步, 40ms/步)

## 六、独有功能

### ClawPuter 独有
- macOS 桌面宠物同步（透明浮动窗口 + 光标跟随）
- 天气模拟系统（晴/雨/雪/雾 + 温度）
- 湿度机制（喷水恢复、移动消耗）
- 聊天记录查看器
- 像素艺术弹窗
- 时间旅行（场景模式中调节 displayHour）

### raising-hell-cardputer 独有
- 完整电子宠物养成循环（喂食/玩耍/睡眠）
- 进化系统（需要特定物品 + 等级条件）
- 死亡/复活机制（2.2s 死亡转场 + 2s 复活宽限期）
- 自主行为引擎：
  - 自动点披萨（饥饿 ≤10 时，扣 25 INF，12h 冷却）
  - 自动入睡（能量 ≤5 时昏倒）
  - 恶作剧（生气时随机扣 5-15 INF，25% 概率）
- 双持久化：EEPROM + SD 卡，支持 `.bub` 格式备份/导入/导出
- 孵化动画（13 步脚本化序列）
- 进化动画（4 阶段脚本化序列）
- 被动经验系统（每 5 分钟，情绪系数 0.1-1.5x）
- 商店系统（5 种物品，INF 货币）
- 完美护理奖励（全属性 ≥80 + 情绪 Happy 时 +25% XP）

## 七、Sprite 精灵数据格式

### ClawPuter
```cpp
// sprites.h — 硬编码 RGB565 数组
static const uint16_t sprite_idle1[256] = { ... };  // 16x16
static const uint16_t sprite_idle2[256] = { ... };
// 共 8 帧：idle1/2/3, happy1/2, sleep1, talk1/2
```

### raising-hell-cardputer
```
SD 卡文件系统:
/assets/devil/stage0/walk/right_0.png
/assets/devil/stage0/walk/right_1.png
/assets/devil/stage0/idle/idle_0.png
/assets/devil/stage1/walk/right_0.png
...
/assets/eldritch/stage0/walk/right_0.png
...
```

## 八、通信协议

### ClawPuter UDP 协议
```
ESP32 → macOS (端口 19820, 每 200ms):
  JSON: {s, f, m, x, y, d, w, t, h, rh}
  s=状态, f=帧, m=湿度, x/y=归一化坐标
  d=方向, w=天气, t=温度, h=湿度%, rh=相对湿度

macOS → ESP32 (端口 19822):
  JSON 命令: triggerAnimate, sendText, sendNotification
```

### raising-hell-cardputer
- 无网络通信，纯本地运行
- 通过 SD 卡 `.bub` 文件实现跨设备数据交换

## 九、总结

| 维度 | ClawPuter | raising-hell-cardputer |
|------|-----------|----------------------|
| 复杂度 | 中等 | 高 |
| 定位 | 桌面陪伴玩具 | 完整电子宠物模拟器 |
| 亮点 | 双端同步、天气系统 | 养成系统、进化树、自主AI |
| 适合场景 | 桌面美化 + 轻互动 | 深度养成 + 长期陪伴 |
| 可扩展性 | 易添加新精灵帧 | 易添加新宠物种类 |
