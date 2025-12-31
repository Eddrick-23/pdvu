#pragma once
#include "parser.h"
namespace kitty {
  std::string get_image_sequence(const std::string &filepath, int img_id,
                                 int img_width, int img_height,
                                 int x_offset_pixels, int y_offset_pixels,
                                 int crop_rect_width, int crop_rect_height,
                                 const std::string &transmission_medium,
                                 bool transmit);

  std::string delete_image_placement();

  std::string get_dim_layer(int term_width, int term_height);
  std::string clear_dim_layer();
}
