#include "sigma9x9.h"
#include <hexagon_standalone.h>
#include "io.h"
#include <stdio.h>
#include <stdlib.h>
static uint8_t max(uint8_t a,  uint8_t b) {
  return a > b ? a : b;
}
static uint8_t min(uint8_t a,  uint8_t b) {
  return a < b ? a : b;
}

static uint8_t mid(uint8_t a,  uint8_t b, uint8_t c) {
  return max(min(max(a, b), c), min(a, b));
}
#ifdef SYNTHETIC
int main(int argc, char **argv) {
  int x, y;
  int i, j;
  int width, height;
  long long start_time, total_cycles;

  // Create the Input.
  uint8_t input[12][128];
  // And the memory where we want to write our output:
  uint8_t output[12][128];

  // don't set these larger than the input/output buffers above
  height = 12;
  width = 128;

#if DEBUG
  printf ("initializing inputs\n");
#endif
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
        input[y][x] =  y + x;
    }
  }
#if DEBUG
  printf ("finished initializing inputs\n");
#endif

#if DEBUG
  printf ("initializing output\n");
#endif
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      output[y][x] = 0;
    }
  }
#if DEBUG
  printf ("finished initializing output\n");
#endif

  int32_t threshold = 8;

  // In AOT-compiled mode, Halide doesn't manage this memory for
  // you. You should use whatever image data type makes sense for
  // your application. Halide just needs pointers to it.

  // Now we make a buffer_t to represent our input1 and output. It's
  // important to zero-initialize them so you don't end up with
  // garbage fields that confuse Halide.
  buffer_t input1_buf = {0}, output_buf = {0};

  // The host pointers point to the start of the image data:
  input1_buf.host = (uint8_t *)&input[0];
  output_buf.host = (uint8_t *)&output[0];

  // To access pixel (x, y) in a two-dimensional buffer_t, Halide
  // looks at memory address:
  // host + elem_size * ((x - min[0])*stride[0] + (y - min[1])*stride[1])
  // The stride in a dimension represents the number of elements in
  // memory between adjacent entries in that dimension. We have a
  // grayscale image stored in scanline order, so stride[0] is 1,
  // because pixels that are adjacent in x are next to each other in
  // memory.
  input1_buf.stride[0] = output_buf.stride[0] = 1;

  // // stride[1] is the width of the image, because pixels that are
  // // adjacent in y are separated by a scanline's worth of pixels in
  // // memory.
  input1_buf.stride[1] = width;  output_buf.stride[1] = width;

  // The extent tells us how large the image is in each dimension.
  input1_buf.extent[0] = width;
  output_buf.extent[0] = width;
  input1_buf.extent[1] = height;
  output_buf.extent[1] = height;

  // We'll leave the mins as zero. This is what they typically
  // are. The host pointer points to the memory location of the min
  // coordinate (not the origin!).  See lesson 6 for more detail
  // about the mins.

  // The elem_size field tells us how many bytes each element
  // uses. For the 8-bit image we use in this test it's one.
  input1_buf.elem_size = 1; output_buf.elem_size = 1;

  SIM_ACQUIRE_HVX;
#if LOG2VLEN == 7
    SIM_SET_HVX_DOUBLE_MODE;
#endif

#if DEBUG
  printf ("Acquired vector context\n");
#endif
  RESET_PMU();
  start_time = READ_PCYCLES();

  int error = sigma9x9(&input1_buf, threshold, &output_buf);

  total_cycles = READ_PCYCLES() - start_time;
  DUMP_PMU();

  SIM_RELEASE_HVX;
#if DEBUG
  printf ("Done calling the halide func. and released the vector context\n");
#endif

  if (error) {
    printf("Halide returned an error: %d\n", error);
    return -1;
  }

  printf("Checking results of sigma9x9\n");
  int s, t;
  int p, center, diff;
  int sum, cnt, out, res;
  const int invLUT[128] = {
    0,32768,16384,10923,8192,6554,5461,4681,4096,3641,3277,2979,2731,2521,2341,2185,
    2048,1928,1820,1725,1638,1560,1489,1425,1365,1311,1260,1214,1170,1130,1092,1057,
    1024,993,964,936,910,886,862,840,819,799,780,762,745,728,712,697,
    683,669,655,643,630,618,607,596,585,575,565,555,546,537,529,520,
    512,504,496,489,482,475,468,462,455,449,443,437,431,426,420,415,
    410,405,400,395,390,386,381,377,372,368,364,360,356,352,349,345,
    341,338,334,331,328,324,321,318,315,312,309,306,303,301,298,295,
    293,290,287,285,282,280,278,275,273,271,269,266,264,262,260,258};

    for (y = 4; y < height - 4; y++) {
        for (x = 4; x < width - 4; x++) {
            center = input[y][x];

            sum = 0;
            cnt = 0;
            for (t = -4; t <= 4; t++)
            {
                for (s = -4; s <= 4; s++)
                {
                    p = input[y + t][x + s];
                    diff = p > center ? (p - center) : (center - p);

                    if (diff <= threshold)
                    {
                        sum += p;
                        cnt++;
                    }
                }
            }

            out = (sum * invLUT[cnt] + (1<<14)) >> 15;
            res = (unsigned char)(out > 255 ? 255: out);
            if ( res != output[y][x]) {
                printf ("output[%d][%d] = %u instead of %u\n", y, x, output[y][x], res);
            }
        }
    }

