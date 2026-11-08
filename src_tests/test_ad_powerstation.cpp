#include <gtest/gtest.h>
#include "herss.h"
#include <cmath>
#include <functional>
#include <vector>

// HERSS-AD: Powerstation::Backward is checked against central finite differences of Powerstation::Simulate.
// The station is built by hand (no input files). The objective is
//     J = profit[t] + c_down * downstream.up_inflow[t]
// which is the profit seed (profit_grad = 1) plus an assumed downstream adjoint c_down for the released water.
// The inputs perturbed are the generator actions, the water arriving at the station (up_inflow) and
// the reservoir level (start/end of step), which are the quantities that Backward writes gradients for.

namespace {

const size_t STPS = 3;
const size_t T    = 1;       // Time step under test. t > 0 so the start/stop cost uses actions[t-1].
const double C_DOWN = 0.37;  // Adjoint of the downstream up_inflow

struct Station {
    Powerstation ps;
    Node         downstream;

    Station(bool shared, const std::vector<double> &max_discharge, const std::vector<double> &actions, double price) {
        ps.nodename = "TESTSTATION";
        ps.idnr     = 1;
        ps.nodetype = NodeType::PSTATION;
        ps.S        = new Scenario(STPS, 86400, 1);
        downstream.S = new Scenario(STPS, 86400, 2);
        ps.ptr_downstream_node = &downstream;

        ps.static_gen_efficiency   = 0.96;
        ps.headlosscoef            = 0.03;
        ps.powstat_masl            = 690.0;
        ps.powstat_startstop       = 2.0;
        ps.powstat_min_discharge   = 0.0;
        ps.init_Power              = 0.0;
        ps.aggressive_actions_cost = 0.0;
        ps.shared_penstock         = shared;
        ps.start_of_stp_masl       = 756.2;
        ps.end_of_stp_masl         = 755.8;

        ps.generators.resize(max_discharge.size());
        for (size_t g = 0; g < max_discharge.size(); ++g) {
            Generator &gen = ps.generators[g];
            gen.max_discharge              = max_discharge[g];
            gen.use_uniform_normalized_curve = false;
            gen.use_quadratic_eff_curve    = true;
            gen.a_quad = -1.610031;
            gen.b_quad = 2.478652;
            gen.c_quad = 0.000273;
            gen.action.assign(STPS, actions[g]);
        }

        for (size_t t = 0; t < STPS; ++t) {
            ps.S->price[t]     = price;
            ps.S->up_inflow[t] = 0.0;
        }
    }

    ~Station() {
        delete ps.S;
        delete downstream.S;
        ps.S = nullptr;
        downstream.S = nullptr;
    }

    double totalRequested() const {
        double total = 0.0;
        for (const Generator &gen : ps.generators) total += gen.action[T] * gen.max_discharge;
        return total;
    }

    double objective() {
        downstream.S->up_inflow[T] = 0.0;      // Simulate adds the flow to it
        ps.Simulate(T);
        return ps.S->profit[T] + C_DOWN * downstream.S->up_inflow[T];
    }
};

double centralDiff(Station &st, double &variable, double h) {
    const double v0 = variable;
    variable = v0 + h;
    const double jp = st.objective();
    variable = v0 - h;
    const double jm = st.objective();
    variable = v0;
    return (jp - jm) / (2.0 * h);
}

void runCase(bool shared, const std::vector<double> &max_q, const std::vector<double> &actions, bool scaled) {
    Station st(shared, max_q, actions, 41.0);

    // up_inflow below the requested flow gives the water-limited branch. Otherwise it is slightly above,
    // which keeps the station's inflow/outflow check satisfied for small perturbations.
    const double requested = st.totalRequested();
    st.ps.S->up_inflow[T] = scaled ? 0.8 * requested : requested + 5.0e-4;

    st.objective();   // Run once so any state is consistent

    // Analytic gradients
    st.ps.S->ClearGradients();
    st.downstream.S->ClearGradients();
    for (Generator &gen : st.ps.generators) gen.action_grad.assign(STPS, 0.0);
    st.ps.S->profit_grad[T]              = 1.0;
    st.downstream.S->up_inflow_grad[T]   = C_DOWN;
    st.ps.Backward(T);

    const double h = 1.0e-5;
    const double tol_rel = 1.0e-6, tol_abs = 1.0e-5;

    for (size_t g = 0; g < st.ps.generators.size(); ++g) {
        const double fd = centralDiff(st, st.ps.generators[g].action[T], h);
        const double an = st.ps.generators[g].action_grad[T];
        EXPECT_NEAR(an, fd, tol_abs + tol_rel * std::fabs(fd))
            << "action_grad generator " << g << " shared=" << shared << " scaled=" << scaled;
    }

    {   // Level at the start and the end of the step moves Hbrutto one to one
        const double v0s = st.ps.start_of_stp_masl, v0e = st.ps.end_of_stp_masl;
        st.ps.start_of_stp_masl = v0s + h;  st.ps.end_of_stp_masl = v0e + h;
        const double jp = st.objective();
        st.ps.start_of_stp_masl = v0s - h;  st.ps.end_of_stp_masl = v0e - h;
        const double jm = st.objective();
        st.ps.start_of_stp_masl = v0s;      st.ps.end_of_stp_masl = v0e;
        const double fd = (jp - jm) / (2.0 * h);
        EXPECT_NEAR(st.ps.S->Hbrutto_grad[T], fd, tol_abs + tol_rel * std::fabs(fd))
            << "Hbrutto_grad shared=" << shared << " scaled=" << scaled;
    }

    {   // Water arriving at the station. Only matters in the water-limited branch.
        const double fd = centralDiff(st, st.ps.S->up_inflow[T], h);
        EXPECT_NEAR(st.ps.S->up_inflow_grad[T], fd, tol_abs + tol_rel * std::fabs(fd))
            << "up_inflow_grad shared=" << shared << " scaled=" << scaled;
        if (!scaled) {
            EXPECT_EQ(st.ps.S->up_inflow_grad[T], 0.0);
        }
    }

    // Power_grad is the price (income = Power * price)
    EXPECT_NEAR(st.ps.S->Power_grad[T], 41.0, 1.0e-12);
}

}  // namespace

