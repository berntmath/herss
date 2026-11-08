#include <gtest/gtest.h>
#include "herss.h"
#include <cmath>
#include <stdexcept>
#include <vector>

// HERSS-AD: routing method C, the cascade of nonlinear reservoirs (NaturalChannel and Channel::Backward).
//
// Per reach  Qout = smin_nn(a S^p, S / dt)  and  S_new = S + (Qin - Qout) dt,  where the smooth minimum replaces the
// hard cap  min(a S^p, S / dt). The tests check that the smoothing keeps the old behaviour away from the cap, that the
// water balance and non-negative storage hold, and that Channel::Backward agrees with central finite differences.

namespace {

const size_t T_STEPS = 30;
const double C_DOWN = 0.37;

// a and the storage of one reach in steady state with the flow q, for a channel defined by the reference state
double coefficientA(int n, double q_ref, double t_ref_hours, double p) {
    return q_ref / std::pow(q_ref * t_ref_hours * 3600.0 / double(n), p);
}
double steadyReachStorage(int n, double q_ref, double t_ref_hours, double p, double q) {
    return std::pow(q / coefficientA(n, q_ref, t_ref_hours, p), 1.0 / p);
}

struct NlParams {
    int    n_reaches   = 3;
    double p           = 1.2;
    double q_ref       = 2.1;
    double t_ref_hours = 8.2;     // The cap is inactive at the reference state when n dt / 3600 < t_ref_hours
    double dt          = 3600.0;
    double required    = 2.0;     // QMIN requirement [m3/s]
    double fee         = 4.0;     // Euro per hour
    bool   downstream  = false;
};

struct NlChannelSystem {
    Channel ch;
    Node sink;
    NlParams s;
    std::vector<double> inflow;
    std::vector<double> init_m3;   // Initial storage of each reach

    NlChannelSystem(const NlParams &setup, const std::vector<double> &q_in, double steady_flow) : s(setup), inflow(q_in) {
        init_m3.assign(size_t(s.n_reaches), steadyReachStorage(s.n_reaches, s.q_ref, s.t_ref_hours, s.p, steady_flow));
        build();
    }
    NlChannelSystem(const NlParams &setup, const std::vector<double> &q_in, const std::vector<double> &init_storage_m3)
        : s(setup), inflow(q_in), init_m3(init_storage_m3) {
        build();
    }

    void build() {
        ch.nodename = "TESTCHANNEL_C";  ch.idnr = 2;  ch.nodetype = NodeType::CHANNEL;
        ch.S    = new Scenario(T_STEPS, size_t(s.dt), 2);
        sink.S  = new Scenario(T_STEPS, size_t(s.dt), 3);
        ch.routing_method   = RoutingMethod::C_NONLINEARRESCASCADE;
        ch.n_reaches        = s.n_reaches;
        ch.referenceFlow    = s.q_ref;
        ch.referenceTravelTime = s.t_ref_hours;
        ch.nonlinearity     = s.p;
        ch.nonlinear_naturalChannel.SetParameters(s.n_reaches, s.q_ref, s.t_ref_hours, s.p);
        ch.qmin_in_use      = true;
        ch.qmin.nr_periods  = 1;
        ch.qmin.timeperiods[0].min_discharge = s.required;
        ch.qmin.timeperiods[0].penalty_cost  = s.fee;
        ch.qmin.timeperiods[0].start_day = 1;  ch.qmin.timeperiods[0].start_month = 1;
        ch.qmin.timeperiods[0].end_day   = 31; ch.qmin.timeperiods[0].end_month = 1;
        ch.downstream_node_in_use = s.downstream;
        ch.ptr_downstream_node    = s.downstream ? &sink : nullptr;
        for (size_t t = 0; t < T_STEPS; ++t) {
            ch.S->year[t] = 2022;  ch.S->month[t] = 1;  ch.S->day[t] = 1;  ch.S->hour[t] = 0;
        }
    }

