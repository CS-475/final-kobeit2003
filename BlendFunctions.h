#ifndef BLEND_UTILS_H
#define BLEND_UTILS_H


#include "include/GPixel.h"
#include "include/GBlendMode.h"


GPixel clear_mode(GPixel src, GPixel dst);
GPixel src_mode(GPixel src, GPixel dst);
GPixel dst_mode(GPixel src, GPixel dst);
GPixel src_ov_mode(GPixel src, GPixel dst);
GPixel dst_ov_mode(GPixel src, GPixel dst);
GPixel src_in_mode(GPixel src, GPixel dst);
GPixel dst_in_mode(GPixel src, GPixel dst);
GPixel src_out_mode(GPixel src, GPixel dst);
GPixel dst_out_mode(GPixel src, GPixel dst);
GPixel src_a_top_mode(GPixel src, GPixel dst);
GPixel dst_a_top_mode(GPixel src, GPixel dst);
GPixel xor_mode(GPixel src, GPixel dst);


using BlendProc = GPixel (*)(GPixel src, GPixel dst);
BlendProc findBlend(GBlendMode mode);


#endif