TEST(AdPowerstation, EfficiencyGradMatchesFiniteDifference) {
    Station st(true, {10.0}, {0.5}, 41.0);
    const double h = 1.0e-6;
    for (double q = 0.5; q <= 10.0; q += 0.7) {
        const double fd = (st.ps.calcEfficiency(0, q + h) - st.ps.calcEfficiency(0, q - h)) / (2.0 * h);
        EXPECT_NEAR(st.ps.calcEfficiencyGrad(0, q), fd, 1.0e-7) << "q=" << q;
    }
}

TEST(AdPowerstation, SharedPenstockOneGenerator)            { runCase(true,  {10.0},      {0.55},       false); }
TEST(AdPowerstation, SharedPenstockOneGeneratorScaled)      { runCase(true,  {10.0},      {0.55},       true);  }
TEST(AdPowerstation, SharedPenstockTwoGenerators)           { runCase(true,  {10.0, 6.0}, {0.55, 0.35}, false); }
TEST(AdPowerstation, SharedPenstockTwoGeneratorsScaled)     { runCase(true,  {10.0, 6.0}, {0.55, 0.35}, true);  }
TEST(AdPowerstation, SeparatePenstocksOneGenerator)         { runCase(false, {10.0},      {0.55},       false); }
TEST(AdPowerstation, SeparatePenstocksOneGeneratorScaled)   { runCase(false, {10.0},      {0.55},       true);  }
TEST(AdPowerstation, SeparatePenstocksTwoGenerators)        { runCase(false, {10.0, 6.0}, {0.55, 0.35}, false); }
TEST(AdPowerstation, SeparatePenstocksTwoGeneratorsScaled)  { runCase(false, {10.0, 6.0}, {0.55, 0.35}, true);  }

// ---------------------------------------------------------------------------------------------------------------
// Start/stop cost: (fee / 2) * (s_prev - s_cur)^2, with s = smooth_on(action) a smoothstep between
// START_STOP_THRESHOLD and START_STOP_THRESHOLD + START_STOP_WIDTH (0.01 to 0.03). Full transitions cost fee / 2 as before.

namespace {

const double FEE = 2.0;    // powstat_startstop of the Station

// Finite-difference estimate closest to the analytic gradient over a sweep of step sizes, central differences and their
// Richardson extrapolation. At the edges of the zone the second derivative of smooth_on jumps, so one step size is not enough.
double bestFd(const std::function<double()> &objective, double &variable, double analytic) {
    double best = 1.0e300;
    for (double h : { 1.0e-5, 1.0e-6, 1.0e-7, 1.0e-8 }) {
        auto central = [&](double step) {
            const double v0 = variable;
            variable = v0 + step;  const double jp = objective();
            variable = v0 - step;  const double jm = objective();
            variable = v0;
            return (jp - jm) / (2.0 * step);
        };
        const double d1 = central(h), d2 = central(0.5 * h);
        for (double estimate : { d1, (4.0 * d2 - d1) / 3.0 }) {
            if (std::fabs(estimate - analytic) < std::fabs(best - analytic)) best = estimate;
        }
    }
    return best;
}

// Builds a station whose generators have the action prev at every step except step, where they have cur.
struct StartStopSystem {
    Station st;
    size_t step;
    explicit StartStopSystem(const std::vector<double> &prev, const std::vector<double> &cur, size_t step_, double init_power = 0.0)
        : st(true, prev.size() == 1 ? std::vector<double>{10.0} : std::vector<double>{10.0, 6.0}, prev, 41.0), step(step_) {
        st.ps.powstat_startstop = FEE;
        st.ps.init_Power = init_power;
        double requested = 0.0;
        for (size_t g = 0; g < prev.size(); ++g) {
            Generator &gen = st.ps.generators[g];
            gen.action.assign(STPS, prev[g]);
            gen.action[step] = cur[g];
            requested += cur[g] * gen.max_discharge;
        }
        st.ps.S->up_inflow[step] = requested + 5.0e-4;     // The water arrives as requested: no scaling, balanced station
    }

