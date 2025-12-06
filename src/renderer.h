#pragma once
#include "parser.h"


void display_page(const std::string& filepath, int img_width, int img_height,
                  const std::string& transmission_medium);

std::string get_image_sequence(const std::string& filepath, int img_width, int img_height,
                  const std::string& transmission_medium);
