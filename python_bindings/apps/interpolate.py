#!/usr/bin/python3
"""
Fast image interpolation using a pyramid.
"""

from __future__ import print_function
from __future__ import division

import time, sys
from halide import *

from datetime import datetime
from scipy.misc import imread, imsave
import numpy as np
import os.path

int_t = Int(32)
float_t = Float(32)

def get_interpolate(input, levels):
    """
    Build function, schedules it, and invokes jit compiler
    :return: halide.Func
    """

    # THE ALGORITHM

    downsampled = [Func('downsampled%d'%i) for i in range(levels)]
    downx = [Func('downx%d'%l) for l in range(levels)]
    interpolated = [Func('interpolated%d'%i) for i in range(levels)]
#     level_widths = [Param(int_t,'level_widths%d'%i) for i in range(levels)]
#     level_heights = [Param(int_t,'level_heights%d'%i) for i in range(levels)]
    upsampled = [Func('upsampled%d'%l) for l in range(levels)]
    upsampledx = [Func('upsampledx%d'%l) for l in range(levels)]
    x = Var('x')
    y = Var('y')
    c = Var('c')

    clamped = Func('clamped')
    clamped[x, y, c] = input[clamp(x, 0, input.width()-1), clamp(y, 0, input.height()-1), c]

    # This triggers a bug in llvm 3.3 (3.2 and trunk are fine), so we
    # rewrite it in a way that doesn't trigger the bug. The rewritten
    # form assumes the input alpha is zero or one.
    # downsampled[0][x, y, c] = select(c < 3, clamped[x, y, c] * clamped[x, y, 3], clamped[x, y, 3])
    downsampled[0][x,y,c] = clamped[x, y, c] * clamped[x, y, 3]

    for l in range(1, levels):
        prev = Func()
        prev = downsampled[l-1]

        if l == 4:
            # Also add a boundary condition at a middle pyramid level
            # to prevent the footprint of the downsamplings to extend
            # too far off the base image. Otherwise we look 512
            # pixels off each edge.
            w = input.width()/(1 << l)
            h = input.height()/(1 << l)
            prev = lambda3D(x, y, c, prev[clamp(x, 0, w), clamp(y, 0, h), c])

        downx[l][x,y,c] = (prev[x*2-1,y,c] + 2.0 * prev[x*2,y,c] + prev[x*2+1,y,c]) * 0.25
        downsampled[l][x,y,c] = (downx[l][x,y*2-1,c] + 2.0 * downx[l][x,y*2,c] + downx[l][x,y*2+1,c]) * 0.25


    interpolated[levels-1][x,y,c] = downsampled[levels-1][x,y,c]
    for l in range(levels-1)[::-1]:
        upsampledx[l][x,y,c] = (interpolated[l+1][x/2, y, c] + interpolated[l+1][(x+1)/2, y, c]) / 2.0
        upsampled[l][x,y,c] = (upsampledx[l][x, y/2, c] + upsampledx[l][x, (y+1)/2, c]) / 2.0
        interpolated[l][x,y,c] = downsampled[l][x,y,c] + (1.0 - downsampled[l][x,y,3]) * upsampled[l][x,y,c]

    normalize = Func('normalize')
    normalize[x,y,c] = interpolated[0][x, y, c] / interpolated[0][x, y, 3]

    final = Func('final')
    final[x,y,c] = normalize[x,y,c]

    print("Finished function setup.")

    # THE SCHEDULE

    sched = 2
    target = get_target_from_environment()
    if target.has_gpu_feature():
        sched = 4
    else:
        sched = 2

    if sched == 0:
        print ("Flat schedule.")
        for l in range(levels):
            downsampled[l].compute_root()
            interpolated[l].compute_root()

        final.compute_root()

    elif sched == 1:
        print("Flat schedule with vectorization.")
        for l in range(levels):
            downsampled[l].compute_root().vectorize(x, 4)
            interpolated[l].compute_root().vectorize(x, 4)

        final.compute_root()

    elif sched == 2:
        print("Flat schedule with parallelization + vectorization")
        xi, yi = Var('xi'), Var('yi')
        clamped.compute_root().parallel(y).bound(c, 0, 4).reorder(c, x, y).reorder_storage(c, x, y).vectorize(c, 4)
        for l in range(1, levels - 1):
            if l > 0:
                downsampled[l].compute_root().parallel(y).reorder(c, x, y).reorder_storage(c, x, y).vectorize(c, 4)
            interpolated[l].compute_root().parallel(y).reorder(c, x, y).reorder_storage(c, x, y).vectorize(c, 4)
            interpolated[l].unroll(x, 2).unroll(y, 2);

        final.reorder(c, x, y).bound(c, 0, 3).parallel(y)
        final.tile(x, y, xi, yi, 2, 2).unroll(xi).unroll(yi)
        final.bound(x, 0, input.width())
        final.bound(y, 0, input.height())

    elif sched == 3:
        print("Flat schedule with vectorization sometimes.")
        for l in range(levels):
            if l + 4 < levels:
                yo, yi = Var('yo'), Var('yi')
                downsampled[l].compute_root().vectorize(x, 4)
                interpolated[l].compute_root().vectorize(x, 4)
            else:
                downsampled[l].compute_root()
                interpolated[l].compute_root()

        final.compute_root();

    elif sched == 4:
        print("GPU schedule.")

        # Some gpus don't have enough memory to process the entire
        # image, so we process the image in tiles.
        yo, yi, xo, xi = Var('yo'), Var('yi'), Var('xo'), Var('xi')
        final.reorder(c, x, y).bound(c, 0, 3).vectorize(x, 4)
        final.tile(x, y, xo, yo, xi, yi, input.width()/4, input.height()/4)
        normalize.compute_at(final, xo).reorder(c, x, y).gpu_tile(x, y, 16, 16, GPU_Default).unroll(c)

        # Start from level 1 to save memory - level zero will be computed on demand
        for l in range(1, levels):
            tile_size = 32 >> l;
            if tile_size < 1: tile_size = 1
            if tile_size > 16: tile_size = 16
            downsampled[l].compute_root().gpu_tile(x, y, c, tile_size, tile_size, 4, GPU_Default)
            interpolated[l].compute_at(final, xo).gpu_tile(x, y, c, tile_size, tile_size, 4, GPU_Default)

    else:
        print("No schedule with this number.")
        exit(1)

    # JIT compile the pipeline eagerly, so we don't interfere with timing
    final.compile_jit(target)

    return final