#if defined(__hexagon__)
    printf("AppReported (HVX%db-mode): Image %dx%d - sigma9x9: %0.4f cycles/pixel\n", 1<<LOG2VLEN, (int)width, (int)height, (float)total_cycles/width/height);
    printf("Pcycles: %0.4lld\n",total_cycles);
#endif
    printf("Done!\n");
    return 0;
}

#else


int main(int argc, char **argv) {
  // Create the Input.

  int x, y;
  int i, j;
  int width, height;
  long long start_time, total_cycles;
  FH fp;

  /* -----------------------------------------------------*/
  /*  Get input parameters                                */
  /* -----------------------------------------------------*/
#if DEBUG
  printf ("Marshall inputs.\n");
#endif

  if (argc != 5){
    printf("usage: %s <width> <height> <input.bin> <output.bin>\n", argv[0]);
    return 1;
  }

  int32_t threshold = 8;

  width  = atoi(argv[1]);
  height = atoi(argv[2]);
  /* -----------------------------------------------------*/
  /*  Allocate memory for input/output                    */
  /* -----------------------------------------------------*/
  unsigned char *input  = (unsigned char *)memalign(64, width*height*sizeof(unsigned char));
  unsigned char *output = (unsigned char *)memalign(64, width*height*sizeof(unsigned char));

  if ( input == NULL || output == NULL ){
    printf("Error: Could not allocate Memory for image\n");
    return 1;
  }

  /* -----------------------------------------------------*/
  /*  Read image input from file                          */
  /* -----------------------------------------------------*/
  if((fp = open(argv[3], O_RDONLY)) < 0 )
    {
      printf("Error: Cannot open %s for input\n", argv[3]);
      return 1;
    }

  for(i = 0; i < height; i++)
    {
      if(read(fp, &input[i*width],  sizeof(unsigned char)*width)!=width)
        {
          printf("Error, Unable to read from %s\n", argv[3]);
          close(fp);
          return 1;
        }
    }
  close(fp);

#if DEBUG
  printf ("finished reading the input.\n");
#endif

  // In AOT-compiled mode, Halide doesn't manage this memory for
  // you. You should use whatever image data type makes sense for
  // your application. Halide just needs pointers to it.

  // Now we make a buffer_t to represent our input1 and output. It's
  // important to zero-initialize them so you don't end up with
  // garbage fields that confuse Halide.
  buffer_t input1_buf = {0}, output_buf = {0};

  // The host pointers point to the start of the image data:
  input1_buf.host = (uint8_t *)&input[0];
  output_buf.host = (uint8_t *)&output[0];

  // To access pixel (x, y) in a two-dimensional buffer_t, Halide
  // looks at memory address:
  // host + elem_size * ((x - min[0])*stride[0] + (y - min[1])*stride[1])
  // The stride in a dimension represents the number of elements in
  // memory between adjacent entries in that dimension. We have a
  // grayscale image stored in scanline order, so stride[0] is 1,
  // because pixels that are adjacent in x are next to each other in
  // memory.
  input1_buf.stride[0] = output_buf.stride[0] = 1;

  // // stride[1] is the width of the image, because pixels that are
  // // adjacent in y are separated by a scanline's worth of pixels in
  // // memory.
  input1_buf.stride[1] = width;  output_buf.stride[1] = width;

  // The extent tells us how large the image is in each dimension.
  input1_buf.extent[0] = width;
  output_buf.extent[0] = width;
  input1_buf.extent[1] = height;
  output_buf.extent[1] = height;

  // We'll leave the mins as zero. This is what they typically
  // are. The host pointer points to the memory location of the min
  // coordinate (not the origin!).  See lesson 6 for more detail
  // about the mins.

  // The elem_size field tells us how many bytes each element
  // uses. For the 8-bit image we use in this test it's one.
  input1_buf.elem_size = 1; output_buf.elem_size = 1;

  SIM_ACQUIRE_HVX;
#if LOG2VLEN == 7
    SIM_SET_HVX_DOUBLE_MODE;
#endif

#if DEBUG
  printf ("Acquired vector context\n");
#endif
  RESET_PMU();
  start_time = READ_PCYCLES();

  int error = sigma9x9(&input1_buf, threshold, &output_buf);

  total_cycles = READ_PCYCLES() - start_time;
  DUMP_PMU();

  SIM_RELEASE_HVX;
#if DEBUG
  printf ("Done calling the halide func. and released the vector context\n");
#endif

  if (error) {
    printf("Halide returned an error: %d\n", error);
    return -1;
  }

  /* -----------------------------------------------------*/
  /*  Write image output to file                          */
  /* -----------------------------------------------------*/
  if((fp = open(argv[4], O_CREAT_WRONLY_TRUNC, 0777)) < 0)
    {
      printf("Error: Cannot open %s for output\n", argv[4]);
      return 1;
    }

  int bound = 4; // exclude the boundary pixels
  for(i = bound; i < height-bound; i++) {
    if(write(fp, &output[i*width+bound], sizeof(unsigned char)*(width-(2*bound)))!=(width-(2*bound))) {
      printf("Error:  Writing file: %s\n", argv[4]);
      return 1;
    }
  }
  close(fp);

  free(input);
  free(output);

#if defined(__hexagon__)
    printf("AppReported (HVX%db-mode): Image %dx%d - sigma9x9: %0.4f cycles/pixel\n", 1<<LOG2VLEN, (int)width, (int)height, (float)total_cycles/width/height);
#endif
    printf("Done!\n");
    return 0;
}

#endif
