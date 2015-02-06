/*{{{
    Copyright © 2015 Matthias Kretz <kretz@kde.org>

    Permission to use, copy, modify, and distribute this software
    and its documentation for any purpose and without fee is hereby
    granted, provided that the above copyright notice appear in all
    copies and that both that the copyright notice and this
    permission notice and warranty disclaimer appear in supporting
    documentation, and that the name of the author not be used in
    advertising or publicity pertaining to distribution of the
    software without specific, written prior permission.

    The author disclaim all warranties with regard to this
    software, including all implied warranties of merchantability
    and fitness.  In no event shall the author be liable for any
    special, indirect or consequential damages or any damages
    whatsoever resulting from loss of use, data or profits, whether
    in an action of contract, negligence or other tortious action,
    arising out of or in connection with the use or performance of
    this software.

}}}*/
// includes {{{1
#include <array>
#include <iostream>
#include <iomanip>
#include <random>
#include "../tsc.h"
#include "spline.h"
#include "spline2.h"
#include "spline3.h"

// settings {{{1
constexpr int NumberOfEvaluations = 10000;
constexpr int FirstMapSize = 4;
constexpr int MaxMapSize = 256;
constexpr int MinRepeat = 100;
constexpr auto StepMultiplier = 1.25;

enum DisabledTests {
    DisabledTestsBegin = -999999,
    DisabledTestsEnd
};
enum EnabledTests {
    Scalar,
    Vectorized,
    Vec2,
    Vec16,
    Vec12,
    Horizontal,
    Horizontal2,
    NBenchmarks
};

// types {{{1
typedef Spline::Point2 Point2;
typedef Spline::Point3 Point3;
typedef Spline::Point2V Point2V;
typedef Spline::Point3V Point3V;

// operator<< overloads {{{1
std::ostream &operator<<(std::ostream &s, const Point2 &xyz)
{
    using std::setw;
    return s << '[' << setw(7) << xyz[0] << ", " << setw(7) << xyz[1] << ']';
}
std::ostream &operator<<(std::ostream &s, const Point2V &xyz)
{
    return s << '[' << xyz[0] << ", " << xyz[1] << ']';
}
std::ostream &operator<<(std::ostream &s, const Point3 &xyz)
{
    using std::setw;
    return s << '[' << setw(7) << xyz[0] << ", " << setw(7) << xyz[1] << ", " << setw(7)
             << xyz[2] << ']';
}
std::ostream &operator<<(std::ostream &s, const Point3V &xyz)
{
    return s << '[' << xyz[0] << ", " << xyz[1] << ", " << xyz[2] << ']';
}

// VectorizeBuffer {{{1
template <typename Input> struct VectorizeBuffer
{
    typedef simdize<Input> InputV;
    InputV input;
    int entries = 0;
    int operator()(Input x)
    {
        simdize_assign(input, entries, x);
        entries = (entries + 1) % InputV::size();
        return entries;
    }
};

// TestInfo {{{1
struct TestInfo
{
    bool enabled = false;
    int id = -1;
    TestInfo(EnabledTests t) : enabled(true), id(t) {}
    TestInfo(EnabledTests t, EnabledTests) : enabled(true), id(t) {}
    TestInfo(EnabledTests t, EnabledTests, EnabledTests) : enabled(true), id(t) {}
    TestInfo(EnabledTests t, EnabledTests, DisabledTests) : enabled(true), id(t) {}
    TestInfo(EnabledTests t, DisabledTests) : enabled(true), id(t) {}
    TestInfo(EnabledTests t, DisabledTests, EnabledTests) : enabled(true), id(t) {}
    TestInfo(EnabledTests t, DisabledTests, DisabledTests) : enabled(true), id(t) {}

    TestInfo(DisabledTests) : enabled(false) {}
    TestInfo(DisabledTests, EnabledTests t) : enabled(true), id(t) {}
    TestInfo(DisabledTests, DisabledTests) : enabled(false) {}
    TestInfo(DisabledTests, EnabledTests t, EnabledTests) : enabled(true), id(t) {}
    TestInfo(DisabledTests, DisabledTests, EnabledTests t) : enabled(true), id(t) {}
    TestInfo(DisabledTests, DisabledTests, DisabledTests) : enabled(false) {}

    TestInfo(DisabledTests, EnabledTests t, DisabledTests) : enabled(true), id(t) {}

    operator bool() const { return enabled; }
    operator int() const { return id; }
    operator long() const { return id; }
};
// Runner Lambda {{{1
struct Runner
{
    // data members{{{2
    const int Repetitions = MinRepeat;
    const int MapSize;
    TimeStampCounter tsc;
    double mean[NBenchmarks] = {};
    double stddev[NBenchmarks] = {};

    // Runner::Runner{{{2
    Runner(int R, int S) : Repetitions(R), MapSize(S) {}

