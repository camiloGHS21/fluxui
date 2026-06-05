// FluxUI - internal SVG rasterization entry points.
//
// The SVG software rasterizer (parser + path/shape fill/stroke) lives in
// renderer_svg.cpp. renderer.cpp's image-decode path calls rasterizeSvgToRgba()
// to turn an <svg> byte blob into RGBA pixels, so that one entry point is
// declared here and shared between the two translation units.
//
// Private to fluxui/src/render; never installed.
#pragma once
#include "fluxui/renderer.h"
#include <string>

namespace FluxUI {

// Decode an in-memory SVG document into image.pixels (RGBA8). Returns false if
// the bytes are not an SVG document. Defined in renderer_svg.cpp.
bool rasterizeSvgToRgba(const unsigned char* data, int dataSize, ImageData& image);

// True if the byte blob looks like an SVG document (has an "<svg" signature).
bool hasSvgSignature(const unsigned char* data, int dataSize);

// ASCII-lowercase a copy of the string (shared with the renderer's font path).
std::string lowerSvgString(std::string value);

} // namespace FluxUI
