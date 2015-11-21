#include "fcam/Demosaic.h"
#include "fcam/Demosaic_ARM.h"

#include "benchmark.h"
#include "curved.h"
#include "halide_image.h"
#include "halide_image_io.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cassert>

#if defined(__hexagon__)
#include "hexagon_standalone.h"
#include "io.h"
#define IMGEXT_IN ".pgm"
#define IMGEXT    ".ppm"
#else
#define IMGEXT_IN ".png"
#define IMGEXT    ".png"
#endif

using namespace Halide::Tools;

int main(int argc, char **argv) {
    if (argc < 7) {
        printf("Usage: ./process raw" IMGEXT_IN " color_temp gamma contrast timing_iterations output" IMGEXT "\n"
               "e.g. ./process raw" IMGEXT_IN " 3200 2 50 5 output" IMGEXT);
        return 0;
    }

    fprintf(stderr, "input: %s\n", argv[1]);
    Image<uint16_t> input = load_image(argv[1]);
    fprintf(stderr, "       %d %d\n", input.width(), input.height());
    // save_image(input, "input.pgm");
    // fprintf(stderr, "       input.pgm\n");

    Image<uint8_t> output(((input.width() - 32)/32)*32, ((input.height() - 48)/32)*32, 3);
    // The ref output will have a width that is a multiple of 40, and a height
    // which is a multiple of 24.
    Image<uint8_t> outref(((input.width() - 32)/40)*40, ((input.height() - 48)/24)*24, 3);

    // These color matrices are for the sensor in the Nokia N900 and are
    // taken from the FCam source.
    float _matrix_3200[][4] = {{ 1.6697f, -0.2693f, -0.4004f, -42.4346f},
                                {-0.3576f,  1.0615f,  1.5949f, -37.1158f},
                                {-0.2175f, -1.8751f,  6.9640f, -26.6970f}};

    float _matrix_7000[][4] = {{ 2.2997f, -0.4478f,  0.1706f, -39.0923f},
                                {-0.3826f,  1.5906f, -0.2080f, -25.4311f},
                                {-0.0888f, -0.7344f,  2.2832f, -20.0826f}};
    Image<float> matrix_3200(4, 3), matrix_7000(4, 3);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            matrix_3200(j, i) = _matrix_3200[i][j];
            matrix_7000(j, i) = _matrix_7000[i][j];
        }
    }

    float color_temp = atof(argv[2]);
    float gamma = atof(argv[3]);
    float contrast = atof(argv[4]);
    int timing_iterations = atoi(argv[5]);

    double best;

#if defined(__hexagon__)
    SIM_ACQUIRE_HVX;
#if LOG2VLEN == 7
    SIM_SET_HVX_DOUBLE_MODE;
#endif
#endif

#ifdef PCYCLES
    RESET_PMU();
    long long start_time = READ_PCYCLES();
#endif
    best = benchmark(timing_iterations, 1, [&]() {
        curved(color_temp, gamma, contrast,
               input, matrix_3200, matrix_7000, output);
    });
#ifdef PCYCLES
    long long total_cycles = READ_PCYCLES() - start_time;
    DUMP_PMU();
    fprintf(stderr, "Halide:\t%0.4f cycles/pixel\n",
            (float)total_cycles/output.height()/output.width()/timing_iterations);
#else
    fprintf(stderr, "Halide:\t%gus\n", best * 1e6);
#endif
    fprintf(stderr, "output: %s\n", argv[6]);
    save_image(output, argv[6]);
    fprintf(stderr, "        %d %d\n", output.width(), output.height());

#if defined(__hexagon__)
    SIM_RELEASE_HVX;
#if DEBUG
    printf ("Done calling the halide func. and released the vector context\n");
#endif
#endif

#ifdef PCYCLES
    RESET_PMU();
    start_time = READ_PCYCLES();
#endif
    best = benchmark(timing_iterations, 1, [&]() {
        FCam::demosaic(input, outref, color_temp, contrast, true, 25, gamma);
    });
#ifdef PCYCLES
    total_cycles = READ_PCYCLES() - start_time;
    DUMP_PMU();
    fprintf(stderr, "C++:\t%0.4f cycles/pixel\n",
            (float)total_cycles/outref.height()/outref.width()/timing_iterations);
#else
    fprintf(stderr, "C++:\t%gus\n", best * 1e6);
#endif
    fprintf(stderr, "outref: fcam_c" IMGEXT "\n");
    save_image(outref, "fcam_c" IMGEXT);
    fprintf(stderr, "        %d %d\n", outref.width(), outref.height());

#if not defined(__hexagon__)
    Image<uint8_t> outarm(((input.width() - 32)/40)*40, ((input.height() - 48)/24)*24, 3);
    best = benchmark(timing_iterations, 1, [&]() {;
        FCam::demosaic_ARM(input, outarm, color_temp, contrast, true, 25, gamma);
    });
    fprintf(stderr, "ASM:\t%gus\n", best * 1e6);
    fprintf(stderr, "outarm: fcam_arm" IMGEXT "\n");
    save_image(outarm, "fcam_arm" IMGEXT);
    fprintf(stderr, "        %d %d\n", outarm.width(), outarm.height());
#endif

    // Timings on N900 as of SIGGRAPH 2012 camera ready are (best of 10)
    // Halide: 722ms, FCam: 741ms

    return 0;
}
