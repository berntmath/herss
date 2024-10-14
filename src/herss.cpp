/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     herss.cpp
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:

Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
********************************************************************************/

#include "herss.h"
#include <random>
#include <functional>
#include <limits>

Herss::Herss(){}

///////////////////////////////////////////////////////////
Herss::Herss(GlobalConfig *gc){

    this->gc = gc;

    if(gc->dt < 1 ) {
        printf("Please set gc->dt correctly\n");
        exit(-9);
    }
    if(gc->nr_nodes < 1 ) {
        printf("Please set gc->nr_nodes correctly\n");
        exit(-9);
    }

    this->dt       = gc->dt;
    this->stps     = gc->stps;
    this->nr_nodes = gc->nr_nodes;

    try {
        rs     = new Riversystem(gc);
        scen = new Scenario*[gc->nr_nodes];
        for(size_t s = 0; s < gc->nr_nodes; s++) {
            scen[s] = new Scenario(gc->stps, gc->dt, s);
        }
    }
    catch(bad_alloc &) {
        cout << "Bad allocation" << std::endl;
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
        exit(EXIT_FAILURE);
    }

}
///////////////////////////////////////////////////////////
Herss::~Herss(){
    delete rs;
    for(size_t s=0; s < nr_nodes; s++) {
        delete scen[s];
    }
    delete [] scen;

    this->gc = NULL;
}
/////////////////////////////////////////////////////////////////////
int Herss::prepaireSimulation(Dataset *data) {

    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->ReadNodeData(gc->topologyfile);
        rs->nodes[n]->S = this->scen[n];
        rs->nodes[n]->S->dt   = gc->dt;
        rs->nodes[n]->S->stps = gc->stps;
    }

    // Transfer data to nodes/scenarios
    for( size_t t = 0; t < stps; ++t ) {
        for(size_t n = 0; n < gc->nr_nodes; n++) {
            rs->nodes[n]->S->inflow[t] = data->inflow[t][n];
            rs->nodes[n]->S->action[t] = data->action[t][n];
            rs->nodes[n]->S->price[t]  = data->price[t];
            rs->nodes[n]->S->year[t]   = data->year[t];
            rs->nodes[n]->S->month[t]  = data->month[t];
            rs->nodes[n]->S->day[t]    = data->day[t];
            rs->nodes[n]->S->hour[t]   = data->hour[t];
        }
    }

    // We need to load statefile
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->ReadStateFile(gc->start_statefile);
    }

    // Initialize all arraycurves 
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        //printf("Init array curves \n");
        rs->nodes[n]->initArrayCurves();
    }

    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        rs->reservoirs[r].InitReservoir();

        size_t node_idnr = rs->reservoirs[r].idnr;
        rs->nodes[node_idnr]->reservoir_idnr = r;

    }

    // Set pointers for the different outlets (RESERVOIR) and downstream nodes (CHANNEL/POWERSTATION)
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        
        if(rs->nodes[n]->downstream_node_in_use) {
            rs->nodes[n]->ptr_downstream_node = rs->nodes[rs->nodes[n]->downstream_idnr];
        }

        if( rs->nodes[n]->outlet_hatch_in_use) {
            rs->nodes[n]->ptr_downstream_node_hatch = rs->nodes[rs->nodes[n]->downstream_idnr_hatch];
        }

        if( rs->nodes[n]->outlet_tunnel_in_use) {
            if(rs->nodes[n]->downstream_idnr_tunnel > int(this->nr_nodes-1)  ) {
                printf("ERROR:  There is something wrong with node idnrs. \n");
                printf("rs->nodes[n]->downstream_idnr_tunnel = %d\n", rs->nodes[n]->downstream_idnr_tunnel);
                printf("nr_nodes = %lu\n", this->nr_nodes);
                printf("Please check your node idnrs in the topology file\n");
                printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
                exit(EXIT_FAILURE);
            }
            rs->nodes[n]->ptr_downstream_node_tunnel = rs->nodes[rs->nodes[n]->downstream_idnr_tunnel];
        }

        if( rs->nodes[n]->outlet_overflow_in_use) {
            if(  rs->nodes[n]->downstream_idnr_overflow > int(this->nr_nodes-1)  ) {
                printf("ERROR:  There is something wrong with node idnrs. \n");
                printf("rs->nodes[n]->downstream_idnr_overflow = %d\n", rs->nodes[n]->downstream_idnr_overflow);
                printf("nr_nodes = %lu\n", this->nr_nodes);
                printf("Please check your node idnrs in the topology file\n");
                printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
                exit(EXIT_FAILURE);
            }
            rs->nodes[n]->ptr_downstream_node_overflow = rs->nodes[rs->nodes[n]->downstream_idnr_overflow];
        }


        if( rs->nodes[n]->outlet_auto_qmin_in_use ) {
            rs->nodes[n]->ptr_downstream_node_auto_qmin = rs->nodes[rs->nodes[n]->downstream_idnr_auto_qmin];
        }
    }

    return 0;
}
/////////////////////////////////////////////////////////////////////
int Herss::WriteStateFile() {

    FILE *fp;
    if((fp = fopen(gc->out_statefile.c_str() ,"w"))==NULL) {
        printf("Cannot open file %s \n", gc->out_statefile.c_str() );
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        exit(EXIT_FAILURE);
    }

    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->WriteStateFile(fp);
    }

    fclose(fp);
    return 0;
}
/////////////////////////////////////////////////////////////////////
void Herss::SetAction(size_t node_idnr, size_t t, double value) {
    // this->scen[idx]->action[t] = value;
    rs->nodes[node_idnr]->S->action[t] = value;
}
/////////////////////////////////////////////////////////////////////
double Herss::GetAction(size_t node_idnr, size_t t) {
    return rs->nodes[node_idnr]->S->action[t];
}
/////////////////////////////////////////////////////////////////////
void Herss::PrintActions() {
    printf("Actions = ");
    for( size_t t = 0; t < stps; ++t ) {
        for(size_t n = 0; n < this->gc->n_action_nodes; ++n) {
            size_t idx = this->gc->actions_idnrs[n];
            printf("%.2f ", this->scen[idx]->action[t]);
        }
        printf("\n");
    }
}
/////////////////////////////////////////////////////////////////////
void Herss::SetReservoir_Init_fr(size_t node_idnr, double value) {

    // We check if the node_idnr is of type Reservoir
    if( rs->nodes[node_idnr]->nodetype != NodeType::RESERVOIR ) {
        printf("\nERROR: You are trying to set initial reservoir levels in a node that is not of NodeType::RESERVOIR\n");
        printf("node_idnr = %lu , nodename = %s\n", node_idnr, rs->nodes[node_idnr]->nodename.c_str() );
        string str_nodetype = EnumToString(rs->nodes[node_idnr]->nodetype);
        cout << "str_nodetype = " << str_nodetype << endl;
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
	    exit(EXIT_FAILURE);
    }

    if(value < 0.0 || value > 1.1) {
        printf("WARNING:  initial reservoir levels are weird\n");
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
    }

    //cout << "reservoir_idnr = " << rs->nodes[node_idnr]->reservoir_idnr << endl;
    size_t res_idnr = rs->nodes[node_idnr]->reservoir_idnr;
    rs->reservoirs[res_idnr].reservoir_init_fr = value;
}
/////////////////////////////////////////////////////////////////////
void Herss::PrintReservoirLevels_fr() {
    printf("Initial Reservoir_fr= ");
    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        printf("%.3f ", rs->reservoirs[r].reservoir_init_fr);
    }
    printf("\n");

    printf("Current Reservoir_fr= ");
    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        rs->reservoirs[r].InitReservoir();
        printf("%.3f ", rs->reservoirs[r].res_fr);
    }
    printf("\n");
}
/////////////////////////////////////////////////////////////////////
void Herss::PrintRemainingChannelWater_Mm3(){
    if( gc->nr_channels > 0 ) {
        printf("RemainingChannelWater_Mm3= ");
        for(size_t c = 0; c < gc->nr_channels; c++) {
            printf("%.5f " , rs->channels[c].remaining_available_Mm3);
        }
        printf("\n");
    }
    else {
        printf("There are no Channels in this river system\n");
    }
}
/////////////////////////////////////////////////////////////////////
double Herss::GetRestPrice() {
    printf("WORK IN PROGRESS\n");
    exit(EXIT_FAILURE);
    return -9;
}
/////////////////////////////////////////////////////////////////////
void Herss::PrintInflowSeries(size_t t) {
    printf("Reservoir inflow= ");
    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        printf("%.4f ", rs->reservoirs[r].S->inflow[t]);
    }
    printf("\n");
}
/////////////////////////////////////////////////////////////////////
// Print Price, Pend, Inflow and initial Reservoir levels. 
void Herss::PrintState() {
    printf("WORK IN PROGRESS");

}
/////////////////////////////////////////////////////////////////////
void Herss::SetPrice(size_t t, double price, double restprice) {
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->S->price[t] = price;
        rs->nodes[n]->S->restprice = restprice;
    }
}
/////////////////////////////////////////////////////////////////////
void Herss::PrintAllInput() {
    printf("Price: ");
    for(size_t t = 0; t < gc->stps; t++) {
        printf("%.2f ", rs->nodes[0]->S->price[t]);
    }
    printf("\n");
    printf("Restprice = %.2f\n", rs->nodes[0]->S->restprice);
    printf("Inflow\n");
    for(size_t t = 0; t < gc->stps; t++) {
        for(size_t r = 0; r < gc->nr_reservoirs; r++) {
            printf("%.4f ", rs->reservoirs[r].S->inflow[t]);
        }
        printf("\n");
    }

    PrintReservoirLevels_fr();

    printf("ACTIONS: \n");
    for(size_t t = 0; t < gc->stps; t++) {
        for(size_t n = 0; n < gc->nr_nodes; n++) {
            if(rs->nodes[n]->S->action[t] > -0.01   &&  rs->nodes[n]->S->action[t] < 1.01  ) { 
                printf("%.2f ", rs->nodes[n]->S->action[t]);
            }
        }
        printf("\n");
    }
}
/////////////////////////////////////////////////////////////////////
void Herss::SetInflowInNode(size_t t, size_t nodenr, double value) {
    rs->nodes[nodenr]->S->inflow[t] = value;
}