    void recordTsc(int Test, double norm)  //{{{2
    {
        const double x = tsc.cycles() / norm;
        mean[Test] += x;
        stddev[Test] += x * x;
    }
    template <typename I, typename J> void printRatio(I i, J j)  //{{{2
    {
        if (TestInfo(i) && TestInfo(j)) {
            const auto ratio = mean[i] / mean[j];
            std::cout << std::setprecision(3) << std::setw(9) << ratio;
            std::cout << std::setprecision(3) << std::setw(9)
                      << ratio * std::sqrt(stddev[i] * stddev[i] / (mean[i] * mean[i]) +
                                           stddev[j] * stddev[j] / (mean[j] * mean[j]));
        }
    }
    // benchmarkSearch{{{2
    template <typename F> void benchmark(const TestInfo Test, F &&fun, double err = 20)
    {
        if (Test) {
            do {
                mean[Test] = 0;
                stddev[Test] = 0;

                fun(); // one cache warm-up run to remove one outlier
                for (auto rep = Repetitions; rep; --rep) {
                    tsc.start();
                    fun();
                    tsc.stop();
                    recordTsc(Test, NumberOfEvaluations);
                }

                mean[Test] /= Repetitions;
                stddev[Test] /= Repetitions;
                stddev[Test] = std::sqrt(stddev[Test] - mean[Test] * mean[Test]);
            } while (stddev[Test] * err > mean[Test]);
            std::cout << std::setw(9) << std::setprecision(3) << mean[Test];
            std::cout << std::setw(9) << std::setprecision(3) << stddev[Test];
            std::cout << std::flush;
        }
    }
    //}}}2
};
int main()  // {{{1
{
    // output header {{{2
    using std::cout;
    using std::setw;
    using std::setprecision;
    cout << "NumberOfEvaluations: " << NumberOfEvaluations << '\n';
    cout << "MinRepeat: " << MinRepeat << '\n';
    cout << setw(8) << "MapSize";
    if (TestInfo(Scalar)) {
        cout << setw(18) << "Scalar";
    }
    if (TestInfo(Vectorized)) {
        cout << setw(18) << "Vectorized";
    }
    if (TestInfo(Vec16)) {
        cout << setw(18) << "Vec16";
    }
    if (TestInfo(Vec2)) {
        cout << setw(18) << "Vec2";
    }
    if (TestInfo(Vec12)) {
        cout << setw(18) << "Vec12";
    }
    if (TestInfo(Horizontal)) {
        cout << setw(18) << "Horizontal";
    }
    if (TestInfo(Horizontal2)) {
        cout << setw(18) << "Horizontal2";
    }
    if (TestInfo(Scalar)) {
        for (int i = 0; i < NBenchmarks; ++i) {
            if (i != Scalar) {
                switch (i) {
                case Vectorized:
                    cout << setw(18) << "Scalar/Vectorized";
                    break;
                case Vec16:
                    cout << setw(18) << "Scalar/Vec16";
                    break;
                case Vec2:
                    cout << setw(18) << "Scalar/Vec2";
                    break;
                case Vec12:
                    cout << setw(18) << "Scalar/Vec12";
                    break;
                case Horizontal:
                    cout << setw(18) << "Scalar/Horizontal";
                    break;
                case Horizontal2:
                    cout << setw(18) << "Scalar/Horizontal2";
                    break;
                default:
                    cout << setw(18) << "Scalar/<unknown>";
                    break;
                }
            }
        }
    }
    cout << std::endl;

    // random number generator {{{2
    std::default_random_engine randomEngine(1);
    std::uniform_real_distribution<float> uniform(-1.f, 1.f);

    // random search points {{{2
    std::vector<Point2> searchPoints;
    searchPoints.reserve(NumberOfEvaluations);
    searchPoints.emplace_back(Point2{-1.f, -1.f});
    searchPoints.emplace_back(Point2{+1.f, +1.f});
    for (int i = 2; i < NumberOfEvaluations; ++i) {
        searchPoints.emplace_back(Point2{uniform(randomEngine), uniform(randomEngine)});
    }

    // MapSize loop {{{2
    for (int MapSize = FirstMapSize; MapSize <= MaxMapSize; MapSize *= StepMultiplier) {
        // initialize map with random values {{{2
        Spline spline(-1.f, 1.f, MapSize, -1.f, 1.f, MapSize);
        Spline2 spline2(-1.f, 1.f, MapSize, -1.f, 1.f, MapSize);
        Spline3 spline3(-1.f, 1.f, MapSize, -1.f, 1.f, MapSize);
        for (int i = 0; i < spline.GetNPoints(); ++i) {
            const float xyz[3] = {uniform(randomEngine), uniform(randomEngine),
                                  uniform(randomEngine)};
            spline.Fill(i, xyz);
            spline2.Fill(i, xyz);
            spline3.Fill(i, xyz);
        }

        Runner runner(MinRepeat, MapSize);
        cout << setw(8) << spline.GetMapSize() << std::flush;

        // Scalar {{{2
        runner.benchmark(Scalar, [&] {
            for (const auto &p : searchPoints) {
                const auto &p2 = spline.GetValueScalar(p);
                asm("" ::"m"(p2));
            }
        });

        // Vectorized {{{2
        runner.benchmark(Vectorized, [&] {
            for (const auto &p : searchPoints) {
                const auto &p2 = spline.GetValue(p);
                asm("" ::"m"(p2));
            }
        });

        // Vec16 {{{2
        runner.benchmark(Vec16, [&] {
            for (const auto &p : searchPoints) {
                const auto &p2 = spline.GetValue16(p);
                asm("" ::"m"(p2));
            }
        });

        // Vec2 {{{2
        runner.benchmark(Vec2, [&] {
            for (const auto &p : searchPoints) {
                const auto &p2 = spline2.GetValue(p);
                asm("" ::"m"(p2));
            }
        });

        // Vec12 {{{2
        runner.benchmark(Vec12, [&] {
            for (const auto &p : searchPoints) {
                const auto &p2 = spline3.GetValue(p);
                asm("" ::"m"(p2));
            }
        });

        // Horizontal {{{2
        runner.benchmark(Horizontal, [&] {
            VectorizeBuffer<Point2> vectorizer;
            for (const auto &p : searchPoints) {
                if (0 == vectorizer(p)) {
                    const auto &p2 = spline.GetValue(vectorizer.input);
                    asm("" ::"m"(p2));
                }
            }
        });

        // Horizontal2 {{{2
        runner.benchmark(Horizontal2, [&] {
            VectorizeBuffer<Point2> vectorizer;
            for (const auto &p : searchPoints) {
                if (0 == vectorizer(p)) {
                    const auto &p2 = spline2.GetValue(vectorizer.input);
                    asm("" ::"m"(p2));
                }
            }
        });

        // print search timings {{{2
        if (TestInfo(Scalar)) {
            for (int i = 0; i < NBenchmarks; ++i) {
                if (i != Scalar) {
                    runner.printRatio(Scalar, static_cast<EnabledTests>(i));
                }
            }
        }
        cout << std::flush;

        // verify equivalence {{{2
        {
            bool failed = false;
            VectorizeBuffer<Point2> vectorizer2;
            VectorizeBuffer<Point3> vectorizer3;
            for (const auto &p : searchPoints) {
                const auto &ps = spline.GetValueScalar(p);
                if (TestInfo(Vectorized)) {  //{{{3
                    const auto &pv = spline.GetValue(p);
                    for (int i = 0; i < 3; ++i) {
                        if (std::abs(ps[i] - pv[i]) > 0.00001f) {
                            std::cout << "\nVectorized not equal at " << p << ": " << ps
                                      << " vs. " << pv;
                            failed = true;
                            break;
                        }
                    }
                }
                if (TestInfo(Vec16)) {  //{{{3
                    const auto &pv = spline.GetValue16(p);
                    for (int i = 0; i < 3; ++i) {
                        if (std::abs(ps[i] - pv[i]) > 0.00001f) {
                            std::cout << "\nVec16 not equal at " << p << ": " << ps
                                      << " vs. " << pv;
                            failed = true;
                            break;
                        }
                    }
                }
                if (TestInfo(Vec2)) {  //{{{3
                    const auto &pv = spline2.GetValue(p);
                    for (int i = 0; i < 3; ++i) {
                        if (std::abs(ps[i] - pv[i]) > 0.00001f) {
                            std::cout << "\nVec2 not equal at " << p << ": " << ps
                                      << " vs. " << pv;
                            failed = true;
                            break;
                        }
                    }
                }
                if (TestInfo(Vec12)) {  //{{{3
                    const auto &pv = spline3.GetValue(p);
                    for (int i = 0; i < 3; ++i) {
                        if (std::abs(ps[i] - pv[i]) > 0.00001f) {
                            std::cout << "\nVec12 not equal at " << p << ": " << ps
                                      << " vs. " << pv;
                            failed = true;
                            break;
                        }
                    }
                }
                vectorizer3(ps);
                if (0 == vectorizer2(p)) {
                    if (TestInfo(Horizontal)) {  //{{{3
                        const auto &pv = spline.GetValue(vectorizer2.input);
                        for (int i = 0; i < 3; ++i) {
                            if (any_of(abs(vectorizer3.input[i] - pv[i]) > 0.00001f)) {
                                cout << "\nHorizontal not equal at " << vectorizer2.input
                                     << ": " << vectorizer3.input << " vs. " << pv;
                                failed = true;
                                break;
                            }
                        }
                    }
                    if (TestInfo(Horizontal2)) {  //{{{3
                        const auto &pv = spline2.GetValue(vectorizer2.input);
                        for (int i = 0; i < 3; ++i) {
                            if (any_of(abs(vectorizer3.input[i] - pv[i]) > 0.00001f)) {
                                cout << "\nHorizontal2 not equal at \n" << vectorizer2.input
                                     << ":\n" << vectorizer3.input << " vs.\n" << pv;
                                failed = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (failed) {
                //std::cout << '\n' << spline. << '\n';
                return 1;
            } else {
                cout << " ✓";
            }
        }
        cout << std::endl;
    }
    return 0;
}  // }}}1

// vim: foldmethod=marker
