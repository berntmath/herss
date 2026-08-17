/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     channel.cpp
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

********************************************************************************/

#include "herss.h"

//-----------------------------------------------------------------------
Channel::Channel(){
    this->K_traveltime_hours   = NOT_INIT;
    this->decay                = NOT_INIT;
    this->casc_reservoirs      = nullptr;

    for(size_t t = 0; t < MAX_TRAVELTIME_HOURS; t++) {
        waterflow_m3[t]        = NOT_INIT;
        init_waterflow_m3[t]   = NOT_INIT;
    }
}
//-----------------------------------------------------------------------
Channel::~Channel(){
    if(this->casc_reservoirs != nullptr) {
        delete this->casc_reservoirs;
        this->casc_reservoirs = nullptr;
    }
}
//-----------------------------------------------------------------------
// We check if the settings for the channel are valid
void Channel::ValidateChannelSettings() {

    if(this->K_traveltime_hours < 0 || this->K_traveltime_hours > MAX_TRAVELTIME_HOURS) {
        LOG_ERR("TRAVELTIME_HOURS must be between 0 and " + 
            std::to_string(MAX_TRAVELTIME_HOURS));
    }

    if(this->num_cascaded_reservoirs < 0
        || this->num_cascaded_reservoirs > MAX_NR_CASCADED_RESERVOIRS) {
        LOG_ERR("N_CASCADE_LINRES must be between 0 and " 
            + std::to_string(MAX_NR_CASCADED_RESERVOIRS));
    }

    // Terje Sandø, 06.07.2026
    // Validate QMIN period definitions (date format, year-crossing, coverage gaps, overlaps, negative values).
    if(qmin_in_use) {
        this->qmin.validatePeriods(this->nodename);
    }

    // We allocate memory to the cascaded reservoirs if we have not done it already.
    if(this->casc_reservoirs == nullptr) {
        this->casc_reservoirs = 
        new CascadedReservoirs(this->K_traveltime_hours, this->num_cascaded_reservoirs);
        // Transfer initial conditions to the cascaded linear reservoirs.
        this->casc_reservoirs->setInitialStorage(this->initial_storage_linres_Mm3);
    }

}
//-----------------------------------------------------------------------
int Channel::initArrayCurves(void) {     return 0; }
//----------------------------------------------------------------------
void Channel::PrintChannelWater(void){

    LOG_ERR("WORK IN PROGRESS");

    // printf ("NODE CHANNEL %d %s\n", int(idnr) , nodename.c_str() );
    // for(size_t t = 0; t <  this->traveltime; t++ ) {  
    //     printf("waterflow_m3[%lu] = %.5f\n", t, waterflow_m3[t]);
    // }


}
//----------------------------------------------------------------------
int Channel::Simulate(size_t t) {

    // BVM, May 2026, Reimplement the routing model.
    // We use a series of cascaded linear reservoirs to model the routing in the channel.

    this->dt     = S->dt;
    this->stps   = S->stps;

    S->channel_storage_Mm3[t] = 0.0;    // To void warnings. 

    double initial_storage_m3 = this->casc_reservoirs->totalStorageM3();

    double total_in_m3 = 0.0;
    double total_out_m3 = 0.0;
    
    double q_in = S->up_inflow[t]; // m3/s
    double q_out = this->casc_reservoirs->route(q_in, dt);

    total_in_m3 += q_in * dt;
    total_out_m3 += q_out * dt;

    double final_storage_m3 = this->casc_reservoirs->totalStorageM3();
    double water_balance_m3 = initial_storage_m3 + total_in_m3 - total_out_m3 - final_storage_m3;

    if(std::abs(water_balance_m3) > 0.0001) {
        LOG_WARN("WARNING: Water balance check FAILED!"); 
        LOG_WARN("WB = " + std::to_string(water_balance_m3));
        LOG_ERR("WB = " + std::to_string(water_balance_m3));
    }

    S->tot_outflow[t] = q_out; // m3/s
    S->channel_storage_Mm3[t] = final_storage_m3 / 1000000.0; // Convert m3 to Mm3

    if(this->downstream_node_in_use) {
        this->ptr_downstream_node->S->up_inflow[t] += S->tot_outflow[t];
    }
    
    S->income[t]  = 0.0;  // No income in Channels 

    S->cost_qmin[t]  = 0.0;
    if(this->qmin_in_use) {
         double qcost;
         double qmin_requirements = this->qmin.calcQminRequirement(S->year[t], S->month[t], S->day[t], &qcost);  // m3/s
         // Change by Terje Sandø, 07.07.2026
         // Tolerance of 1e-3 m3/s (1 liter/s) is close enough to accept as meeting QMIN requirements!?. 
         // Although this tolerance could also be used just to avoid floating point precision issues and could be pushed down to 1e-12.
         const double QMIN_TOLERANCE_M3S = 1e-3;
         if(S->tot_outflow[t]  < qmin_requirements - QMIN_TOLERANCE_M3S) {
             S->cost_qmin[t]  = qcost*S->dt/3600;
         }
    }
    S->cost[t] = S->cost_qmin[t];

    remaining_Mm3 = S->channel_storage_Mm3[t];

    if(remaining_Mm3 < 0.0) {
         remaining_Mm3 = 0.0; // Used to calculate remaining available energy in system. Cannot be negative.
    }

    // For now we dont put value on the water in the channel.
    remaining_active_Mm3 = 0.0;

    S->Power[t]   = 0.0;  // No power production in Channels
    S->profit[t]  = S->income[t] - S->cost[t];
    S->inflow[t]  = 0.0;

    return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int Channel::ReadNodeData(string filename) {
	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;
    size_t tmp_idnr;
    string token;
    string str_name;
    string str_downstream_node;

    bool inside_node = false;

    // cout << "Channel::ReadNodeData        nodename: " << nodename << ", idnr: " << idnr << ", nodetype: " << EnumToString(nodetype) << "\n";

    for (size_t i = 0; i < gc->topoparser.getLineCount(); ++i) {
        line = gc->topoparser.getLine(i);
        string tmpline = line;  // Create a copy of the line for parsing

        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);

        if ( keyword.compare("ENDNODE") == 0 ) {
            inside_node = false;
        }

        if ( keyword.compare("NODE") == 0 && value.compare("CHANNEL") == 0 ) {
            
            // Old format, changed routing model. 
            // # NODE NODETYPE IDNR NAME DOWNSTREAM_NODE_IDNR
            // NODE CHANNEL 2 VANAROSEN -9
            // TRAVELTIME 1
            // DECAY 0.95
            // QMIN -9999

            // New format, BVM May 2026 
            // # NODE NODETYPE IDNR NAME DOWNSTREAM_NODE_IDNR
            // NODE CHANNEL 2 VANAROSEN -9
            // N_LINRES 3
            // TRAVELTIME_HOURS 4
            // QMIN -9999
            // ENDNODE

            token = line_obj.extractNextElementFromLine(&line);
            tmp_idnr = atoi(token.c_str() );

            if(tmp_idnr == idnr) {

                str_name = line_obj.extractNextElementFromLine(&line);
                str_downstream_node = line_obj.extractNextElementFromLine(&line);

                // Here I have the problem using size_t. It doesnt understand negative numbers. BVM June 2026. 
                int tmp_down_idnr = atoi(str_downstream_node.c_str() );

                if(tmp_down_idnr >= 0) {
                    downstream_node_in_use = true;
                    this->downstream_idnr = size_t(tmp_down_idnr);
                } else {
                    downstream_node_in_use = false;
                }


                // Now we are inside the correct node, we can extract the variables.
                // We continue to loop over the lines until we find the next node, then we stop.
                inside_node = true;

                size_t k = i + 1;  // Start looking from the next line
                

                while(inside_node) {
                    bool found_keyword = false;

                    if (k >= gc->topoparser.getLineCount()) {
                        LOG_ERR("Reached end of topology file (" + filename + ") while looking for node data");
                    }

                    line = gc->topoparser.getLine(k);
                    string tmpline2 = line;  // Create a copy of the line for parsing
                    string keyword2 = line_obj.extractNextElementFromLine(&line);
                    string value2   = line_obj.extractNextElementFromLine(&line);


                    if(keyword2.compare("ENDNODE") == 0) {
                        found_keyword = true;
                        inside_node = false;
                        break;
                    }

                    if(keyword2.compare("TRAVELTIME") == 0 ) {
                        string outstr = string("TRAVELTIME is not in use anymore, ") +
                            "it has been replaced by TRAVELTIME_HOURS." +
                            "Please check your topology file " + filename;
                        
                        LOG_WARN("Channel::ReadNodeData  nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype));

                        LOG_ERR(outstr);
                    }

                    if(keyword2.compare("DECAY") == 0) {
                        string outstr = string("DECAY is not in use anymore, ") +
                            "it has been replaced by TRAVELTIME_HOURS." +
                            "Please check your topology file " + filename;

                        LOG_WARN("Channel::ReadNodeData  nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype));
                        LOG_ERR(outstr);
                    }

                    if(keyword2.compare("K_TRAVELTIME_HOURS") == 0 ) {
                        found_keyword = true;
                        this->K_traveltime_hours = atof(value2.c_str() );
                    }

                    if(keyword2.compare("N_CASCADE_LINRES") == 0 ) {
                        found_keyword = true;
                        this->num_cascaded_reservoirs = atoi(value2.c_str() );
                    }


                    // Terje Sandø, 06.07.2026
                    // Parse QMIN with seasonal minimum-discharge periods and a per-period economic penalty cost.
                    // Channel QMIN is a pure economic penalty on the channel's OUTLET flow
                    //       
                    //Format: QMIN <nr_periods>
                    //Each following line:
                    //Start date, end date, min. discharge, penalty cost 
                    //DD.MM DD.MM m3s cost
                    if(keyword2.compare("QMIN") == 0) {
                        found_keyword = true;

                        this->qmin.nr_periods = atoi(value2.c_str());

                        if(this->qmin.nr_periods <= 0) {
                            this->qmin_in_use = false;
                        } else {
                            this->qmin_in_use = true;
                            // LOG_ERR("WORK IN PROGRESS, BVM May 2026\nThis functionality has not yet been quality controlled");

                            if(this->qmin.nr_periods > MAX_NUMBER_OF_QMIN_PERIODS) {
                                LOG_WARN("QMIN nr_periods out of bounds: " + std::to_string(this->qmin.nr_periods) + " has to be between 1 and " + std::to_string(MAX_NUMBER_OF_QMIN_PERIODS));
                                LOG_ERR("Check QMIN in topology file " + filename + " for channel " + nodename);
                            }

                            // Reads each period's DD.MM DD.MM line by number.
                            // Just checks the text is long enough for substr() - not a real
                            // date-format check (that happens in Qmin::validatePeriods()).
                            for(int q = 0; q < this->qmin.nr_periods; q++) {
                                line = gc->topoparser.getLine(k+q+1);

                                value = line_obj.extractNextElementFromLine(&line);
                                if(value.length() < 3) {
                                    LOG_WARN("Qmin period " + std::to_string(q+1) + " start date token '"
                                        + value + "' is too short to parse. Expected DD.MM format (e.g. 01.04).");
                                    LOG_ERR("Check QMIN periods in topology file " + filename + " for channel " + nodename + ".");
                                }
                                qmin.timeperiods[q].start_day   = atoi(value.substr(0,2).c_str());
                                qmin.timeperiods[q].start_month = atoi(value.substr(3,2).c_str());
                        
                                value = line_obj.extractNextElementFromLine(&line);
                                if(value.length() < 3) {
                                    LOG_WARN("Qmin period " + std::to_string(q+1) + " end date token '"
                                        + value + "' is too short to parse. Expected DD.MM format (e.g. 31.03).");
                                    LOG_ERR("Check QMIN periods in topology file " + filename + " for channel " + nodename + ".");
                                }
                                qmin.timeperiods[q].end_day   = atoi(value.substr(0,2).c_str());
                                qmin.timeperiods[q].end_month = atoi(value.substr(3,2).c_str());

                                value = line_obj.extractNextElementFromLine(&line);
                                qmin.timeperiods[q].min_discharge = atof(value.c_str());

                                value = line_obj.extractNextElementFromLine(&line);
                                qmin.timeperiods[q].penalty_cost = atof(value.c_str());
                            }

                            // Skip past the period lines just read (k+q+1). 
                            //Otherwise the next iteration misreads them as invalid keywords and hits LOG_ERR below.
                            k += this->qmin.nr_periods;
                        }
                    }

                    if(!found_keyword) {
                        LOG_ERR("ERROR: Invalid keyword in topology file " + filename + " see line: " + tmpline2);
                    }
                    k++;
                }
            }
        }
    }


    return 0;
}
////////////////////////////////////////////////////////////////////////////
int Channel::SetStartState(void) {

    this->casc_reservoirs->setInitialStorage(this->initial_storage_linres_Mm3);

    for(size_t t = 0; t < S->stps; t++ ) {
         S->up_inflow[t]    = 0.0;
    }

    // Not sure if we need this in new routing model..... 
    // Revisit at some time. 
    // for(size_t t = 0; t <  this->traveltime; t++ ) {
    //     waterflow_m3[t]    = init_waterflow_m3[t];
    // }
    return 0;
}
////////////////////////////////////////////////////////////////////////////

int Channel::ReadStateFile(string filename){
    bool found_node = false;
	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;
    size_t tmp_idnr;
    string token;

	myfile.open(filename.c_str() );

	if (!myfile.is_open()) 	{
		LOG_ERR("The statefile " + filename + " could not be found/opened.");
	}

    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {

            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);

            if ( keyword.compare("NODE") == 0 && value.compare("CHANNEL") == 0 ) {
                token = line_obj.extractNextElementFromLine(&line);
                tmp_idnr = atoi(token.c_str() );
                keyword = line_obj.extractNextElementFromLine(&line);

                // NODE CHANNEL 2 VANAROSEN 0.1 1.2 2.3 
                if(tmp_idnr == idnr && keyword == nodename) {
                    found_node = true;
                    for(size_t t = 0; t <  this->num_cascaded_reservoirs; t++ ) {
                        value   = line_obj.extractNextElementFromLine(&line);
                        initial_storage_linres_Mm3.push_back(atof(value.c_str()));
                    }
                }
            }
        }
    }
    myfile.close();

    if(!found_node) {
        LOG_INFO("nodename=" + nodename + " idnr=" + std::to_string(int(idnr)) + " nodetype=CHANNEL");
		LOG_ERR("We could not find node " + nodename + " with idnr=" + std::to_string(int(idnr)) + " in the statefile " + filename);
    }

    return 0;
}
//------------------------------------------------------------------------
int Channel::CheckWaterBalance(Herss *herss_obj) { 

    double start_channel_Mm3 = 0.0;
    double end_channel_Mm3   = 0.0;

    double sum_inflow_Mm3       = 0.0;
    double sum_outflow_Mm3      = 0.0;

    
    for (size_t i = 0; i < this->initial_storage_linres_Mm3.size(); i++) {
        start_channel_Mm3 += this->initial_storage_linres_Mm3[i];
    }

    // Now we go into the cascaded linear reservoirs and gets the end volume sitting in the channel. 
    end_channel_Mm3 = this->casc_reservoirs->totalStorageM3() / 1000000.0; // Convert m3 to Mm3


    for(size_t t = 0; t < this->stps; t++) {
        int current_dt = herss_obj->getDeltaT(t);  // Use variable timestep
        sum_inflow_Mm3 += MACRO_m3s_2_Mm3( (this->S->inflow[t] + this->S->up_inflow[t]) , current_dt);
        sum_outflow_Mm3 += MACRO_m3s_2_Mm3(this->S->tot_outflow[t], current_dt);
    }

    double waterbalance = start_channel_Mm3 + sum_inflow_Mm3 - end_channel_Mm3 - sum_outflow_Mm3;

    if(WATERBALANCE_WARNINGS) {
        LOG_INFO("CHANNEL WATERBALANCE for idnr=" + std::to_string(int(idnr)) + "   nodename=" + nodename);
        LOG_INFO("start_channel_Mm3 = " + std::to_string(start_channel_Mm3));
        LOG_INFO("sum_inflow_Mm3    = " + std::to_string(sum_inflow_Mm3));
        LOG_INFO("sum_outflow_Mm3   = " + std::to_string(sum_outflow_Mm3));
        LOG_INFO("end_channel_Mm3   = " + std::to_string(end_channel_Mm3));
        LOG_INFO("waterbalance      = " + std::to_string(waterbalance));
    }

    if(abs(waterbalance) > 0.0001) {
        LOG_WARN("WATERBALANCE CHANNEL ERROR for idnr=" + std::to_string(int(idnr)) + "   nodename=" + nodename);
        LOG_WARN("start_channel_Mm3 = " + std::to_string(start_channel_Mm3));
        LOG_WARN("sum_inflow_Mm3    = " + std::to_string(sum_inflow_Mm3));
        LOG_WARN("sum_outflow_Mm3   = " + std::to_string(sum_outflow_Mm3));
        LOG_WARN("end_channel_Mm3   = " + std::to_string(end_channel_Mm3));
        LOG_WARN("waterbalance      = " + std::to_string(waterbalance));
        LOG_ERR ("idnr=" + nodename + " " + std::to_string(int(idnr)) + nodename.c_str()  );
    }

    return 0; 
}
//------------------------------------------------------------------------
double Channel::GetStartWater_Mm3(void) {

    double start_channel_Mm3 = 0.0;
    
    for (size_t i = 0; i < this->initial_storage_linres_Mm3.size(); i++) {
        start_channel_Mm3 += this->initial_storage_linres_Mm3[i];
    }

    return start_channel_Mm3; // Mm3
}
//------------------------------------------------------------------------
double Channel::GetEndWater_Mm3(void) {
    return this->casc_reservoirs->totalStorageM3() / 1000000.0; // Convert m3 to Mm3
}
//------------------------------------------------------------------------
int Channel::WriteNodeOutput(GlobalConfig *gc){

    FILE *fp;
    char outfilename [100];
    sprintf (outfilename, "%snode%lu_%s.txt", gc->outputdir.c_str() , (idnr) , nodename.c_str()  );
    char outstr [100];

    if((fp = fopen(  outfilename ,"w"))==NULL) {
        LOG_ERR("Cannot open file " + std::string(outfilename));
    }

    sprintf (outstr, "CHANNEL node %d %s\n", int(idnr), nodename.c_str());
    fprintf(fp, "%s", outstr);

    fprintf(fp, "N_CASCADE_LINRES %d\n", int(this->num_cascaded_reservoirs));
    fprintf(fp, "K_TRAVELTIME_HOURS %.2f\n", this->K_traveltime_hours);

    fprintf(fp, "yyyy mm dd hh [m3/s]    [Mm3]       [m3/s]      [Euro]\n");
    fprintf(fp, "yyyy mm dd hh Up_Inflow Storage_Mm3 tot_outflow Qmin_Cost\n");

    for(size_t t = 0; t < this->stps; t++) {
        fprintf(fp, "%d %d %d %d ", S->year[t], S->month[t], S->day[t], S->hour[t]);
        fprintf(fp, "%.4f %.8f ", S->up_inflow[t], S->channel_storage_Mm3[t]);
        fprintf(fp, "%.4f ", S->tot_outflow[t]);
        fprintf(fp, "%.4f ", S->cost[t]);
        fprintf(fp, "\n");
    }
    fclose(fp);

    return 0;
} 
//------------------------------------------------------------------------
double Channel::GetTunnelFLow(size_t t) {
    printf("ERROR Channel cannot use the function: GetTunnelFLow \n");
    printf("Have you connected a tunnel from a reservoir to a channel? - check input\n");
    printf( "NODE CHANNEL idnr=%d  nodename=%s\n", int(idnr), nodename.c_str()  );
    printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
    exit(EXIT_FAILURE);
}
//////////////////////////////////////////////////////////////////////////////////
int Channel::WriteStateFile(FILE *fp) {
    fprintf (fp, "NODE CHANNEL %d %s ", int(idnr) , nodename.c_str() );
    
    for( size_t s = 0; s < this->num_cascaded_reservoirs; s++) { 
        fprintf(fp, "%.5f ", this->casc_reservoirs->getStorageMm3(s) );
    }
    
    fprintf(fp, "\n");
    return 0;
}
//////////////////////////////////////////////////////////////////////////////////