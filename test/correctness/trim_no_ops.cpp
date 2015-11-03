#include "Halide.h"

using namespace Halide;

class CountConditionals : public Internal::IRVisitor {
public:
    int count = 0;
    bool in_produce = false;
private:
    using Internal::IRVisitor::visit;

    void visit(const Internal::Select *op) {
        if (in_produce) count++;
        Internal::IRVisitor::visit(op);
    }

    void visit(const Internal::IfThenElse *op) {
        if (in_produce) count++;
        Internal::IRVisitor::visit(op);
    }

    void visit(const Internal::ProducerConsumer *op) {
        bool old_in_produce = in_produce;
        in_produce = true;
        Internal::IRVisitor::visit(op);
        in_produce = old_in_produce;
    }
};

int main(int argc, char **argv) {

    {
        // Loop iterations that would be no-ops should be trimmed off.
        Func f;
        Var x;
        f(x) = x;
        f(x) += select(x > 10 && x < 20, 1, 0);
        f(x) += select(x < 10, 0, 1);
        f(x) *= select(x > 20 && x < 30, 2, 1);
        f(x) = select(x >= 60 && x <= 100, 100 - f(x), f(x));
        Module m = f.compile_to_module({});

        // There should be no selects after trim_no_ops runs
        CountConditionals s;
        m.functions[0].body.accept(&s);
        if (s.count != 0) {
            std::cerr << "There were selects in the lowered code: " << m.functions[0].body << "\n";
            return -1;
        }

        // Also check the output is correct
        Image<int> im = f.realize(100);
        for (int x = 0; x < im.width(); x++) {
            int correct = x;
            correct += (x > 10 && x < 20) ? 1 : 0;
            correct += (x < 10) ? 0 : 1;
            correct *= (x > 20 && x < 30) ? 2 : 1;
            correct = (x >= 60 && x <= 100) ? (100 - correct) : correct;
            if (im(x) != correct) {
                printf("im(%d) = %d instead of %d\n",
                       x, im(x), correct);
                return -1;
            }
        }
    }

    {
        // Test a tiled histogram
        Func f;
        Var x, y;
        f(x, y) = cast<uint8_t>(random_int());
        f.compute_root();

        Func hist;
        {
            RDom r(0, 10, 0, 10, 0, 10, 0, 10);
            Expr xi = r[0] + r[2]*10, yi = r[1] + r[3]*10;
            hist(x) = 0;
            hist(f(clamp(xi, 0, 73), clamp(yi, 0, 73))) +=
                select(xi >= 0 && xi <= 73 && yi >= 0 && yi <= 73, likely(1), 0);

            Module m = hist.compile_to_module({});
            CountConditionals s;
            m.functions[0].body.accept(&s);
            if (s.count != 0) {
                std::cerr << "There were selects in the lowered code: " << m.functions[0].body << "\n";
                return -1;
            }
        }
        Image<int> hist_result = hist.realize(256);

        // Also check the output is correct.
        Func true_hist;
        {
            RDom r(0, 74, 0, 74);
            true_hist(x) = 0;
            true_hist(f(r.x, r.y)) += 1;
        }
        Image<int> true_hist_result = true_hist.realize(256);

        for (int i = 0; i < 256; i++) {
            if (hist_result(i) != true_hist_result(i)) {
                printf("hist(%d) = %d instead of %d\n",
                       i, hist_result(i), true_hist_result(i));
                return -1;
            }
        }
    }

    // Test tiled iteration over a triangle, where the condition is an
    // if statement instead of a select.
    {
        Func f;
        Var x, y;
        f(x, y) = select(2*x < y, 5, undef<int>());

        Var xi, yi;
        f.tile(x, y, xi, yi, 4, 4);

        // Check there are no if statements.
        Module m = f.compile_to_module({});
        CountConditionals s;
        m.functions[0].body.accept(&s);
        if (s.count != 0) {
            std::cerr << "There were selects or ifs in the lowered code: " << m.functions[0].body << "\n";
            return -1;
        }
    }

    printf("Success!\n");
    return 0;
}
