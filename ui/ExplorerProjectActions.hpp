#pragma once

struct ExplorerProjectActionBand {
  float content_min_x = 0.0f;
  float content_max_x = 0.0f;
  float row_min_y = 0.0f;
  float row_max_y = 0.0f;

  bool contains(float mouse_x, float mouse_y) const {
    return mouse_x >= content_min_x && mouse_x <= content_max_x &&
           mouse_y >= row_min_y && mouse_y <= row_max_y;
  }
};

inline bool ShouldShowExplorerProjectActions(bool mouse_in_band,
                                             bool mouse_down,
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
