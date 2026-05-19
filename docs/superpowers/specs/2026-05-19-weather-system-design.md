# ClawPuter 天气系统迁移设计

## 目标

将 ClawPuter 的天气系统移植到 zedpet，支持 w 键循环切换 8 种天气，纯本地模拟，包含背景、粒子、天体、配件渲染。

## 新增文件

```
src/weather.h       # WeatherType + WeatherData + AccessoryType + 常量
src/weather.cpp     # 粒子管理 + 背景/粒子/天体/配件绘制函数
```

## 修改文件

```
src/main.cpp        # w 键处理：切换天气类型
src/pet.cpp         # update() 渲染顺序调整，先画天气层再画 UI+角色
```

## WeatherType 枚举

```cpp
enum class WeatherType : uint8_t {
    CLEAR,          // 0
    PARTLY_CLOUDY,  // 1
    OVERCAST,       // 2
    FOG,            // 3
    DRIZZLE,        // 4
    RAIN,           // 5
    SNOW,           // 6
    THUNDER,        // 7
    WEATHER_COUNT   // 8
};
```

w 键每次切换到 `(current + 1) % WEATHER_COUNT`。

## AccessoryType 枚举

```cpp
enum class AccessoryType : uint8_t {
    NONE, SUNGLASSES, UMBRELLA, SNOW_HAT, MASK
};
```

| 天气 | 配件 |
|------|------|
| CLEAR / PARTLY_CLOUDY | SUNGLASSES |
| RAIN / DRIZZLE / THUNDER | UMBRELLA |
| SNOW | SNOW_HAT |
| FOG / OVERCAST | MASK |

## WeatherData 结构

```cpp
struct WeatherData {
    WeatherType type = WeatherType::CLEAR;
    bool valid = true;
};
```

简化为只保留 type，不包含温度、湿度等网络数据。

## 天气模块 API

```cpp
// 初始化
void weatherBegin();
// 切换天气类型（w 键触发）
void weatherNext();
// 绘制完整天气层（背景 + 天体 + 粒子 + 地面）
void weatherDraw(M5Canvas& canvas);
// 获取当前天气配件
AccessoryType weatherGetAccessory();
// 获取天气名称
const char* weatherTypeName();
```

## 渲染分层

pet.cpp `update()` 中渲染顺序：

```
1. 天空背景   — weatherDrawBackground()
    白天(6-17): 蓝
    黄昏(17-19): 橙
    夜晚(19-6): 深蓝黑
    + 天气色调混合

2. 天体       — weatherDrawSkyElements()
    白天: 太阳(带光线) + 2朵云
    黄昏: 落日
    夜晚: 月牙 + 12颗闪烁星星
    恶劣天气: 隐藏天体，画暗云

3. 天气粒子   — weatherDrawParticles()
    RAIN/DRIZZLE: 雨滴竖线
    SNOW: 雪花飘落
    FOG: 点阵雾
    THUNDER: 雨滴 + 闪电闪光

4. 地面       — weatherDrawGround()
    白天: 绿色草地 + 草丛
    夜间: 深色地面 + 草丛

5. UI 层      — 状态名 + 时间 + WiFi IP

6. 宠物层     — 精灵 + 位移 + 配件叠加
```

## 天气粒子

| 天气 | 粒子数 | 速度 | 效果 |
|------|--------|------|------|
| RAIN | 15 | 5px/帧 | 蓝色竖线 5px 长 |
| DRIZZLE | 8 | 3px/帧 | 蓝色竖线 3px 长 |
| SNOW | 15 | 1px/帧 | 白点飘落，漂移±1 |
| FOG | 无固定 | - | 4px间隔点阵，时间偏移 |
| THUNDER | 15 | 5px/帧 | 雨滴 + 每3-5s白屏50ms |

## 天文天体

- **太阳**: 屏幕右上角(x=200,y=18)，10px圆 + 8条光线
- **月亮**: 左上角(x=30,y=20)，10px圆 + 偏移挖空形成月牙
- **星星**: 12颗随机x/y，800ms切换可见性，1/3为十字星
- **云朵**: 2朵白色圆角矩形，恶劣天气时改为暗色

## 天空颜色常量

```cpp
SKY_DAY    = rgb565(60, 120, 200)   // 蓝
SKY_SUNSET = rgb565(180, 80, 60)    // 橙
SKY_NIGHT  = rgb565(10, 10, 30)     // 深蓝黑
GROUND_DAY = rgb565(80, 140, 60)    // 绿草地
```

## w 键处理

```cpp
// main.cpp loop() 中
auto ks = M5Cardputer.Keyboard.keysState();
bool wDown = std::find(ks.word.begin(), ks.word.end(), 'w') != ks.word.end();
if (wDown && !wWasDown) {
    weatherNext();
}
wWasDown = wDown;
```

## 不迁移内容

- 真实天气 API 调用
- 「时间旅行」偏移（displayHour）
- 湿度系统
- 底部模拟状态栏
- 温度/湿度百分比显示

## 屏幕布局

```
┌──────────────────────────────────────────┐
│  ☀️ ☁        °                    192.x │  ← 天气层（天空+天体）
│  · · · · ·  · · ·      (可能下雨/雪)     │  ← 粒子层
│ ─────────────────────────────────────── │  ← 地面线
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │  ← 地面
│                                          │
│  IDLE         14:30        192.168.1.23 │  ← UI 层
│              ┌──────┐                    │  ← 角色层
│              │ 🦞🕶 │                    │
│              └──────┘                    │
└──────────────────────────────────────────┘
```

## 实现要点

1. `weather.cpp` 是纯渲染模块，不依赖 Pet 类
2. 粒子数组用 static 变量，惰性初始化
3. 天空混色用 `blendRGB565()` 工具函数
4. 配件用 `fillRect`/`fillCircle` 几何图形，不新增精灵帧
5. 天气绘制在 pet 的 `M5Canvas` 上，不额外分配画布
