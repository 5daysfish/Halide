#include "HalideRuntime.h"
#include "HalideBuffer.h"

#include <math.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <map>

#include "metadata_tester.h"
#include "metadata_tester_ucon.h"

using namespace Halide;

const int kSize = 32;

#define EXPECT_EQ(exp, act)                                                                                  \
    do {                                                                                                     \
        if ((exp) != (act)) {                                                                                \
            fprintf(stderr, "%s == %s: Expected %f, Actual %f\n", #exp, #act, (double)(exp), (double)(act)); \
            exit(-1);                                                                                        \
        }                                                                                                    \
    } while (0);

#define EXPECT_STREQ(exp, act)                                                                       \
    do {                                                                                             \
        if (strcmp((exp), (act)) != 0) {                                                             \
            fprintf(stderr, "%s == %s: Expected \"%s\", Actual \"%s\"\n", #exp, #act, (exp), (act)); \
            exit(-1);                                                                                \
        }                                                                                            \
    } while (0);

#define EXPECT_SCALAR_UNION_EQ(code, bits, exp, act)                  \
    do {                                                              \
        if (!scalar_union_ptr_equal((code), (bits), (exp), (act))) {  \
            fprintf(stderr, "%s == %s: did not match\n", #exp, #act); \
            exit(-1);                                                 \
        }                                                             \
    } while (0);

bool scalar_union_equal(int32_t type_code,
                        int32_t type_bits,
                        const halide_scalar_value_t &e,
                        const halide_scalar_value_t &a) {
    switch (type_code) {
    case halide_type_int:
        switch (type_bits) {
        case 8:
            return e.u.i8 == a.u.i8;
        case 16:
            return e.u.i16 == a.u.i16;
        case 32:
            return e.u.i32 == a.u.i32;
        case 64:
            return e.u.i64 == a.u.i64;
        }
    case halide_type_uint:
        switch (type_bits) {
        case 1:
            return e.u.b == a.u.b;
        case 8:
            return e.u.u8 == a.u.u8;
        case 16:
            return e.u.u16 == a.u.u16;
        case 32:
            return e.u.u32 == a.u.u32;
        case 64:
            return e.u.u64 == a.u.u64;
        }
    case halide_type_float:
        switch (type_bits) {
        case 32:
            return e.u.f32 == a.u.f32;
        case 64:
            return e.u.f64 == a.u.f64;
        }
    case halide_type_handle:
        return e.u.handle == a.u.handle;
    }
    fprintf(stderr, "Unsupported type %d or size %d\n", type_code, type_bits);
    exit(-1);
}

bool scalar_union_ptr_equal(int32_t type_code,
                            int32_t type_bits,
                            const halide_scalar_value_t *e,
                            const halide_scalar_value_t *a) {
    if (e) {
        if (a) {
            return scalar_union_equal(type_code, type_bits, *e, *a);
        } else {
            return false;
        }
    } else {
        if (a) {
            return false;
        } else {
            return true;
        }
    }
}

void match_argument(const halide_filter_argument_t &e, const halide_filter_argument_t &a) {
    EXPECT_STREQ(e.name, a.name);
    EXPECT_EQ(e.dimensions, a.dimensions);
    EXPECT_EQ(e.kind, a.kind);
    EXPECT_EQ(e.type.code, a.type.code);
    EXPECT_EQ(e.type.bits, a.type.bits);
    EXPECT_EQ(e.dimensions, a.dimensions);
    EXPECT_SCALAR_UNION_EQ(e.type.code, e.type.bits, e.def, a.def);
    EXPECT_SCALAR_UNION_EQ(e.type.code, e.type.bits, e.min, a.min);
    EXPECT_SCALAR_UNION_EQ(e.type.code, e.type.bits, e.max, a.max);
}

template <typename Type>
Buffer<Type> make_image() {
    Buffer<Type> im(kSize, kSize, 3);
    for (int x = 0; x < kSize; x++) {
        for (int y = 0; y < kSize; y++) {
            for (int c = 0; c < 3; c++) {
                im(x, y, c) = static_cast<Type>(x + y + c);
            }
        }
    }
    return im;
}

template <typename InputType, typename OutputType>
void verify(const Buffer<InputType> &input, 
            const Buffer<OutputType> &output0, 
            const Buffer<OutputType> &output1, 
            const Buffer<OutputType> &output_scalar, 
            const Buffer<OutputType> &output_array0, 
            const Buffer<OutputType> &output_array1) {
    if (output_scalar.dimensions() != 0) {
        fprintf(stderr, "output_scalar should be zero-dimensional\n");
        exit(-1);
    }
    if (output_scalar() != 1234.25f) {
        fprintf(stderr, "output_scalar value is wrong (%f)\n", output_scalar());
        exit(-1);
    }
    for (int x = 0; x < kSize; x++) {
        for (int y = 0; y < kSize; y++) {
            for (int c = 0; c < 3; c++) {
                const OutputType expected0 = static_cast<OutputType>(input(x, y, c));
                const float expected1 = expected0 + 1;
                const OutputType actual0 = output0(x, y, c);
                const float actual1 = output1(x, y, c);
                if (expected0 != actual0) {
                    fprintf(stderr, "img0[%d, %d, %d] = %f, expected %f\n", x, y, c, (double)actual0, (double)expected0);
                    exit(-1);
                }
                if (expected1 != actual1) {
                    fprintf(stderr, "img1[%d, %d, %d] = %f, expected %f\n", x, y, c, (double)actual1, (double)expected1);
                    exit(-1);
                }
                if (output_array0(x, y, c) != 1.5f) {
                    fprintf(stderr, "output_array0[%d, %d, %d] = %f, expected %f\n", x, y, c, output_array0(x, y, c), 1.5f);
                    exit(-1);
                }
                if (output_array1(x, y, c) != 3.0f) {
                    fprintf(stderr, "output_array1[%d, %d, %d] = %f, expected %f\n", x, y, c, output_array1(x, y, c), 3.0f);
                    exit(-1);
                }
            }
        }
    }
}

template <typename T>
const halide_scalar_value_t *make_scalar(T v);

template <>
const halide_scalar_value_t *make_scalar(bool v) {
    halide_scalar_value_t *s = new halide_scalar_value_t();
    s->u.b = v;
    return s;
}

template <>
const halide_scalar_value_t *make_scalar(int8_t v) {
    halide_scalar_value_t *s = new halide_scalar_value_t();
    s->u.i8 = v;
    return s;
}

template <>
const halide_scalar_value_t *make_scalar(int16_t v) {
    halide_scalar_value_t *s = new halide_scalar_value_t();
    s->u.i16 = v;
    return s;
}

template <>
const halide_scalar_value_t *make_scalar(int32_t v) {
    halide_scalar_value_t *s = new halide_scalar_value_t();
    s->u.i32 = v;
    return s;
}

template <>
const halide_scalar_value_t *make_scalar(int64_t v) {
    halide_scalar_value_t *s = new halide_scalar_value_t();
    s->u.i64 = v;
    return s;
}

template <>
const halide_scalar_value_t *make_scalar(uint8_t v) {
    halide_scalar_value_t *s = new halide_scalar_value_t();
    s->u.u8 = v;
    return s;
}

template <>
const halide_scalar_value_t *make_scalar(uint16_t v) {
    halide_scalar_value_t *s = new halide_scalar_value_t();
    s->u.u16 = v;
    return s;
}

template <>
const halide_scalar_value_t *make_scalar(uint32_t v) {
    halide_scalar_value_t *s = new halide_scalar_value_t();
    s->u.u32 = v;
    return s;
}

template <>
const halide_scalar_value_t *make_scalar(uint64_t v) {
    halide_scalar_value_t *s = new halide_scalar_value_t();
    s->u.u64 = v;
    return s;
}

template <>
const halide_scalar_value_t *make_scalar(float v) {
    halide_scalar_value_t *s = new halide_scalar_value_t();
    s->u.f32 = v;
    return s;
}

template <>
const halide_scalar_value_t *make_scalar(double v) {
    halide_scalar_value_t *s = new halide_scalar_value_t();
    s->u.f64 = v;
    return s;
}

template <>
const halide_scalar_value_t *make_scalar(void *v) {
    halide_scalar_value_t *s = new halide_scalar_value_t();
    s->u.handle = v;
    return s;
}

void check_metadata(const halide_filter_metadata_t &md, bool expect_ucon_at_0) {
    // target will vary depending on where we are testing, but probably
    // will contain "x86" or "arm".
    if (!strstr(md.target, "x86") &&
        !strstr(md.target, "powerpc") &&
        !strstr(md.target, "arm")) {
        fprintf(stderr, "Expected x86 or arm, Actual %s\n", md.target);
        exit(-1);
    }

    // Not static, since we free make_scalar() results each time
    const halide_filter_argument_t kExpectedArguments[] = {
        {
          "__user_context",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_handle, 64),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "input",
          halide_argument_kind_input_buffer,
          3,
          halide_type_t(halide_type_uint, 8),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "b",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_uint, 1),
          make_scalar<bool>(true),
          nullptr,
          nullptr,
        },
        {
          "i8",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 8),
          make_scalar<int8_t>(8),
          make_scalar<int8_t>(-8),
          make_scalar<int8_t>(127),
        },
        {
          "i16",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 16),
          make_scalar<int16_t>(16),
          make_scalar<int16_t>(-16),
          make_scalar<int16_t>(127),
        },
        {
          "i32",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 32),
          make_scalar<int32_t>(32),
          make_scalar<int32_t>(-32),
          make_scalar<int32_t>(127),
        },
        {
          "i64",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 64),
          make_scalar<int64_t>(64),
          make_scalar<int64_t>(-64),
          make_scalar<int64_t>(127),
        },
        {
          "u8",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_uint, 8),
          make_scalar<uint8_t>(80),
          make_scalar<uint8_t>(8),
          make_scalar<uint8_t>(255),
        },
        {
          "u16",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_uint, 16),
          make_scalar<uint16_t>(160),
          make_scalar<uint16_t>(16),
          make_scalar<uint16_t>(2550),
        },
        {
          "u32",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_uint, 32),
          make_scalar<uint32_t>(320),
          make_scalar<uint32_t>(32),
          make_scalar<uint32_t>(2550),
        },
        {
          "u64",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_uint, 64),
          make_scalar<uint64_t>(640),
          make_scalar<uint64_t>(64),
          make_scalar<uint64_t>(2550),
        },
        {
          "f32",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_float, 32),
          make_scalar<float>(32.1234f),
          make_scalar<float>(-3200.1234f),
          make_scalar<float>(3200.1234f),
        },
        {
          "f64",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_float, 64),
          make_scalar<double>(64.25),
          make_scalar<double>(-6400.25),
          make_scalar<double>(6400.25),
        },
        {
          "h",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_handle, 64),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "input_not_nod",
          halide_argument_kind_input_buffer,
          3,
          halide_type_t(halide_type_uint, 8),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "input_nod",
          halide_argument_kind_input_buffer,
          3,
          halide_type_t(halide_type_uint, 8),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "input_not",
          halide_argument_kind_input_buffer,
          3,
          halide_type_t(halide_type_uint, 8),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "array_input_0",
          halide_argument_kind_input_buffer,
          3,
          halide_type_t(halide_type_uint, 8),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "array_input_1",
          halide_argument_kind_input_buffer,
          3,
          halide_type_t(halide_type_uint, 8),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "array2_input_0",
          halide_argument_kind_input_buffer,
          3,
          halide_type_t(halide_type_uint, 8),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "array2_input_1",
          halide_argument_kind_input_buffer,
          3,
          halide_type_t(halide_type_uint, 8),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "array_i8_0",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 8),
          make_scalar<int8_t>(0),
          nullptr,
          nullptr,
        },
        {
          "array_i8_1",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 8),
          make_scalar<int8_t>(0),
          nullptr,
          nullptr,
        },
        {
          "array2_i8_0",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 8),
          make_scalar<int8_t>(0),
          nullptr,
          nullptr,
        },
        {
          "array2_i8_1",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 8),
          make_scalar<int8_t>(0),
          nullptr,
          nullptr,
        },
        {
          "array_i16_0",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 16),
          make_scalar<int16_t>(16),
          nullptr,
          nullptr,
        },
        {
          "array_i16_1",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 16),
          make_scalar<int16_t>(16),
          nullptr,
          nullptr,
        },
        {
          "array2_i16_0",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 16),
          make_scalar<int16_t>(16),
          nullptr,
          nullptr,
        },
        {
          "array2_i16_1",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 16),
          make_scalar<int16_t>(16),
          nullptr,
          nullptr,
        },
        {
          "array_i32_0",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 32),
          make_scalar<int32_t>(32),
          make_scalar<int32_t>(-32),
          make_scalar<int32_t>(127),
        },
        {
          "array_i32_1",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 32),
          make_scalar<int32_t>(32),
          make_scalar<int32_t>(-32),
          make_scalar<int32_t>(127),
        },
        {
          "array2_i32_0",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 32),
          make_scalar<int32_t>(32),
          make_scalar<int32_t>(-32),
          make_scalar<int32_t>(127),
        },
        {
          "array2_i32_1",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_int, 32),
          make_scalar<int32_t>(32),
          make_scalar<int32_t>(-32),
          make_scalar<int32_t>(127),
        },
        {
          "array_h_0",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_handle, 64),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "array_h_1",
          halide_argument_kind_input_scalar,
          0,
          halide_type_t(halide_type_handle, 64),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "output.0",
          halide_argument_kind_output_buffer,
          3,
          halide_type_t(halide_type_float, 32),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "output.1",
          halide_argument_kind_output_buffer,
          3,
          halide_type_t(halide_type_float, 32),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "output_scalar",
          halide_argument_kind_output_buffer,
          0,
          halide_type_t(halide_type_float, 32),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "array_outputs_0",
          halide_argument_kind_output_buffer,
          3,
          halide_type_t(halide_type_float, 32),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "array_outputs_1",
          halide_argument_kind_output_buffer,
          3,
          halide_type_t(halide_type_float, 32),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "array_outputs2_0",
          halide_argument_kind_output_buffer,
          3,
          halide_type_t(halide_type_float, 32),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "array_outputs2_1",
          halide_argument_kind_output_buffer,
          3,
          halide_type_t(halide_type_float, 32),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "array_outputs3_0",
          halide_argument_kind_output_buffer,
          0,
          halide_type_t(halide_type_float, 32),
          nullptr,
          nullptr,
          nullptr,
        },
        {
          "array_outputs3_1",
          halide_argument_kind_output_buffer,
          0,
          halide_type_t(halide_type_float, 32),
          nullptr,
          nullptr,
          nullptr,
        }
    };
    const int kExpectedArgumentCount = (int)sizeof(kExpectedArguments) / sizeof(kExpectedArguments[0]);

    EXPECT_EQ(kExpectedArgumentCount - (expect_ucon_at_0 ? 0 : 1), md.num_arguments);

    const halide_filter_argument_t* expected = &kExpectedArguments[expect_ucon_at_0 ? 0 : 1];
    for (int i = 0; i < md.num_arguments; ++i) {
        fprintf(stdout, "checking arg %d %s\n", i, md.arguments[i].name);
        match_argument(expected[i], md.arguments[i]);
    }

    for (int i = 0; i < kExpectedArgumentCount; ++i) {
        delete kExpectedArguments[i].def;
        delete kExpectedArguments[i].min;
        delete kExpectedArguments[i].max;
    }
}

