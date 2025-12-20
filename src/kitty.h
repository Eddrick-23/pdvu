#pragma once
#include "parser.h"

std::string get_image_sequence(const std::string& filepath,
                               int img_width, int img_height,
                               const std::string& transmission_medium);

std::string get_dim_layer(int term_width, int term_height);

std::string clear_dim_layer();
