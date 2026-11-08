#include <gtest/gtest.h>
#include "herss.h"
#include <cmath>
#include <functional>
#include <vector>

// HERSS-AD: Reservoir::Backward (with Powerstation::Backward and the tunnel flow adjoint) is checked against
// central finite differences of Reservoir::Simulate + Powerstation::Simulate for one time step.
//
// The system is built by hand: a reservoir with geometry and a spillway, a tunnel to a powerstation, a hatch,
// an auto-qmin outlet, and one sink node for each outlet. The objective is
//     J = profit(reservoir) + profit(station) + sum_k c_k * up_inflow(sink k) + W * res_Mm3 + Wm * res_masl,
// i.e. the profit seeds plus assumed adjoints for the water leaving the reservoir and for its end state.
//
// Every smoothed branch has its own test that places the system exactly at the transition (the closed-form
// crossing point of the smooth minimum, plus offsets inside the blend zone), because that is where a wrong
// derivative of a smoothing function would show.

namespace {

const double DT = 86400.0;
const double C  = DT / 1000000.0;     // m3/s -> Mm3 per time step

// Assumed adjoints of the outputs
const double C_HATCH = 0.5, C_AQ = 0.7, C_OVF = 0.3, C_STATION_DOWN = 0.37;
const double W_VOLUME = 3600.0, W_MASL = 2.0;

struct Params {
    double aq_masl = 749.0;     // Outlet level of the auto-qmin release
    double qmin    = 1.2;       // Required auto-qmin release [m3/s]
    double up_in   = 2.0;       // Water arriving at the reservoir [m3/s]
    double a_st    = 0.4;       // Station action (max discharge 10 m3/s)
    double a_hatch = 0.3;       // Hatch action
};

struct Gradients {
    double dV0, dUpInflow, dHatchAction, dStationAction, dInit;
};

struct System {
    Reservoir res;
    Powerstation ps;
    Node hatchNode, aqNode, ovNode, stDown;
    Params p;
    size_t step;                // Time step under test, 0 or 1
    static const size_t STPS = 3;

    System(const Params &params, size_t step_) : p(params), step(step_) {
        res.nodename = "TESTRES";  res.idnr = 0;  res.nodetype = NodeType::RESERVOIR;
        ps.nodename  = "TESTSTATION"; ps.idnr = 1; ps.nodetype = NodeType::PSTATION;
        res.S        = new Scenario(STPS, 86400, 0);
        ps.S         = new Scenario(STPS, 86400, 1);
        hatchNode.S  = new Scenario(STPS, 86400, 2);
        aqNode.S     = new Scenario(STPS, 86400, 3);
        ovNode.S     = new Scenario(STPS, 86400, 4);
        stDown.S     = new Scenario(STPS, 86400, 5);

        // Reservoir: geometry, spillway at HRW, tunnel, hatch and auto-qmin outlets
        res.res_HRW = 757.0;  res.res_LRW = 748.0;  res.res_penalty = 300.0;
        res.use_reservoir_geometry = true;  res.use_reservoir_curve = false;
        res.width_m = 1000.0;  res.length_m = 1000.0;  res.theta = 20.0;  res.bottom_masl = 745.0;
        res.use_spillway = true;  res.spillway_C = 2.1;  res.spillway_L = 50.0;  res.spillway_level_masl = 757.0;
        res.fast_overflow = false;  res.use_overflow_curve = false;
        res.outlet_tunnel_in_use = true;      res.ptr_downstream_node_tunnel = &ps;
        res.outlet_hatch_in_use  = true;      res.ptr_downstream_node_hatch  = &hatchNode;
        res.minQ_hatch = 0.5;  res.maxQ_hatch = 8.0;  res.hatch_masl = 752.0;
        res.outlet_auto_qmin_in_use = true;   res.ptr_downstream_node_auto_qmin = &aqNode;
        res.outlet_auto_qmin_masl = p.aq_masl;
        res.ptr_downstream_node_overflow = &ovNode;
        res.reservoir_init_fr = 0.5;
        res.qmin.nr_periods = 1;
        res.qmin.timeperiods[0].min_discharge = p.qmin;
        res.qmin.timeperiods[0].start_day = 1;   res.qmin.timeperiods[0].start_month = 1;
        res.qmin.timeperiods[0].end_day   = 31;  res.qmin.timeperiods[0].end_month   = 12;
        res.qmin.timeperiods[0].penalty_cost = 0.0;
        for (size_t t = 0; t < STPS; ++t) {
            res.S->year[t] = 2022;  res.S->month[t] = 9;  res.S->day[t] = 1 + int(t);
            res.S->inflow[t] = 0.0;
        }
        res.InitReservoir();     // Computes the volumes at LRW, HRW, hatch level and the spillway crest

        // Powerstation: one generator, shared penstock, quadratic efficiency
        ps.static_gen_efficiency = 0.96;  ps.headlosscoef = 0.03;  ps.powstat_masl = 690.0;
        ps.powstat_startstop = 2.0;  ps.powstat_min_discharge = 0.0;  ps.init_Power = 0.0;
        ps.aggressive_actions_cost = 0.0;  ps.shared_penstock = true;  ps.auto_qmin = -9999.0;
        ps.ptr_downstream_node = &stDown;
        ps.generators.resize(1);
        Generator &gen = ps.generators[0];
        gen.max_discharge = 10.0;  gen.use_uniform_normalized_curve = false;  gen.use_quadratic_eff_curve = true;
        gen.a_quad = -1.610031;  gen.b_quad = 2.478652;  gen.c_quad = 0.000273;
        gen.action.assign(STPS, p.a_st);

        for (size_t t = 0; t < STPS; ++t) {
            ps.S->price[t] = 41.0;
            res.S->action[t][0] = p.a_hatch;
        }
    }

