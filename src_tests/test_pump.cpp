/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     test_pump.cpp
Developer:    Terje Sandø (pump-station work, July 2026)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

Permanent regression tests for the PUMP node type. Two groups:

1. PumpUnitTest - lightweight, no file I/O, mirrors the style of
   test_reservoir.cpp/test_riversystem.cpp (construct a Pump directly, set
   only the fields a given test needs).

2. PumpIntegrationTest - loads the real fixture dataset in src_tests/pump_test/
   (a copy of data/mini_pump_test/) through the normal Herss startup sequence,
   and includes a VF-payoff A/B comparison: running the SAME topology twice
   with only the pump's action changed must increase the target reservoir's
   water, decrease the source reservoir's water by the exact same amount
   (mass conservation), and increase tot_remaining_MWh by exactly
   moved_Mm3 * LOCAL_ENERGY_EQUIVALENT.
********************************************************************************/

#include <gtest/gtest.h>
#include "herss.h"

// ================================================================
// PumpUnitTest
// ================================================================

class PumpUnitTest : public ::testing::Test {
protected:
    Pump pump;
    GlobalConfig* gc = nullptr;

    void SetUp() override {
        gc = new GlobalConfig();
        gc->nr_nodes = 6;
        for(size_t i = 0; i < gc->nr_nodes; i++) {
            gc->nodetypes[i] = NodeType::RESERVOIR;
        }
        pump.gc = gc;
        pump.idnr = 0;
        pump.nodename = "TEST_PUMP";
    }

    void TearDown() override {
        delete gc;
    }

    // Minimum set of fields ValidatePumpSettings needs beyond source/target
    // idnr, so individual tests only vary the one thing they are testing.
    void setValidBaseline() {
        pump.pump_source_idnr = 1;
        pump.pump_target_idnr = 2;
        pump.max_discharge = 2.0;
        pump.headlosscoef = 0.1;
        pump.static_motor_efficiency = 0.97;
        pump.pump_inlet_masl = 10.0;
        pump.pump_startstop = 100.0;
        pump.use_uniform_normalized_curve = true;
        for(size_t i = 0; i < N_UNIFORM_EFF_CURVE_POINTS; i++) {
            pump.uniform_normalized_curve[i] = 80.0;
        }
    }
};

TEST_F(PumpUnitTest, Constructor_InitializesCorrectly) {
    Pump p;
    EXPECT_EQ(p.pump_source_idnr, size_t(NOT_INIT));
    EXPECT_EQ(p.pump_target_idnr, size_t(NOT_INIT));
    EXPECT_FALSE(p.pump_source_target_in_use);
    EXPECT_FALSE(p.use_uniform_normalized_curve);
    EXPECT_EQ(p.ptr_pump_source, nullptr);
    EXPECT_EQ(p.ptr_pump_target, nullptr);
}

TEST_F(PumpUnitTest, GetStartAndEndWater_AreAlwaysZero) {
    // A pump stores no water of its own.
    EXPECT_DOUBLE_EQ(pump.GetStartWater_Mm3(), 0.0);
    EXPECT_DOUBLE_EQ(pump.GetEndWater_Mm3(), 0.0);
}

TEST_F(PumpUnitTest, CheckWaterBalance_IsNoOp) {
    // Mass conservation is guaranteed by construction in Simulate(); nothing
    // to check here (see pump.cpp).
    EXPECT_EQ(pump.CheckWaterBalance(nullptr), 0);
}

TEST_F(PumpUnitTest, CalcEfficiency_UniformCurve_InterpolatesCorrectly) {
    pump.max_discharge = 10.0;
    pump.use_uniform_normalized_curve = true;
    double curve[11] = {0, 50, 75, 86, 90, 92, 91.5, 90, 88, 85, 82};
    for(int i = 0; i < 11; i++) pump.uniform_normalized_curve[i] = curve[i];

    EXPECT_DOUBLE_EQ(pump.calcEfficiency(0.0), 0.0);          // below the 1e-6 guard
    EXPECT_NEAR(pump.calcEfficiency(5.0), 0.92, 1e-9);        // Qn=0.5 exactly -> point 5
    EXPECT_NEAR(pump.calcEfficiency(2.5), 0.805, 1e-9);       // Qn=0.25 -> halfway 75/86
}

