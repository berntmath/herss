#include <gtest/gtest.h>
#include "herss.h"
#include "logger.h"
#include "optimizer.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <memory>
#include <unistd.h>

// HERSS-AD: Adam, the objective L = -VF with its gradient, and the optimization loop.
//
// AdamOptimizer is tested on analytic problems. HerssObjective and RunAdam are tested on complete HERSS runs: the data
// folders under data/ are copied to a temporary folder, read exactly as main.cpp does, and evaluated in one process.

namespace fs = std::filesystem;

namespace {

// Absolute, because the tests change the working directory
const fs::path DATA_DIR = fs::weakly_canonical(fs::absolute(fs::path(__FILE__).parent_path().parent_path() / "data"));

// A river system that is read like main.cpp does it. The data folder is copied, because the working directory is the
// folder of the global file (the input files are found relative to it).
class Pipeline {
public:
    // extra_global: lines that are appended to the global file (for example the optimizer keywords)
    Pipeline(const fs::path &data_folder, const std::string &global_file, const std::string &extra_global = "") {
        Logger::instance().setQuiet(true);

        tmp_ = fs::temp_directory_path() / ("herss_opt_test_" + std::to_string(getpid()) + "_" + std::to_string(counter_++));
        fs::create_directories(tmp_);
        fs::copy(data_folder, tmp_, fs::copy_options::recursive);
        fs::create_directories(tmp_ / "output");

        old_cwd_ = fs::current_path();
        fs::current_path(tmp_);

        if(!extra_global.empty()) {
            std::ofstream g(global_file, std::ios::app);
            g << "\n" << extra_global << "\n";
        }

        gc = new GlobalConfig();
        gc->globalfile = global_file;
        gc->readGlobalFile();
        gc->SetDirectoriesAndFilenames();
        gc->Diagnose();
        gc->checkNrSteps();
        data = new Dataset(gc);
        data->readAllData();
        herss = new Herss(gc);
        herss->prepaireSimulation(data);
        herss->rs->DiagnoseRiversystemConfiguration();
    }

    ~Pipeline() {
        delete herss;
        delete data;
        delete gc;
        fs::current_path(old_cwd_);
        std::error_code ec;
        fs::remove_all(tmp_, ec);
        Logger::instance().setQuiet(false);
    }