    double objective() {
        st.downstream.S->up_inflow[step] = 0.0;
        st.ps.Simulate(step);
        return st.ps.S->profit[step] + C_DOWN * st.downstream.S->up_inflow[step];
    }
    double startStopCost() { objective(); return st.ps.S->startStopCost[step]; }
};

void checkStartStopGradient(const std::vector<double> &prev, const std::vector<double> &cur, size_t step, double init_power = 0.0) {
    StartStopSystem sys(prev, cur, step, init_power);
    sys.objective();

    sys.st.ps.S->ClearGradients();
    sys.st.downstream.S->ClearGradients();
    for (Generator &gen : sys.st.ps.generators) gen.action_grad.assign(STPS, 0.0);
    sys.st.ps.S->profit_grad[step] = 1.0;
    sys.st.downstream.S->up_inflow_grad[step] = C_DOWN;
    sys.st.ps.Backward(step);

    auto J = [&]() { return sys.objective(); };
    for (size_t g = 0; g < prev.size(); ++g) {
        Generator &gen = sys.st.ps.generators[g];
        const double tol_abs = 1.0e-6, tol_rel = 5.0e-6;
        double fd = bestFd(J, gen.action[step], gen.action_grad[step]);
        EXPECT_NEAR(gen.action_grad[step], fd, tol_abs + tol_rel * std::fabs(fd))
            << "action gradient of generator " << g << " at step " << step << ", prev " << prev[g] << ", cur " << cur[g];
        if (step > 0) {
            fd = bestFd(J, gen.action[step - 1], gen.action_grad[step - 1]);
            EXPECT_NEAR(gen.action_grad[step - 1], fd, tol_abs + tol_rel * std::fabs(fd))
                << "action gradient of generator " << g << " at step " << step - 1 << ", prev " << prev[g] << ", cur " << cur[g];
        }
    }
}

}  // namespace

TEST(AdStartStop, SmoothOnIsASmoothstepBetweenThresholdAndThresholdPlusWidth) {
    EXPECT_EQ(smooth_on(0.0), 0.0);
    EXPECT_EQ(smooth_on(START_STOP_THRESHOLD), 0.0);                                    // exactly the old threshold: off
    EXPECT_NEAR(smooth_on(START_STOP_THRESHOLD + 0.5 * START_STOP_WIDTH), 0.5, 1.0e-15);
    EXPECT_EQ(smooth_on(START_STOP_THRESHOLD + START_STOP_WIDTH), 1.0);
    EXPECT_EQ(smooth_on(1.0), 1.0);
    EXPECT_NEAR(smooth_on_d(START_STOP_THRESHOLD + 0.5 * START_STOP_WIDTH), 1.5 / START_STOP_WIDTH, 1.0e-12);   // peak slope
    EXPECT_EQ(smooth_on_d(START_STOP_THRESHOLD), 0.0);                                  // C1 at the lower edge
    EXPECT_NEAR(smooth_on_d(START_STOP_THRESHOLD + START_STOP_WIDTH), 0.0, 1.0e-10);   // C1 at both edges
    double previous = 0.0;
    for (double a = 0.0; a <= 0.05; a += 0.0005) {
        EXPECT_GE(smooth_on(a), previous - 1.0e-15) << "monotone at " << a;
        previous = smooth_on(a);
    }
    for (double a = 0.0105; a < 0.0295; a += 0.001) {
        const double h = 1.0e-7;
        EXPECT_NEAR(smooth_on_d(a), (smooth_on(a + h) - smooth_on(a - h)) / (2.0 * h), 1.0e-5) << "derivative at " << a;
    }
}

TEST(AdStartStop, FullTransitionsCostHalfTheFeeAsBefore) {
    EXPECT_NEAR(StartStopSystem({0.0}, {0.5}, 1).startStopCost(), FEE / 2.0, 1.0e-12);   // start
    EXPECT_NEAR(StartStopSystem({0.5}, {0.0}, 1).startStopCost(), FEE / 2.0, 1.0e-12);   // stop
    EXPECT_EQ(StartStopSystem({0.0}, {0.0}, 1).startStopCost(), 0.0);
    EXPECT_EQ(StartStopSystem({0.5}, {0.4}, 1).startStopCost(), 0.0);
}

