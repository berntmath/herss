#include <gtest/gtest.h>
#include "herss.h"
#include <string>
#include <fstream>
#include <cmath>
#include <vector>

// Test fixture for Powerstation tests using real uTAHPS topology data
class PowerstationTest : public ::testing::Test
{
protected:
    Powerstation* powerstation;
    GlobalConfig* gc;
    Scenario* scenario;
    Herss* herss_obj;
    Dataset* dataset;
    
    void SetUp() override 
    {
        // Initialize global config with real uTAHPS parameters
        gc = new GlobalConfig();
        gc->globalfile = "../src_tests/utahps_test/global.txt";
        gc->topologyfile = "../src_tests/utahps_test/topology.txt";
        gc->pricefile = "../src_tests/utahps_test/pricefile.txt";
        gc->inflowfile = "../src_tests/utahps_test/inflow.txt";
        gc->actionsfile = "../src_tests/utahps_test/actions.txt";
        gc->start_statefile = "../src_tests/utahps_test/start_state.txt";
        gc->outputdir = "../src_tests/utahps_test/output/";
        
        // Read global configuration
        gc->readGlobalFile();
    // Use INPUTDIR/OUTPUTDIR prefixes from global.txt for relative filenames
    gc->SetDirectoriesAndFilenames();
    // Derive sizes from topology and price file for a consistent Herss setup
    gc->DiagnoseTopologyFile();
    gc->checkNrSteps();
        
    // Set up basic test parameters (explicit overrides if needed)
    // gc->stps = 16; // Determined by pricefile
    // gc->nr_nodes = 12; // Determined by topology
        
        // Create dataset to initialize price and time data
        dataset = new Dataset(gc);
        dataset->readAllData(); // This loads real price/inflow data and calls multi_temporal_resolution
        
        // Create herss object for variable timestep testing  
        herss_obj = new Herss(gc);
        herss_obj->data = dataset;
        
        // Initialize powerstation - we'll set specific ones in individual tests
        powerstation = nullptr;
    }
    
    void TearDown() override 
    {
        if (powerstation) {
            delete powerstation;
        }
        delete herss_obj;
        delete dataset;
        delete gc;
    }
    
    // Set up SVOLETJONN powerstation (idnr=1) from uTAHPS topology
    void setupSvoletjonn() {
        powerstation = new Powerstation();
        powerstation->idnr = 1;
        powerstation->nodename = "SVOLETJONN";
        powerstation->stps = gc->stps;
        
        // Initialize scenario data
        scenario = new Scenario(gc->stps, gc->dt, 0);
        powerstation->S = scenario;
        
        // Real SVOLETJONN parameters from topology.txt
        powerstation->static_gen_efficiency = 0.96;
        powerstation->headlosscoef = 0.3;
        powerstation->powstat_masl = 690.0;
        powerstation->powstat_min_discharge = 0.0;
        powerstation->powstat_startstop = 2.0;
        powerstation->shared_penstock = false; // FALSE in topology
        powerstation->auto_qmin = -9999.0;
        powerstation->aggressive_actions_cost = 0.0;
        powerstation->local_energy_equivalent = 0.11;
        powerstation->downstream_idnr = 2;
        powerstation->downstream_node_in_use = true;
        
        // Set realistic water levels (HJELLE reservoir feeds SVOLETJONN)
        powerstation->start_of_stp_masl = 752.5; // Mid-range of HJELLE (748-757)
        powerstation->end_of_stp_masl = 700.0;   // Downstream level
        
        // Real SVOLETJONN turbine efficiency curve from topology.txt
        powerstation->generators.resize(1);
        powerstation->generators[0].turb_virkn_Q = {0.00, 1.00, 1.72, 2.27, 2.79, 3.19, 3.47, 3.67, 3.79, 4.10};
        powerstation->generators[0].turb_virkn_psnt = {0, 50, 80, 90, 93, 93, 93, 92, 91, 88};
        powerstation->generators[0].max_discharge = 4.0; // Real max discharge
        
        // Initialize efficiency curve
        powerstation->generators[0].eff_curve.nr_pts = 10;
        for (int i = 0; i < 10; i++) {
            powerstation->generators[0].eff_curve.x_points[i] = powerstation->generators[0].turb_virkn_Q[i];
            powerstation->generators[0].eff_curve.y_points[i] = powerstation->generators[0].turb_virkn_psnt[i];
        }
        powerstation->generators[0].eff_curve.initializeArrays();
        
        // Initialize actions and scenario data
        powerstation->generators[0].action.resize(gc->stps, 0.0);
        
        // Copy real price and time data from dataset
        for (size_t t = 0; t < gc->stps; t++) {
            scenario->price[t] = dataset->price[t];
            scenario->year[t] = dataset->year[t];
            scenario->month[t] = dataset->month[t];
            scenario->day[t] = dataset->day[t];
            scenario->hour[t] = dataset->hour[t];
            scenario->up_inflow[t] = 0.0;
            scenario->inflow[t] = 0.0;
            scenario->dt = gc->dt; // Will be updated per timestep in simulation
        }
    }
    