    GlobalConfig *gc = nullptr;
    Dataset *data = nullptr;
    Herss *herss = nullptr;

private:
    fs::path tmp_, old_cwd_;
    static inline int counter_ = 0;
};

const std::vector<std::string> AD_CASES = {
    "A2_drawdown", "A3_fill_crest", "A4_flood", "A5_startstop", "A7_flood_hourly", "A10_qmin_channel", "A11_qmin_channel_lagged"
};

std::unique_ptr<Pipeline> loadCase(const std::string &name) {
    return std::make_unique<Pipeline>(DATA_DIR / "ad_tests" / name, "global.txt");
}

// A different, deterministic set of actions in (0.05, 0.95)
std::vector<double> otherActions(size_t n, double phase) {
    std::vector<double> x(n);
    for(size_t i = 0; i < n; ++i) x[i] = 0.5 + 0.45 * std::sin(0.37 * double(i) + phase);
    return x;
}

double dot(const std::vector<double> &a, const std::vector<double> &b) {
    double s = 0.0;
    for(size_t i = 0; i < a.size(); ++i) s += a[i] * b[i];
    return s;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// AdamOptimizer
// ---------------------------------------------------------------------------------------------------------------
TEST(AdamOptimizer, FirstStepIsTheLearningRateAgainstTheSignOfTheGradient) {
    // With bias correction m_hat / sqrt(v_hat) = g / |g| at the first step, whatever the size of g
    AdamOptimizer adam;
    adam.learning_rate = 0.01;
    std::vector<double> x = {0.5, 0.5, 0.5, 0.5};
    adam.step(x, {1.0e6, -1.0e-3, 5.0, 0.0}, 0.0, 1.0);
    EXPECT_NEAR(x[0], 0.49, 1.0e-9);
    EXPECT_NEAR(x[1], 0.51, 1.0e-6);
    EXPECT_NEAR(x[2], 0.49, 1.0e-9);
    EXPECT_DOUBLE_EQ(x[3], 0.5);      // zero gradient, no move
    EXPECT_EQ(adam.steps(), 1u);
}

TEST(AdamOptimizer, MatchesAHandComputedSecondStep) {
    AdamOptimizer adam;
    adam.learning_rate = 0.1;
    std::vector<double> x = {0.0};
    x[0] = 0.5;
    adam.step(x, {2.0}, 0.0, 1.0);          // m = 0.2, v = 0.004, m_hat = 2, v_hat = 4 -> x = 0.5 - 0.1
    ASSERT_NEAR(x[0], 0.4, 1.0e-8);
    adam.step(x, {-1.0}, 0.0, 1.0);
    // m = 0.9 * 0.2 - 0.1 = 0.08, v = 0.999 * 0.004 + 0.001 = 0.004996
    // m_hat = 0.08 / (1 - 0.81) = 0.42105263, v_hat = 0.004996 / (1 - 0.998001) = 2.4992496
    const double expected = 0.4 - 0.1 * (0.08 / 0.19) / (std::sqrt(0.004996 / 0.001999) + 1.0e-8);
    EXPECT_NEAR(x[0], expected, 1.0e-9);
}

TEST(AdamOptimizer, MinimizesAQuadraticWithBounds) {
    // f = sum c_i (x_i - target_i)^2 with targets inside and outside [0, 1] and very different curvatures.
    const std::vector<double> target = {0.3, 0.8, -0.5, 1.7, 0.5};
    const std::vector<double> c      = {1.0, 50.0, 3.0, 0.01, 1.0e4};
    std::vector<double> x(5, 0.5);
    AdamOptimizer adam;
    adam.learning_rate = 0.02;
    for(int k = 0; k < 2000; ++k) {
        if(k == 1000) adam.learning_rate = 0.002;
        std::vector<double> g(5);
        for(size_t i = 0; i < 5; ++i) g[i] = 2.0 * c[i] * (x[i] - target[i]);
        adam.step(x, g, 0.0, 1.0);
    }
    EXPECT_NEAR(x[0], 0.3, 2.0e-3);
    EXPECT_NEAR(x[1], 0.8, 2.0e-3);
    EXPECT_DOUBLE_EQ(x[2], 0.0);          // clipped at the lower bound
    EXPECT_DOUBLE_EQ(x[3], 1.0);          // clipped at the upper bound
    EXPECT_NEAR(x[4], 0.5, 2.0e-3);
}

TEST(AdamOptimizer, ResetClearsTheMemory) {
    AdamOptimizer adam;
    adam.learning_rate = 0.01;
    std::vector<double> a = {0.5}, b = {0.5};
    adam.step(a, {3.0}, 0.0, 1.0);
    adam.step(a, {3.0}, 0.0, 1.0);
    adam.reset(1);
    EXPECT_EQ(adam.steps(), 0u);
    adam.step(b, {3.0}, 0.0, 1.0);
    EXPECT_NEAR(b[0], 0.49, 1.0e-9);      // like a first step
}

// ---------------------------------------------------------------------------------------------------------------
// HerssObjective
// ---------------------------------------------------------------------------------------------------------------
TEST(HerssObjective, FindsTheGeneratorActions) {
    auto p = loadCase("A10_qmin_channel");
    HerssObjective obj(p->herss);
    EXPECT_EQ(obj.size(), 72u);                       // one generator, 72 hourly steps
    EXPECT_EQ(obj.controls()[5].node, 1u);
    EXPECT_EQ(obj.controls()[5].t, 5u);
    EXPECT_NEAR(obj.getActions()[0], 0.30, 1.0e-6);   // From actions.txt
}

TEST(HerssObjective, ReturnsMinusTheValueFunction) {
    auto p = loadCase("A10_qmin_channel");
    HerssObjective obj(p->herss);
    std::vector<double> g;
    const double L = obj.evaluate(obj.getActions(), &g);
    EXPECT_DOUBLE_EQ(L, -p->herss->rs->valuefunction_Euro);
    EXPECT_DOUBLE_EQ(L, -obj.valueFunction());
    EXPECT_GT(obj.valueFunction(), 1.0e4);
}

// Evaluations must not depend on what was run before: the optimizer runs the simulation many times in one process.
TEST(HerssObjective, EvaluationsAreReproducibleInOneProcess) {
    for(const std::string &name : AD_CASES) {
        auto p = loadCase(name);
        HerssObjective obj(p->herss);
        const std::vector<double> x0 = obj.getActions();
        const std::vector<double> x1 = otherActions(obj.size(), 0.0);

        std::vector<double> g0a, g0b, g1, g0c;
        const double L0a = obj.evaluate(x0, &g0a);
        const double L0b = obj.evaluate(x0, &g0b);            // Same input again
        const double L1  = obj.evaluate(x1, &g1);             // Something else in between
        const double L0c = obj.evaluate(x0, &g0c);            // And back

        EXPECT_EQ(L0a, L0b) << name;
        EXPECT_EQ(L0a, L0c) << name << ": the result depends on the previous evaluation";
        EXPECT_NE(L0a, L1) << name;
        for(size_t i = 0; i < g0a.size(); ++i) {
            ASSERT_EQ(g0a[i], g0b[i]) << name << ", gradient " << i;
            ASSERT_EQ(g0a[i], g0c[i]) << name << ", gradient " << i << ": depends on the previous evaluation";
        }
    }
}

TEST(HerssObjective, GradientAndFunctionValueAreIndependentOfAskingForTheGradient) {
    auto p = loadCase("A2_drawdown");
    HerssObjective obj(p->herss);
    const std::vector<double> x = otherActions(obj.size(), 1.0);
    std::vector<double> g;
    const double with    = obj.evaluate(x, &g);
    const double without = obj.evaluate(x, nullptr);
    EXPECT_EQ(with, without);
}

// In-process finite differences have no float32 limit on the step, since the actions are set directly.
TEST(HerssObjective, GradientMatchesFiniteDifferences) {
    for(const std::string &name : AD_CASES) {
        auto p = loadCase(name);
        HerssObjective obj(p->herss);
        const std::vector<double> x = otherActions(obj.size(), 0.4);   // Actions away from the start/stop zone and the bounds
        std::vector<double> g;
        obj.evaluate(x, &g);

        const double h = 1.0e-6;
        for(size_t i = 1; i < obj.size(); i += std::max<size_t>(1, obj.size() / 7)) {
            std::vector<double> xp = x, xm = x;
            xp[i] += h;  xm[i] -= h;
            const double fd = (obj.evaluate(xp, nullptr) - obj.evaluate(xm, nullptr)) / (2.0 * h);
            EXPECT_NEAR(g[i], fd, 2.0e-5 * std::max(1.0, std::fabs(fd))) << name << ", control " << i;
        }
    }
}

TEST(HerssObjective, MinusTheGradientIsADescentDirection) {
    // The sign convention: L = -VF decreases (VF increases) along -g
    for(const std::string &name : AD_CASES) {
        auto p = loadCase(name);
        HerssObjective obj(p->herss);
        const std::vector<double> x = otherActions(obj.size(), 2.0);
        std::vector<double> g;
        const double L0 = obj.evaluate(x, &g);
        ASSERT_GT(dot(g, g), 0.0) << name;

        std::vector<double> x1 = x;
        const double step = 1.0e-5 / std::sqrt(dot(g, g));
        for(size_t i = 0; i < x1.size(); ++i) x1[i] -= step * g[i];
        const double L1 = obj.evaluate(x1, nullptr);
        EXPECT_LT(L1, L0) << name;
        EXPECT_NEAR(L1 - L0, -step * dot(g, g), 1.0e-3 * step * dot(g, g)) << name << ": first order change";
    }
}

// ---------------------------------------------------------------------------------------------------------------
// RunAdam
// ---------------------------------------------------------------------------------------------------------------
TEST(RunAdam, IncreasesTheValueFunctionAndReportsAConsistentResult) {
    auto p = loadCase("A10_qmin_channel");
    HerssObjective obj(p->herss);

    AdamOptions opt;
    opt.max_iterations = 150;
    opt.learning_rate  = 0.02;
    std::vector<double> x0(obj.size(), 0.3);

    std::vector<double> g0;
    const double L_start = obj.evaluate(x0, &g0);
    const AdamResult r = RunAdam(obj, x0, opt);

    EXPECT_NEAR(r.start_value_Euro, -L_start, 1.0e-9);
    EXPECT_GT(r.best_value_Euro, r.start_value_Euro + 100.0);
    EXPECT_EQ(r.history_Euro.size(), r.iterations);
    EXPECT_EQ(r.history_gradient_norm.size(), r.iterations);
    for(double v : r.history_Euro) EXPECT_LE(v, r.best_value_Euro);

    // The best value is the value of the returned actions, also after a fresh evaluation, and they are inside the bounds.
    const double L_best = obj.evaluate(r.x, nullptr);
    EXPECT_NEAR(-L_best, r.best_value_Euro, 1.0e-9);
    for(double xi : r.x) {
        EXPECT_GE(xi, 0.0);
        EXPECT_LE(xi, 1.0);
    }
    // The simulation is left with the best actions
    EXPECT_NEAR(obj.valueFunction(), r.best_value_Euro, 1.0e-9);
}

TEST(RunAdam, ReachesAStationaryPointOnASmallProblem) {
    // 24 hourly steps of the A10 data would be small; use the whole case and many iterations. At the end the gradient of
    // the free actions must be small compared to the start, and no feasible small move may improve the value.
    auto p = loadCase("A5_startstop");
    HerssObjective obj(p->herss);

    AdamOptions opt;
    opt.max_iterations = 600;
    opt.learning_rate  = 0.02;
    std::vector<double> x0(obj.size(), 0.4);
    std::vector<double> g0;
    obj.evaluate(x0, &g0);
    double n0 = 0.0;
    for(double v : g0) n0 += v * v;

    const AdamResult r = RunAdam(obj, x0, opt);
    EXPECT_GT(r.best_value_Euro, r.start_value_Euro);
    EXPECT_LT(r.projected_gradient_norm, 0.2 * std::sqrt(n0));

    // Local optimality: random small feasible perturbations do not increase the value
    for(int trial = 0; trial < 5; ++trial) {
        std::vector<double> xp = r.x;
        for(size_t i = 0; i < xp.size(); ++i) {
            const double d = 2.0e-3 * std::sin(1.3 * double(i) + 7.0 * trial);
            xp[i] = std::min(1.0, std::max(0.0, xp[i] + d));
        }
        EXPECT_LE(-obj.evaluate(xp, nullptr), r.best_value_Euro + 1.0e-3) << "perturbation " << trial;
    }
}

TEST(RunAdam, StopsWhenTheGradientOfTheFreeActionsIsSmall) {
    auto p = loadCase("A10_qmin_channel");
    HerssObjective obj(p->herss);
    AdamOptions opt;
    opt.max_iterations = 1000;
    opt.grad_tolerance = 1.0e12;      // Larger than any gradient, so it stops after the first evaluation
    const AdamResult r = RunAdam(obj, std::vector<double>(obj.size(), 0.3), opt);
    EXPECT_EQ(r.iterations, 1u);
    ASSERT_EQ(r.history_gradient_norm.size(), 1u);
    EXPECT_DOUBLE_EQ(r.history_gradient_norm.front(), r.projected_gradient_norm);
    EXPECT_DOUBLE_EQ(r.history_Euro.front(), r.start_value_Euro);

    // A tight tolerance is not met far from a stationary point, so all iterations run
    opt.grad_tolerance = 1.0e-6;
    AdamOptions o2 = opt;
    o2.max_iterations = 3;
    const AdamResult r2 = RunAdam(obj, std::vector<double>(obj.size(), 0.3), o2);
    EXPECT_EQ(r2.iterations, 3u);
}

TEST(RunAdam, StopsEarlyWhenNothingImproves) {
    auto p = loadCase("A10_qmin_channel");
    HerssObjective obj(p->herss);
    AdamOptions opt;
    opt.max_iterations = 100000;
    opt.learning_rate  = 0.0;         // x never moves, so the value never improves
    opt.patience       = 5;
    const AdamResult r = RunAdam(obj, std::vector<double>(obj.size(), 0.3), opt);
    EXPECT_LE(r.iterations, 7u);
    EXPECT_EQ(r.history_Euro.size(), r.iterations);
    EXPECT_EQ(r.history_gradient_norm.size(), r.iterations);
    EXPECT_EQ(r.evaluations, r.iterations + 1);
}

TEST(RunAdam, RecordsTheValueAndProjectedNormBeforeEveryUpdate) {
    Pipeline p(DATA_DIR / "ad_utahps_hourly", "global_utahps_hourly.txt");
    HerssObjective obj(p.herss);
    AdamOptions opt;
    opt.max_iterations = 3;
    std::vector<double> x(obj.size(), 0.3);
    x.front() = opt.lower;
    x.back() = opt.upper;
    const AdamResult r = RunAdam(obj, x, opt);
    ASSERT_EQ(r.history_Euro.size(), opt.max_iterations);
    ASSERT_EQ(r.history_gradient_norm.size(), opt.max_iterations);

    AdamOptimizer adam;
    adam.reset(x.size());
    for(size_t k = 0; k < r.iterations; ++k) {
        std::vector<double> g;
        const double vf = -obj.evaluate(x, &g);
        double sum = 0.0;
        for(size_t i = 0; i < x.size(); ++i) {
            if(x[i] == opt.lower && g[i] >= 0.0) continue;
            if(x[i] == opt.upper && g[i] <= 0.0) continue;
            sum += g[i] * g[i];
        }
        EXPECT_DOUBLE_EQ(r.history_Euro[k], vf);
        EXPECT_DOUBLE_EQ(r.history_gradient_norm[k], std::sqrt(sum));
        const double progress = double(k) / double(opt.max_iterations - 1);
        adam.learning_rate = opt.learning_rate
            * (opt.lr_final_fraction + (1.0 - opt.lr_final_fraction) * 0.5 * (1.0 + std::cos(PI * progress)));
        adam.step(x, g, opt.lower, opt.upper);
    }

    opt.max_iterations = 10;
    opt.patience = 2;
    opt.learning_rate = 0.0;
    const size_t prior_evaluations = obj.evaluations();
    const AdamResult stopped = RunAdam(obj, x, opt);
    EXPECT_EQ(stopped.iterations, 3u);
    EXPECT_EQ(stopped.evaluations - prior_evaluations, stopped.iterations + 1);
    ASSERT_EQ(stopped.history_Euro.size(), stopped.iterations);
    ASSERT_EQ(stopped.history_gradient_norm.size(), stopped.iterations);
    for(size_t k = 0; k < stopped.iterations; ++k) {
        EXPECT_DOUBLE_EQ(stopped.history_Euro[k], stopped.start_value_Euro);
        EXPECT_DOUBLE_EQ(stopped.history_gradient_norm[k], stopped.projected_gradient_norm);
    }
}

TEST(HerssObjectiveYear, WritesPairedHistoryIncludingTheStoppingIteration) {
    Pipeline p(DATA_DIR / "ad_utahps_hourly", "global_utahps_hourly.txt",
               "MAX_ITERATIONS 3\nSTART_ACTIONS 0.3\n");
    HerssObjective obj(p.herss);
    AdamOptions opt;
    opt.max_iterations = 3;
    const AdamResult expected = RunAdam(obj, std::vector<double>(obj.size(), 0.3), opt);
    const fs::path history_file = fs::path(p.gc->outputdir) / ("adam_history_" + p.gc->systemname + ".txt");

    RunAdamOptimization(p.herss, p.gc);
    std::ifstream history(history_file);
    ASSERT_TRUE(history.is_open());
    std::string line;
    std::getline(history, line);
    EXPECT_EQ(line, "# iteration valuefunction_Euro projected_gradient_norm");
    for(size_t k = 0; k < expected.iterations; ++k) {
        ASSERT_TRUE(bool(std::getline(history, line)));
        std::istringstream row(line);
        size_t iteration;
        double vf, norm;
        ASSERT_TRUE(bool(row >> iteration >> vf >> norm));
        std::string extra;
        EXPECT_FALSE(bool(row >> extra));
        EXPECT_EQ(iteration, k);
        EXPECT_DOUBLE_EQ(vf, expected.history_Euro[k]);
        EXPECT_DOUBLE_EQ(norm, expected.history_gradient_norm[k]);
    }
    EXPECT_FALSE(bool(std::getline(history, line)));
    EXPECT_DOUBLE_EQ(p.herss->rs->valuefunction_Euro, expected.best_value_Euro);
    EXPECT_EQ(obj.getActions(), expected.x);
    history.close();

    // Overwriting a previous history must not leave stale rows after early stopping.
    p.gc->adam_grad_tolerance = 1.0e12;
    RunAdamOptimization(p.herss, p.gc);
    history.open(history_file);
    ASSERT_TRUE(bool(std::getline(history, line)));
    ASSERT_TRUE(bool(std::getline(history, line)));
    std::istringstream row(line);
    size_t iteration;
    double vf, norm;
    ASSERT_TRUE(bool(row >> iteration >> vf >> norm));
    EXPECT_EQ(iteration, 0u);
    EXPECT_DOUBLE_EQ(vf, expected.start_value_Euro);
    EXPECT_DOUBLE_EQ(norm, expected.history_gradient_norm.front());
    EXPECT_FALSE(bool(std::getline(history, line)));
}

TEST(HerssObjectiveYear, ReportsOptimizerHistoryOutputFailures) {
    Pipeline p(DATA_DIR / "ad_utahps_hourly", "global_utahps_hourly.txt",
               "MAX_ITERATIONS 1\nSTART_ACTIONS 0.3\n");
    const fs::path history_file = fs::path(p.gc->outputdir) / ("adam_history_" + p.gc->systemname + ".txt");
    fs::remove(history_file);
    fs::create_directory(history_file);
    ASSERT_EXIT(RunAdamOptimization(p.herss, p.gc), ::testing::ExitedWithCode(EXIT_FAILURE), "");
    if(fs::exists("/dev/full")) {
        fs::remove(history_file);
        fs::create_symlink("/dev/full", history_file);
        ASSERT_EXIT(RunAdamOptimization(p.herss, p.gc), ::testing::ExitedWithCode(EXIT_FAILURE), "");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// A full year at hourly resolution, with four powerstations and cascaded channels.
// ---------------------------------------------------------------------------------------------------------------
TEST(HerssObjectiveYear, ReproducibleAndFastEnough) {
    Pipeline p(DATA_DIR / "ad_utahps_hourly", "global_utahps_hourly.txt");
    HerssObjective obj(p.herss);
    EXPECT_EQ(obj.size(), 4u * 8760u);

    // Actions of the dataset are far above the inflow. Use a constant, feasible schedule.
    const std::vector<double> x(obj.size(), 0.1);

    using clk = std::chrono::steady_clock;
    std::vector<double> ga, gb;
    const auto t0 = clk::now();
    const double La = obj.evaluate(x, &ga);
    const auto t1 = clk::now();
    const double Lb = obj.evaluate(x, &gb);
    const double Lc = obj.evaluate(otherActions(obj.size(), 0.0), nullptr);
    const double Ld = obj.evaluate(x, nullptr);
    const auto t2 = clk::now();

    EXPECT_EQ(La, Lb);
    EXPECT_EQ(La, Ld);
    EXPECT_NE(La, Lc);
    EXPECT_TRUE(ga == gb);
    for(double v : ga) ASSERT_TRUE(std::isfinite(v));

    const double ms_with_gradient = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double ms_forward       = std::chrono::duration<double, std::milli>(t2 - t1).count() / 3.0;
    std::printf("    one year, hourly, 4 stations: forward + backward %.1f ms, mean of the following evaluations %.1f ms\n",
                ms_with_gradient, ms_forward);
    EXPECT_LT(ms_with_gradient, 2000.0);
}

// ---------------------------------------------------------------------------------------------------------------
// The keywords of the global file and RunAdamOptimization
// ---------------------------------------------------------------------------------------------------------------
TEST(AdamGlobalFile, DefaultsWhenTheKeywordsAreMissing) {
    auto p = loadCase("A10_qmin_channel");
    EXPECT_FALSE(p->gc->adam_optimizer);
    EXPECT_DOUBLE_EQ(p->gc->adam_learning_rate, 0.02);
    EXPECT_EQ(p->gc->adam_max_iterations, 300u);
    EXPECT_EQ(p->gc->adam_patience, 100u);
    EXPECT_DOUBLE_EQ(p->gc->adam_grad_tolerance, 0.0);
    EXPECT_TRUE(p->gc->adam_start_from_file);
    EXPECT_FALSE(p->gc->adam_verbose);
    EXPECT_EQ(p->gc->adam_actions_out, "./output/actions_optimized.txt");
}

TEST(AdamGlobalFile, ReadsTheKeywords) {
    Pipeline p(DATA_DIR / "ad_tests" / "A10_qmin_channel", "global.txt",
               "ADAM_OPTIMIZER TRUE\nLEARNING_RATE 0.01\nMAX_ITERATIONS 77\nPATIENCE 12\nGRAD_TOLERANCE 0.5\n"
               "START_ACTIONS 0.25\nOPTIMIZED_ACTIONFILE my_actions.txt\nADAM_VERBOSE TRUE\n");
    EXPECT_TRUE(p.gc->adam_optimizer);
    EXPECT_DOUBLE_EQ(p.gc->adam_learning_rate, 0.01);
    EXPECT_EQ(p.gc->adam_max_iterations, 77u);
    EXPECT_EQ(p.gc->adam_patience, 12u);
    EXPECT_DOUBLE_EQ(p.gc->adam_grad_tolerance, 0.5);
    EXPECT_FALSE(p.gc->adam_start_from_file);
    EXPECT_DOUBLE_EQ(p.gc->adam_start_constant, 0.25);
    EXPECT_EQ(p.gc->adam_actions_out, "./output/my_actions.txt");
    EXPECT_TRUE(p.gc->adam_verbose);

    Pipeline q(DATA_DIR / "ad_tests" / "A10_qmin_channel", "global.txt", "START_ACTIONS FILE\n");
    EXPECT_TRUE(q.gc->adam_start_from_file);
}

namespace {
std::vector<double> readActionColumn(const std::string &file, size_t col) {
    std::ifstream in(file);
    std::string line;
    std::getline(in, line);
    std::vector<double> v;
    while(std::getline(in, line)) {
        std::istringstream is(line);
        std::string tok;
        for(size_t c = 0; c <= col; ++c) is >> tok;
        v.push_back(std::stod(tok));
    }
    return v;
}
}

TEST(AdamGlobalFile, RunAdamOptimizationWritesTheActionsAndLeavesTheBestActionsInTheSimulation) {
    Pipeline p(DATA_DIR / "ad_tests" / "A10_qmin_channel", "global.txt",
               "ADAM_OPTIMIZER TRUE\nMAX_ITERATIONS 120\nSTART_ACTIONS 0.3\nOPTIMIZED_ACTIONFILE opt.txt\n");
    HerssObjective obj(p.herss);
    const double vf_start = -obj.evaluate(std::vector<double>(obj.size(), 0.3), nullptr);

    RunAdamOptimization(p.herss, p.gc);

    EXPECT_GT(p.herss->rs->valuefunction_Euro, vf_start + 100.0);

    const fs::path history_file = fs::path(p.gc->outputdir) / ("adam_history_" + p.gc->systemname + ".txt");
    std::ifstream history(history_file);
    ASSERT_TRUE(history.is_open());
    std::string header;
    std::getline(history, header);
    EXPECT_EQ(header, "# iteration valuefunction_Euro projected_gradient_norm");
    size_t row = 0, iteration;
    double vf, norm, best_vf = -1.0e300;
    while(history >> iteration >> vf >> norm) {
        EXPECT_EQ(iteration, row);
        EXPECT_TRUE(std::isfinite(vf));
        EXPECT_TRUE(std::isfinite(norm));
        EXPECT_GE(norm, 0.0);
        if(row == 0) {
            EXPECT_DOUBLE_EQ(vf, vf_start);
        }
        best_vf = std::max(best_vf, vf);
        ++row;
    }
    EXPECT_TRUE(history.eof());
    EXPECT_EQ(row, 120u);
    EXPECT_DOUBLE_EQ(best_vf, p.herss->rs->valuefunction_Euro);

    // The file is in the input format, with the actions of the simulation
    const std::vector<double> written = readActionColumn("./output/opt.txt", 1);
    const std::vector<double> in_sim  = obj.getActions();
    ASSERT_EQ(written.size(), in_sim.size());
    for(size_t i = 0; i < written.size(); ++i) EXPECT_NEAR(written[i], in_sim[i], 1.0e-8) << i;

    // Reading the written file back gives the same value function (round trip through the file format)
    fs::copy_file("./output/opt.txt", "./actions.txt", fs::copy_options::overwrite_existing);
    GlobalConfig gc2;
    gc2.globalfile = "global.txt";
    gc2.readGlobalFile();
    gc2.SetDirectoriesAndFilenames();
    gc2.Diagnose();
    gc2.checkNrSteps();
    Dataset data2(&gc2);
    data2.readAllData();
    Herss herss2(&gc2);
    herss2.prepaireSimulation(&data2);
    herss2.rs->DiagnoseRiversystemConfiguration();
    HerssObjective obj2(&herss2);
    EXPECT_NEAR(-obj2.evaluate(obj2.getActions(), nullptr), p.herss->rs->valuefunction_Euro, 1.0e-6);
}
