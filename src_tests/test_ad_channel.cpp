#include <gtest/gtest.h>
#include "herss.h"
#include <cmath>
#include <vector>

// HERSS-AD: Channel QMIN penalty and Channel::Backward (routing method A, cascaded linear reservoirs).
//
// The channel is built by hand and run for several time steps. The objective is
//     J = sum_t ( profit[t] + c_down * downstream.up_inflow[t] ),    profit = -cost_qmin,
// and the gradient d J / d up_inflow[t] from Backward (run for t = T-1 ... 0, as Riversystem::Backward does)
// is compared with central finite differences. Because the cascade has memory, the gradient of step t
// includes the effect of the inflow on all later outflows, which tests the carry between steps.

namespace {

const size_t T_STEPS = 30;
const double DT = 3600.0;
const double C_DOWN = 0.37;

struct ChannelParams {
    double required = 1.0;         // QMIN requirement [m3/s]
    double fee      = 4.0;         // Euro per hour
    double k_hours  = 4.0;         // K_TRAVELTIME_HOURS
    size_t n_res    = 3;           // N_CASCADE_LINRES
    bool downstream = false;       // Feed a downstream node that has an adjoint
    int period_last_day = 31;      // The requirement applies from 1 Jan to this day of January
    double day_step = 0.0;         // Steps per day, 0 means all steps on 1 Jan
    int delay_hours = -1;          // -1: routing method A. >= 0: method B with this ROUTING_DELAY_HOURS
};

struct ChannelSystem {
    Channel ch;
    Node sink;
    ChannelParams s;
    double steady = 0.0;
    std::vector<double> inflow;    // up_inflow[t]
    std::vector<double> init_Mm3;  // Initial storage of each linear reservoir

    ChannelSystem(const ChannelParams &setup, const std::vector<double> &q_in, double steady_flow) : inflow(q_in), s(setup) {
        ch.nodename = "TESTCHANNEL";  ch.idnr = 2;  ch.nodetype = NodeType::CHANNEL;
        ch.S    = new Scenario(T_STEPS, 3600, 2);
        sink.S  = new Scenario(T_STEPS, 3600, 3);
        ch.routing_method          = s.delay_hours >= 0 ? RoutingMethod::B_LAGGED_CASCADEDLINRES : RoutingMethod::A_CASCADEDLINRES;
        ch.routing_delay_hours     = size_t(s.delay_hours >= 0 ? s.delay_hours : 0);
        steady = steady_flow;
        ch.K_traveltime_hours      = s.k_hours;
        ch.num_cascaded_reservoirs = s.n_res;
        ch.casc_reservoirs         = new CascadedReservoirs(s.k_hours, s.n_res);
        ch.qmin_in_use             = true;
        ch.qmin.nr_periods         = 1;
        ch.qmin.timeperiods[0].min_discharge = s.required;
        ch.qmin.timeperiods[0].penalty_cost  = s.fee;
        ch.qmin.timeperiods[0].start_day = 1;  ch.qmin.timeperiods[0].start_month = 1;
        ch.qmin.timeperiods[0].end_day   = s.period_last_day;  ch.qmin.timeperiods[0].end_month = 1;
        ch.downstream_node_in_use  = s.downstream;
        ch.ptr_downstream_node     = s.downstream ? &sink : nullptr;

        // Initial storage in steady state for the given flow, S_i = K_i * q
        const double k_res_s = s.k_hours * 3600.0 / double(s.n_res);
        init_Mm3.assign(s.n_res, k_res_s * steady_flow / 1.0e6);

        for (size_t t = 0; t < T_STEPS; ++t) {
            const int day = 1 + (s.day_step > 0.0 ? int(double(t) / s.day_step) : 0);
            ch.S->year[t] = 2022;  ch.S->month[t] = 1;  ch.S->day[t] = day;  ch.S->hour[t] = 0;
        }
    }

    ~ChannelSystem() {
        delete ch.S;  delete sink.S;
        ch.S = sink.S = nullptr;
    }

    double objective() {
        ch.casc_reservoirs->setInitialStorage(init_Mm3);
        if(s.delay_hours >= 0) ch.lagged_outflow_volume_m3.assign(size_t(s.delay_hours), steady * DT);
        double J = 0.0;
        for (size_t t = 0; t < T_STEPS; ++t) {
            sink.S->up_inflow[t]  = 0.0;
            ch.S->up_inflow[t]    = inflow[t];
            ch.Simulate(t);
            J += ch.S->profit[t] + C_DOWN * sink.S->up_inflow[t];
        }
        return J;
    }