    // Set up SVEIGSHYL_I powerstation (idnr=6) from uTAHPS topology
    void setupSveigshylI() {
        powerstation = new Powerstation();
        powerstation->idnr = 6;
        powerstation->nodename = "SVEIGSHYL_I";
        powerstation->stps = gc->stps;
        
        // Initialize scenario data
        scenario = new Scenario(gc->stps, gc->dt, 0);
        powerstation->S = scenario;
        
        // Real SVEIGSHYL_I parameters from topology.txt
        powerstation->static_gen_efficiency = 0.96;
        powerstation->headlosscoef = 0.2;
        powerstation->powstat_masl = 581.0;
        powerstation->powstat_min_discharge = 0.0;
        powerstation->powstat_startstop = 2.0;
        powerstation->shared_penstock = false;
        powerstation->auto_qmin = -9999.0;
        powerstation->aggressive_actions_cost = 0.0;
        powerstation->local_energy_equivalent = 0.11;
        powerstation->downstream_idnr = 8;
        powerstation->downstream_node_in_use = true;
        
        // Set realistic water levels (TOPPSY reservoir feeds SVEIGSHYL_I)
        powerstation->start_of_stp_masl = 635.0; // Mid-range of TOPPSY (620-650)
        powerstation->end_of_stp_masl = 590.0;   // Downstream level
        
        // Real SVEIGSHYL_I turbine efficiency curve from topology.txt
        powerstation->generators.resize(1);
        powerstation->generators[0].turb_virkn_Q = {0.00, 1.48, 2.54, 3.36, 4.13, 4.72, 5.13, 5.43, 5.61, 5.90};
        powerstation->generators[0].turb_virkn_psnt = {0.0, 50.0, 80.0, 90.0, 93.0, 93.0, 93.0, 92.0, 91.0, 90.0};
        powerstation->generators[0].max_discharge = 5.9;
        
        // Initialize efficiency curve
        powerstation->generators[0].eff_curve.nr_pts = 10;
        for (int i = 0; i < 10; i++) {
            powerstation->generators[0].eff_curve.x_points[i] = powerstation->generators[0].turb_virkn_Q[i];
            powerstation->generators[0].eff_curve.y_points[i] = powerstation->generators[0].turb_virkn_psnt[i];
        }
        powerstation->generators[0].eff_curve.initializeArrays();
        
        // Initialize actions and scenario data
        powerstation->generators[0].action.resize(gc->stps, 0.0);
        
        // Copy real data from dataset
        for (size_t t = 0; t < gc->stps; t++) {
            scenario->price[t] = dataset->price[t];
            scenario->year[t] = dataset->year[t];
            scenario->month[t] = dataset->month[t];
            scenario->day[t] = dataset->day[t];
            scenario->hour[t] = dataset->hour[t];
            scenario->up_inflow[t] = 0.0;
            scenario->inflow[t] = 0.0;
            scenario->dt = gc->dt;
        }
    }
    