int main(int argc, char **argv) {
    void* user_context = nullptr;

    int result;

    Buffer<uint8_t> input = make_image<uint8_t>();

    Buffer<float> output0(kSize, kSize, 3);
    Buffer<float> output1(kSize, kSize, 3);
    Buffer<float> output_scalar = Buffer<float>::make_scalar();
    Buffer<float> output_array[2] = {{kSize, kSize, 3}, {kSize, kSize, 3}};
    Buffer<float> output_array2[2] = {{kSize, kSize, 3}, {kSize, kSize, 3}};
    Buffer<float> output_array3[2] = {{1}, {1}};

    result = metadata_tester(
        input,             // Input<Func>
        false,             // Input<bool>
        0,                 // Input<i8>
        0,                 // Input<i16>
        0,                 // Input<i32>
        0,                 // Input<i64>
        0,                 // Input<u8>
        0,                 // Input<u16>
        0,                 // Input<u32>
        0,                 // Input<u64>
        0.f,               // Input<float>
        0.0,               // Input<double>
        nullptr,           // Input<void*>
        input,             // Input<Func> 
        input,             // Input<Func> 
        input,             // Input<Func> 
        input, input,      // Input<Func[]>
        input, input,      // Input<Func[2]>
        0, 0,              // Input<int8_t[]>
        0, 0,              // Input<int8_t[2]>
        0, 0,              // Input<int16_t[]>
        0, 0,              // Input<int16_t[2]>
        0, 0,              // Input<int32_t[]>
        0, 0,              // Input<int32_t[2]>
        nullptr, nullptr,  // Input<void*[]>
        output0, output1,  // Output<Tuple(Func, Func)>
        output_scalar,     // Output<float>
        output_array[0], output_array[1],   // Output<Func[]>
        output_array2[0], output_array2[1], // Output<Func[2]>
        output_array3[0], output_array3[1]  // Output<float[2]>
    );  
    EXPECT_EQ(0, result);

    result = metadata_tester_ucon(
        user_context, 
        input,             // Input<Func>
        false,             // Input<bool>
        0,                 // Input<i8>
        0,                 // Input<i16>
        0,                 // Input<i32>
        0,                 // Input<i64>
        0,                 // Input<u8>
        0,                 // Input<u16>
        0,                 // Input<u32>
        0,                 // Input<u64>
        0.f,               // Input<float>
        0.0,               // Input<double>
        nullptr,           // Input<void*>
        input,             // Input<Func> 
        input,             // Input<Func> 
        input,             // Input<Func> 
        input, input,      // Input<Func[]>
        input, input,      // Input<Func[2]>
        0, 0,              // Input<int8_t[]>
        0, 0,              // Input<int8_t[2]>
        0, 0,              // Input<int16_t[]>
        0, 0,              // Input<int16_t[2]>
        0, 0,              // Input<int32_t[]>
        0, 0,              // Input<int32_t[2]>
        nullptr, nullptr,  // Input<void*[]>
        output0, output1,  // Output<Tuple(Func, Func)>
        output_scalar,     // Output<float>
        output_array[0], output_array[1],    // Output<Func[]>
        output_array2[0], output_array2[1], // Output<Func[2]>
        output_array3[0], output_array3[1]  // Output<float[2]>
    );
    EXPECT_EQ(0, result);

    verify(input, output0, output1, output_scalar, output_array[0], output_array[1]);

    check_metadata(*metadata_tester_metadata(), false);
    if (!strcmp(metadata_tester_metadata()->name, "metadata_tester_metadata")) {
        fprintf(stderr, "Expected name %s\n", "metadata_tester_metadata");
        exit(-1);
    }

    check_metadata(*metadata_tester_ucon_metadata(), true);
    if (!strcmp(metadata_tester_ucon_metadata()->name, "metadata_tester_ucon_metadata")) {
        fprintf(stderr, "Expected name %s\n", "metadata_tester_ucon_metadata");
        exit(-1);
    }

    printf("Success!\n");
    return 0;
}