    std::vector<double> backward() {
        ch.S->ClearGradients();
        sink.S->ClearGradients();
        for (size_t t = 0; t < T_STEPS; ++t) {
            ch.S->profit_grad[t]        = 1.0;
            sink.S->up_inflow_grad[t]   = C_DOWN;
        }
        for (size_t t = T_STEPS; t-- > 0; ) ch.Backward(t);
        return std::vector<double>(ch.S->up_inflow_grad, ch.S->up_inflow_grad + T_STEPS);
    }

    double centralDiff(size_t t, double h) {
        const double q0 = inflow[t];
        inflow[t] = q0 + h;  const double jp = objective();
        inflow[t] = q0 - h;  const double jm = objective();
        inflow[t] = q0;
        return (jp - jm) / (2.0 * h);
    }

    // Finite-difference estimate closest to the analytic gradient over a sweep of step sizes, central differences and their
    // Richardson extrapolation. Where the outflow sits exactly at the edge of the smoothing zone the second derivative of
    // the penalty jumps, and a single step size straddling it is accurate only to about 1e-5.
    double bestFiniteDiff(size_t t, double analytic) {
        double best = 1.0e300;
        for (double h : { 1.0e-4, 1.0e-5, 1.0e-6, 1.0e-7, 1.0e-8 }) {
            const double d1 = centralDiff(t, h);
            const double d2 = centralDiff(t, 0.5 * h);
            for (double estimate : { d1, (4.0 * d2 - d1) / 3.0 }) {
                if (std::fabs(estimate - analytic) < std::fabs(best - analytic)) best = estimate;
            }
        }
        return best;
    }
};

void checkGradient(const ChannelParams &setup, const std::vector<double> &q_in, double steady_flow, const char *label) {
    ChannelSystem sys(setup, q_in, steady_flow);
    sys.objective();
    const std::vector<double> an = sys.backward();
    for (size_t t = 0; t < T_STEPS; ++t) {
        const double fd = sys.bestFiniteDiff(t, an[t]);
        EXPECT_NEAR(an[t], fd, 1.0e-6 + 2.0e-6 * std::fabs(fd)) << label << ", d J / d inflow at step " << t;
    }
}

// Inflow that moves linearly from q_start to q_end over the run
std::vector<double> ramp(double q_start, double q_end) {
    std::vector<double> q(T_STEPS);
    for (size_t t = 0; t < T_STEPS; ++t) q[t] = q_start + (q_end - q_start) * double(t) / double(T_STEPS - 1);
    return q;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
TEST(AdChannel, PenaltyIsZeroWhenMetAndNearlyTheFullFeeWithoutOutflow) {
    ChannelParams s;
    const double fee_per_step = s.fee * DT / 3600.0;
    const double eps = 0.01;       // min(EPS_M3S, 5 % of the requirement) for a requirement of 1 m3/s
    for (double q : { 0.0, 1.0, 3.0 }) {
        ChannelSystem sys(s, std::vector<double>(T_STEPS, q), q);
        sys.objective();
        for (size_t t = 0; t < T_STEPS; ++t) {
            if (q == 0.0)      EXPECT_NEAR(sys.ch.S->cost_qmin[t], fee_per_step * (1.0 - eps / s.required), 1.0e-12);   // nothing leaves
            else if (q == 1.0) EXPECT_NEAR(sys.ch.S->cost_qmin[t], 0.0, 1.0e-12);                                      // exactly the requirement
            else               EXPECT_EQ(sys.ch.S->cost_qmin[t], 0.0);                                                  // above it
        }
    }
}

TEST(AdChannel, PenaltyGrowsWithTheUnmetFraction) {
    ChannelParams s;
    const double fee_per_step = s.fee * DT / 3600.0;
    // Steady outflow of 0.5 m3/s against a requirement of 1.0 m3/s: half the fee minus the smoothing width
    ChannelSystem sys(s, std::vector<double>(T_STEPS, 0.5), 0.5);
    sys.objective();
    EXPECT_NEAR(sys.ch.S->cost_qmin[T_STEPS - 1] / fee_per_step, 0.5 - 0.01, 1.0e-9);
}

TEST(AdChannel, ShortfallWithinTheSmoothingWidthCostsAlmostNothing) {
    // A shortfall of 1e-3 m3/s, the tolerance of the old fixed fee, costs 2.5e-5 of the fee: (1e-3)^2 / (4 * 0.01)
    ChannelParams s;
    ChannelSystem sys(s, std::vector<double>(T_STEPS, 0.999), 0.999);
    sys.objective();
    EXPECT_NEAR(sys.ch.S->cost_qmin[T_STEPS - 1] / (s.fee * DT / 3600.0), 2.5e-5, 1.0e-9);
}

TEST(AdChannel, SmallRequirementKeepsAtLeast95PercentOfTheFee) {
    // The smoothing width is 5 % of a requirement below 0.2 m3/s
    ChannelParams s;
    s.required = 0.05;
    ChannelSystem sys(s, std::vector<double>(T_STEPS, 0.0), 0.0);
    sys.objective();
    EXPECT_NEAR(sys.ch.S->cost_qmin[T_STEPS - 1] / (s.fee * DT / 3600.0), 0.95, 1.0e-12);
}

TEST(AdChannel, GradientFarBelowTheRequirement) {
    checkGradient(ChannelParams(), ramp(0.2, 0.7), 0.2, "far below");
}

TEST(AdChannel, GradientThroughTheBlendZone) {
    // The outflow passes through the requirement of 1.0 m3/s. The penalty is smoothed for outflows between 0.98 and 1.0 m3/s
    checkGradient(ChannelParams(), ramp(1.2, 0.8), 1.2, "ramp down through the requirement");
    checkGradient(ChannelParams(), ramp(0.8, 1.2), 0.8, "ramp up through the requirement");
    // Slow ramps that spend many steps inside the blend zone, ending inside it and on both sides of it
    checkGradient(ChannelParams(), ramp(1.03, 0.97), 1.03, "slow ramp down through the zone");
    checkGradient(ChannelParams(), ramp(0.97, 1.03), 0.97, "slow ramp up through the zone");
    checkGradient(ChannelParams(), ramp(1.005, 0.985), 1.005, "ramp inside the zone");
}

TEST(AdChannel, GradientAboveTheRequirement) {
    checkGradient(ChannelParams(), ramp(2.0, 3.0), 2.0, "above");
}

TEST(AdChannel, GradientWithWigglingInflow) {
    std::vector<double> q(T_STEPS);
    for (size_t t = 0; t < T_STEPS; ++t) q[t] = 1.0 + 0.4 * std::sin(0.7 * double(t)) + 0.05 * std::cos(2.3 * double(t));
    checkGradient(ChannelParams(), q, 1.0, "wiggle");
}

TEST(AdChannel, GradientWithSmallRequirement) {
    // The smoothing width shrinks to 5 % of a small requirement
    ChannelParams s;
    s.required = 0.05;
    checkGradient(s, ramp(0.07, 0.03), 0.07, "small requirement");
}

TEST(AdChannel, GradientWithDownstreamNodeAndOtherCascades) {
    ChannelParams s;
    s.downstream = true;
    checkGradient(s, ramp(1.3, 0.7), 1.3, "downstream node");

    s.n_res = 1;  s.k_hours = 12.0;
    checkGradient(s, ramp(1.3, 0.7), 1.3, "one linear reservoir");

    s.n_res = 5;  s.k_hours = 1.5;
    checkGradient(s, ramp(0.7, 1.3), 0.7, "five linear reservoirs");
}

TEST(AdChannel, GradientWithZeroTravelTime) {
    ChannelParams s;
    s.k_hours = 0.0;
    s.n_res   = 1;
    checkGradient(s, ramp(1.2, 0.8), 1.2, "zero travel time");
}

TEST(AdChannel, RepeatedBackwardGivesTheSameGradient) {
    // The storage adjoint is carried between steps, and must start from zero in every backward run
    ChannelSystem sys(ChannelParams(), ramp(1.2, 0.8), 1.2);
    sys.objective();
    const std::vector<double> first  = sys.backward();
    const std::vector<double> second = sys.backward();
    for (size_t t = 0; t < T_STEPS; ++t) EXPECT_DOUBLE_EQ(first[t], second[t]) << "step " << t;
}

TEST(AdChannel, GradientWhenTheRequirementEndsInTheRun) {
    // The period covers 1 January only. Steps are 6 hours per "day" so the period ends at step 6
    ChannelParams s;
    s.period_last_day = 1;
    s.day_step = 6.0;
    checkGradient(s, ramp(1.2, 0.2), 1.2, "requirement ends");
}

// ---------------------------------------------------------------------------------------------------------------
// Routing method B: the cascade followed by a delay of ROUTING_DELAY_HOURS.

TEST(AdChannelLagged, OutflowIsTheCascadeOutflowShiftedByTheDelay) {
    const std::vector<double> q = ramp(1.5, 0.6);
    ChannelParams a;
    ChannelSystem sysA(a, q, 1.5);
    sysA.objective();
    for (int L : { 0, 1, 4, 9 }) {
        ChannelParams b;
        b.delay_hours = L;
        ChannelSystem sysB(b, q, 1.5);
        sysB.objective();
        for (size_t t = 0; t < T_STEPS; ++t) {
            const double expected = (t < size_t(L)) ? 1.5 : sysA.ch.S->tot_outflow[t - size_t(L)];
            EXPECT_NEAR(sysB.ch.S->tot_outflow[t], expected, 1.0e-9) << "delay " << L << ", step " << t;
        }
    }
}

TEST(AdChannelLagged, GradientForSeveralDelays) {
    for (int L : { 0, 1, 2, 5, 11 }) {
        ChannelParams s;
        s.delay_hours = L;
        checkGradient(s, ramp(1.3, 0.6), 1.3, ("delay " + std::to_string(L) + ", ramp down through the requirement").c_str());
        checkGradient(s, ramp(0.7, 1.4), 0.7, ("delay " + std::to_string(L) + ", ramp up through the requirement").c_str());
    }
}

TEST(AdChannelLagged, GradientWithWigglingInflowAndDownstreamNode) {
    std::vector<double> q(T_STEPS);
    for (size_t t = 0; t < T_STEPS; ++t) q[t] = 1.0 + 0.4 * std::sin(0.7 * double(t)) + 0.05 * std::cos(2.3 * double(t));
    ChannelParams s;
    s.delay_hours = 3;
    checkGradient(s, q, 1.0, "wiggle");
    s.downstream = true;
    checkGradient(s, q, 1.0, "wiggle, downstream node");
    s.n_res = 1;  s.k_hours = 6.0;  s.delay_hours = 7;
    checkGradient(s, q, 1.0, "one linear reservoir, delay 7, downstream node");
}

TEST(AdChannelLagged, ZeroDelayGivesTheSameGradientAsMethodA) {
    const std::vector<double> q = ramp(1.2, 0.8);
    ChannelParams a;  a.downstream = true;
    ChannelParams b = a;  b.delay_hours = 0;
    ChannelSystem sysA(a, q, 1.2), sysB(b, q, 1.2);
    sysA.objective();  sysB.objective();
    const std::vector<double> ga = sysA.backward(), gb = sysB.backward();
    for (size_t t = 0; t < T_STEPS; ++t) EXPECT_DOUBLE_EQ(ga[t], gb[t]) << "step " << t;
}

TEST(AdChannelLagged, DelayLongerThanTheRunGivesNoGradient) {
    ChannelParams s;
    s.delay_hours = int(T_STEPS) + 3;
    s.downstream = true;
    ChannelSystem sys(s, ramp(1.2, 0.8), 1.2);
    sys.objective();
    for (double g : sys.backward()) EXPECT_EQ(g, 0.0);
}

TEST(AdChannelLagged, InflowInTheLastDelayHoursHasNoGradient) {
    // Water that enters in the last L hours is still in the channel when the run ends, where it has no value
    ChannelParams s;
    s.delay_hours = 4;
    s.downstream = true;
    ChannelSystem sys(s, ramp(1.2, 0.8), 1.2);
    sys.objective();
    const std::vector<double> g = sys.backward();
    EXPECT_EQ(g[T_STEPS - 1], 0.0);
    EXPECT_GT(std::fabs(g[T_STEPS - 5]), 0.0);
}

TEST(AdChannelLagged, RepeatedBackwardGivesTheSameGradient) {
    ChannelParams s;
    s.delay_hours = 5;
    ChannelSystem sys(s, ramp(1.2, 0.8), 1.2);
    sys.objective();
    const std::vector<double> first  = sys.backward();
    const std::vector<double> second = sys.backward();
    for (size_t t = 0; t < T_STEPS; ++t) EXPECT_DOUBLE_EQ(first[t], second[t]) << "step " << t;
}
