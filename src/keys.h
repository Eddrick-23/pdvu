#pragma once
enum Key {
  key_none = 0,
  key_char, // set char value only for this
  key_escape,
  key_enter,
  key_tab,
  key_delete,
  key_backspace,
  key_up_arrow,
  key_down_arrow,
  key_right_arrow,
  key_left_arrow,
  key_alt_backspace,
  key_alt_char,
  key_ctrl_char,
};

struct InputEvent {
  Key key;         // enum value
  char char_value; // ascii value of key (valid for key_char only
};