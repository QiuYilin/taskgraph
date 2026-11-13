#pragma once

/** \brief Explorer 标题行动作显隐（纯几何，不依赖 ImGui HoveredWindow）。
 *  Tooltip 会抢走 IsWindowHovered，若只靠它会导致按钮卸掉、点击丢失。 */
struct ExplorerHeaderActionBand {
  float content_min_x = 0.0f;
  float content_max_x = 0.0f;
  float header_min_y = 0.0f;
  float header_max_y = 0.0f;

  bool contains(float mouse_x, float mouse_y) const {
    return mouse_x >= content_min_x && mouse_x <= content_max_x &&
           mouse_y >= header_min_y && mouse_y <= header_max_y;
  }
};

/** \brief 是否画出 + Node / + Graph。按下期间保持显示，避免松键前一帧卸掉控件。 */
inline bool ShouldShowExplorerHeaderActions(bool mouse_in_band, bool mouse_down,
                                           bool& hold_while_pressed) {
  if (mouse_in_band) {
    if (mouse_down) {
      hold_while_pressed = true;
    }
    return true;
  }
  if (hold_while_pressed && mouse_down) {
    return true;
  }
  hold_while_pressed = false;
  return false;
}