    // Set up EASTER powerstation (idnr=10) - largest unit in uTAHPS
    void setupEaster() {
        powerstation = new Powerstation();
        powerstation->idnr = 10;
        powerstation->nodename = "EASTER";
        powerstation->stps = gc->stps;
        
        // Initialize scenario data
        scenario = new Scenario(gc->stps, gc->dt, 0);
        powerstation->S = scenario;
        
        // Real EASTER parameters from topology.txt
        powerstation->static_gen_efficiency = 0.96;
        powerstation->headlosscoef = 0.145;
        powerstation->powstat_masl = 218.0;
        powerstation->powstat_min_discharge = 0.0;
        powerstation->powstat_startstop = 2.0;
        powerstation->shared_penstock = false;
        powerstation->auto_qmin = -9999.0;
        powerstation->aggressive_actions_cost = 0.0;
        powerstation->local_energy_equivalent = 0.11;
        powerstation->downstream_idnr = 11;
        powerstation->downstream_node_in_use = true;
        
        // Set realistic water levels (KROKNESVATN reservoir feeds EASTER)
        powerstation->start_of_stp_masl = 400.0; // Mid-range of KROKNESVATN (333-433)
        powerstation->end_of_stp_masl = 230.0;   // Downstream level
        
        // Real EASTER turbine efficiency curve from topology.txt
        powerstation->generators.resize(1);
        powerstation->generators[0].turb_virkn_Q = {0.00, 2.45, 4.21, 5.59, 6.86, 7.84, 8.53, 9.02, 9.31, 9.80};
        powerstation->generators[0].turb_virkn_psnt = {0.00, 50.00, 80.00, 90.00, 93.00, 93.00, 93.00, 92.00, 91.00, 90.00};
        powerstation->generators[0].max_discharge = 9.8; // Largest in the system
        
        // Initialize efficiency curve
        powerstation->generators[0].eff_curve.nr_pts = 10;
        for (int i = 0; i < 10; i++) {
            powerstation->generators[0].eff_curve.x_points[i] = powerstation->generators[0].turb_virkn_Q[i];
            powerstation->generators[0].eff_curve.y_points[i] = powerstation->generators[0].turb_virkn_psnt[i];
        }
        powerstation->generators[0].eff_curve.initializeArrays();
        
        // Initialize actions and scenario data
        powerstation->generators[0].action.resize(gc->stps, 0.0);
        
        // Copy real data from dataset
        for (size_t t = 0; t < gc->stps; t++) {
            scenario->price[t] = dataset->price[t];
            scenario->year[t] = dataset->year[t];
            scenario->month[t] = dataset->month[t];
            scenario->day[t] = dataset->day[t];
            scenario->hour[t] = dataset->hour[t];
            scenario->up_inflow[t] = 0.0;
            scenario->inflow[t] = 0.0;
            scenario->dt = gc->dt;
        }
    }
    
    // Set up dual generator powerstation for testing multiple generators
    void setupDualGenerators() {
        powerstation = new Powerstation();
        powerstation->idnr = 99; // Test ID
        powerstation->nodename = "TEST_DUAL";
        powerstation->stps = gc->stps;
        
        // Initialize scenario data
        scenario = new Scenario(gc->stps, gc->dt, 0);
        powerstation->S = scenario;
        
        // Test parameters for dual generator setup
        powerstation->static_gen_efficiency = 0.95;
        powerstation->headlosscoef = 0.001;
        powerstation->powstat_masl = 100.0;
        powerstation->powstat_min_discharge = 0.0;
        powerstation->powstat_startstop = 1000.0;
        powerstation->shared_penstock = false; // Will be overridden in tests
        powerstation->auto_qmin = -9999.0;
        powerstation->aggressive_actions_cost = 0.0;
        powerstation->local_energy_equivalent = 0.11;
        powerstation->downstream_idnr = -1;
        powerstation->downstream_node_in_use = false;
        
        // Set test water levels
        powerstation->start_of_stp_masl = 200.0;
        powerstation->end_of_stp_masl = 190.0;
        
        // Set up two generators with different capacities
        powerstation->generators.resize(2);
        
        // Generator 0: Smaller unit (12 m3/s max)
        powerstation->generators[0].turb_virkn_Q = {0.0, 3.0, 6.0, 9.0, 12.0};
        powerstation->generators[0].turb_virkn_psnt = {0.0, 70.0, 85.0, 90.0, 85.0};
        powerstation->generators[0].max_discharge = 12.0;
        powerstation->generators[0].eff_curve.nr_pts = 5;
        for (int i = 0; i < 5; i++) {
            powerstation->generators[0].eff_curve.x_points[i] = powerstation->generators[0].turb_virkn_Q[i];
            powerstation->generators[0].eff_curve.y_points[i] = powerstation->generators[0].turb_virkn_psnt[i];
        }
        powerstation->generators[0].eff_curve.initializeArrays();
        powerstation->generators[0].action.resize(gc->stps, 0.0);
        
        // Generator 1: Larger unit (20 m3/s max)
        powerstation->generators[1].turb_virkn_Q = {0.0, 5.0, 10.0, 15.0, 20.0};
        powerstation->generators[1].turb_virkn_psnt = {0.0, 60.0, 80.0, 85.0, 70.0};
        powerstation->generators[1].max_discharge = 20.0;
        powerstation->generators[1].eff_curve.nr_pts = 5;
        for (int i = 0; i < 5; i++) {
            powerstation->generators[1].eff_curve.x_points[i] = powerstation->generators[1].turb_virkn_Q[i];
            powerstation->generators[1].eff_curve.y_points[i] = powerstation->generators[1].turb_virkn_psnt[i];
        }
        powerstation->generators[1].eff_curve.initializeArrays();
        powerstation->generators[1].action.resize(gc->stps, 0.0);
        
        // Copy real price and time data from dataset
        for (size_t t = 0; t < gc->stps; t++) {
            scenario->price[t] = dataset->price[t];
            scenario->year[t] = dataset->year[t];
            scenario->month[t] = dataset->month[t];
            scenario->day[t] = dataset->day[t];
            scenario->hour[t] = dataset->hour[t];
            scenario->up_inflow[t] = 0.0;
            scenario->inflow[t] = 0.0;
            scenario->dt = gc->dt;
        }
    }
};

