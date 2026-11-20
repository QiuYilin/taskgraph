#pragma once

// UI 无关的基础几何与颜色类型；两个前端各自映射到自己的渲染类型。

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;

  constexpr Vec2() = default;
  constexpr Vec2(float x_value, float y_value) : x(x_value), y(y_value) {}
};

struct Color {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 1.0f;

  constexpr Color() = default;
  constexpr Color(float r_value, float g_value, float b_value,
                  float a_value = 1.0f)
      : r(r_value), g(g_value), b(b_value), a(a_value) {}
};