TEST_F(PumpUnitTest, CalcEfficiency_NonUniformCurve_InterpolatesCorrectly) {
    pump.max_discharge = 2.0;
    pump.use_uniform_normalized_curve = false;
    pump.pump_curve_Q    = {0.0, 1.0, 2.0};
    pump.pump_curve_psnt = {0.0, 90.0, 78.0};
    pump.pump_eff_curve.nr_pts = 3;
    for(size_t i = 0; i < 3; i++) {
        pump.pump_eff_curve.x_points[i] = pump.pump_curve_Q[i];
        pump.pump_eff_curve.y_points[i] = pump.pump_curve_psnt[i];
    }
    pump.pump_eff_curve.initializeArrays();

    EXPECT_NEAR(pump.calcEfficiency(1.0), 0.90, 1e-9);
}

TEST_F(PumpUnitTest, CalcEfficiency_NegativeDischarge_Dies) {
    setValidBaseline();
    EXPECT_DEATH(pump.calcEfficiency(-1.0), ".*");
}

TEST_F(PumpUnitTest, CalcEfficiency_AboveMaxDischarge_Dies) {
    setValidBaseline();
    EXPECT_DEATH(pump.calcEfficiency(pump.max_discharge * 2.0), ".*");
}

TEST_F(PumpUnitTest, ValidatePumpSettings_ValidSettings_DoesNotDie) {
    setValidBaseline();
    pump.ValidatePumpSettings();
    SUCCEED();
}

TEST_F(PumpUnitTest, ValidatePumpSettings_NegativeStartStop_Dies) {
    setValidBaseline();
    pump.pump_startstop = -1.0;
    EXPECT_DEATH(pump.ValidatePumpSettings(), ".*");
}

// A PUMP_STARTSTOP left undefined keeps its negative NOT_INIT sentinel, which would
// otherwise turn every start/stop event into a large negative (i.e. rewarded) cost.
TEST_F(PumpUnitTest, ValidatePumpSettings_MissingStartStop_Dies) {
    setValidBaseline();
    Pump fresh;
    fresh.gc = gc;
    fresh.idnr = 0;
    fresh.nodename = "TEST_PUMP";
    fresh.pump_source_idnr = 1;
    fresh.pump_target_idnr = 2;
    fresh.max_discharge = 2.0;
    fresh.headlosscoef = 0.1;
    fresh.static_motor_efficiency = 0.97;
    fresh.pump_inlet_masl = 10.0;
    fresh.use_uniform_normalized_curve = true;
    for(size_t i = 0; i < N_UNIFORM_EFF_CURVE_POINTS; i++) {
        fresh.uniform_normalized_curve[i] = 80.0;
    }
    EXPECT_DEATH(fresh.ValidatePumpSettings(), ".*");
}

TEST_F(PumpUnitTest, ValidatePumpSettings_MissingSourceOrTarget_Dies) {
    setValidBaseline();
    pump.pump_source_idnr = NOT_INIT;
    EXPECT_DEATH(pump.ValidatePumpSettings(), ".*");
}

TEST_F(PumpUnitTest, ValidatePumpSettings_SelfReference_Dies) {
    setValidBaseline();
    pump.pump_source_idnr = pump.idnr;
    EXPECT_DEATH(pump.ValidatePumpSettings(), ".*");
}

TEST_F(PumpUnitTest, ValidatePumpSettings_SourceEqualsTarget_Dies) {
    setValidBaseline();
    pump.pump_target_idnr = pump.pump_source_idnr;
    EXPECT_DEATH(pump.ValidatePumpSettings(), ".*");
}

TEST_F(PumpUnitTest, ValidatePumpSettings_SourceNotReservoir_Dies) {
    setValidBaseline();
    gc->nodetypes[pump.pump_source_idnr] = NodeType::CHANNEL;
    EXPECT_DEATH(pump.ValidatePumpSettings(), ".*");
}

TEST_F(PumpUnitTest, ValidatePumpSettings_TargetNotReservoir_Dies) {
    setValidBaseline();
    gc->nodetypes[pump.pump_target_idnr] = NodeType::PSTATION;
    EXPECT_DEATH(pump.ValidatePumpSettings(), ".*");
}