// Basic Constructor/Destructor Tests
TEST_F(PowerstationTest, Constructor_InitializesCorrectly)
{
    Powerstation ps;
    EXPECT_EQ(ps.stps, size_t(0));
    EXPECT_EQ(ps.dt, 0);
    EXPECT_EQ(ps.static_gen_efficiency, NOT_INIT);
    EXPECT_EQ(ps.headlosscoef, NOT_INIT);
    EXPECT_EQ(ps.powstat_masl, NOT_INIT);
    EXPECT_EQ(ps.powstat_min_discharge, NOT_INIT);
    EXPECT_EQ(ps.powstat_startstop, NOT_INIT);
    EXPECT_EQ(ps.init_Power, NOT_INIT);
    EXPECT_EQ(ps.aggressive_actions_cost, NOT_INIT);
    // shared_penstock is a bool; constructor assigns NOT_INIT which coerces to true. Don't assert sentinel.
    EXPECT_TRUE(ps.generators.empty());
}

// Test Dataset Integration - Variable Timesteps from uTAHPS
TEST_F(PowerstationTest, Dataset_VariableTimesteps_LoadedCorrectly)
{
    // Verify that dataset loaded variable timesteps correctly
    EXPECT_GT(dataset->getDeltaT(0), 0);
    EXPECT_EQ(gc->stps, 16);
    
    // Check that multi_temporal_resolution was called
    EXPECT_EQ(dataset->delta_t.size(), 15); // stps-1 intervals
    
    // Most timesteps should be 1 hour (3600s), last might be different
    for (size_t t = 0; t < dataset->delta_t.size() - 1; t++) {
        EXPECT_EQ(dataset->getDeltaT(t), 3600);
    }
    
    // Explicitly check the last interval is different (longer)
    
    size_t last_idx = dataset->delta_t.size() - 1;
    EXPECT_GT(dataset->getDeltaT(last_idx), 3600);

}

// Test Real Price Data Loading
TEST_F(PowerstationTest, Dataset_RealPriceData_LoadedCorrectly)
{
    // Verify real price data was loaded from pricefile.txt
    EXPECT_GT(dataset->price[0], 0.0); // Should have positive prices
    EXPECT_LT(dataset->price[0], 1000.0); // Should be reasonable price
    
    // Check that we have time data
    EXPECT_GE(dataset->year[0], 2000);
    EXPECT_GE(dataset->month[0], 1);
    EXPECT_LE(dataset->month[0], 12);
    EXPECT_GE(dataset->day[0], 1);
    EXPECT_LE(dataset->day[0], 31);
}

// Basic Simulation Tests
// SVOLETJONN Powerstation Tests (Real uTAHPS Unit)
TEST_F(PowerstationTest, Svoletjonn_NoAction_ZeroPower)
{
    setupSvoletjonn();
    
    // All generator actions are 0.0
    int result = powerstation->Simulate(0);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(powerstation->S->Power[0], 0.0);
    EXPECT_EQ(powerstation->S->tot_outflow[0], 0.0);
    EXPECT_EQ(powerstation->S->income[0], 0.0);
    EXPECT_EQ(powerstation->remaining_Mm3, 0.0);
    EXPECT_EQ(powerstation->remaining_active_Mm3, 0.0);
}

