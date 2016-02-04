#ifndef FCAM_DEMOSAIC_ARM_H
#define FCAM_DEMOSAIC_ARM_H
//#ifdef FCAM_ARCH_ARM

#include "halide_image.h"

// Arm-specific optimized post-processing routines

namespace FCam {
void demosaic_ARM(Halide::Tools::Image<uint16_t> input, Halide::Tools::Image<uint8_t> out, float colorTemp, float contrast, bool denoise, int blackLevel, int whiteLevel, float gamma
#ifdef FCAMLUT
        , unsigned char *lut
#endif
);

// Make a linear luminance -> pixel value lookup table
void makeLUT(float contrast, int blackLevel, int whiteLevel, float gamma, unsigned char *lut);
}

//#endif
#endif
