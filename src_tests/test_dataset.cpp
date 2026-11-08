#include <gtest/gtest.h>
#include "herss.h"
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>

// Test fixture for Dataset tests
class DatasetTest : public ::testing::Test
{
protected:
    GlobalConfig *gc;
    Dataset *dataset;
    
    void SetUp() override 
    { 
        gc = new GlobalConfig();
        // Set up basic configuration for testing
        gc->stps = 16;
        gc->nr_nodes = 12;
        gc->pricefile = "../src_tests/utahps_test/pricefile.txt";
        gc->inflowfile = "../src_tests/utahps_test/inflow.txt";
        gc->actionsfile = "../src_tests/utahps_test/actions.txt";
        gc->dt_last = 3600;
        
        dataset = new Dataset(gc);
    }
    
    void TearDown() override 
    { 
        delete dataset;
        delete gc;
    }
};


// Test: readPricefile correctly reads price data
TEST_F(DatasetTest, ReadPricefileReadsCorrectly)
{
    dataset->readPricefile();
    
    // Check that restprice was read correctly
    EXPECT_EQ(dataset->restprice, 100.0);
    
    // Check that price data was read correctly
    EXPECT_NEAR(dataset->price[0], 626.954, 0.001);
    EXPECT_NEAR(dataset->price[1], 618.766, 0.001);
    EXPECT_NEAR(dataset->price[2], 618.742, 0.001);
    
    // Check that date data was parsed correctly
    EXPECT_EQ(dataset->year[0], 2022);
    EXPECT_EQ(dataset->month[0], 9);
    EXPECT_EQ(dataset->day[0], 1);
    EXPECT_EQ(dataset->hour[0], 0);
    
    EXPECT_EQ(dataset->year[1], 2022);
    EXPECT_EQ(dataset->month[1], 9);
    EXPECT_EQ(dataset->day[1], 1);
    EXPECT_EQ(dataset->hour[1], 1);
}

// Test: readInflowFile correctly reads inflow data
TEST_F(DatasetTest, ReadInflowFileReadsCorrectly)
{
    // First read price file to get the dates
    dataset->readPricefile();
    dataset->readInflowFile();
    
    // Check that inflow data was read correctly for the nodes mentioned in the file
    // Node 0
    EXPECT_NEAR(dataset->inflow[0][0], 0.36, 0.001);
    EXPECT_NEAR(dataset->inflow[1][0], 0.36, 0.001);
    
    // Node 3
    EXPECT_NEAR(dataset->inflow[0][3], 0.23, 0.001);
    EXPECT_NEAR(dataset->inflow[1][3], 0.23, 0.001);
    
    // Node 5
    EXPECT_NEAR(dataset->inflow[0][5], 0.31, 0.001);
    EXPECT_NEAR(dataset->inflow[1][5], 0.30, 0.001);
    
    // Node 9
    EXPECT_NEAR(dataset->inflow[0][9], 0.67, 0.001);
    EXPECT_NEAR(dataset->inflow[1][9], 0.67, 0.001);
    
    // Nodes not in the file should remain 0.0
    EXPECT_EQ(dataset->inflow[0][1], 0.0);
    EXPECT_EQ(dataset->inflow[0][2], 0.0);
}

// Test: readActionsFile correctly reads action data
TEST_F(DatasetTest, ReadActionsFileReadsCorrectly)
{
    // First read price file to get the dates
    dataset->readPricefile();
    dataset->readActionsFile();
    
    // Check that action data was read correctly
    // All actions in the test file are 0.80
    EXPECT_NEAR(dataset->action[0][0], 0.80, 0.001);
    EXPECT_NEAR(dataset->action[1][0], 0.80, 0.001);
    EXPECT_NEAR(dataset->action[15][0], 0.80, 0.001);
    
    // Check that action column names were stored
    EXPECT_EQ(dataset->action_colnames.size(), 4);
    EXPECT_EQ(dataset->action_colnames[0], "1_0");
    EXPECT_EQ(dataset->action_colnames[1], "6_0");
    EXPECT_EQ(dataset->action_colnames[2], "7_0");
    EXPECT_EQ(dataset->action_colnames[3], "10_0");
}


// Test: multi_temporal_resolution calculates delta_t correctly
TEST_F(DatasetTest, MultiTemporalResolutionCalculatesDeltaT)
{
    dataset->readPricefile();
    dataset->multi_temporal_resolution();
    
    // Check that delta_t vector has the correct size (stps - 1)
    EXPECT_EQ(dataset->delta_t.size(), dataset->stps - 1);
    
    // For hourly data, each delta_t should be 3600 seconds (1 hour)
    for(size_t i = 0; i < dataset->delta_t.size() - 1; i++) {
    EXPECT_EQ(dataset->delta_t[i], 3600);
    }
    EXPECT_EQ(dataset->delta_t.back(), 7200);
}