// The core architectural rule: a PUMP's idnr must be strictly LOWER than both
// its source and target, since Herss::Simulate() is single-pass. Deliberately
// tested with values that would have been valid under a same-timestep redo
// design, to guard against ever silently reintroducing that more permissive
// and bug-prone behaviour.
TEST_F(PumpUnitTest, ValidatePumpSettings_SourceIdnrNotHigherThanPump_Dies) {
    setValidBaseline();
    pump.idnr = 5;
    pump.pump_source_idnr = 5; // equal - not higher
    EXPECT_DEATH(pump.ValidatePumpSettings(), ".*");

    pump.pump_source_idnr = 3; // lower - the old "redo-triggering" case
    EXPECT_DEATH(pump.ValidatePumpSettings(), ".*");
}

TEST_F(PumpUnitTest, ValidatePumpSettings_TargetIdnrNotHigherThanPump_Dies) {
    setValidBaseline();
    pump.idnr = 5;
    pump.pump_target_idnr = 5; // equal - not higher
    EXPECT_DEATH(pump.ValidatePumpSettings(), ".*");

    pump.pump_target_idnr = 1; // lower
    EXPECT_DEATH(pump.ValidatePumpSettings(), ".*");
}

// ================================================================
// PumpIntegrationTest - loads src_tests/pump_test/ (copy of
// data/mini_pump_test/) through the real Herss startup sequence.
// ================================================================

class PumpIntegrationTest : public ::testing::Test {
protected:
    GlobalConfig* gc = nullptr;
    Dataset* data = nullptr;
    Herss* herss_obj = nullptr;

    void SetUp() override {
        gc = new GlobalConfig();
        gc->globalfile = "../src_tests/pump_test/global.txt";
        gc->readGlobalFile();
        gc->SetDirectoriesAndFilenames();
        gc->Diagnose();
        gc->checkNrSteps();

        data = new Dataset(gc);
        data->readAllData();

        herss_obj = new Herss(gc);
        herss_obj->prepaireSimulation(data);
        herss_obj->rs->DiagnoseRiversystemConfiguration();
    }

    void TearDown() override {
        delete herss_obj;
        delete data;
        delete gc;
    }
};

TEST_F(PumpIntegrationTest, LoadsTopologyWithCorrectPumpWiring) {
    ASSERT_EQ(gc->nr_pumps, 1u);
    Pump* pump = &herss_obj->rs->pumps[0];
    EXPECT_EQ(pump->pump_source_idnr, 3u); // reservoir_B
    EXPECT_EQ(pump->pump_target_idnr, 1u); // reservoir_A
    EXPECT_TRUE(pump->pump_source_target_in_use);
    ASSERT_NE(pump->ptr_pump_source, nullptr);
    ASSERT_NE(pump->ptr_pump_target, nullptr);
    EXPECT_EQ(pump->ptr_pump_source->nodetype, NodeType::RESERVOIR);
    EXPECT_EQ(pump->ptr_pump_target->nodetype, NodeType::RESERVOIR);
}

TEST_F(PumpIntegrationTest, HerssSetActionAndGetActionWorkForPumpNodes) {
    Pump* pump = &herss_obj->rs->pumps[0];
    const size_t pump_idnr = pump->idnr;

    herss_obj->SetAction(pump_idnr, 0, 0, 0.75);

    EXPECT_DOUBLE_EQ(herss_obj->GetAction(pump_idnr, 0, 0), 0.75);
    EXPECT_DOUBLE_EQ(pump->S->action[0][pump_idnr], 0.75);
}

TEST_F(PumpIntegrationTest, FullSimulation_CompletesWithCleanWaterBalance) {
    // CheckWaterBalance()/GlobalWaterBalance() abort the process (LOG_ERR) if
    // any node's water balance is off by more than 1e-4 Mm3 - reaching
    // SUCCEED() below is itself the assertion that both passed cleanly.
    herss_obj->Simulate();
    herss_obj->CheckWaterBalance();
    herss_obj->GlobalWaterBalance();
    SUCCEED();
}

