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
}
/////////////////////////////////////////////////////////////////////
int Herss::prepaireSimulation(GlobalConfig *gc, Dataset *data) {

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
    char infilename [100];
    sprintf (infilename, "%s%s", gc->inputdir.c_str() , gc->start_statefile.c_str() );
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        //printf("Starting to read statefile at node n=%lu\n", n);
        rs->nodes[n]->ReadStateFile(infilename);
        //printf("Done reading state for node n=%lu\n", n);
    }

    // Initialize all arraycurves 
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        //printf("Init array curves \n");
        rs->nodes[n]->initArrayCurves();
    }

    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        rs->reservoirs[r].InitReservoir();
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
            rs->nodes[n]->ptr_downstream_node_tunnel = rs->nodes[rs->nodes[n]->downstream_idnr_tunnel];
        }

        if( rs->nodes[n]->outlet_overflow_in_use) {
            rs->nodes[n]->ptr_downstream_node_overflow = rs->nodes[rs->nodes[n]->downstream_idnr_overflow];
        }
        
        if( rs->nodes[n]->outlet_auto_qmin_in_use ) {
            rs->nodes[n]->ptr_downstream_node_auto_qmin = rs->nodes[rs->nodes[n]->downstream_idnr_auto_qmin];
        }
    }

    return 0;
}
/////////////////////////////////////////////////////////////////////
int Herss::WriteStateFile(GlobalConfig *gc) {

    FILE *fp;
    char outfilename [100];
    sprintf (outfilename, "%s%s", gc->outputdir.c_str() , gc->out_statefile.c_str() );

    if((fp = fopen(outfilename  ,"w"))==NULL) {
        printf("Cannot open file %s \n", outfilename );
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
int Herss::Simulate(GlobalConfig *gc) {
    // DO NOT CHANGE THIS AROUND - IT EFFECTS THE RESULTS
    for( size_t t = 0; t < stps; ++t ) {
        for(size_t n = 0; n < gc->nr_nodes; n++) {
            rs->nodes[n]->Simulate(t);
        }
    }
    return 0;
}
/////////////////////////////////////////////////////////////////////
int Herss::CheckWaterBalance(GlobalConfig *gc) {
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        // printf("Checking WB in node %zu\n", n); 
        rs->nodes[n]->CheckWaterBalance();
    }
    return 0;
}
/////////////////////////////////////////////////////////////////////
int Herss::GlobalWaterBalance(GlobalConfig *gc, Dataset *data){

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

    if(abs(rs->waterbalance) > 0.0001) {
        printf("-----------------------------------------\n");
        printf( "GLOBAL WATERBALANCE ERROR \n");
        printf("start_water_Mm3   = %.6f\n", rs->start_water_Mm3 );
        printf("inflow_volume_Mm3 = %.6f\n", rs->inflow_volume_Mm3); 
        printf("end_water_Mm3     = %.6f\n", rs->end_water_Mm3);
        printf("outgoing_Mm3      = %.6f\n", rs->outgoing_Mm3);
        printf("waterbalance      = %.6f\n", rs->waterbalance);
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
	    exit(EXIT_FAILURE);
    }
    return 0;
}
/////////////////////////////////////////////////////////////////////
int Herss::WriteNodeOutput(GlobalConfig *gc){
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->WriteNodeOutput(gc);
    }
    return 0;
}  
/////////////////////////////////////////////////////////////////////
int Herss::CalcAdjustmenCosts(GlobalConfig *gc) {
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
