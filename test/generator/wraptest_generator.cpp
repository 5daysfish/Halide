#include "Halide.h"

namespace {

enum class BagType { Paper, Plastic };

class Wrappee : public Halide::Generator<Wrappee> {
public:
    GeneratorParam<Type> input_type{ "input_type", UInt(8) };
    GeneratorParam<Type> output_type{ "output_type", Float(32) };
    GeneratorParam<int> array_count{ "array_count", 2 };
    GeneratorParam<BagType> bag_type{ "bag_type",
                                      BagType::Paper,
                                      { { "paper", BagType::Paper },
                                        { "plastic", BagType::Plastic } } };

    ScheduleParam<bool> vectorize{ "vectorize", true };
    ScheduleParam<LoopLevel> intermediate_level{ "intermediate_level", "undefined" };

    Input<Func[]> input{ array_count, "input", input_type, 3 };
    Input<float> float_arg{ "float_arg", 1.0f, 0.0f, 100.0f }; 
    Input<int32_t[]> int_arg{ array_count, "int_arg", 1 };

    Output<Func> f{"f", {input_type, output_type}, 3};
    Output<Func[]> g{ array_count, "g", Int(16), 2};

    void generate() {
        assert(array_count >= 1);

        // Gratuitous intermediate for the purpose of exercising
        // ScheduleParam<LoopLevel>
        intermediate(x, y, c) = input[0](x, y, c) * float_arg;

        f(x, y, c) = Tuple(
                intermediate(x, y, c),
                cast(output_type, intermediate(x, y, c) + int_arg[0]));

        for (size_t i = 0; i < input.size(); ++i) {
            g[i](x, y) = cast<int16_t>(input[i](x, y, 0) + int_arg[i]);
        }
    }

    void schedule() {
        intermediate.compute_at(intermediate_level);
        if (vectorize) intermediate.vectorize(x, natural_vector_size<float>());
    }

private:
    Var x{"x"}, y{"y"}, c{"c"};

    Func intermediate{"intermediate"};
};

}  // namespace

namespace WrapNS1 {
namespace WrapNS2 {

// must forward-declare the name we want for the wrapper, inside the proper namespace(s).
// None of the namespace(s) may be anonymous (if you do, failures will occur at Halide
// compilation time).
class Wrapper;

// If the fully-qualified wrapper name specified for third argument hasn't been declared
// properly, a compile error will result. The fully-qualified name *must* have at least one
// namespace (i.e., a name at global scope is not acceptable). The class may not be inside
// anonymous namespace(s), o
auto register_me = HALIDE_REGISTER_GENERATOR(Wrappee, "wraptest", WrapNS1::WrapNS2::Wrapper);

}  // namespace WrapNS2
}  // namespace WrapNS1