// Test that power calculation for the second last timestep (with longer dt) is correct
TEST_F(PowerstationTest, Svoletjonn_SecondLastStep_LongTimestep_Power)
{
    setupSvoletjonn();
    // Use full action for the second last timestep
    size_t t = gc->stps - 2;
    powerstation->generators[0].action[t] = 1.0;
    // Set dt for this step to the actual value from dataset
    powerstation->S->dt = dataset->getDeltaT(t);
    int result = powerstation->Simulate(t);
    EXPECT_EQ(result, 0);
    // Power should be positive and scale with dt
    EXPECT_GT(powerstation->S->Power[t], 0.0);
    // Explicitly check that dt for this interval is 7200 (2 hours)
    double dt_first = dataset->getDeltaT(0);
    double dt_last = dataset->getDeltaT(t);
    EXPECT_EQ(dt_last, 7200);
    // Simulate first hour for reference
    powerstation->generators[0].action[0] = 1.0;
    powerstation->S->dt = dt_first;
    powerstation->Simulate(0);
    // Print power values for debugging
    std::cout << "Power for 2-hour interval (t=" << t << "): " << powerstation->S->Power[t] << std::endl;
    std::cout << "Power for 1-hour interval (t=0): " << powerstation->S->Power[0] << std::endl;
    // Power for 2-hour interval should be greater than for 1-hour interval
    EXPECT_GT(powerstation->S->Power[t], powerstation->S->Power[0]);
}

TEST_F(PowerstationTest, Svoletjonn_FullAction_CalculatesPowerCorrectly)
{
    setupSvoletjonn();
    
    // Set generator to full action (100% = 4.0 m3/s)
    powerstation->generators[0].action[0] = 1.0;
    
    // Update timestep - this is critical for variable timestep functionality
    powerstation->S->dt = dataset->getDeltaT(0);
    
    int result = powerstation->Simulate(0);
    
    EXPECT_EQ(result, 0);
    
    // Expected calculations for SVOLETJONN:
    // Q = 1.0 * 4.0 = 4.0 m3/s (at max efficiency ~88%)
    // Hbrutto = (752.5 + 700)/2 - 690 = 726.25 - 690 = 36.25 m
    // headloss = 0.3 * 4^2 = 4.8 m  
    // Hnetto = 36.25 - 4.8 = 31.45 m
    // turbine_efficiency = 88% (from real curve at Q=4.0)
    // P = 0.88 * 1000 * 9.81 * 31.45 * 4.0 / 1000000 ≈ 1.08 MW
    // P_gen = 1.08 * 0.96 ≈ 1.04 MW
    // Power = 1.04 MWh (for 1-hour timestep)
    
    EXPECT_NEAR(powerstation->S->tot_outflow[0], 4.0, 0.001);
    EXPECT_NEAR(powerstation->S->Hbrutto[0], 36.25, 0.05);
    EXPECT_NEAR(powerstation->S->Power[0], 1.04, 0.1);  // Produces power
    EXPECT_GT(powerstation->S->income[0], 0.0); // Positive income with positive price
}


TEST_F(PowerstationTest, Svoletjonn_OptimalEfficiency_MaxPowerPerFlow)
{
    setupSvoletjonn();
    
    // Set generator to optimal efficiency point (Q=2.79-3.19 m3/s = 93%)
    powerstation->generators[0].action[0] = 0.75; // 75% = 3.0 m3/s (93% efficiency range)
    powerstation->S->dt = dataset->getDeltaT(0);
    
    int result = powerstation->Simulate(0);
    
    EXPECT_EQ(result, 0);
    EXPECT_NEAR(powerstation->S->tot_outflow[0], 3.0, 0.001);
    EXPECT_GT(powerstation->S->Power[0], 0.6); // Should be efficient power production
}

// Head Loss and Hydraulic Tests
TEST_F(PowerstationTest, Simulate_SharedPenstock_CombinedHeadloss)
{
    setupDualGenerators();
    powerstation->shared_penstock = true;
    
    // Set both generators to 50%
    powerstation->generators[0].action[0] = 0.5; // 6 m3/s (50% of 12)
    powerstation->generators[1].action[0] = 0.5; // 10 m3/s (50% of 20)
    
    int result = powerstation->Simulate(0);
    
    EXPECT_EQ(result, 0);
    
    // Total Q = 6 + 10 = 16 m3/s
    // Shared penstock: headloss = 0.001 * 16^2 = 0.256 m
    EXPECT_NEAR(powerstation->S->tot_outflow[0], 16.0, 0.001);
    
    double expected_headloss = 0.001 * 16.0 * 16.0;
    double expected_hbrutto = (200.0 + 190.0)/2.0 - 100.0; // 95.0 m
    double expected_hnetto = expected_hbrutto - expected_headloss;
    EXPECT_NEAR(powerstation->S->Hnetto[0], expected_hnetto, 0.001);
}