// VF-payoff A/B comparison. Runs the SAME topology twice, changing only the
// pump's action between runs, and checks that CalcVF's existing (pump-unaware)
// remaining-water machinery values the pumped water exactly right - no
// pump-specific code exists in that machinery; this test is what proves that
// is safe.
TEST_F(PumpIntegrationTest, PumpingMovesWaterAndIncreasesRemainingValueExactly) {
    Pump* pump = &herss_obj->rs->pumps[0];
    const size_t pump_idnr = pump->idnr;
    Reservoir* reservoir_A = static_cast<Reservoir*>(herss_obj->rs->nodes[1]); // target
    Reservoir* reservoir_B = static_cast<Reservoir*>(herss_obj->rs->nodes[3]); // source
    const double local_energy_equivalent_MWh_per_Mm3 = 78.5; // pstation_A: 0.0785 kWh/m3 * 1e6 / 1000

    // Run A: pump never active.
    for(size_t t = 0; t < gc->stps; t++) pump->S->action[t][pump_idnr] = 0.0;
    herss_obj->Simulate();
    herss_obj->rs->CalcVF(data->restprice);
    const double resA_end_no_pump = reservoir_A->res_Mm3;
    const double resB_end_no_pump = reservoir_B->res_Mm3;
    const double remaining_MWh_no_pump = herss_obj->rs->tot_remaining_MWh;

    // Run B: pump at full discharge every timestep.
    for(size_t t = 0; t < gc->stps; t++) pump->S->action[t][pump_idnr] = 1.0;
    herss_obj->Simulate();
    herss_obj->rs->CalcVF(data->restprice);
    const double resA_end_with_pump = reservoir_A->res_Mm3;
    const double resB_end_with_pump = reservoir_B->res_Mm3;
    const double remaining_MWh_with_pump = herss_obj->rs->tot_remaining_MWh;

    const double moved_Mm3 = resA_end_with_pump - resA_end_no_pump;
    ASSERT_GT(moved_Mm3, 0.01); // pump actually did something

    // Mass conservation: source lost exactly what the target gained.
    EXPECT_NEAR(resB_end_no_pump - resB_end_with_pump, moved_Mm3, 1e-6);

    // The core claim: CalcVF's existing (pump-unaware) remaining-water
    // propagation values the pumped water exactly right.
    const double expected_MWh_gain = moved_Mm3 * local_energy_equivalent_MWh_per_Mm3;
    EXPECT_NEAR(remaining_MWh_with_pump - remaining_MWh_no_pump, expected_MWh_gain, 1e-3);
}

// PUMP_STARTSTOP mirrors POWSTAT_STARTSTOP: half the cost per start event and half per
// stop event, based on the action (the requested operating state), not on how much water
// the pump actually managed to move.
TEST_F(PumpIntegrationTest, StartStopCostIsChargedOnEveryTransition) {
    Pump* pump = &herss_obj->rs->pumps[0];
    const size_t pump_idnr = pump->idnr;
    const double half_cost = pump->pump_startstop / 2.0;
    ASSERT_GT(half_cost, 0.0);

    // off, on, on, off, off, ...
    for(size_t t = 0; t < gc->stps; t++) pump->S->action[t][pump_idnr] = 0.0;
    pump->S->action[1][pump_idnr] = 1.0;
    pump->S->action[2][pump_idnr] = 1.0;

    pump->init_Power = 0.0; // the pump was off at the end of the previous run
    herss_obj->Simulate();

    EXPECT_DOUBLE_EQ(pump->S->startStopCost[0], 0.0);       // off -> off
    EXPECT_DOUBLE_EQ(pump->S->startStopCost[1], half_cost); // off -> on  (start)
    EXPECT_DOUBLE_EQ(pump->S->startStopCost[2], 0.0);       // on  -> on
    EXPECT_DOUBLE_EQ(pump->S->startStopCost[3], half_cost); // on  -> off (stop)
}

// The statefile decides what happened BEFORE t=0, so a pump that was already running is
// not charged a start cost for simply continuing to run.
TEST_F(PumpIntegrationTest, FirstTimestepStartStopUsesTheStartState) {
    Pump* pump = &herss_obj->rs->pumps[0];
    const size_t pump_idnr = pump->idnr;
    const double half_cost = pump->pump_startstop / 2.0;

    for(size_t t = 0; t < gc->stps; t++) pump->S->action[t][pump_idnr] = 1.0;

    pump->init_Power = 0.0; // was off
    herss_obj->Simulate();
    const double cost_when_previously_off = pump->S->startStopCost[0];

    pump->init_Power = 0.9; // was running
    herss_obj->Simulate();
    const double cost_when_previously_on = pump->S->startStopCost[0];

    EXPECT_DOUBLE_EQ(cost_when_previously_off, half_cost);
    EXPECT_DOUBLE_EQ(cost_when_previously_on, 0.0);
}

