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
#include <cmath>

Herss::Herss(){}

///////////////////////////////////////////////////////////
Herss::Herss(GlobalConfig *gc){

    this->gc = gc;

    if(gc->nr_nodes < 1 ) {
        LOG_ERR("nr_nodes < 1");
    }

    if(gc->dt < 1 ) {
        LOG_ERR("dt < 1");
    }

    if(gc->stps < 1 ) {
        LOG_ERR("stps < 1");
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
        LOG_ERR("Bad allocation");
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
// This function is used to read the topology file.
// It is used when we instansiate the herss object from scratch
void Herss::ReadTopologyFile(string filename) {

    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->ReadNodeData(gc->topologyfile);
        rs->nodes[n]->S = this->scen[n];
        rs->nodes[n]->S->dt   = gc->dt;
        rs->nodes[n]->S->stps = gc->stps;
        rs->nodes[0]->S->restprice = -1*NOT_INIT;
    }

    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->ReadStateFile(gc->start_statefile);
    }

    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->initArrayCurves();
    }

    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        rs->reservoirs[r].InitReservoir();
        size_t node_idnr = rs->reservoirs[r].idnr;
        rs->nodes[node_idnr]->reservoir_idnr = r;
    }

    SetPointers();

}
/////////////////////////////////////////////////////////////////////
int Herss::prepaireSimulation(Dataset *data) {

	string line;
    string keyword;
    string value;
    string str_idnr;
    string str_name;
    Line line_obj;

    this->data = data;  // Store dataset reference for variable timesteps

    // Set the nodenames, nodetypes in each node
    for (size_t i = 0; i < gc->topoparser.getLineCount(); ++i) {
        line = gc->topoparser.getLine(i);
        string tmpline = line;  // Create a copy of the line for parsing
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);
        
        if (keyword == "NODE") {
            if(value.empty()) {
                LOG_ERR("ERROR: something is worng in topology file "  + gc->topologyfile + "see line " + tmpline);
            }

            if (value == "RESERVOIR") {
                str_idnr = line_obj.extractNextElementFromLine(&line);
                str_name = line_obj.extractNextElementFromLine(&line);

                size_t node_id = std::stoul(str_idnr);
                if (node_id >= gc->nr_nodes) {
                    LOG_ERR("Error: Node ID " + std::to_string(node_id) + " exceeds the number of nodes (" + std::to_string(gc->nr_nodes) );
                }

                rs->nodes[node_id]->nodename = str_name;
                rs->nodes[node_id]->idnr = node_id;   
                rs->nodes[node_id]->nodetype = NodeType::RESERVOIR; 
            }
            else if (value == "PSTATION") {
                str_idnr = line_obj.extractNextElementFromLine(&line);
                str_name = line_obj.extractNextElementFromLine(&line);
                size_t node_id = std::stoul(str_idnr);
                if (node_id >= gc->nr_nodes) {
                    LOG_ERR("Error: Node ID " + std::to_string(node_id) + " exceeds the number of nodes (" + std::to_string(gc->nr_nodes) + ")");
                }
                rs->nodes[node_id]->nodename = str_name;
                rs->nodes[node_id]->idnr = node_id;   
                rs->nodes[node_id]->nodetype = NodeType::PSTATION; 
            }
            else if (value == "CHANNEL") {
                str_idnr = line_obj.extractNextElementFromLine(&line);
                str_name = line_obj.extractNextElementFromLine(&line);
                size_t node_id = std::stoul(str_idnr);
                if (node_id >= gc->nr_nodes) {
                    LOG_ERR("Error: Node ID " + std::to_string(node_id) + " exceeds the number of nodes (" + std::to_string(gc->nr_nodes) + ")");
                }
                rs->nodes[node_id]->nodename = str_name;
                rs->nodes[node_id]->idnr = node_id;   
                rs->nodes[node_id]->nodetype = NodeType::CHANNEL;
            }
            // Terje Sandø, pump-station work, July 2026.
            else if (value == "PUMP") {
                str_idnr = line_obj.extractNextElementFromLine(&line);
                str_name = line_obj.extractNextElementFromLine(&line);
                size_t node_id = std::stoul(str_idnr);
                if (node_id >= gc->nr_nodes) {
                    LOG_ERR("Error: Node ID " + std::to_string(node_id) + " exceeds the number of nodes (" + std::to_string(gc->nr_nodes) + ")");
                }
                rs->nodes[node_id]->nodename = str_name;
                rs->nodes[node_id]->idnr = node_id;   
                rs->nodes[node_id]->nodetype = NodeType::PUMP;
            }
            else {
                LOG_ERR("ERROR: Invalid nodetype in topology file it must be RESERVOIR, PSTATION, CHANNEL or PUMP" + gc->topologyfile);
            }
        }
    }


    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->ReadNodeData(gc->topologyfile);
        rs->nodes[n]->S = this->scen[n];
        rs->nodes[n]->S->dt   = gc->dt;
        rs->nodes[n]->S->stps = gc->stps;
        rs->nodes[0]->S->restprice = data->restprice;
    }


    // Transfer data to nodes/scenarios
    for( size_t t = 0; t < stps; ++t ) {
        for(size_t n = 0; n < gc->nr_nodes; n++) {
            rs->nodes[n]->S->inflow[t]    = data->inflow[t][n];
            rs->nodes[n]->S->action[t][n] = data->action[t][n];
            rs->nodes[n]->S->price[t]     = data->price[t];
            rs->nodes[n]->S->year[t]      = data->year[t];
            rs->nodes[n]->S->month[t]     = data->month[t];
            rs->nodes[n]->S->day[t]       = data->day[t];
            rs->nodes[n]->S->hour[t]      = data->hour[t];
        }
    }


    // We need to load statefile
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->ReadStateFile(gc->start_statefile);
    }


    // Initialize all arraycurves 
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->initArrayCurves();
    }

    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        rs->reservoirs[r].InitReservoir();
        size_t node_idnr = rs->reservoirs[r].idnr;
        rs->nodes[node_idnr]->reservoir_idnr = r;
    }

    SetPointers();

    // BVM June 2026.
    // Make sure actions for RESERVOIRS are set correctly
    if(!data->action_colnames.empty()) {

        for (size_t n = 0; n < gc->nr_nodes; ++n) {
            Node* node = rs->nodes[n];

            if(node->nodetype == NodeType::RESERVOIR) {

                Reservoir* res = static_cast<Reservoir*>(node);

                // We only do this if outlet_hatch is in use, 
                // otherwise the action vector for reservoirs is not used and can be left as is.
                if(res->outlet_hatch_in_use) {
                    // Find the column index in Dataset
                    int col_idx = -1;
                    for (size_t c = 0; c < data->action_colnames.size(); ++c) {
                        if (data->action_colnames[c] == std::to_string(res->idnr)) {
                            col_idx = c;
                            break;
                        }
                    }

                    if (col_idx == -1) {
                        LOG_ERR("ERROR: Could not find action column for " + std::to_string(res->idnr) + " in action file");
                    }

                    for (size_t t = 0; t < data->stps; ++t) {
                        res->S->action[t][res->idnr] = data->action[t][col_idx];
                    }
                } else {
                    // Reservoir has no hatch — clear the action slot that was
                    // incorrectly populated by the generic positional load loop above.
                    for (size_t t = 0; t < data->stps; ++t) {
                        res->S->action[t][res->idnr] = 0.0;
                    }
                }
            }
        }
    }

    // CHANGES BY OVE: Map generator actions from actions.txt to each generator in each powerstation
    if(!data->action_colnames.empty()) {
        for (size_t n = 0; n < gc->nr_nodes; ++n) {
            Node* node = rs->nodes[n];
            if (node->nodetype == NodeType::PSTATION) {
                Powerstation* ps = static_cast<Powerstation*>(node);
                int ps_id = ps->idnr;
                for (size_t g = 0; g < ps->generators.size(); ++g) {
                    // Build the expected column name "1_0"
                    std::string colname = std::to_string(ps_id) + "_" + std::to_string(g);
                
                    // Find the column index in Dataset
                    int col_idx = -1;
                    for (size_t c = 0; c < data->action_colnames.size(); ++c) {
                        if (data->action_colnames[c] == colname) {
                            col_idx = c;
                            break;
                        }
                    }
                    if (col_idx == -1) {
                        LOG_ERR("ERROR: Could not find action column for generator " + colname + " in action file");
                    }
                
                    // Fill the generators action vector for all timesteps
                    ps->generators[g].action.clear();
                    for (size_t t = 0; t < data->stps; ++t) {
                        ps->generators[g].action.push_back(data->action[t][col_idx]);
                    }
                }
            }
        }
    } else {
        // If no action file data, initialize generator action vectors with zeros 
        for (size_t n = 0; n < gc->nr_nodes; ++n) {
            Node* node = rs->nodes[n];
            if (node->nodetype == NodeType::PSTATION) {
                Powerstation* ps = static_cast<Powerstation*>(node);
                for (size_t g = 0; g < ps->generators.size(); ++g) {
                    ps->generators[g].action.resize(data->stps, 0.0);
                }
            }
        }
        LOG_ERR("WARNING: No action column names found in dataset, generator actions initialized to zero");
    }

    // Terje Sandø, pump-station work, July 2026.
    // PUMP nodes always have exactly one action column (single unit in v1),
    // named by the only pump idnr in the action file.
    if(!data->action_colnames.empty()) {
        for (size_t n = 0; n < gc->nr_nodes; ++n) {
            Node* node = rs->nodes[n];
            if(node->nodetype == NodeType::PUMP) {

                int col_idx = -1;
                for (size_t c = 0; c < data->action_colnames.size(); ++c) {
                    if (data->action_colnames[c] == std::to_string(node->idnr)) {
                        col_idx = c;
                        break;
                    }
                }

                if (col_idx == -1) {
                    LOG_ERR("ERROR: Could not find action column for PUMP node " + std::to_string(node->idnr) + " in action file");
                }

                for (size_t t = 0; t < data->stps; ++t) {
                    node->S->action[t][node->idnr] = data->action[t][col_idx];
                }
            }
        }
    }


    return 0;
}
/////////////////////////////////////////////////////////////////////
void Herss::SetPointers() {

    // Set pointers for the different outlets (RESERVOIR) and downstream nodes (CHANNEL/POWERSTATION)
    for(size_t n = 0; n < gc->nr_nodes; n++) {

        if(rs->nodes[n]->downstream_node_in_use) {
            rs->nodes[n]->ptr_downstream_node = rs->nodes[rs->nodes[n]->downstream_idnr];
        }

        if( rs->nodes[n]->outlet_hatch_in_use) {
            rs->nodes[n]->ptr_downstream_node_hatch = rs->nodes[rs->nodes[n]->downstream_idnr_hatch];
        }

        if( rs->nodes[n]->outlet_tunnel_in_use) {
            if(rs->nodes[n]->downstream_idnr_tunnel > (this->nr_nodes-1)) {
                LOG_WARN("ERROR:  There is something wrong with node idnrs");
                LOG_WARN("rs->nodes[n]->downstream_idnr_tunnel = " + std::to_string(rs->nodes[n]->downstream_idnr_tunnel));
                LOG_WARN("nr_nodes = " + std::to_string(this->nr_nodes));
                LOG_ERR("Please check your node idnrs in the topology file");
            }
            rs->nodes[n]->ptr_downstream_node_tunnel = rs->nodes[rs->nodes[n]->downstream_idnr_tunnel];
        }

        if( rs->nodes[n]->outlet_overflow_in_use) {
            // We have either a spillway or an overflow curve, 
            // but both have the same downstream node idnr, so we only need to check this once.
            if(  rs->nodes[n]->downstream_idnr_overflow > (this->nr_nodes-1)  ) {
                LOG_WARN("ERROR:  There is something wrong with node idnrs");
                LOG_WARN("rs->nodes[n]->downstream_idnr_overflow = " + std::to_string(rs->nodes[n]->downstream_idnr_overflow));
                LOG_WARN("nr_nodes = " + std::to_string(this->nr_nodes));
                LOG_ERR("Please check your node idnrs in the topology file");
            }
            rs->nodes[n]->ptr_downstream_node_overflow = rs->nodes[rs->nodes[n]->downstream_idnr_overflow];
        }

        if( rs->nodes[n]->outlet_auto_qmin_in_use ) {
            rs->nodes[n]->ptr_downstream_node_auto_qmin = rs->nodes[rs->nodes[n]->downstream_idnr_auto_qmin];
        }

        // Terje Sandø, pump-station work, July 2026.
        if( rs->nodes[n]->nodetype == NodeType::PUMP ) {
            Pump* pump = static_cast<Pump*>(rs->nodes[n]);
            if(pump->pump_source_idnr == NOT_INIT || pump->pump_target_idnr == NOT_INIT) {
                LOG_WARN("ERROR: PUMP node " + std::to_string(int(n)) + " (" + pump->nodename + ") is missing PUMP_SOURCE_IDNR/PUMP_TARGET_IDNR.");
                LOG_ERR("Check your topology file - a PUMP node must define both PUMP_SOURCE_IDNR and PUMP_TARGET_IDNR.");
            }
            if(pump->pump_source_idnr > (this->nr_nodes-1) || pump->pump_target_idnr > (this->nr_nodes-1)) {
                LOG_WARN("ERROR: PUMP node " + std::to_string(int(n)) + " (" + pump->nodename + ") PUMP_SOURCE_IDNR/PUMP_TARGET_IDNR out of range.");
                LOG_ERR("Please check PUMP_SOURCE_IDNR/PUMP_TARGET_IDNR in the topology file.");
            }
            pump->pump_source_target_in_use = true;
            pump->ptr_pump_source = rs->nodes[pump->pump_source_idnr];
            pump->ptr_pump_target = rs->nodes[pump->pump_target_idnr];

            // Let source/target reservoirs push their start/end MASL into the pump
            // each timestep, so the pump computes a stable averaged head (same
            // convention as Powerstation).
            if(pump->ptr_pump_source->nodetype == NodeType::RESERVOIR) {
                static_cast<Reservoir*>(pump->ptr_pump_source)->ptr_pumps_as_source.push_back(pump);
            }
            if(pump->ptr_pump_target->nodetype == NodeType::RESERVOIR) {
                static_cast<Reservoir*>(pump->ptr_pump_target)->ptr_pumps_as_target.push_back(pump);
            }
        }
    }
}
/////////////////////////////////////////////////////////////////////
int Herss::WriteStateFile() {

    FILE *fp;
    if((fp = fopen(gc->out_statefile.c_str() ,"w"))==NULL) {
        LOG_ERR("Cannot open file " + gc->out_statefile);
    }

    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->WriteStateFile(fp);
    }

    fclose(fp);
    return 0;
}
/////////////////////////////////////////////////////////////////////
void Herss::SetAction(size_t node_idnr, size_t gen_idnr, size_t t, double value) {

    if (rs->nodes[node_idnr]->nodetype == NodeType::PSTATION) {
        Powerstation* ps = static_cast<Powerstation*>(rs->nodes[node_idnr]);
        if (gen_idnr >= ps->generators.size()) {
            LOG_ERR("SetAction: gen_idnr " + std::to_string(gen_idnr) + " out of range");
        }
        ps->generators[gen_idnr].action[t] = value;
    }
    else if (rs->nodes[node_idnr]->nodetype == NodeType::PUMP) {
        Pump* pump = static_cast<Pump*>(rs->nodes[node_idnr]);
        pump->S->action[t][node_idnr] = value;
    }
    else if (rs->nodes[node_idnr]->nodetype == NodeType::RESERVOIR) {
        Reservoir* res = static_cast<Reservoir*>(rs->nodes[node_idnr]);
        if (!res->outlet_hatch_in_use) {
            LOG_ERR("SetAction: RESERVOIR node " + std::to_string(node_idnr) + " has no hatch outlet");
        }
        res->S->action[t][node_idnr] = value;
    }
    else {
        LOG_ERR("SetAction: node " + std::to_string(node_idnr) + " is not a PSTATION, PUMP or RESERVOIR");
    }

    // Old code before multi-generator support    
    // rs->nodes[node_idnr]->S->action[t][node_idnr] = value;

}
/////////////////////////////////////////////////////////////////////
double Herss::GetAction(size_t node_idnr, size_t gen_idnr, size_t t) {

    if (rs->nodes[node_idnr]->nodetype == NodeType::PSTATION) {
        Powerstation* ps = static_cast<Powerstation*>(rs->nodes[node_idnr]);
        if (gen_idnr >= ps->generators.size()) {
            LOG_ERR("GetAction: gen_idnr " + std::to_string(gen_idnr) + " out of range");
        }
        return ps->generators[gen_idnr].action[t];
    }
    else if (rs->nodes[node_idnr]->nodetype == NodeType::PUMP) {
        Pump* pump = static_cast<Pump*>(rs->nodes[node_idnr]);
        return pump->S->action[t][node_idnr];
    }
    else if (rs->nodes[node_idnr]->nodetype == NodeType::RESERVOIR) {
        Reservoir* res = static_cast<Reservoir*>(rs->nodes[node_idnr]);
        if (!res->outlet_hatch_in_use) {
            LOG_ERR("GetAction: RESERVOIR node " + std::to_string(node_idnr) + " has no hatch outlet");
        }
        return res->S->action[t][node_idnr];
    }
    else {
        LOG_ERR("GetAction: node " + std::to_string(node_idnr) + " is not a PSTATION, PUMP or RESERVOIR");
    }

    return -9.0;

    // Old code before multi-generator support  
    // return rs->nodes[node_idnr]->S->action[t][node_idnr];

}
/////////////////////////////////////////////////////////////////////
void Herss::PrintActions() {
    printf("ACTIONS: \n");
    for(size_t t = 0; t < gc->stps; t++) {
        for(size_t n = 0; n < gc->nr_nodes; n++) {
            for(size_t g = 0; g < gc->nr_nodes; g++) { 
                if(rs->nodes[n]->S->action[t][g] > -0.01 && rs->nodes[n]->S->action[t][g] < 1.01) {
                    printf("%.5f ", rs->nodes[n]->S->action[t][g]);
                }
            }
        }
        printf("\n");
    }
}
/////////////////////////////////////////////////////////////////////
void Herss::SetReservoir_Init_fr(size_t node_idnr, double value) {

    // We check if the node_idnr is of type Reservoir
    if( rs->nodes[node_idnr]->nodetype != NodeType::RESERVOIR ) {
        LOG_WARN("\nERROR: You are trying to set initial reservoir levels in a node that is not of NodeType::RESERVOIR\n");
        LOG_WARN("node_idnr = " + std::to_string(node_idnr) + " , nodename = " + rs->nodes[node_idnr]->nodename);
        string str_nodetype = EnumToString(rs->nodes[node_idnr]->nodetype);
        LOG_WARN("str_nodetype = " + str_nodetype);
        LOG_ERR("Please check your input make sure you are setting initial reservoir levels in the correct nodes");
    }

    if(value < 0.0 || value > 1.1) {
        size_t res_idnr = rs->nodes[node_idnr]->reservoir_idnr;
        const char* res_name = rs->reservoirs[res_idnr].nodename.c_str();
        LOG_WARN("WARNING: initial reservoir level is weird: value= " + std::to_string(value) + ", reservoir=" + std::string(res_name));
        
    }

    //cout << "reservoir_idnr = " << rs->nodes[node_idnr]->reservoir_idnr << endl;
    size_t res_idnr = rs->nodes[node_idnr]->reservoir_idnr;
    rs->reservoirs[res_idnr].reservoir_init_fr = value;
}
/////////////////////////////////////////////////////////////////////
void Herss::PrintReservoirLevels_fr() {
    printf("Initial Reservoir_fr= ");
    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        printf("%.5f ", rs->reservoirs[r].reservoir_init_fr);
    }
    printf("\n");

    printf("Current Reservoir_fr= ");
    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        rs->reservoirs[r].InitReservoir();
        printf("%.5f ", rs->reservoirs[r].res_fr);
    }
    printf("\n");
}
/////////////////////////////////////////////////////////////////////
void Herss::PrintRemainingChannelWater_Mm3(){
    if( gc->nr_channels > 0 ) {
        printf("RemainingChannelWater_Mm3= ");
        for(size_t c = 0; c < gc->nr_channels; c++) {
            printf("%.5f " , rs->channels[c].remaining_Mm3);
        }
        printf("\n");
    }
    else {
        printf("There are no Channels in this river system\n");
    }
}
/////////////////////////////////////////////////////////////////////
double Herss::GetRestPrice() {
    LOG_ERR("WORK IN PROGRESS\n");
    return -9;
}
/////////////////////////////////////////////////////////////////////
void Herss::PrintInflowSeries(size_t t) {
    printf("Reservoir inflow:\n");
    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        printf("RESERVOIR:  %-20s  %.4f \n", rs->reservoirs[r].nodename.c_str()   , rs->reservoirs[r].S->inflow[t]);
    }
    printf("\n");
}
/////////////////////////////////////////////////////////////////////
// Print Price, Pend, Inflow and initial Reservoir levels. 
void Herss::PrintState() {
    LOG_ERR("WORK IN PROGRESS");
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
        printf("%.5f ", rs->nodes[0]->S->price[t]);
    }
    printf("\n");

    printf("Restprice = %.5f\n", rs->nodes[0]->S->restprice);

    printf("Inflow\n");
    for(size_t t = 0; t < gc->stps; t++) {
        for(size_t r = 0; r < gc->nr_reservoirs; r++) {
            printf("%.5f ", rs->reservoirs[r].S->inflow[t]);
        }
        printf("\n");
    }

    printf("ACTIONS: \n");
    for(size_t t = 0; t < gc->stps; t++) {
        for(size_t n = 0; n < gc->nr_nodes; n++) {
            if(rs->nodes[n]->S->action[t][n] > -0.01   &&  rs->nodes[n]->S->action[t][n] < 1.01  ) { 
                printf("%.2f ", rs->nodes[n]->S->action[t][n]);
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
// Calculate the water value in the system at a specific timestep
// Note that we can calculate the watervalue at the beginning of a timestep or at the end 
// In this function we calculate the watervalue at the END of the timestep.
double Herss::CalcWaterValue_atEndofStp(size_t t) {

    if(gc->nr_reservoirs > 1) {
        LOG_ERR("ERROR:  This function is not implemented yet");
    }

    double deltaR = 0.005;

    // First we simulate the whole timeseries
    Simulate();

    rs->CalcVF(rs->nodes[0]->S->restprice);

    rs->ValueFunction[t]  = rs->CalcVF_atEndOfStp(rs->nodes[0]->S->restprice, t);

    double vf0 = rs->ValueFunction[t];

    // Now we find sum production + remaining energy in the system from start time to end of the current timestep t 
    // We do this by summing up production from start to end of current stp. 
    // We subtract this from sum_total_energy 
    double start_sum_production = 0.0;
    for(size_t mytime = 0; mytime <= t; mytime++) {
        for(size_t p = 0; p < gc->nr_pstations; p++) {
            start_sum_production += rs->pstations[p].S->Power[mytime];
        }
    }
    double E0 = rs->sum_total_MWh - start_sum_production;

    double old_inflow = GetInflowInNode(t, 0);
    double max_res_Mm3 = rs->reservoirs[0].filling_at_hrw_Mm3 - rs->reservoirs[0].filling_at_lrw_Mm3;
    int current_dt = getDeltaT(t); 
    double q_m3s = MACRO_Mm3_2_m3s( deltaR * max_res_Mm3, current_dt);

    // We will add so much inflow that we increase the reservoir level by deltaR
    SetInflowInNode(t, 0, old_inflow + q_m3s);
    Simulate();

    rs->CalcVF(rs->nodes[0]->S->restprice);

    double myValueFunc1 = rs->CalcVF_atEndOfStp(rs->nodes[0]->S->restprice, t);
    double vf1 = myValueFunc1;


    // Find the energy 
    start_sum_production = 0.0;
    for(size_t mytime = 0; mytime <= t; mytime++) {
        for(size_t p = 0; p < gc->nr_pstations; p++) {
            start_sum_production += rs->pstations[p].S->Power[mytime];
        }
    }
    double E1 = rs->sum_total_MWh - start_sum_production;


    // In the last timestep we have can add water to the end reservoir.
    double watervalue = (vf1 - vf0)/(E1 - E0);

    rs->WaterValue[t]     = watervalue;

    // Reset to old inflow 
    SetInflowInNode(t, 0, old_inflow);
    Simulate();
    rs->CalcVF(rs->nodes[0]->S->restprice);
    return watervalue;

}  
/////////////////////////////////////////////////////////////////////
double Herss::GetValueFunction_atStp(size_t t) {
    return rs->ValueFunction[t];
};
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
        rs->nodes[n]->remaining_Mm3 = 0.0;
        rs->nodes[n]->upstream_remaining_Mm3 = 0.0;
        rs->nodes[n]->remaining_active_Mm3 = 0.0;
        rs->nodes[n]->upstream_remaining_active_Mm3 = 0.0;
    }

    // DO NOT CHANGE THIS AROUND - IT EFFECTS THE RESULTS (OVE CHANGED IT TO HANDLE VARIABLE TIMESTEPS)
    for( size_t t = 0; t < stps; ++t ) {
        // Set current timestep for ArrayCurve error reporting
        ArrayCurve::setCurrentTimestep(t);

        // Update the current timestep dt for all scenarios
        int current_dt = getDeltaT(t);

        for(size_t n = 0; n < gc->nr_nodes; n++) {

            rs->nodes[n]->S->dt = current_dt;
            
            // Set current node information for ArrayCurve error reporting
            ArrayCurve::setCurrentNode(rs->nodes[n]->idnr, rs->nodes[n]->nodename);
                    
            rs->nodes[n]->Simulate(t);
        }
        
        // Terje Sandø, pump-station work, August 2026.
        // Pump power and cost require average source and target reservoir levels,
        // so calculate them after all nodes have completed this timestep.
        for(size_t p = 0; p < gc->nr_pumps; p++) {
            rs->pumps[p].CalcPowerAndCost(t);
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
            rs->nodes[n]->ptr_downstream_node->upstream_remaining_Mm3          += (rs->nodes[n]->remaining_Mm3         + rs->nodes[n]->upstream_remaining_Mm3);
            
            rs->nodes[n]->ptr_downstream_node->upstream_remaining_active_Mm3  += (rs->nodes[n]->remaining_active_Mm3  + rs->nodes[n]->upstream_remaining_active_Mm3);

        }
    }

    return 0;
}
/////////////////////////////////////////////////////////////////////
int Herss::CheckWaterBalance() {
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        rs->nodes[n]->CheckWaterBalance(this);
    }
    return 0;
}
/////////////////////////////////////////////////////////////////////
int Herss::GlobalWaterBalance(){
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
        int current_dt = getDeltaT(t);  // Use variable timestep
        for(size_t n = 0; n < gc->nr_nodes; n++) {
            rs->inflow_volume_Mm3 += MACRO_m3s_2_Mm3( rs->nodes[n]->S->inflow[t], current_dt);
        }
    }
    // How much did leave the Riversystem?
    // We need to get the total volume of water leaving the most downstream node.
    rs->outgoing_Mm3 = 0.0;

    for(size_t t=0; t < gc->stps; t++) {
        int current_dt = getDeltaT(t);  // Use variable timestep
        rs->outgoing_Mm3 += MACRO_m3s_2_Mm3(rs->nodes[gc->nr_nodes-1]->S->tot_outflow[t], current_dt);
    }
    rs->waterbalance = rs->start_water_Mm3 + rs->inflow_volume_Mm3 - rs->end_water_Mm3 - rs->outgoing_Mm3;
    if(WATERBALANCE_WARNINGS) { 
        LOG_INFO("-----------------------------------------");
        LOG_INFO( "GLOBAL TOTAL WATERBALANCE   (note: Total = available + dead water) " );
        LOG_INFO("start_water_Mm3   = " + std::to_string(rs->start_water_Mm3) );
        LOG_INFO("inflow_Mm3        = " + std::to_string(rs->inflow_volume_Mm3)); 
        LOG_INFO("outflow_Mm3       = " + std::to_string(rs->outgoing_Mm3));
        LOG_INFO("remaining_Mm3     = " + std::to_string(rs->end_water_Mm3));
        LOG_INFO("waterbalance      = " + std::to_string(rs->waterbalance));
        LOG_INFO("-----------------------------------------");
    }

    if(abs(rs->waterbalance) > 0.0001) {
        LOG_WARN("-----------------------------------------");
        LOG_WARN( "GLOBAL WATERBALANCE ERROR " );
        LOG_WARN("start_water_Mm3   = " + std::to_string(rs->start_water_Mm3) );
        LOG_WARN("inflow_volume_Mm3 = " + std::to_string(rs->inflow_volume_Mm3)); 
        LOG_WARN("outflow_Mm3       = " + std::to_string(rs->outgoing_Mm3));
        LOG_WARN("remaining_Mm3     = " + std::to_string(rs->end_water_Mm3));
        LOG_ERR("waterbalance      = " + std::to_string(rs->waterbalance));
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
        if(rs->nodes[n]->nodetype == NodeType::PSTATION) { 
            if(rs->nodes[n]->max_adjustment_pr_day > 0 ) {
                rs->adjust_cost = rs->pstations[rs->nodes[n]->pstation_idnr].CalcAdjustmenCosts();
            }
        }
    }
    return 0;
}
/////////////////////////////////////////////////////////////////////
// Get variable timestep for simulation
int Herss::getDeltaT(size_t timestep) {
    if (data != nullptr) {
        return data->getDeltaT(timestep);
    }
    LOG_ERR("ERROR: Variable timestep requested but Dataset not available");
    return -9; // Will likely crash program..... 
}

void Herss::SetDate(size_t t, int Y, int M, int D, int H) {
    if(!data) return;              // data is set later via prepaireSimulation
    if(t >= data->stps) return;
    data->year[t]  = Y;
    data->month[t] = M;
    data->day[t]   = D;
    data->hour[t]  = H;
}