TEST_F(PowerstationTest, Simulate_SeparatePenstock_IndividualHeadloss)
{
    setupDualGenerators();
    powerstation->shared_penstock = false;
    
    // Set both generators to 50%
    powerstation->generators[0].action[0] = 0.5; // 6 m3/s (50% of 12)
    powerstation->generators[1].action[0] = 0.5; // 10 m3/s (50% of 20)
    
    int result = powerstation->Simulate(0);
    
    EXPECT_EQ(result, 0);
    
    // Total Q = 6 + 10 = 16 m3/s (same total flow)
    EXPECT_NEAR(powerstation->S->tot_outflow[0], 16.0, 0.001);
    
    // But Hnetto should be different due to individual headloss calculations
    // Gen0: headloss = 0.001 * 6^2 = 0.036 m
    // Gen1: headloss = 0.001 * 10^2 = 0.1 m
    // Each generator has its own Hnetto for power calculation
    EXPECT_GT(powerstation->S->Power[0], 0.0); // Should still produce power
}

// Start/Stop Cost Tests
TEST_F(PowerstationTest, Simulate_StartupCost_AppliedCorrectly)
{
    setupSvoletjonn(); // Use real SVOLETJONN with startstop cost = 2.0
    
    // Timestep 0: off, Timestep 1: on
    powerstation->generators[0].action[0] = 0.0;
    powerstation->generators[0].action[1] = 0.5;
    
    // Simulate timestep 0
    powerstation->Simulate(0);
    EXPECT_EQ(powerstation->S->startStopCost[0], 0.0); // No start/stop cost
    
    // Simulate timestep 1 
    powerstation->Simulate(1);
    EXPECT_EQ(powerstation->S->startStopCost[1], 1.0); // Half of startstop cost (2.0/2)
}

TEST_F(PowerstationTest, Simulate_ShutdownCost_AppliedCorrectly)
{
    setupSvoletjonn(); // Use real SVOLETJONN with startstop cost = 2.0
    
    // Timestep 0: on, Timestep 1: off
    powerstation->generators[0].action[0] = 0.5;
    powerstation->generators[0].action[1] = 0.0;
    
    // Simulate timestep 0
    powerstation->Simulate(0);
    EXPECT_EQ(powerstation->S->startStopCost[0], 1.0); // Change from 0->on costs half (2.0/2)
    
    // Simulate timestep 1
    powerstation->Simulate(1);
    EXPECT_EQ(powerstation->S->startStopCost[1], 1.0); // Half of startstop cost (2.0/2)
}

TEST_F(PowerstationTest, Simulate_NoStartStopCost_WhenRunningContinuously)
{
    setupSvoletjonn();
    // Both timesteps: on
    powerstation->generators[0].action[0] = 0.5;
    powerstation->generators[0].action[1] = 0.5;
    
    powerstation->Simulate(0);
    powerstation->Simulate(1);
    
    EXPECT_EQ(powerstation->S->startStopCost[0], 1.0); // First step toggles from off->on
    EXPECT_EQ(powerstation->S->startStopCost[1], 0.0);
}

// The statefile decides what happened BEFORE t=0. A station that was already running at
// the end of the previous run must not be charged a start cost for continuing to run.
TEST_F(PowerstationTest, Simulate_FirstTimestep_UsesStartStateFromStatefile)
{
    setupSvoletjonn(); // startstop cost = 2.0
    powerstation->generators[0].action[0] = 0.5;

    powerstation->init_Power = 5.0; // was producing at the end of the previous run
    powerstation->Simulate(0);
    EXPECT_EQ(powerstation->S->startStopCost[0], 0.0);

    powerstation->init_Power = 0.0; // was off at the end of the previous run
    powerstation->Simulate(0);
    EXPECT_EQ(powerstation->S->startStopCost[0], 1.0);
}

// A station that was running and is then switched off in the first timestep pays a stop cost.
TEST_F(PowerstationTest, Simulate_FirstTimestep_ShutdownFromStartState)
{
    setupSvoletjonn();
    powerstation->generators[0].action[0] = 0.0;

    powerstation->init_Power = 5.0; // was running, now off
    powerstation->Simulate(0);
    EXPECT_EQ(powerstation->S->startStopCost[0], 1.0);
}

// Multiple Generator Tests
TEST_F(PowerstationTest, Simulate_DualGenerators_CombinedOutput)
{
    setupDualGenerators();
    
    // Set different actions for each generator
    powerstation->generators[0].action[0] = 0.5; // 6 m3/s (50% of 12)
    powerstation->generators[1].action[0] = 0.3; // 6 m3/s (30% of 20)
    
    int result = powerstation->Simulate(0);
    
    EXPECT_EQ(result, 0);
    EXPECT_NEAR(powerstation->S->tot_outflow[0], 12.0, 0.001); // 6 + 6 = 12
    EXPECT_GT(powerstation->S->Power[0], 0.0); // Should produce power
}