    ~NlChannelSystem() {
        delete ch.S;  delete sink.S;
        ch.S = sink.S = nullptr;
    }

    double objective() {
        ch.nonlinear_naturalChannel.SetReachStorage(init_m3);
        double J = 0.0;
        for (size_t t = 0; t < T_STEPS; ++t) {
            sink.S->up_inflow[t] = 0.0;
            ch.S->up_inflow[t]   = inflow[t];
            ch.Simulate(t);
            J += ch.S->profit[t] + C_DOWN * sink.S->up_inflow[t];
        }
        return J;
    }

    std::vector<double> backward() {
        ch.S->ClearGradients();
        sink.S->ClearGradients();
        for (size_t t = 0; t < T_STEPS; ++t) {
            ch.S->profit_grad[t]      = 1.0;
            sink.S->up_inflow_grad[t] = C_DOWN;
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

    // Finite-difference estimate closest to the analytic gradient over a sweep of step sizes, central differences and
    // their Richardson extrapolation. The second derivative of the smooth cap and of the QMIN penalty jumps at the
    // edges of their blend zones, and a single step size straddling an edge is accurate only to about 1e-5.
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

void checkGradient(const NlParams &setup, const std::vector<double> &q_in, double steady_flow, const char *label) {
    NlChannelSystem sys(setup, q_in, steady_flow);
    sys.objective();
    const std::vector<double> an = sys.backward();
    double largest = 0.0;
    for (size_t t = 0; t < T_STEPS; ++t) {
        const double fd = sys.bestFiniteDiff(t, an[t]);
        largest = std::max(largest, std::fabs(fd));
        EXPECT_NEAR(an[t], fd, 1.0e-6 + 2.0e-6 * std::fabs(fd)) << label << ", d J / d inflow at step " << t;
    }
    // A test where the gradient is zero everywhere would prove nothing
    EXPECT_GT(largest, 1.0e-3) << label << ": the gradient is zero everywhere, the case does not test anything";
}

std::vector<double> ramp(double q_start, double q_end) {
    std::vector<double> q(T_STEPS);
    for (size_t t = 0; t < T_STEPS; ++t) q[t] = q_start + (q_end - q_start) * double(t) / double(T_STEPS - 1);
    return q;
}

std::vector<double> wiggle(double mean, double amplitude) {
    std::vector<double> q(T_STEPS);
    for (size_t t = 0; t < T_STEPS; ++t) q[t] = mean + amplitude * std::sin(0.7 * double(t)) + 0.1 * amplitude * std::cos(2.3 * double(t));
    return q;
}

// The routing of the code before the smoothing, kept here as the reference
struct HardMinChannel {
    int n;  double a, p;  std::vector<double> S;
    HardMinChannel(int n_, double q_ref, double t_ref_hours, double p_) : n(n_), p(p_) {
        a = coefficientA(n, q_ref, t_ref_hours, p);
        S.assign(size_t(n), q_ref * t_ref_hours * 3600.0 / double(n));
    }
    double route(double Qin, double dt) {
        double flow = Qin;
        for (int i = 0; i < n; i++) {
            double Qout = a * std::pow(std::max(S[size_t(i)], 0.0), p);
            Qout = std::min(Qout, S[size_t(i)] / dt);
            S[size_t(i)] += (flow - Qout) * dt;
            S[size_t(i)] = std::max(0.0, S[size_t(i)]);
            flow = Qout;
        }
        return flow;
    }
};

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
TEST(AdNaturalChannel, NonlinearityBelowOneIsRejected) {
    EXPECT_THROW(NaturalChannel(3, 2.1, 8.2, 0.9), std::invalid_argument);
    EXPECT_THROW(NaturalChannel(3, 2.1, 8.2, 0.0), std::invalid_argument);
    EXPECT_THROW(NaturalChannel(3, 2.1, 8.2, std::nan("")), std::invalid_argument);
    NaturalChannel nc(3, 2.1, 8.2, 1.2);
    EXPECT_THROW(nc.SetParameters(3, 2.1, 8.2, 0.5), std::invalid_argument);
    EXPECT_NO_THROW(nc.SetParameters(3, 2.1, 8.2, 1.0));    // A linear reservoir is fine
    EXPECT_NO_THROW(nc.SetParameters(3, 2.1, 8.2, 2.5));
}

TEST(AdNaturalChannel, SameOutflowAsTheHardMinWhenTheCapIsNotActive) {
    // 8.2 hours of travel time in 3 reaches with a 1 hour step: Qout is far below S / dt in every reach
    for (double p : { 1.0, 1.2, 1.5 }) {
        NaturalChannel nc(3, 2.1, 8.2, p);
        HardMinChannel ref(3, 2.1, 8.2, p);
        const std::vector<double> q_in = wiggle(2.1, 1.2);
        for (size_t t = 0; t < T_STEPS; ++t) {
            const double smooth = nc.Route(q_in[t], 3600.0);
            const double hard   = ref.route(q_in[t], 3600.0);
            EXPECT_DOUBLE_EQ(smooth, hard) << "p = " << p << ", step " << t;
        }
        const std::vector<double> st = nc.GetReachStorage();
        for (size_t i = 0; i < 3; ++i) EXPECT_DOUBLE_EQ(st[i], ref.S[i]) << "p = " << p << ", reach " << i;
    }
}

TEST(AdNaturalChannel, CapOnlyDiffersFromTheHardMinInsideTheBlendZone) {
    // One reach. smin_nn is never above min(a S^p, S / dt) and at most EPS_REL_FLOW / 4 of a S^p below it
    NaturalChannel nc(1, 3.0, 3.0, 1.2);          // t_ref = dt: the cap is exactly at its blend centre at the reference state
    const double a = coefficientA(1, 3.0, 3.0, 1.2);
    for (double scale : { 0.0, 0.01, 0.2, 0.9, 1.0, 1.1, 3.0, 10.0 }) {
        const double S = scale * 3.0 * 3.0 * 3600.0;
        nc.SetReachStorage({ S });
        std::vector<double> before(1);
        const double q = nc.Route(0.0, 3600.0, before.data());
        const double potential = a * std::pow(S, 1.2);
        const double hard = std::min(potential, S / 3600.0);
        EXPECT_LE(q, hard + 1.0e-15) << "scale " << scale;
        EXPECT_GE(q, hard - 0.25 * EPS_REL_FLOW * potential - 1.0e-15) << "scale " << scale;
        EXPECT_DOUBLE_EQ(before[0], S);
    }
}

TEST(AdNaturalChannel, WaterBalanceAndNonNegativeStorageWithAnActiveCap) {
    // A fast channel: 3 reaches in 2 hours with a 1 hour step, so the cap is active at and above the reference state
    NaturalChannel nc(3, 2.1, 2.0, 1.2);
    const std::vector<double> q_in = wiggle(4.0, 3.9);
    for (size_t t = 0; t < T_STEPS; ++t) {
        const double before = nc.GetStoredVolume();
        const double q_out  = nc.Route(std::max(q_in[t], 0.0), 3600.0);
        const double after  = nc.GetStoredVolume();
        EXPECT_NEAR(before + std::max(q_in[t], 0.0) * 3600.0 - q_out * 3600.0 - after, 0.0, 1.0e-6) << "step " << t;
        for (double v : nc.GetReachStorage()) EXPECT_GE(v, 0.0) << "step " << t;
        EXPECT_GE(q_out, 0.0);
    }
}

TEST(AdNaturalChannel, EmptyChannelStaysEmptyWithoutInflow) {
    NaturalChannel nc(3, 2.1, 8.2, 1.2);
    nc.SetReachStorage({ 0.0, 0.0, 0.0 });
    EXPECT_EQ(nc.Route(0.0, 3600.0), 0.0);
    for (double v : nc.GetReachStorage()) EXPECT_EQ(v, 0.0);
}

// ---------------------------------------------------------------------------------------------------------------
// NaturalChannel::RouteBackward against finite differences of Route, for the storage adjoint and the inflow adjoint.

namespace {

// F = sum_i w_i S_end_i + c Qout, one step from the storages S0 and the inflow qin
double stepFunctional(const NlParams &s, std::vector<double> S0, double qin, const std::vector<double> &w, double c) {
    NaturalChannel nc(s.n_reaches, s.q_ref, s.t_ref_hours, s.p);
    nc.SetReachStorage(S0);
    const double qout = nc.Route(qin, s.dt);
    double F = c * qout;
    const std::vector<double> end = nc.GetReachStorage();
    for (size_t i = 0; i < end.size(); ++i) F += w[i] * end[i];
    return F;
}

void checkStepAdjoint(const NlParams &s, const std::vector<double> &S0, double qin, const char *label) {
    const std::vector<double> w = { 0.3, -1.1, 0.7, 0.45, -0.2 };
    const double c = 0.9;
    NaturalChannel nc(s.n_reaches, s.q_ref, s.t_ref_hours, s.p);
    nc.SetReachStorage(S0);
    std::vector<double> before(size_t(s.n_reaches));
    nc.Route(qin, s.dt, before.data());

    std::vector<double> storage_grad(w.begin(), w.begin() + s.n_reaches);
    double q_in_grad = 0.0;
    nc.RouteBackward(before.data(), c, s.dt, storage_grad, q_in_grad);

    const double hq = 1.0e-6 * std::max(1.0, qin);
    const double fdq = (stepFunctional(s, S0, qin + hq, w, c) - stepFunctional(s, S0, qin - hq, w, c)) / (2.0 * hq);
    EXPECT_NEAR(q_in_grad, fdq, 1.0e-6 * std::max(1.0, std::fabs(fdq))) << label << ": inflow";

    for (size_t i = 0; i < S0.size(); ++i) {
        const double hs = 1.0e-5 * std::max(1.0, S0[i]);
        std::vector<double> sp = S0, sm = S0;
        sp[i] += hs;  sm[i] -= hs;
        const double fds = (stepFunctional(s, sp, qin, w, c) - stepFunctional(s, sm, qin, w, c)) / (2.0 * hs);
        EXPECT_NEAR(storage_grad[i], fds, 1.0e-6 * std::max(1.0, std::fabs(fds))) << label << ": storage of reach " << i;
    }
}

}  // namespace

TEST(AdNaturalChannel, StepAdjointMatchesFiniteDifferences) {
    NlParams s;
    const double spr = s.q_ref * s.t_ref_hours * 3600.0 / double(s.n_reaches);
    checkStepAdjoint(s, { spr, spr, spr }, 2.1, "reference state");
    checkStepAdjoint(s, { 0.3 * spr, 1.7 * spr, 0.8 * spr }, 3.5, "mixed storages");
    checkStepAdjoint(s, { 5.0 * spr, 0.05 * spr, 2.0 * spr }, 0.2, "far from the reference");

    NlParams fast = s;  fast.t_ref_hours = 2.0;                     // The cap is active above the reference storage
    const double spr_fast = fast.q_ref * fast.t_ref_hours * 3600.0 / double(fast.n_reaches);
    checkStepAdjoint(fast, { spr_fast, 2.0 * spr_fast, 0.5 * spr_fast }, 3.0, "fast channel, cap active");

    NlParams centre = s;  centre.t_ref_hours = 3.0;                 // The reference state is at the centre of the blend zone
    const double spr_centre = centre.q_ref * centre.t_ref_hours * 3600.0 / double(centre.n_reaches);
    checkStepAdjoint(centre, { spr_centre, 1.0005 * spr_centre, 0.9995 * spr_centre }, 2.1, "blend zone");

    NlParams lin = s;  lin.p = 1.0;
    checkStepAdjoint(lin, { spr, 0.4 * spr, 2.0 * spr }, 1.5, "linear reservoirs");
    lin.t_ref_hours = 1.0;                                          // a dt = 3: the cap is active for every storage
    checkStepAdjoint(lin, { spr, 0.4 * spr, 2.0 * spr }, 1.5, "linear reservoirs, always capped");
}

TEST(AdNaturalChannel, StepAdjointForEmptyReaches) {
    NlParams s;
    for (double p : { 1.0, 1.2, 2.0 }) {
        s.p = p;
        // The slope of an empty reach is one-sided, so the empty reach is checked only through the other quantities
        std::vector<double> S0 = { 0.0, 0.0, 0.0 };
        NaturalChannel nc(3, s.q_ref, s.t_ref_hours, p);
        nc.SetReachStorage(S0);
        std::vector<double> before(3);
        nc.Route(1.0, s.dt, before.data());
        std::vector<double> storage_grad = { 0.0, 0.0, 1.0 };
        double q_in_grad = 0.0;
        nc.RouteBackward(before.data(), 0.0, s.dt, storage_grad, q_in_grad);
        for (double g : storage_grad) EXPECT_TRUE(std::isfinite(g)) << "p = " << p;
        EXPECT_TRUE(std::isfinite(q_in_grad)) << "p = " << p;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Channel::Backward through several steps. J = sum_t ( -cost_qmin[t] + c_down * downstream inflow[t] ).

TEST(AdNaturalChannelGradient, SlowChannelRampThroughTheRequirement) {
    NlParams s;
    checkGradient(s, ramp(1.0, 3.5), 1.0, "ramp up through the requirement");
    checkGradient(s, ramp(3.5, 1.0), 3.5, "ramp down through the requirement");
}

TEST(AdNaturalChannelGradient, NonlinearityValues) {
    for (double p : { 1.0, 1.05, 1.2, 1.5, 2.0 }) {
        NlParams s;
        s.p = p;
        checkGradient(s, wiggle(2.0, 0.9), 2.0, ("p = " + std::to_string(p)).c_str());
    }
}

TEST(AdNaturalChannelGradient, NumberOfReaches) {
    for (int n : { 1, 2, 5, 10 }) {
        NlParams s;
        s.n_reaches = n;
        s.downstream = true;
        checkGradient(s, wiggle(2.0, 0.9), 2.0, ("n reaches = " + std::to_string(n)).c_str());
    }
}

TEST(AdNaturalChannelGradient, FastChannelWithAnActiveCap) {
    // 3 reaches in 2 hours with a 1 hour step: Qout is limited by S / dt at and above the reference storage
    NlParams s;
    s.t_ref_hours = 2.0;
    s.downstream = true;
    checkGradient(s, ramp(0.5, 5.0), 0.5, "fast, ramp up through the cap");
    checkGradient(s, ramp(5.0, 0.5), 5.0, "fast, ramp down through the cap");
    checkGradient(s, wiggle(2.5, 2.0), 2.5, "fast, wiggle");
}

TEST(AdNaturalChannelGradient, ThroughTheBlendZoneOfTheCap) {
    // n dt / 3600 = t_ref, so a S^(p-1) dt = 1 at the reference storage: the centre of the blend zone
    NlParams s;
    s.t_ref_hours = 3.0;
    s.downstream = true;
    checkGradient(s, std::vector<double>(T_STEPS, 2.1), 2.1, "steady at the blend centre");
    std::vector<double> nudged(T_STEPS, 2.1);
    for (size_t t = 5; t < T_STEPS; ++t) nudged[t] = 2.1 * (1.0 + 2.0e-3 * std::sin(0.5 * double(t)));
    checkGradient(s, nudged, 2.1, "steady, small wiggle inside the zone");
    checkGradient(s, ramp(1.9, 2.3), 1.9, "slow ramp through the zone");
}

TEST(AdNaturalChannelGradient, LinearReservoirsAtAndAboveTheCap) {
    // p = 1: a S^(p-1) dt = n dt / (t_ref 3600), the same for every storage
    for (double t_ref : { 12.0, 3.0, 1.5 }) {      // ratios 0.25, 1.0 (centre of the blend zone) and 2.0
        NlParams s;
        s.p = 1.0;
        s.t_ref_hours = t_ref;
        s.downstream = true;
        checkGradient(s, wiggle(2.0, 0.9), 2.0, ("linear, t_ref = " + std::to_string(t_ref)).c_str());
    }
}

TEST(AdNaturalChannelGradient, StartingFromAnEmptyChannel) {
    for (double p : { 1.0, 1.2, 2.0 }) {
        NlParams s;
        s.p = p;
        s.downstream = true;
        NlChannelSystem sys(s, ramp(0.1, 3.0), std::vector<double>(3, 0.0));
        sys.objective();
        const std::vector<double> an = sys.backward();
        for (size_t t = 0; t < T_STEPS; ++t) {
            const double fd = sys.bestFiniteDiff(t, an[t]);
            EXPECT_NEAR(an[t], fd, 1.0e-6 + 2.0e-6 * std::fabs(fd)) << "p = " << p << ", step " << t;
        }
    }
}

TEST(AdNaturalChannelGradient, DailyTimeStep) {
    NlParams s;
    s.dt = 86400.0;
    s.t_ref_hours = 60.0;
    s.downstream = true;
    checkGradient(s, wiggle(2.0, 0.9), 2.0, "daily step");
}

TEST(AdNaturalChannelGradient, GradientOfInflowIsZeroWithoutQminAndDownstream) {
    NlParams s;
    NlChannelSystem sys(s, ramp(1.0, 3.5), 1.0);
    sys.ch.qmin_in_use = false;
    sys.objective();
    for (double g : sys.backward()) EXPECT_EQ(g, 0.0);
}

TEST(AdNaturalChannelGradient, RepeatedBackwardGivesTheSameGradient) {
    // The reach storage adjoint is carried between steps, and must start from zero in every backward run
    NlParams s;
    s.t_ref_hours = 2.0;
    s.downstream = true;
    NlChannelSystem sys(s, ramp(0.5, 5.0), 0.5);
    sys.objective();
    const std::vector<double> first  = sys.backward();
    const std::vector<double> second = sys.backward();
    for (size_t t = 0; t < T_STEPS; ++t) EXPECT_DOUBLE_EQ(first[t], second[t]) << "step " << t;
}

TEST(AdNaturalChannelGradient, DownstreamValueOfWaterIsPropagatedThroughTheChannel) {
    // With only the downstream term J = c_down * sum of the outflow, an extra m3/s in step t adds water to the system.
    // The routing is explicit: the outflow of a reach depends on its storage at the start of the step. Inflow in step t
    // therefore reaches the outflow of 3 reaches at step t + 3 at the earliest, so the last 3 steps have no effect, and
    // part of the water of the later steps is still in the channel at the end of the run.
    NlParams s;
    s.downstream = true;
    NlChannelSystem sys(s, ramp(1.0, 3.0), 1.0);
    sys.ch.qmin_in_use = false;
    sys.objective();
    const std::vector<double> g = sys.backward();
    EXPECT_EQ(g[T_STEPS - 1], 0.0);
    EXPECT_EQ(g[T_STEPS - 3], 0.0);
    EXPECT_GT(g[T_STEPS - 4], 0.0);
    EXPECT_GT(g[0], g[T_STEPS - 4]);
    EXPECT_LE(g[0], C_DOWN * 1.0001);
}
