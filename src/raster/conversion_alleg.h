/* Aseprite
 * Copyright (C) 2001-2013  David Capello
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef RASTER_CONVERSION_ALLEG_H_INCLUDED
#define RASTER_CONVERSION_ALLEG_H_INCLUDED
#pragma once

#include <allegro/color.h>

struct BITMAP;

namespace raster {
  class Image;
  class Palette;

  void convert_image_to_allegro(const Image* image, BITMAP* bmp, int x, int y, const Palette* palette);
  void convert_palette_to_allegro(const Palette* palette, RGB* rgb);
  void convert_palette_from_allegro(const RGB* rgb, Palette* palette);

} // namespace raster

#endif