TEST_F(PowerstationTest, Simulate_DualGenerators_IndependentStartStop)
{
    setupDualGenerators();
    
    // Gen0: off->on, Gen1: on->off
    powerstation->generators[0].action[0] = 0.0;
    powerstation->generators[0].action[1] = 0.5;
    powerstation->generators[1].action[0] = 0.5;
    powerstation->generators[1].action[1] = 0.0;
    
    powerstation->Simulate(0);
    powerstation->Simulate(1);
    
    // Should have start/stop costs for both generators (startstop cost = 1000.0)
    EXPECT_EQ(powerstation->S->startStopCost[1], 1000.0); // 500 + 500 (both generators change state)
}

// Water Balance Tests  
TEST_F(PowerstationTest, CheckWaterBalance_CorrectCalculation)
{
    setupSvoletjonn();
    
    // Set up inflow and outflow
    for (size_t t = 0; t < gc->stps; t++) {
        powerstation->S->up_inflow[t] = 10.0; // 10 m3/s inflow
        powerstation->S->inflow[t] = 0.0;     // No local inflow
        powerstation->S->tot_outflow[t] = 10.0; // 10 m3/s outflow
    }
    
    int result = powerstation->CheckWaterBalance(herss_obj);
    EXPECT_EQ(result, 0); // Should pass water balance
}

TEST_F(PowerstationTest, CheckWaterBalance_WithVariableTimesteps)
{
    setupSvoletjonn();
    
    // Test that water balance uses variable timesteps correctly
    for (size_t t = 0; t < gc->stps; t++) {
        powerstation->S->up_inflow[t] = 5.0;
        powerstation->S->tot_outflow[t] = 5.0;
    }
    
    int result = powerstation->CheckWaterBalance(herss_obj);
    EXPECT_EQ(result, 0);
}


// GetTunnelFLow Tests (Critical for reservoir interactions)
TEST_F(PowerstationTest, GetTunnelFLow_CalculatesCorrectly)
{
    setupSvoletjonn();
    powerstation->generators[0].action[0] = 0.5; // 50% action = 2.0 m3/s
    powerstation->up_res_Mm3 = 100.0; // Plenty of water available
    
    double flow = powerstation->GetTunnelFLow(0);
    
    EXPECT_NEAR(flow, 2.0, 0.001); // 0.5 * 4.0 = 2.0 m3/s
    EXPECT_EQ(powerstation->aggressive_actions_cost, 0.0); // No aggressive action penalty
}

TEST_F(PowerstationTest, GetTunnelFLow_WithAutoQmin)
{
    setupSvoletjonn();
    powerstation->auto_qmin = 3.0; // Minimum flow requirement
    powerstation->generators[0].action[0] = 0.25; // Only 1.0 m3/s (25% of 4.0)
    powerstation->up_res_Mm3 = 100.0;
    
    double flow = powerstation->GetTunnelFLow(0);
    
    EXPECT_NEAR(flow, 3.0, 0.001); // Should be auto_qmin, not calculated flow
    EXPECT_EQ(powerstation->S->auto_qmin_m3s[0], 3.0);
}

TEST_F(PowerstationTest, GetTunnelFLow_AggressiveAction_Penalty)
{
    setupSvoletjonn();
    powerstation->generators[0].action[0] = 1.0; // Full action = 4.0 m3/s
    powerstation->up_res_Mm3 = 0.001; // Very little water available
    
    double flow = powerstation->GetTunnelFLow(0);
    
    EXPECT_EQ(flow, 0.0); // Should be shut down
    EXPECT_GT(powerstation->aggressive_actions_cost, 0.0); // Should have penalty
}

TEST_F(PowerstationTest, GetTunnelFLow_NegativeAction_Error)
{
    setupSvoletjonn();
    powerstation->generators[0].action[0] = -0.1; // Negative action
    // This should trigger an exit, so we use a death test
    EXPECT_DEATH(powerstation->GetTunnelFLow(0), ".*");
}

// Economic Calculation Tests
TEST_F(PowerstationTest, Simulate_IncomeCalculation)
{
    setupSvoletjonn();
    powerstation->generators[0].action[0] = 0.5;
    powerstation->S->price[0] = 150.0; // Euro/MWh
    
    powerstation->Simulate(0);
    
    // Income should be Power * Price
    double expected_income = powerstation->S->Power[0] * 150.0;
    EXPECT_NEAR(powerstation->S->income[0], expected_income, 0.001);
}