TEST(AdStartStop, SameAsTheOldHardRuleOutsideTheZone) {
    // The old rule: cost fee / 2 when (prev > 0.01) != (cur > 0.01). Identical for actions outside the zone 0.01 to 0.03.
    const double outside[] = { 0.0, 0.001, 0.005, 0.0099, 0.01, 0.03, 0.0301, 0.05, 0.3, 1.0 };
    for (double prev : outside) {
        for (double cur : outside) {
            const double old_cost = ((prev > 0.01) != (cur > 0.01)) ? FEE / 2.0 : 0.0;
            EXPECT_NEAR(StartStopSystem({prev}, {cur}, 1).startStopCost(), old_cost, 1.0e-12) << "prev " << prev << ", cur " << cur;
        }
    }
}

TEST(AdStartStop, PartialCostInsideTheZone) {
    // The half-way action 0.02 counts as half running: (1 - 0.5)^2 = 0.25 of fee / 2
    EXPECT_NEAR(StartStopSystem({0.5}, {0.02}, 1).startStopCost(), FEE / 2.0 * 0.25, 1.0e-12);
    EXPECT_NEAR(StartStopSystem({0.02}, {0.0}, 1).startStopCost(), FEE / 2.0 * 0.25, 1.0e-12);
    EXPECT_NEAR(StartStopSystem({0.02}, {0.02}, 1).startStopCost(), 0.0, 1.0e-12);
}

TEST(AdStartStop, FirstStepUsesTheStateFileValue) {
    // At t = 0 the previous state is running if init_Power > 0.01
    EXPECT_NEAR(StartStopSystem({0.5}, {0.0}, 0, 1.0).startStopCost(), FEE / 2.0, 1.0e-12);    // was running, now stopped
    EXPECT_EQ(StartStopSystem({0.5}, {0.5}, 0, 1.0).startStopCost(), 0.0);                    // was running, still running
    EXPECT_NEAR(StartStopSystem({0.5}, {0.5}, 0, 0.0).startStopCost(), FEE / 2.0, 1.0e-12);    // was off, now running
    EXPECT_NEAR(StartStopSystem({0.5}, {0.02}, 0, 1.0).startStopCost(), FEE / 2.0 * 0.25, 1.0e-12);
}

TEST(AdStartStop, GradientOutsideTheZoneIsTheOldOne) {
    checkStartStopGradient({0.001}, {0.005}, 1);   // off, off
    checkStartStopGradient({0.5},   {0.4},   1);   // on, on
    checkStartStopGradient({0.001}, {0.5},   1);   // start
    checkStartStopGradient({0.5},   {0.001}, 1);   // stop
}

TEST(AdStartStop, GradientInsideTheZone) {
    checkStartStopGradient({0.5},   {0.02},  1);
    checkStartStopGradient({0.02},  {0.5},   1);
    checkStartStopGradient({0.015}, {0.025}, 1);
    checkStartStopGradient({0.02},  {0.02},  1);
    checkStartStopGradient({0.001}, {0.015}, 1);
    checkStartStopGradient({0.025}, {0.002}, 1);
    checkStartStopGradient({0.012}, {0.028}, 1);
    checkStartStopGradient({0.5},   {0.0125}, 1);
    checkStartStopGradient({0.0275}, {0.5},   1);
}

TEST(AdStartStop, GradientNextToTheEdgesOfTheZone) {
    // Exactly at an edge the second derivative of the cost jumps, so a finite difference is accurate only to O(h * jump)
    // there (smooth_on_d is checked to be 0 at the edges above). Just inside and just outside the edges it is exact.
    checkStartStopGradient({0.5},    {0.0102}, 1);
    checkStartStopGradient({0.5},    {0.0298}, 1);
    checkStartStopGradient({0.5},    {0.0098}, 1);
    checkStartStopGradient({0.0102}, {0.5},    1);
    checkStartStopGradient({0.0302}, {0.0098}, 1);
    checkStartStopGradient({0.0102}, {0.0298}, 1);
}

TEST(AdStartStop, GradientWithTwoGenerators) {
    checkStartStopGradient({0.02, 0.5},  {0.5, 0.015}, 1);
    checkStartStopGradient({0.5, 0.5},   {0.02, 0.4},  1);
}

TEST(AdStartStop, GradientAtTheFirstStep) {
    checkStartStopGradient({0.5}, {0.02},  0, 1.0);    // running before, partly stopping now
    checkStartStopGradient({0.5}, {0.015}, 0, 0.0);    // off before, partly starting now
    checkStartStopGradient({0.5}, {0.5},   0, 1.0);
}
