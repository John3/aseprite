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

#include "app/util/render.h"

#include "app/color_utils.h"
#include "app/document.h"
#include "app/ini_file.h"
#include "raster/raster.h"
#include "app/settings/document_settings.h"
#include "app/settings/settings.h"
#include "app/ui_context.h"

namespace app {

//////////////////////////////////////////////////////////////////////
// Zoomed merge

template<class DstTraits, class SrcTraits>
class BlenderHelper
{
  BLEND_COLOR m_blend_color;
  uint32_t m_mask_color;
public:
  BlenderHelper(const Image* src, const Palette* pal, int blend_mode)
  {
    m_blend_color = SrcTraits::get_blender(blend_mode);
    m_mask_color = src->getMaskColor();
  }
  inline void operator()(typename DstTraits::pixel_t& scanline,
                         const typename DstTraits::pixel_t& dst,
                         const typename SrcTraits::pixel_t& src,
                         int opacity)
  {
    if (src != m_mask_color)
      scanline = (*m_blend_color)(dst, src, opacity);
    else
      scanline = dst;
  }
};

template<>
class BlenderHelper<RgbTraits, GrayscaleTraits>
{
  BLEND_COLOR m_blend_color;
  uint32_t m_mask_color;
public:
  BlenderHelper(const Image* src, const Palette* pal, int blend_mode)
  {
    m_blend_color = RgbTraits::get_blender(blend_mode);
    m_mask_color = src->getMaskColor();
  }
  inline void operator()(RgbTraits::pixel_t& scanline,
                         const RgbTraits::pixel_t& dst,
                         const GrayscaleTraits::pixel_t& src,
                         int opacity)
  {
    if (src != m_mask_color) {
      int v = graya_getv(src);
      scanline = (*m_blend_color)(dst, rgba(v, v, v, graya_geta(src)), opacity);
    }
    else
      scanline = dst;
  }
};

template<>
class BlenderHelper<RgbTraits, IndexedTraits>
{
  const Palette* m_pal;
  int m_blend_mode;
  uint32_t m_mask_color;
public:
  BlenderHelper(const Image* src, const Palette* pal, int blend_mode)
  {
    m_blend_mode = blend_mode;
    m_mask_color = src->getMaskColor();
    m_pal = pal;
  }
  inline void operator()(RgbTraits::pixel_t& scanline,
                         const RgbTraits::pixel_t& dst,
                         const IndexedTraits::pixel_t& src,
                         int opacity)
  {
    if (m_blend_mode == BLEND_MODE_COPY) {
      scanline = m_pal->getEntry(src);
    }
    else {
      if (src != m_mask_color) {
        scanline = rgba_blend_normal(dst, m_pal->getEntry(src), opacity);
      }
      else
        scanline = dst;
    }
  }
};

template<class DstTraits, class SrcTraits>
static void merge_zoomed_image(Image* dst, const Image* src, const Palette* pal,
                               int x, int y, int opacity,
                               int blend_mode, int zoom)
{
  BlenderHelper<DstTraits, SrcTraits> blender(src, pal, blend_mode);
  int src_x, src_y, src_w, src_h;
  int dst_x, dst_y, dst_w, dst_h;
  int box_x, box_y, box_w, box_h;
  int first_box_w, first_box_h;
  int line_h, bottom;

  box_w = 1<<zoom;
  box_h = 1<<zoom;

  src_x = 0;
  src_y = 0;
  src_w = src->getWidth();
  src_h = src->getHeight();

  dst_x = x;
  dst_y = y;
  dst_w = src->getWidth()<<zoom;
  dst_h = src->getHeight()<<zoom;

  // clipping...
  if (dst_x < 0) {
    src_x += (-dst_x)>>zoom;
    src_w -= (-dst_x)>>zoom;
    dst_w -= (-dst_x);
    first_box_w = box_w - ((-dst_x) % box_w);
    dst_x = 0;
  }
  else
    first_box_w = 0;

  if (dst_y < 0) {
    src_y += (-dst_y)>>zoom;
    src_h -= (-dst_y)>>zoom;
    dst_h -= (-dst_y);
    first_box_h = box_h - ((-dst_y) % box_h);
    dst_y = 0;
  }
  else
    first_box_h = 0;

  if (dst_x+dst_w > dst->getWidth()) {
    src_w -= (dst_x+dst_w-dst->getWidth()) >> zoom;
    dst_w = dst->getWidth() - dst_x;
  }

  if (dst_y+dst_h > dst->getHeight()) {
    src_h -= (dst_y+dst_h-dst->getHeight()) >> zoom;
    dst_h = dst->getHeight() - dst_y;
  }

  if ((src_w <= 0) || (src_h <= 0) ||
      (dst_w <= 0) || (dst_h <= 0))
    return;

  bottom = dst_y+dst_h-1;

  // the scanline variable is used to blend src/dst pixels one time for each pixel
  typedef std::vector<typename DstTraits::pixel_t> Scanline;
  Scanline scanline(src_w);
  typename Scanline::iterator scanline_it;
  typename Scanline::iterator scanline_end = scanline.end();

  // Lock all necessary bits
  const LockImageBits<SrcTraits> srcBits(src, gfx::Rect(src_x, src_y, src_w, src_h));
  LockImageBits<DstTraits> dstBits(dst, gfx::Rect(dst_x, dst_y, dst_w, dst_h));
  typename LockImageBits<SrcTraits>::const_iterator src_it = srcBits.begin(), src_end = srcBits.end();
  typename LockImageBits<DstTraits>::iterator dst_it, dst_end;

  // For each line to draw of the source image...
  for (y=0; y<src_h; ++y) {
    dst_it = dstBits.begin_area(gfx::Rect(dst_x, dst_y, dst_w, 1));
    dst_end = dstBits.end_area(gfx::Rect(dst_x, dst_y, dst_w, 1));

    // Read 'src' and 'dst' and blend them, put the result in `scanline'
    scanline_it = scanline.begin();
    for (x=0; x<src_w; ++x) {
      ASSERT(src_it >= srcBits.begin() && src_it < src_end);
      ASSERT(dst_it >= dstBits.begin() && dst_it < dst_end);
      ASSERT(scanline_it >= scanline.begin() && scanline_it < scanline_end);

      blender(*scanline_it, *dst_it, *src_it, opacity);

      ++src_it;

      int delta;
      if ((x == 0) && (first_box_w > 0))
        delta = first_box_w;
      else
        delta = box_w;

      while (dst_it != dst_end && delta-- > 0)
        ++dst_it;

      ++scanline_it;
    }

    // Get the 'height' of the line to be painted in 'dst'
    if ((y == 0) && (first_box_h > 0))
      line_h = first_box_h;
    else
      line_h = box_h;

    // Draw the line in 'dst'
    for (box_y=0; box_y<line_h; ++box_y) {
      dst_it = dstBits.begin_area(gfx::Rect(dst_x, dst_y, dst_w, 1));
      dst_end = dstBits.end_area(gfx::Rect(dst_x, dst_y, dst_w, 1));
      scanline_it = scanline.begin();

      x = 0;

      // first pixel
      if (first_box_w > 0) {
        for (box_x=0; box_x<first_box_w; ++box_x) {
          ASSERT(scanline_it != scanline_end);
          ASSERT(dst_it != dst_end);

          *dst_it = *scanline_it;

          ++dst_it;
          if (dst_it == dst_end)
            goto done_with_line;
        }

        ++scanline_it;
        ++x;
      }

      // the rest of the line
      for (; x<src_w; ++x) {
        for (box_x=0; box_x<box_w; ++box_x) {
          ASSERT(dst_it !=  dst_end);

          *dst_it = *scanline_it;

          ++dst_it;
          if (dst_it == dst_end)
            goto done_with_line;
        }

        ++scanline_it;
      }

done_with_line:;
      if (++dst_y > bottom)
        goto done_with_blit;
    }

    // go to the next line in the source image
    ++src_y;
  }

done_with_blit:;
}

//////////////////////////////////////////////////////////////////////
// Render Engine

static RenderEngine::CheckedBgType checked_bg_type;
static bool checked_bg_zoom;
static app::Color checked_bg_color1;
static app::Color checked_bg_color2;

static int global_opacity = 255;
static const Layer* selected_layer = NULL;
static Image* rastering_image = NULL;

// static
void RenderEngine::loadConfig()
{
  checked_bg_type = (CheckedBgType)get_config_int("Options", "CheckedBgType",
                                                  (int)RenderEngine::CHECKED_BG_16X16);
  checked_bg_zoom = get_config_bool("Options", "CheckedBgZoom", true);
  checked_bg_color1 = get_config_color("Options", "CheckedBgColor1", app::Color::fromRgb(128, 128, 128));
  checked_bg_color2 = get_config_color("Options", "CheckedBgColor2", app::Color::fromRgb(192, 192, 192));
}

// static
RenderEngine::CheckedBgType RenderEngine::getCheckedBgType()
{
  return checked_bg_type;
}

// static
void RenderEngine::setCheckedBgType(CheckedBgType type)
{
  checked_bg_type = type;
  set_config_int("Options", "CheckedBgType", (int)type);
}

// static
bool RenderEngine::getCheckedBgZoom()
{
  return checked_bg_zoom;
}

// static
void RenderEngine::setCheckedBgZoom(bool state)
{
  checked_bg_zoom = state;
  set_config_bool("Options", "CheckedBgZoom", state);
}

// static
app::Color RenderEngine::getCheckedBgColor1()
{
  return checked_bg_color1;
}

// static
void RenderEngine::setCheckedBgColor1(const app::Color& color)
{
  checked_bg_color1 = color;
  set_config_color("Options", "CheckedBgColor1", color);
}

// static
app::Color RenderEngine::getCheckedBgColor2()
{
  return checked_bg_color2;
}

// static
void RenderEngine::setCheckedBgColor2(const app::Color& color)
{
  checked_bg_color2 = color;
  set_config_color("Options", "CheckedBgColor2", color);
}

//////////////////////////////////////////////////////////////////////

RenderEngine::RenderEngine(const Document* document,
                           const Sprite* sprite,
                           const Layer* currentLayer,
                           FrameNumber currentFrame)
  : m_document(document)
  , m_sprite(sprite)
  , m_currentLayer(currentLayer)
  , m_currentFrame(currentFrame)
{
}

// static
void RenderEngine::setPreviewImage(const Layer* layer, Image* image)
{
  selected_layer = layer;
  rastering_image = image;
}

/**
   Draws the @a frame of animation of the specified @a sprite
   in a new image and return it.

   Positions source_x, source_y, width and height must have the
   zoom applied (sorce_x<<zoom, source_y<<zoom, width<<zoom, etc.)
 */
Image* RenderEngine::renderSprite(int source_x, int source_y,
                                  int width, int height,
                                  FrameNumber frame, int zoom,
                                  bool draw_tiled_bg)
{
  void (*zoomed_func)(Image*, const Image*, const Palette*, int, int, int, int, int);
  const LayerImage* background = m_sprite->getBackgroundLayer();
  bool need_checked_bg = (background != NULL ? !background->isReadable(): true);
  uint32_t bg_color = 0;
  Image *image;

  switch (m_sprite->getPixelFormat()) {

    case IMAGE_RGB:
      zoomed_func = merge_zoomed_image<RgbTraits, RgbTraits>;
      break;

    case IMAGE_GRAYSCALE:
      zoomed_func = merge_zoomed_image<RgbTraits, GrayscaleTraits>;
      break;

    case IMAGE_INDEXED:
      zoomed_func = merge_zoomed_image<RgbTraits, IndexedTraits>;
      if (!need_checked_bg)
        bg_color = m_sprite->getPalette(frame)->getEntry(m_sprite->getTransparentColor());
      break;

    default:
      return NULL;
  }

  // Create a temporary RGB bitmap to draw all to it
  image = Image::create(IMAGE_RGB, width, height);
  if (!image)
    return NULL;

  // Draw checked background
  if (need_checked_bg && draw_tiled_bg)
    renderCheckedBackground(image, source_x, source_y, zoom);
  else
    clear_image(image, bg_color);

  // Onion-skin feature: draw the previous frame
  IDocumentSettings* docSettings = UIContext::instance()
    ->getSettings()->getDocumentSettings(m_document);

  if (docSettings->getUseOnionskin()) {
    // Draw background layer of the current frame with opacity=255
    global_opacity = 255;
    renderLayer(m_sprite->getFolder(), image,
                source_x, source_y, frame, zoom, zoomed_func,
                true, false);

    // Draw transparent layers of the previous/next frames with different opacity (<255) (it is the onion-skinning)
    {
      int prevs = docSettings->getOnionskinPrevFrames();
      int nexts = docSettings->getOnionskinNextFrames();
      int opacity_base = docSettings->getOnionskinOpacityBase();
      int opacity_step = docSettings->getOnionskinOpacityStep();

      for (FrameNumber f=frame.previous(prevs); f <= frame.next(nexts); ++f) {
        if (f == frame || f < 0 || f > m_sprite->getLastFrame())
          continue;
        else if (f < frame)
          global_opacity = opacity_base - opacity_step * ((frame - f)-1);
        else
          global_opacity = opacity_base - opacity_step * ((f - frame)-1);

        if (global_opacity > 0)
          renderLayer(m_sprite->getFolder(), image,
                      source_x, source_y, f, zoom, zoomed_func,
                      false, true);
      }
    }

    // Draw transparent layers of the current frame with opacity=255
    global_opacity = 255;
    renderLayer(m_sprite->getFolder(), image,
                source_x, source_y, frame, zoom, zoomed_func,
                false, true);
  }
  // Onion-skin is disabled: just draw the current frame
  else {
    renderLayer(m_sprite->getFolder(), image,
                source_x, source_y, frame, zoom, zoomed_func,
                true, true);
  }

  return image;
}

// static
void RenderEngine::renderCheckedBackground(Image* image,
                                           int source_x, int source_y,
                                           int zoom)
{
  int x, y, u, v;
  int tile_w = 16;
  int tile_h = 16;
  int c1 = color_utils::color_for_image(checked_bg_color1, image->getPixelFormat());
  int c2 = color_utils::color_for_image(checked_bg_color2, image->getPixelFormat());

  switch (checked_bg_type) {

    case CHECKED_BG_16X16:
      tile_w = 16;
      tile_h = 16;
      break;

    case CHECKED_BG_8X8:
      tile_w = 8;
      tile_h = 8;
      break;

    case CHECKED_BG_4X4:
      tile_w = 4;
      tile_h = 4;
      break;

    case CHECKED_BG_2X2:
      tile_w = 2;
      tile_h = 2;
      break;

  }

  if (checked_bg_zoom) {
    tile_w <<= zoom;
    tile_h <<= zoom;
  }

  // Tile size
  if (tile_w < (1<<zoom)) tile_w = (1<<zoom);
  if (tile_h < (1<<zoom)) tile_h = (1<<zoom);

  // Tile position (u,v) is the number of tile we start in (source_x,source_y) coordinate
  u = (source_x / tile_w);
  v = (source_y / tile_h);

  // Position where we start drawing the first tile in "image"
  int x_start = -(source_x % tile_w);
  int y_start = -(source_y % tile_h);

  // Draw checked background (tile by tile)
  int u_start = u;
  for (y=y_start-tile_h; y<image->getHeight()+tile_h; y+=tile_h) {
    for (x=x_start-tile_w; x<image->getWidth()+tile_w; x+=tile_w) {
      fill_rect(image, x, y, x+tile_w-1, y+tile_h-1,
                (((u+v))&1)? c1: c2);
      ++u;
    }
    u = u_start;
    ++v;
  }
}

// static
void RenderEngine::renderImage(Image* rgb_image, Image* src_image, const Palette* pal,
                               int x, int y, int zoom)
{
  void (*zoomed_func)(Image*, const Image*, const Palette*, int, int, int, int, int);

  ASSERT(rgb_image->getPixelFormat() == IMAGE_RGB && "renderImage accepts RGB destination images only");

  switch (src_image->getPixelFormat()) {

    case IMAGE_RGB:
      zoomed_func = merge_zoomed_image<RgbTraits, RgbTraits>;
      break;

    case IMAGE_GRAYSCALE:
      zoomed_func = merge_zoomed_image<RgbTraits, GrayscaleTraits>;
      break;

    case IMAGE_INDEXED:
      zoomed_func = merge_zoomed_image<RgbTraits, IndexedTraits>;
      break;

    default:
      return;
  }

  (*zoomed_func)(rgb_image, src_image, pal, x, y, 255, BLEND_MODE_NORMAL, zoom);
}

void RenderEngine::renderLayer(const Layer* layer,
                               Image *image,
                               int source_x, int source_y,
                               FrameNumber frame, int zoom,
                               void (*zoomed_func)(Image*, const Image*, const Palette*, int, int, int, int, int),
                               bool render_background,
                               bool render_transparent)
{
  // we can't read from this layer
  if (!layer->isReadable())
    return;

  switch (layer->type()) {

    case OBJECT_LAYER_IMAGE: {
      if ((!render_background  &&  layer->isBackground()) ||
          (!render_transparent && !layer->isBackground()))
        break;

      const Cel* cel = static_cast<const LayerImage*>(layer)->getCel(frame);
      if (cel != NULL) {
        Image* src_image;

        // Is the 'rastering_image' set to be used with this layer?
        if ((frame == m_currentFrame) &&
            (selected_layer == layer) &&
            (rastering_image != NULL)) {
          src_image = rastering_image;
        }
        // If not, we use the original cel-image from the images' stock
        else if ((cel->getImage() >= 0) &&
                 (cel->getImage() < m_sprite->getStock()->size()))
          src_image = m_sprite->getStock()->getImage(cel->getImage());
        else
          src_image = NULL;

        if (src_image) {
          int output_opacity;
          register int t;

          output_opacity = MID(0, cel->getOpacity(), 255);
          output_opacity = INT_MULT(output_opacity, global_opacity, t);

          src_image->setMaskColor(m_sprite->getTransparentColor());

          (*zoomed_func)(image, src_image, m_sprite->getPalette(frame),
                         (cel->getX() << zoom) - source_x,
                         (cel->getY() << zoom) - source_y,
                         output_opacity,
                         static_cast<const LayerImage*>(layer)->getBlendMode(), zoom);
        }
      }
      break;
    }

    case OBJECT_LAYER_FOLDER: {
      LayerConstIterator it = static_cast<const LayerFolder*>(layer)->getLayerBegin();
      LayerConstIterator end = static_cast<const LayerFolder*>(layer)->getLayerEnd();

      for (; it != end; ++it) {
        renderLayer(*it, image,
                    source_x, source_y,
                    frame, zoom, zoomed_func,
                    render_background,
                    render_transparent);
      }
      break;
    }

  }

  // Draw extras
  if (layer == m_currentLayer &&
      m_document->getExtraCel() != NULL) {
    Cel* extraCel = m_document->getExtraCel();
    if (extraCel->getOpacity() > 0) {
      Image* extraImage = m_document->getExtraCelImage();

      (*zoomed_func)(image, extraImage, m_sprite->getPalette(frame),
                     (extraCel->getX() << zoom) - source_x,
                     (extraCel->getY() << zoom) - source_y,
                     extraCel->getOpacity(), BLEND_MODE_NORMAL, zoom);
    }
  }
}

} // namespace app
