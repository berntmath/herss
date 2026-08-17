#include <gtest/gtest.h>
#include "herss.h"
#include <cstdio>
#include <fstream>
#include <string>

// Test fixture for tests that might need a GlobalConfig object
class GlobalConfigTest : public ::testing::Test
{
protected:
    GlobalConfig *gc;
    void SetUp() override { gc = new GlobalConfig(); }
    void TearDown() override { delete gc; }
};

// Test: Constructor initializes all members to their default values
TEST_F(GlobalConfigTest, InitializesWithDefaultValues)
{
    ASSERT_NE(gc, nullptr);

    // Check all string members
    EXPECT_EQ(gc->globalfile, DEFAULT_STRING_INIT);
    EXPECT_EQ(gc->topologyfile, DEFAULT_STRING_INIT);
    EXPECT_EQ(gc->actionsfile, DEFAULT_STRING_INIT);
    EXPECT_EQ(gc->pricefile, DEFAULT_STRING_INIT);
    EXPECT_EQ(gc->outputfile, DEFAULT_STRING_INIT);
    EXPECT_EQ(gc->inflowfile, DEFAULT_STRING_INIT);
    EXPECT_EQ(gc->systemname, DEFAULT_STRING_INIT);
    EXPECT_EQ(gc->start_statefile, DEFAULT_STRING_INIT);
    EXPECT_EQ(gc->out_statefile, DEFAULT_STRING_INIT);
    EXPECT_EQ(gc->outputdir, DEFAULT_STRING_INIT);
    EXPECT_EQ(gc->inputdir, DEFAULT_STRING_INIT);

    // Check all boolean members
    EXPECT_FALSE(gc->found_topologyfilename);
    EXPECT_FALSE(gc->found_actionsfilename);
    EXPECT_FALSE(gc->found_pricefilename);
    EXPECT_FALSE(gc->found_inflowfilename);
    EXPECT_FALSE(gc->found_systemname);
    EXPECT_FALSE(gc->found_start_statefilename);
    EXPECT_FALSE(gc->found_outputfilename);
    EXPECT_FALSE(gc->found_dt);
    EXPECT_FALSE(gc->write_nodefiles);

    // Check all numeric members
    EXPECT_EQ(gc->dt, static_cast<size_t>(NOT_INIT));
    EXPECT_EQ(gc->dt_last, static_cast<size_t>(NOT_INIT));
    EXPECT_EQ(gc->stps, static_cast<size_t>(NOT_INIT));
    EXPECT_EQ(gc->discount_rate, static_cast<double>(NOT_INIT));
    EXPECT_EQ(gc->discount_factor, static_cast<double>(NOT_INIT));
    EXPECT_EQ(gc->nr_nodes, static_cast<size_t>(NOT_INIT));
    EXPECT_EQ(gc->nr_pstations, static_cast<size_t>(NOT_INIT));
    EXPECT_EQ(gc->nr_reservoirs, static_cast<size_t>(NOT_INIT));
    EXPECT_EQ(gc->nr_channels, static_cast<size_t>(NOT_INIT));
    EXPECT_EQ(gc->n_action_nodes, static_cast<size_t>(NOT_INIT));
    EXPECT_EQ(gc->n_inflow_nodes, static_cast<size_t>(NOT_INIT));

    // Check arrays
    for (size_t n = 0; n < MAX_NR_NODES; n++) {
        EXPECT_EQ(gc->actions_idnrs[n], static_cast<size_t>(NOT_INIT));
        EXPECT_EQ(gc->inflows_idnrs[n], static_cast<size_t>(NOT_INIT));
    }
}