    ~System() {
        delete res.S; delete ps.S; delete hatchNode.S; delete aqNode.S; delete ovNode.S; delete stDown.S;
        res.S = ps.S = hatchNode.S = aqNode.S = ovNode.S = stDown.S = nullptr;
    }

    double &startVolume() { return (step == 0) ? res.reservoir_init_Mm3 : res.S->res_Mm3[step-1]; }
    double &upInflow()    { return res.S->up_inflow[step]; }
    double &hatchAction() { return res.S->action[step][0]; }
    double &stationAction() { return ps.generators[0].action[step]; }

    double requestedStation() const { return p.a_st * 10.0; }                                  // m3/s
    double desiredHatch()     const { return 0.5 + p.a_hatch * (8.0 - 0.5); }                  // m3/s

    double objective() {
        Node *sinks[] = { &hatchNode, &aqNode, &ovNode, &stDown, &ps };
        for (Node *n : sinks) n->S->up_inflow[step] = 0.0;
        res.res_Mm3  = startVolume();
        res.res_masl = res.calcResMasl(res.res_Mm3);
        res.Simulate(step);
        ps.Simulate(step);
        return res.S->profit[step] + ps.S->profit[step]
             + C_HATCH * hatchNode.S->up_inflow[step] + C_AQ * aqNode.S->up_inflow[step]
             + C_OVF * ovNode.S->up_inflow[step] + C_STATION_DOWN * stDown.S->up_inflow[step]
             + W_VOLUME * res.S->res_Mm3[step] + W_MASL * res.S->res_masl[step];
    }