// Test: multi_temporal_resolution handles last two-hour step
TEST_F(DatasetTest, MultiTemporalResolutionHandlesLastTwoHourStep)
{
    dataset->readPricefile();
    dataset->multi_temporal_resolution();

    // Check that delta_t vector has the correct size
    EXPECT_EQ(dataset->delta_t.size(), dataset->stps - 1);

    // All intervals except the last should be 3600
    for(size_t i = 0; i < dataset->delta_t.size() - 1; i++) {
        EXPECT_EQ(dataset->delta_t[i], 3600);
    }
    EXPECT_EQ(dataset->delta_t.back(), 7200);
}


// Test: getDeltaT throws on out-of-bounds timestep
TEST_F(DatasetTest, GetDeltaTThrowsOnOutOfBounds)
{
    dataset->readPricefile();
    dataset->multi_temporal_resolution();
    
    // Test that out-of-bounds timestep causes exit
    EXPECT_EXIT(dataset->getDeltaT(dataset->stps + 1), 
                ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

// Test: readPricefile throws on missing file
TEST_F(DatasetTest, ReadPricefileThrowsOnMissingFile)
{
    gc->pricefile = "nonexistent_price.txt";
    EXPECT_EXIT(dataset->readPricefile(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

// Test: readInflowFile throws on missing file
TEST_F(DatasetTest, ReadInflowFileThrowsOnMissingFile)
{
    gc->inflowfile = "nonexistent_inflow.txt";
    EXPECT_EXIT(dataset->readInflowFile(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

// Test: readActionsFile throws on missing file
TEST_F(DatasetTest, ReadActionsFileThrowsOnMissingFile)
{
    gc->actionsfile = "nonexistent_actions.txt";
    EXPECT_EXIT(dataset->readActionsFile(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

// Test: Date validation works correctly
TEST_F(DatasetTest, DateValidationWorksCorrectly)
{
    // This test verifies that date mismatches between files are caught
    // We'll test this indirectly by reading valid files that should match
    dataset->readPricefile();
    dataset->readInflowFile();
    dataset->readActionsFile();
    
    // If we get here without exit, the dates matched correctly
    SUCCEED();
}

// Test: more action columns than nodes. src_tests/pump_test/ (5 nodes) with pstation_A given
// 4 generators has 6 action columns, so Dataset::action needs more than nr_nodes slots per step.
TEST(DatasetActionColumnsTest, MoreActionColumnsThanNodesAreReadIntact)
{
    const std::string src = "../src_tests/pump_test/";
    const std::string dir = ::testing::TempDir() + "pump_test_4gen/";
    std::filesystem::create_directories(dir);
    for(const char* f : {"inflowseries.txt", "pricefile.txt", "start_state.txt"}) {
        std::filesystem::copy_file(src + f, dir + f, std::filesystem::copy_options::overwrite_existing);
    }

    // Topology: generator 0's block repeated as generators 1-3.
    std::ostringstream topo;
    topo << std::ifstream(src + "topology.txt").rdbuf();
    std::string s = topo.str();
    s.replace(s.find("NR_GENERATORS 1"), 15, "NR_GENERATORS 4");
    const size_t a = s.find("GENERATOR 0");
    const size_t b = s.find("STATIC_GENERATOR_EFFICIENCY");
    std::string extra;
    for(int g = 1; g <= 3; g++) {
        extra += s.substr(a, b - a).replace(0, 11, "GENERATOR " + std::to_string(g));
    }
    s.insert(b, extra);
    std::ofstream(dir + "topology.txt") << s;

    // Actions: same dates, one distinct value per column.
    std::ifstream in(src + "actions.txt");
    std::ofstream out(dir + "actions.txt");
    std::string line;
    std::getline(in, line);
    out << "Date_NodeID\t2_0\t2_1\t2_2\t2_3\t3\t0\n";
    while(std::getline(in, line)) {
        if(!line.empty()) out << line.substr(0, line.find('\t')) << "\t0.125\t0.25\t0.5\t0.75\t0.375\t0.625\n";
    }
    out.close();

    GlobalConfig* gc = new GlobalConfig();
    gc->globalfile = src + "global.txt";
    gc->readGlobalFile();
    gc->inputdir = dir;
    gc->SetDirectoriesAndFilenames();
    gc->Diagnose();
    gc->checkNrSteps();
    ASSERT_EQ(gc->n_action_nodes_from_topology, 6u);
    ASSERT_GT(gc->n_action_nodes_from_topology, gc->nr_nodes);

    Dataset* data = new Dataset(gc);
    data->readAllData();
    Herss* herss_obj = new Herss(gc);
    herss_obj->prepaireSimulation(data);
    herss_obj->rs->DiagnoseRiversystemConfiguration();
    herss_obj->Simulate();

    const double gen[4] = {0.125, 0.25, 0.5, 0.75};
    for(size_t t = 0; t < gc->stps; t++) {
        for(size_t g = 0; g < 4; g++) EXPECT_DOUBLE_EQ(herss_obj->GetAction(2, g, t), gen[g]);
        EXPECT_DOUBLE_EQ(herss_obj->GetAction(3, 0, t), 0.375); // reservoir_B hatch
        EXPECT_DOUBLE_EQ(herss_obj->GetAction(0, 0, t), 0.625); // pump_AB
    }

    delete herss_obj;
    delete data;
    delete gc;
}