TEST_F(PowerstationTest, Simulate_ProfitCalculation)
{
    setupSvoletjonn();
    powerstation->generators[0].action[0] = 0.5;
    powerstation->S->price[0] = 100.0;
    
    powerstation->Simulate(0);
    
    // Profit = Income - Costs
    double expected_profit = powerstation->S->income[0] - powerstation->S->cost[0];
    EXPECT_NEAR(powerstation->S->profit[0], expected_profit, 0.001);
}

// Energy Equivalent (EEKV) Tests
TEST_F(PowerstationTest, Simulate_EstimatedEEKV_Calculation)
{
    setupSvoletjonn();
    powerstation->generators[0].action[0] = 0.5;
    
    powerstation->Simulate(0);
    
    if (powerstation->S->Power[0] > 0.0) {
        // EEKV = GWh / Mm3
        double q_Mm3 = MACRO_m3s_2_Mm3(powerstation->S->tot_outflow[0], powerstation->S->dt);
        double est_GWh = powerstation->S->Power[0] / 1000.0;
        double expected_eekv = est_GWh / q_Mm3;
        
        EXPECT_NEAR(powerstation->S->EstimatedEEKV[0], expected_eekv, 0.001);
    }
}



// Downstream Node Tests
TEST_F(PowerstationTest, Simulate_DownstreamNode_ReceivesInflow)
{
    setupDualGenerators();
    // Create a mock downstream node
    Powerstation downstream;
    downstream.S = new Scenario(gc->stps, gc->dt, 0);
    powerstation->ptr_downstream_node = &downstream;
    powerstation->downstream_node_in_use = true;
    // Initialize downstream inflow
    for (size_t t = 0; t < gc->stps; t++) {
        downstream.S->up_inflow[t] = 0.0;
    }
    powerstation->generators[1].action[0] = 0.5; // 10 m3/s on gen1
    powerstation->Simulate(0);
    // Downstream node should receive the outflow as inflow
    EXPECT_NEAR(downstream.S->up_inflow[0], 10.0, 0.001);
    delete downstream.S;
}

// Performance Tests
TEST_F(PowerstationTest, Performance_MultipleGeneratorSimulation)
{
    // Ensure we have a valid station/scenario then expand to test performance
    setupDualGenerators();
    const int MAX_GENS = 5;
    powerstation->generators.resize(MAX_GENS);
    
    for (int g = 0; g < MAX_GENS; g++) {
        powerstation->generators[g].turb_virkn_Q = {0.0, 5.0, 10.0, 15.0, 20.0};
        powerstation->generators[g].turb_virkn_psnt = {0.0, 80.0, 90.0, 85.0, 70.0};
        powerstation->generators[g].max_discharge = 20.0;
        
        powerstation->generators[g].eff_curve.nr_pts = 5;
        for (int i = 0; i < 5; i++) {
            powerstation->generators[g].eff_curve.x_points[i] = powerstation->generators[g].turb_virkn_Q[i];
            powerstation->generators[g].eff_curve.y_points[i] = powerstation->generators[g].turb_virkn_psnt[i];
        }
        powerstation->generators[g].eff_curve.initializeArrays();
    // Use assign (not resize) to ensure existing generators get updated values
    powerstation->generators[g].action.assign(gc->stps, 0.3 + 0.1 * g);
    }
    
    // Simulate all timesteps with all generators
    for (size_t t = 0; t < gc->stps; t++) {
        // Sanity: ensure we actually have MAX_GENS generators
        ASSERT_EQ(powerstation->generators.size(), static_cast<size_t>(MAX_GENS));

        // Compute expected outflow from configured actions and max discharges
        double manual_outflow = 0.0;
        for (int g = 0; g < MAX_GENS; ++g) {
            double act = (powerstation->generators[g].action.size() > t) ? powerstation->generators[g].action[t] : 0.0;
            manual_outflow += act * powerstation->generators[g].max_discharge;
        }

        int result = powerstation->Simulate(t);
        EXPECT_EQ(result, 0);
        
        // Should have significant combined output
        EXPECT_GT(powerstation->S->Power[t], 10.0);
        // Sum of flows: 20*(0.3 + 0.4 + 0.5 + 0.6 + 0.7) = 50
        EXPECT_NEAR(manual_outflow, 50.0, 1e-9);
        EXPECT_NEAR(powerstation->S->tot_outflow[t], manual_outflow, 1.0);
    }
}
