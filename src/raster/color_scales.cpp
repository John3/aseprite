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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "base/clamp.h"

// Based on Allegro _rgb_scale_5 and _rgb_scale_6 tables

namespace raster {

int scale_5bits_to_8bits(int channel5bits)
{
  static int scale[32] = {
    0,   8,   16,  24,  33,  41,  49,  57,
    66,  74,  82,  90,  99,  107, 115, 123,
    132, 140, 148, 156, 165, 173, 181, 189,
    198, 206, 214, 222, 231, 239, 247, 255
  };

  ASSERT(channel5bits >= 0);
  ASSERT(channel5bits < 32);
  return scale[channel5bits];
}

int scale_6bits_to_8bits(int channel6bits)
{
  static int scale[64] = {
    0,   4,   8,   12,  16,  20,  24,  28,
    32,  36,  40,  44,  48,  52,  56,  60,
    65,  69,  73,  77,  81,  85,  89,  93,
    97,  101, 105, 109, 113, 117, 121, 125,
    130, 134, 138, 142, 146, 150, 154, 158,
    162, 166, 170, 174, 178, 182, 186, 190,
    195, 199, 203, 207, 211, 215, 219, 223,
    227, 231, 235, 239, 243, 247, 251, 255
  };
  ASSERT(channel6bits >= 0);
  ASSERT(channel6bits < 64);
  return scale[channel6bits];
}

} // namespace raster
