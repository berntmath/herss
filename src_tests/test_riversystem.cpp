#include <gtest/gtest.h>
#include "herss.h"

// Helper to build a minimal GlobalConfig with specified node layout
static GlobalConfig* makeGC(size_t stps,
                            std::vector<NodeType> types) {
    auto* gc = new GlobalConfig();
    gc->stps = stps;
    gc->dt = 3600;
    gc->nr_nodes = types.size();
    gc->nr_reservoirs = 0;
    gc->nr_pstations  = 0;
    gc->nr_channels   = 0;
    gc->nr_pumps      = 0; // GlobalConfig defaults this to NOT_INIT; Riversystem's
                           // constructor does `new Pump[gc->nr_pumps]`, so this
                           // must be explicitly zeroed here like the other counts.
    for (size_t i = 0; i < types.size(); ++i) {
        gc->nodetypes[i] = types[i];
        if (types[i] == NodeType::RESERVOIR) gc->nr_reservoirs++;
        if (types[i] == NodeType::PSTATION) gc->nr_pstations++;
        if (types[i] == NodeType::CHANNEL) gc->nr_channels++;
    }
    return gc;
}

// Attach a fresh Scenario to a Node with zeroed arrays
static void attachScenario(Node* n, size_t stps, size_t dt) {
    n->S = new Scenario(stps, dt, n->idnr);
    // Zero arrays for determinism
    for (size_t t = 0; t < stps; ++t) {
        n->S->price[t] = 0.0;
        n->S->income[t] = 0.0;
        n->S->cost[t] = 0.0;
        n->S->Power[t] = 0.0;
    }
}

class RiversystemTest : public ::testing::Test {
protected:
    GlobalConfig* gc = nullptr;
    Riversystem* rs = nullptr;

    void TearDown() override {
        // Riversystem destructor deletes its arrays; but scenarios are owned by Nodes
        if (rs) {
            for (size_t n = 0; n < gc->nr_nodes; ++n) {
                delete rs->nodes[n]->S; // safe if set
            }
            delete rs;
        }
        delete gc;
    }
};

TEST_F(RiversystemTest, Constructor_WiresNodesByType) {
    gc = makeGC(3, {RESERVOIR, PSTATION, CHANNEL});
    rs = new Riversystem(gc);
    ASSERT_EQ(rs->nr_nodes, 3u);
    ASSERT_EQ(rs->nr_reservoirs, 1u);
    ASSERT_EQ(rs->nr_pstations, 1u);
    ASSERT_EQ(rs->nr_channels, 1u);
    // nodetype field is set when reading topology; here we verify wiring by array membership and id assignment
    EXPECT_EQ(rs->nodes[0], static_cast<Node*>(&rs->reservoirs[0]));
    EXPECT_EQ(rs->nodes[1], static_cast<Node*>(&rs->pstations[0]));
    EXPECT_EQ(rs->nodes[2], static_cast<Node*>(&rs->channels[0]));
    EXPECT_EQ(rs->nodes[0]->idnr, 0u);
    EXPECT_EQ(rs->nodes[1]->idnr, 1u);
    EXPECT_EQ(rs->nodes[2]->idnr, 2u);
}

TEST_F(RiversystemTest, GetEndingReservoirLevel_ReturnsLastFraction) {
    gc = makeGC(4, {RESERVOIR, PSTATION});
    rs = new Riversystem(gc);
    // Attach scenarios
    attachScenario(rs->nodes[0], gc->stps, gc->dt);
    attachScenario(rs->nodes[1], gc->stps, gc->dt);
    rs->nodes[0]->nodetype = RESERVOIR;
    rs->nodes[1]->nodetype = PSTATION;
    // Set reservoir end fraction
    rs->reservoirs[0].S->res_fr[gc->stps - 1] = 0.73;
    double got = rs->GetEndingReservoirLevel(0);
    EXPECT_NEAR(got, 0.73, 1e-12);
}