    Gradients backward() {
        Scenario *all[] = { res.S, ps.S, hatchNode.S, aqNode.S, ovNode.S, stDown.S };
        for (Scenario *s : all) s->ClearGradients();
        ps.generators[0].action_grad.assign(STPS, 0.0);
        res.res_Mm3_init_grad = 0.0;

        res.S->profit_grad[step] = 1.0;
        ps.S->profit_grad[step]  = 1.0;
        hatchNode.S->up_inflow_grad[step] = C_HATCH;
        aqNode.S->up_inflow_grad[step]    = C_AQ;
        ovNode.S->up_inflow_grad[step]    = C_OVF;
        stDown.S->up_inflow_grad[step]    = C_STATION_DOWN;
        res.S->res_Mm3_grad[step]  = W_VOLUME;
        res.S->res_masl_grad[step] = W_MASL;

        ps.Backward(step);      // Downstream nodes first, as Riversystem::Backward does
        res.Backward(step);

        Gradients g;
        g.dV0            = (step == 0) ? 0.0 : res.S->res_Mm3_grad[step-1];
        g.dInit          = res.res_Mm3_init_grad;
        g.dUpInflow      = res.S->up_inflow_grad[step];
        g.dHatchAction   = res.S->action_grad[step];
        g.dStationAction = ps.generators[0].action_grad[step];
        return g;
    }

    double centralDiff(double &variable, double h) {
        const double v0 = variable;
        variable = v0 + h;  const double jp = objective();
        variable = v0 - h;  const double jm = objective();
        variable = v0;
        return (jp - jm) / (2.0 * h);
    }

