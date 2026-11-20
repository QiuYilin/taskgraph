#pragma once

#include <QColor>

#include "Types.hpp"

inline constexpr float kColorChannelMax = 255.0f;
inline constexpr int kColorChannelMaxInt = 255;
inline constexpr float kColorRounding = 0.5f;

// 把中性 Color 转成 QColor，供 Qt 前端用主题 token 上色。
inline QColor ToQColor(const Color& color) {
  auto channel = [](float value) {
    const int scaled =
        static_cast<int>(value * kColorChannelMax + kColorRounding);
    return scaled < 0 ? 0
                      : (scaled > kColorChannelMaxInt ? kColorChannelMaxInt
                                                      : scaled);
  };
  return QColor(channel(color.r), channel(color.g), channel(color.b),
                channel(color.a));
}