// Test: readGlobalFile throws on missing file
TEST_F(GlobalConfigTest, ReadGlobalFileThrowsOnMissingFile)
{
    gc->globalfile = "nonexistent_file.txt";
    EXPECT_EXIT(gc->readGlobalFile(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

// Test: SetDirectoriesAndFilenames correctly prepends directories
TEST_F(GlobalConfigTest, SetDirectoriesAndFilenamesPrependsDirs)
{
    gc->inputdir = "input/";
    gc->outputdir = "output/";
    gc->topologyfile = "topo.txt";
    gc->pricefile = "price.txt";
    gc->inflowfile = "inflow.txt";
    gc->actionsfile = "actions.txt";
    gc->start_statefile = "start.txt";
    gc->out_statefile = "outstate.txt";
    gc->outputfile = "output.txt";

    gc->SetDirectoriesAndFilenames();

    EXPECT_EQ(gc->topologyfile, "input/topo.txt");
    EXPECT_EQ(gc->pricefile, "input/price.txt");
    EXPECT_EQ(gc->inflowfile, "input/inflow.txt");
    EXPECT_EQ(gc->actionsfile, "input/actions.txt");
    EXPECT_EQ(gc->start_statefile, "input/start.txt");
    EXPECT_EQ(gc->out_statefile, "output/outstate.txt");
    EXPECT_EQ(gc->outputfile, "output/output.txt");
}

// Test: DiagnoseTopologyFile throws on missing file
TEST_F(GlobalConfigTest, DiagnoseTopologyFileThrowsOnMissingFile)
{
    gc->topologyfile = "nonexistent_topology.txt";
    EXPECT_EXIT(gc->DiagnoseTopologyFile(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

TEST_F(GlobalConfigTest, DiagnoseTopologyFileRejectsTooManyNodes)
{
    const std::string topologyfile = "/tmp/herss_too_many_nodes_topology.txt";
    {
        std::ofstream output(topologyfile);
        for(size_t node = 0; node <= MAX_NR_NODES; node++) {
            output << "NODE RESERVOIR " << node << " reservoir_" << node << "\n";
        }
    }
    gc->topologyfile = topologyfile;

    EXPECT_EXIT(gc->DiagnoseTopologyFile(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");

    std::remove(topologyfile.c_str());
}

// Test: Diagnose throws on missing actions file
TEST_F(GlobalConfigTest, DiagnoseThrowsOnMissingActionsFile)
{
    gc->topologyfile = "../src_tests/utahps_test/topology.txt"; // Use a valid topology file
    gc->actionsfile = "nonexistent_actions.txt";
    gc->inflowfile = "../src_tests/utahps_test/inflow.txt"; // Use a valid inflow file
    EXPECT_EXIT(gc->Diagnose(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

// Test: Diagnose throws on missing inflow file
TEST_F(GlobalConfigTest, DiagnoseThrowsOnMissingInflowFile)
{
    gc->topologyfile = "../src_tests/utahps_test/topology.txt"; // Use a valid topology file
    gc->actionsfile = "../src_tests/utahps_test/actions.txt"; // Use a valid actions file
    gc->inflowfile = "nonexistent_inflow.txt";
    EXPECT_EXIT(gc->Diagnose(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

// Test: checkNrSteps throws on missing price file
TEST_F(GlobalConfigTest, CheckNrStepsThrowsOnMissingPriceFile)
{
    gc->pricefile = "nonexistent_price.txt";
    EXPECT_EXIT(gc->checkNrSteps(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

// Test: printGlobalInfo runs without crashing
TEST_F(GlobalConfigTest, PrintGlobalInfoRuns)
{
    // Set reasonable values to avoid segmentation fault
    gc->n_action_nodes = 0;
    gc->n_inflow_nodes = 0;
    EXPECT_NO_THROW(gc->printGlobalInfo());
}

// Test: checkNrSteps counts the correct number of steps in a valid price file
TEST_F(GlobalConfigTest, CheckNrStepsCountsStepsCorrectly)
{
    gc->pricefile = "../src_tests/utahps_test/pricefile.txt";
    gc->checkNrSteps();

    EXPECT_EQ(gc->stps, static_cast<size_t>(16)); 
}