def get_input_data():

    image_path = os.path.join(os.path.dirname(__file__), "../../apps/images/rgba.png")
    assert os.path.exists(image_path), "Could not find %s" % image_path
    rgba_data = imread(image_path)
    #print("rgba_data", type(rgba_data), rgba_data.shape, rgba_data.dtype)

    input_data = np.copy(rgba_data, order="F").astype(np.float32) / 255.0
    # input data is in range [0, 1]
    #print("input_data", type(input_data), input_data.shape, input_data.dtype)

    return input_data


def main():

    input = ImageParam(float_t, 3, "input")
    levels = 10

    interpolate = get_interpolate(input, levels)

    # preparing input and output memory buffers (numpy ndarrays)
    input_data = get_input_data()
    assert input_data.shape[2] == 4
    input_image = Buffer(input_data)
    input.set(input_image)

    input_width, input_height = input_data.shape[:2]

    t0 = datetime.now()
    output_image = interpolate.realize(input_width, input_height, 3)
    t1 = datetime.now()
    print('Interpolated in %.5f secs' % (t1-t0).total_seconds())

    output_data = buffer_to_ndarray(output_image)

    # save results
    input_path = "interpolate_input.png"
    output_path = "interpolate_result.png"
    imsave(input_path, input_data)
    imsave(output_path, output_data)
    print("\nblur realized on output image.",
          "Result saved at", output_path,
          "( input data copy at", input_path, ")")

    print("\nEnd of game. Have a nice day!")


if __name__ == '__main__':
    main()
