#include <Halide.h>
#include "halide-hexagon-setup.h"
#include <stdio.h>
using namespace Halide;

#define COMPILE_OBJ(X)  ((X).compile_to_file("dilate3x3", args, target))

void test_dilate3x3(Target& target) {
  Halide::Var x("x"),y("y");
  ImageParam input(type_of<uint8_t>(), 2);
  set_min(input, 0, 0);
  set_min(input, 1, 0);
  set_stride_multiple(input, 1, 1 << LOG2VLEN);

  Halide::Func max_x("max_x");
  Halide::Func dilate3x3("dilate3x3");
  max_x(x,y) = max(max(input(x-1,y),input(x,y)),
                   input(x+1,y));

  dilate3x3(x,y) = max(max(max_x(x, y-1), max_x(x, y)), max_x(x, y+1));
  set_output_buffer_min(dilate3x3, 0, 0);
  set_output_buffer_min(dilate3x3, 1, 0);
  set_stride_multiple(dilate3x3, 1, 1 << LOG2VLEN);

#ifndef NOVECTOR
  dilate3x3.vectorize(x, 1<<LOG2VLEN);
#endif
  std::vector<Argument> args(1);
  args[0]  = input;
#ifdef BITCODE
  dilate3x3.compile_to_bitcode("dilate3x3.bc", args, target);
#endif
#ifdef STMT
  dilate3x3.compile_to_lowered_stmt("dilate3x3.html", args, HTML);
#endif
#ifdef ASSEMBLY
  dilate3x3.compile_to_assembly("dilate3x3.s", args, target);
#endif
#ifdef RUN
  COMPILE_OBJ(dilate3x3);
#endif
}

int main(int argc, char **argv) {
	Target target;
	setupHexagonTarget(target, LOG2VLEN == 7 ? Target::HVX_128 : Target::HVX_64);
        commonPerfSetup(target);
        target.set_cgoption(Target::BuffersAligned);
	test_dilate3x3(target);
	printf ("Done\n");
	return 0;
}