    // Finite-difference estimate closest to the analytic gradient over a sweep of step sizes, central differences
    // and their Richardson extrapolation. A single step size is not enough: too large a step straddles the narrow
    // smoothing zones (and kinks in the second derivative at their edges), too small a step is swamped by rounding
    // in the forward model (calcResMasl subtracts numbers of order 1e9). A wrong gradient does not agree with any of them.
    double bestFiniteDiff(double &variable, double analytic) {
        double best = 1.0e300;
        for (double h : { 1.0e-3, 1.0e-4, 1.0e-5, 1.0e-6, 1.0e-7, 1.0e-8, 1.0e-9 }) {
            const double d1 = centralDiff(variable, h);
            const double d2 = centralDiff(variable, 0.5 * h);
            for (double estimate : { d1, (4.0 * d2 - d1) / 3.0 }) {
                if (std::fabs(estimate - analytic) < std::fabs(best - analytic)) best = estimate;
            }
        }
        return best;
    }
};

void expectClose(double analytic, double fd, const std::string &what, double V0) {
    EXPECT_NEAR(analytic, fd, 1.0e-6 + 5.0e-6 * std::fabs(fd)) << what << " at start volume V0 = " << V0;
}

// Runs all gradient comparisons at one start volume.
void checkPoint(const Params &p, double V0, size_t step = 1) {
    System sys(p, step);
    sys.startVolume() = V0;
    sys.upInflow()    = p.up_in;
    sys.objective();
    const Gradients g = sys.backward();

    if (step == 0) {
        expectClose(g.dInit, sys.bestFiniteDiff(sys.startVolume(), g.dInit), "init volume gradient", V0);
    } else {
        expectClose(g.dV0,   sys.bestFiniteDiff(sys.startVolume(), g.dV0),   "start volume gradient", V0);
    }
    expectClose(g.dUpInflow,      sys.bestFiniteDiff(sys.upInflow(),       g.dUpInflow),      "up_inflow gradient", V0);
    expectClose(g.dHatchAction,   sys.bestFiniteDiff(sys.hatchAction(),    g.dHatchAction),   "hatch action gradient", V0);
    expectClose(g.dStationAction, sys.bestFiniteDiff(sys.stationAction(),  g.dStationAction), "station action gradient", V0);
}

// Offsets, in units of the blend half-width of the smooth minimum (smin_nn blends where |a - b| < EPS_REL_FLOW * a)
const std::vector<double> ZONE_FRACTIONS = { -6.0, -3.0, -1.5, -0.9, -0.5, -0.1, 0.0, 0.1, 0.5, 0.9, 1.5, 3.0, 6.0 };

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
TEST(AdReservoir, InteriorEverythingUncapped) {
    Params p;
    for (double V0 : { 5.0, 8.0, 10.0, 12.0 }) checkPoint(p, V0);
}

TEST(AdReservoir, TunnelFlowCapAndAggressiveCost) {
    // The tunnel flow is smin_nn(requested, water above LRW). Its blend zone and the aggressive-action cost
    // (relu_lo of the excess) are at the same start volume.
    Params p;
    System probe(p, 1);
    const double requested_vol = probe.requestedStation() * C;
    const double V0_cross = probe.res.filling_at_lrw_Mm3 - p.up_in * C + requested_vol;
    for (double frac : ZONE_FRACTIONS) checkPoint(p, V0_cross - frac * EPS_REL_FLOW * requested_vol);
    // Little water above LRW: the zone of relu_lo(V1 - V_LRW)
    for (double d : { -2.0e-4, 1.0e-5, 5.0e-5, 1.5e-4, 4.0e-4 }) {
        checkPoint(p, probe.res.filling_at_lrw_Mm3 - p.up_in * C + d);
    }
}

TEST(AdReservoir, HatchCap) {
    Params p;
    System probe(p, 1);
    const double desired_vol   = probe.desiredHatch() * C;
    const double requested_vol = probe.requestedStation() * C;
    const double V0_cross = probe.res.filling_at_hatchlevel + desired_vol - p.up_in * C + requested_vol;
    for (double frac : ZONE_FRACTIONS) checkPoint(p, V0_cross - frac * EPS_REL_FLOW * desired_vol);
    for (double d : { -2.0e-4, 1.0e-5, 5.0e-5, 1.5e-4, 4.0e-4 }) {
        checkPoint(p, probe.res.filling_at_hatchlevel - p.up_in * C + requested_vol + d);
    }
}

TEST(AdReservoir, AutoQminCap) {
    Params p;
    System probe(p, 1);
    const double fill_outlet  = probe.res.calcResVolume(p.aq_masl);
    const double required_vol = p.qmin * C;
    const double requested_vol = probe.requestedStation() * C;
    const double V0_cross = fill_outlet + required_vol - p.up_in * C + requested_vol;
    for (double frac : ZONE_FRACTIONS) checkPoint(p, V0_cross - frac * EPS_REL_FLOW * required_vol);
    for (double d : { -2.0e-4, 1.0e-5, 5.0e-5, 1.5e-4, 4.0e-4 }) {
        checkPoint(p, fill_outlet - p.up_in * C + requested_vol + d);
    }
}

TEST(AdReservoir, SpillwayAndOverflowCap) {
    Params p;
    System probe(p, 1);
    const Reservoir &r = probe.res;
    const double crest = r.filling_at_spillway_Mm3;
    const double aq_vol = p.qmin * C;
    const double V0_of_V4 = -p.up_in * C + (probe.requestedStation() + probe.desiredHatch()) * C + aq_vol;   // V0 = V4 + this

    // The spillway law o(V4) and the availability V4 - crest cross twice: just above the crest, where the smoothed head
    // has a small tail, and again at larger heights where the law outgrows the availability. At a crossing the overflow
    // switches between the law and the cap, so the blend zone of smin_nn is there.
    auto law = [&](double V4) {
        const double h = relu_hi(0.5 * (probe.res.calcResMasl(V4) - r.spillway_level_masl), EPS_M);
        return r.spillway_C * r.spillway_L * h * std::sqrt(h) * C;
    };
    auto g = [&](double V4) { return law(V4) - (V4 - crest); };       // o - b
    auto findCrossing = [&](double lo, double hi) {
        const bool rising = g(lo) < 0.0;
        EXPECT_NE(g(lo) < 0.0, g(hi) < 0.0) << "no crossing in the bracket";
        for (int i = 0; i < 200; ++i) {
            const double mid = 0.5 * (lo + hi);
            ((g(mid) < 0.0) == rising ? lo : hi) = mid;
        }
        return 0.5 * (lo + hi);
    };
    const double crossings[] = { findCrossing(crest + 1.0e-5, crest + 0.02), findCrossing(crest + 0.02, crest + 1.0) };

    // Head zone (relu_hi of the head, EPS_M), availability zone (relu_lo, EPS_MM3), and the saturated regions
    for (double x : { -0.03, -0.01, -0.005, -0.001, 0.0, 1.0e-5, 5.0e-5, 1.5e-4, 5.0e-4, 0.002, 0.005, 0.01, 0.02, 0.05, 0.15, 0.3, 1.0 }) {
        checkPoint(p, crest + x + V0_of_V4);
    }
    for (double V4_cross : crossings) {
        const double slope = (g(V4_cross + 1.0e-7) - g(V4_cross - 1.0e-7)) / 2.0e-7;    // d(o - b)/dV4
        const double o_cross = law(V4_cross);
        for (double frac : ZONE_FRACTIONS) checkPoint(p, V4_cross + frac * EPS_REL_FLOW * o_cross / slope + V0_of_V4);
    }
}

TEST(AdReservoir, LrwPenalty) {
    // The auto-qmin outlet is below LRW, so the level ends below LRW by roughly qmin * dt.
    // 0.0 puts the end level just above LRW, 0.05 inside the penalty blend zone (EPS_M = 1 cm), 0.15 outside it.
    for (double qmin : { 0.0, 0.05, 0.15 }) {
        Params p;
        p.aq_masl = 746.0;
        p.qmin    = qmin;
        System probe(p, 1);
        const double requested_vol = probe.requestedStation() * C;
        const double V0_cross = probe.res.filling_at_lrw_Mm3 - p.up_in * C + requested_vol;
        for (double frac : ZONE_FRACTIONS) checkPoint(p, V0_cross - frac * EPS_REL_FLOW * requested_vol);
        for (double d : { 0.0, 0.5, 1.0, 2.0 }) checkPoint(p, V0_cross + d);
    }
}

TEST(AdReservoir, StartOfRunGradientIsTheInitialVolumeGradient) {
    Params p;
    System probe(p, 0);
    for (double V0 : { 6.0, 9.0, 12.4 }) checkPoint(p, V0, 0);
    const double requested_vol = probe.requestedStation() * C;
    checkPoint(p, probe.res.filling_at_lrw_Mm3 - p.up_in * C + requested_vol, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// HATCH_ADJUSTMENT_COST: cost = fee (sqrt(d^2 + eps^2) - eps), d = action[t] - action[t-1], free at t = 0.
TEST(AdReservoir, HatchAdjustmentCostGradient) {
    const double fee = 1500.0;
    // The previous action is 0.3 (the System default). Changes of 0, inside the smoothing zone, and far outside it.
    for (double a_now : { 0.3, 0.301, 0.305, 0.31, 0.35, 0.5, 0.05 }) {
        Params p;
        System sys(p, 1);
        sys.res.hatch_adjustment_cost = fee;
        sys.startVolume() = 9.0;
        sys.upInflow()    = p.up_in;
        sys.hatchAction() = a_now;
        sys.objective();
        const Gradients g = sys.backward();
        const double g_prev = sys.res.S->action_grad[0];

        const double d = a_now - 0.3;
        const double expected_cost = fee * (std::sqrt(d * d + EPS_HATCH_ACTION * EPS_HATCH_ACTION) - EPS_HATCH_ACTION);
        EXPECT_NEAR(sys.res.S->adjust_cost[1], expected_cost, 1.0e-9);

        double &prev = sys.res.S->action[0][0];
        expectClose(g.dHatchAction, sys.bestFiniteDiff(sys.hatchAction(), g.dHatchAction), "hatch action gradient, step t", a_now);
        expectClose(g_prev,         sys.bestFiniteDiff(prev, g_prev),                      "hatch action gradient, step t-1", a_now);
    }
}

TEST(AdReservoir, HatchAdjustmentCostIsFreeAtFirstStep) {
    Params p;
    System with(p, 0), without(p, 0);
    with.res.hatch_adjustment_cost = 1500.0;
    for (System *s : { &with, &without }) {
        s->startVolume() = 9.0;
        s->upInflow()    = p.up_in;
        s->hatchAction() = 0.8;
    }
    EXPECT_DOUBLE_EQ(with.objective(), without.objective());
    EXPECT_EQ(with.res.S->adjust_cost[0], 0.0);
}