// The statefile row must round-trip, so a chained run starts from the correct state.
TEST_F(PumpIntegrationTest, StateFileRoundTripsThePumpOperatingState) {
    Pump* pump = &herss_obj->rs->pumps[0];
    EXPECT_DOUBLE_EQ(pump->init_Power, 0.0); // read from src_tests/pump_test/start_state.txt
}

// tot_inflow/tot_outflow are the reservoir's TOTAL physical flows, so pumped water has to
// show up there next to the ordinary inflows and outlets - that is what makes the water
// balance work without a separate pump bookkeeping term.
TEST_F(PumpIntegrationTest, ReservoirTotalFlowsIncludePumpedWater) {
    Pump* pump = &herss_obj->rs->pumps[0];
    const size_t pump_idnr = pump->idnr;
    Reservoir* target = static_cast<Reservoir*>(herss_obj->rs->nodes[1]);
    Reservoir* source = static_cast<Reservoir*>(herss_obj->rs->nodes[3]);

    for(size_t t = 0; t < gc->stps; t++) pump->S->action[t][pump_idnr] = 1.0;
    herss_obj->Simulate();

    bool pumped_at_least_once = false;

    for(size_t t = 0; t < gc->stps; t++) {
        EXPECT_NEAR(target->S->tot_inflow[t],
                    target->S->inflow[t] + target->S->up_inflow[t] + target->S->pump_in_m3s[t],
                    1e-9);

        const double source_ordinary_out = source->S->tunnelflow_m3s[t] + source->S->hatchflow_m3s[t]
                                         + source->S->overflow_m3s[t] + source->S->auto_qmin_m3s[t];
        EXPECT_NEAR(source->S->tot_outflow[t],
                    source_ordinary_out + source->S->pump_out_m3s[t],
                    1e-9);

        // One pump, so what leaves the source in a timestep is exactly what enters the target.
        EXPECT_NEAR(source->S->pump_out_m3s[t], target->S->pump_in_m3s[t], 1e-9);

        if(source->S->pump_out_m3s[t] > 1e-9) pumped_at_least_once = true;
    }

    EXPECT_TRUE(pumped_at_least_once);

    // Both reservoirs still balance (CheckWaterBalance aborts the process if they do not).
    herss_obj->CheckWaterBalance();
    herss_obj->GlobalWaterBalance();
}

// pump_in_m3s/pump_out_m3s are accumulated with +=, so they must be cleared before every
// run - otherwise a second Simulate() on the same object doubles them and the water
// balance breaks.
TEST_F(PumpIntegrationTest, PumpFlowSeriesAreResetBetweenSimulations) {
    Pump* pump = &herss_obj->rs->pumps[0];
    const size_t pump_idnr = pump->idnr;
    Reservoir* source = static_cast<Reservoir*>(herss_obj->rs->nodes[3]);
    Reservoir* target = static_cast<Reservoir*>(herss_obj->rs->nodes[1]);

    for(size_t t = 0; t < gc->stps; t++) pump->S->action[t][pump_idnr] = 1.0;

    herss_obj->Simulate();
    std::vector<double> pump_out_first_run(gc->stps);
    std::vector<double> pump_in_first_run(gc->stps);
    for(size_t t = 0; t < gc->stps; t++) {
        pump_out_first_run[t] = source->S->pump_out_m3s[t];
        pump_in_first_run[t]  = target->S->pump_in_m3s[t];
    }

    herss_obj->Simulate();
    for(size_t t = 0; t < gc->stps; t++) {
        EXPECT_NEAR(source->S->pump_out_m3s[t], pump_out_first_run[t], 1e-9);
        EXPECT_NEAR(target->S->pump_in_m3s[t], pump_in_first_run[t], 1e-9);
    }

    herss_obj->CheckWaterBalance();
}