/////////////////////////////////////////////////////////////////////
double Herss::GetInflowInNode(size_t t, size_t nodenr) {
    return rs->nodes[nodenr]->S->inflow[t];
}
/////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////
double Herss::GetPrice(size_t t) {
    return rs->nodes[0]->S->price[t];
}
/////////////////////////////////////////////////////////////////////
// Note that idnr goes from 0 -> n-1
double Herss::GetReservoir_Init_fr(size_t idnr) {
    return rs->reservoirs[idnr].reservoir_init_fr;
}  // Get starting reservoir fraction.
/////////////////////////////////////////////////////////////////////
double Herss::GetReservoirLevel_fr(size_t node_idnr, size_t t) { 
    return rs->nodes[node_idnr]->S->res_fr[t];
}
/////////////////////////////////////////////////////////////////////
int Herss::Simulate() {

    //-----------------------------------------------------------------
    // BVM, July 2024. 
    // When doing sampling we need to initialize states every time.
    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        rs->reservoirs[r].InitReservoir();
    }

    for(size_t c = 0; c < gc->nr_channels; c++) {
        rs->channels[c].SetStartState();
    }

    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->remaining_available_Mm3 = 0.0;
        rs->nodes[n]->upstream_remaining_available_Mm3 = 0.0;
    }

    // DO NOT CHANGE THIS AROUND - IT EFFECTS THE RESULTS
    for( size_t t = 0; t < stps; ++t ) {
        for(size_t n = 0; n < gc->nr_nodes; n++) {
            rs->nodes[n]->Simulate(t);
        }
    }

    // We need to update the remaining water in the node pointers (up, down)
    // Note that in reservoirs the water below LRW is DEAD.
    // It needs to be accounted for in the waterbalance calulations, 
    // but it is not water available for energy production.
    // Available water and total amount of water in the riversystem is not the same. 
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        if(rs->nodes[n]->ptr_downstream_node != NULL) {
            // We now increase the downstream nodes available water, should have been initialized to zero.
            rs->nodes[n]->ptr_downstream_node->upstream_remaining_available_Mm3 += 
                (rs->nodes[n]->remaining_available_Mm3 + rs->nodes[n]->upstream_remaining_available_Mm3);
                //    local node water                  +   upstream water 
        }

    }

    return 0;
}
/////////////////////////////////////////////////////////////////////
int Herss::CheckWaterBalance() {
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->CheckWaterBalance();
    }
    return 0;
}
/////////////////////////////////////////////////////////////////////
int Herss::GlobalWaterBalance(Dataset *data){
    rs->start_water_Mm3 = 0.0;
    rs->end_water_Mm3 = 0.0;
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->start_water_Mm3 += rs->nodes[n]->GetStartWater_Mm3();
    }
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->end_water_Mm3 += rs->nodes[n]->GetEndWater_Mm3();
    }
    rs->inflow_volume_Mm3 = 0.0;
    for(size_t t=0; t < gc->stps; t++) {
        for(size_t n = 0; n < gc->nr_nodes; n++) {
            rs->inflow_volume_Mm3 += MACRO_m3s_2_Mm3(data->inflow[t][n] , gc->dt);
        }
    }
    // How much did leave the Riversystem?
    // We need to get the total volume of water leaving the most downstream node.
    rs->outgoing_Mm3 = 0.0;

    for(size_t t=0; t < gc->stps; t++) {
        rs->outgoing_Mm3 += MACRO_m3s_2_Mm3(rs->nodes[gc->nr_nodes-1]->S->tot_outflow[t], gc->dt);
    }
    rs->waterbalance = rs->start_water_Mm3 + rs->inflow_volume_Mm3 - rs->end_water_Mm3 - rs->outgoing_Mm3;
    if(WATERBALANCE_WARNINGS) { 
        printf("-----------------------------------------\n");
        printf( "GLOBAL TOTAL WATERBALANCE   (note: Total = available + dead water) \n");
        printf("start_water_Mm3   = %.6f\n", rs->start_water_Mm3 );
        printf("inflow_Mm3        = %.6f\n", rs->inflow_volume_Mm3); 
        printf("outflow_Mm3       = %.6f\n", rs->outgoing_Mm3);
        printf("remaining_Mm3     = %.6f\n", rs->end_water_Mm3);
        printf("waterbalance      = %.6f\n", rs->waterbalance);
        printf("-----------------------------------------\n");
    }

    if(abs(rs->waterbalance) > 0.0001) {
        printf("-----------------------------------------\n");
        printf( "GLOBAL WATERBALANCE ERROR \n");
        printf("start_water_Mm3   = %.6f\n", rs->start_water_Mm3 );
        printf("inflow_volume_Mm3 = %.6f\n", rs->inflow_volume_Mm3); 
        printf("outflow_Mm3       = %.6f\n", rs->outgoing_Mm3);
        printf("remaining_Mm3     = %.6f\n", rs->end_water_Mm3);
        printf("waterbalance      = %.6f\n", rs->waterbalance);
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
	    exit(EXIT_FAILURE);
    }
    return 0;
}
/////////////////////////////////////////////////////////////////////
int Herss::WriteNodeOutput(){
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->WriteNodeOutput(gc);
    }
    return 0;
}  
/////////////////////////////////////////////////////////////////////
int Herss::CalcAdjustmenCosts() {
    // For each powerstation We loop over all timesteps and check if we break maximum number of adjustemnts pr day.
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        if(rs->nodes[n]->nodetype == NodeType::POWERSTATION) { 
            if(rs->nodes[n]->max_adjustment_pr_day > 0 ) {
                rs->adjust_cost = rs->pstations[rs->nodes[n]->pstation_idnr].CalcAdjustmenCosts();
            }
        }
    }
    return 0;
}
/////////////////////////////////////////////////////////////////////
