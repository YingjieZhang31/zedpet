# IMU Tilt Physics for Pet Offset

**Date**: 2026-05-24
**Status**: accepted

## Goal

Use the M5Cardputer's internal IMU to move the on-screen pet character by tilting the device, with simulated acceleration/deceleration (physics-based marble-on-plane model).

## Non-Goals

- Only IDLE state responds to tilt (extensible to other states via one-line change)
- Does not affect Claude UI mode
- Does not change pet sprite rendering beyond position offset

## Physics Model

```
Tilt angle (pitch, roll) → acceleration a = SENSITIVITY × sin(θ)
                         → velocity v += a × dt
                         → v *= (1 - DAMPING × dt)   // friction each frame
                         → position p += v × dt
                         → boundary elastic bounce: v *= -BOUNCE on edge hit
```

### Tunable Parameters (in config.h)

| Parameter | Default | Meaning |
|-----------|---------|---------|
| `IMU_SENSITIVITY` | 3.0 | Tilt-to-acceleration gain. Higher = pet moves faster for same tilt |
| `IMU_DAMPING` | 2.5 | Per-second friction factor. Higher = pet stops sooner when device is flat |
| `IMU_BOUNCE` | 0.5 | Velocity multiplier on screen-edge collision. 0 = dead stop, 1 = perfect bounce |
| `IMU_DEAD_ZONE` | 0.03 | Ignore tilt below this sin(θ) threshold to avoid drift from sensor noise |
| `IMU_VELOCITY_CAP` | 120.0 | Maximum velocity (px/s) to prevent teleporting at extreme tilts |

All parameters are `constexpr float` in config.h — change via code edit and recompile.

### IMU Mapping

- **Roll** (left/right tilt) → X-axis acceleration
- **Pitch** (forward/back tilt) → Y-axis acceleration
- IMU values are zero-calibrated at `begin()` time: `calibration = first reading`, effective angle = `current - calibration`

### Frame-Independent Physics

All calculations use `dt` (seconds since last frame) so behavior is consistent regardless of framerate. `dt` is clamped to 0.1s max to prevent large jumps after lag spikes.

## Architecture

### New internal state in Pet class

```cpp
struct ImuState {
    float posX = 0, posY = 0;    // current offset from center (px)
    float velX = 0, velY = 0;    // velocity (px/s)
    float calibRoll = 0;         // calibration baseline
    float calibPitch = 0;
    bool calibrated = false;
};
```

### Boundary collision

The pet sprite is 48×48 px (CHAR_DRAW_W × CHAR_DRAW_H). The drawable area is (0,0) to (SCREEN_W, SCREEN_H). Boundary checks use the sprite edges:

```
minX = 0 - SPRITE_X          // can't go past left edge
maxX = SCREEN_W - SPRITE_X - CHAR_DRAW_W  // can't go past right edge
minY = 0 - SPRITE_Y
maxY = SCREEN_H - SPRITE_Y - CHAR_DRAW_W
```

On edge hit: `vel *= -BOUNCE` (elastic) and position clamped to boundary.

### State transitions

- Enter IDLE: physics active (resumes from current offset if returning to IDLE)
- Leave IDLE: offset and velocity reset to 0 (pet snaps back to center)
- Other states: no physics update, offset stays at 0

To enable tilt for another state, add one line in `drawCharacter()` offset logic.

## Files Changed

| File | Change |
|------|--------|
| `src/config.h` | Add IMU physics parameters |
| `src/pet.h` | Add `ImuState` member, `updatePhysics()` declaration |
| `src/pet.cpp` | Add physics update loop, modify `drawCharacter()` to use offset, modify `update()` to call physics in IDLE |

## Testing

- Build and flash to M5Cardputer
- Tilt left/right: pet slides horizontally with acceleration feel
- Tilt forward/back: pet slides vertically
- Place flat: pet decelerates and stops (does NOT return to center)
- Tilt hard into edge: pet bounces with reduced velocity
- Switch to non-IDLE state (press Q): pet snaps to center
- Switch back to IDLE: physics resumes