TEST_F(RiversystemTest, GetEndingReservoirLevel_OutOfRange_Exit) {
    gc = makeGC(2, {RESERVOIR});
    rs = new Riversystem(gc);
    // No need to attach scenarios; function will exit before use
    EXPECT_EXIT(rs->GetEndingReservoirLevel(5), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

TEST_F(RiversystemTest, CalcSimulationProfit_SumsIncomeMinusCostAcrossNodes) {
    gc = makeGC(3, {RESERVOIR, PSTATION, CHANNEL});
    rs = new Riversystem(gc);
    for (size_t i = 0; i < gc->nr_nodes; ++i) attachScenario(rs->nodes[i], gc->stps, gc->dt);
    // Fill incomes/costs
    // t0: sum income= (1+2+3)=6, cost=(0.5+0.5+0.5)=1.5
    // t1: sum income= (2+4+6)=12, cost=(1+1+1)=3
    // t2: sum income= (3+6+9)=18, cost=(1.5+1.5+1.5)=4.5
    // total profit = (6-1.5)+(12-3)+(18-4.5) = 27.0
    for (size_t t = 0; t < gc->stps; ++t) {
        rs->nodes[0]->S->income[t] = 1.0*(t+1);
        rs->nodes[1]->S->income[t] = 2.0*(t+1);
        rs->nodes[2]->S->income[t] = 3.0*(t+1);
        rs->nodes[0]->S->cost[t] = 0.5*(t+1);
        rs->nodes[1]->S->cost[t] = 0.5*(t+1);
        rs->nodes[2]->S->cost[t] = 0.5*(t+1);
    }
    double profit = rs->CalcSimulationProfit();
    EXPECT_NEAR(profit, 27.0, 1e-9);
}

TEST_F(RiversystemTest, CalcVF_ComputesValueFunctionAndTotals) {
    gc = makeGC(3, {RESERVOIR, PSTATION, CHANNEL});
    rs = new Riversystem(gc);
    for (size_t i = 0; i < gc->nr_nodes; ++i) attachScenario(rs->nodes[i], gc->stps, gc->dt);
    // Set nodetypes explicitly since topology reader isn't invoked in this unit test
    rs->nodes[0]->nodetype = RESERVOIR;
    rs->nodes[1]->nodetype = PSTATION;
    rs->nodes[2]->nodetype = CHANNEL;

    // Set downstream totals for remaining water bookkeeping
    rs->nodes[2]->upstream_remaining_Mm3 = 5.0;        // not used in value, but tracked
    rs->nodes[2]->upstream_remaining_active_Mm3 = 2.0; // used only for printing; value uses pstation below

    // Powerstation remaining energy contribution
    // tot_remaining_MWh = sum over pstations of (EEKV * upstream_active_Mm3 * 1000)
    // Set one pstation with upstream_active=2.0 Mm3 and EEKV=0.10 => 0.10*1000*2.0 = 200 MWh
    auto* ps = static_cast<Powerstation*>(rs->nodes[1]);
    ps->local_energy_equivalent = 0.10;
    ps->upstream_remaining_active_Mm3 = 2.0;

    // Production over timesteps contributes to sum_total_MWh
    rs->nodes[1]->S->Power[0] = 5.0;
    rs->nodes[1]->S->Power[1] = 0.0;
    rs->nodes[1]->S->Power[2] = 7.0;

    // Incomes and costs across nodes
    for (size_t t = 0; t < gc->stps; ++t) {
        rs->nodes[0]->S->income[t] = 10.0; rs->nodes[0]->S->cost[t] = 1.0;
        rs->nodes[1]->S->income[t] = 20.0; rs->nodes[1]->S->cost[t] = 2.0;
        rs->nodes[2]->S->income[t] = 0.0;  rs->nodes[2]->S->cost[t] = 0.5;
        rs->nodes[0]->S->price[t]  = 50.0; // used for avg_price; not asserted
    }

    double restprice = 50.0;
    double vf = rs->CalcVF(restprice);

    double sum_income = 3 * 10.0 + 3 * 20.0 + 0.0;
    double sum_cost   = 3 * 1.0  + 3 * 2.0  + 3 * 0.5;
    double tot_profit = sum_income - sum_cost; // 90 - 10.5 = 79.5
    double tot_remaining_MWh = 200.0;
    double tot_remaining_Euro = tot_remaining_MWh * restprice; // 10000
    double expected_vf = tot_profit + tot_remaining_Euro; // 79.5 + 10000

    EXPECT_NEAR(vf, expected_vf, 1e-9);
    EXPECT_NEAR(rs->valuefunction_Euro, expected_vf, 1e-9);
    EXPECT_NEAR(rs->sum_production, 12.0, 1e-12); // 5 + 0 + 7
}


TEST_F(RiversystemTest, WriteSelectedOutputMatrix_Exit) {
    gc = makeGC(1, {RESERVOIR});
    rs = new Riversystem(gc);
    attachScenario(rs->nodes[0], gc->stps, gc->dt);
    EXPECT_EXIT(rs->WriteSelectedOutputMatrix(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}
