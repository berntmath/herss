/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     reservoir.cpp                                                        
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

Reservoir::Reservoir(){
    reservoir_init_fr              = NOT_INIT;
    reservoir_init_masl            = NOT_INIT;
    reservoir_init_Mm3             = NOT_INIT;

    res_HRW                        = NOT_INIT;
    filling_at_hrw_Mm3             = NOT_INIT;
    filling_at_hatchlevel          = NOT_INIT;
    res_LRW                        = NOT_INIT;
    filling_at_lrw_Mm3             = NOT_INIT;
    res_penalty                    = NOT_INIT;
    res_Mm3                        = NOT_INIT;
    res_masl                       = NOT_INIT;
    res_fr                         = NOT_INIT;
    nr_points_res_curve            = 0;
    nr_points_ovefl_curve          = 0;
    outlet_hatch_in_use            = false;
    outlet_tunnel_in_use           = false;

    minQ_hatch                     = NOT_INIT;
    maxQ_hatch                     = NOT_INIT;
    hatch_masl                     = NOT_INIT;

}

Reservoir::~Reservoir(){}

int Reservoir::initArrayCurves(void) {
    
    // RESERVOIR CURVE    X=MASL  ,  Y = Mm3
    ac_res_masl_2_Mm3.nr_pts = this->nr_points_res_curve;
    for (int p = 0; p < ac_res_masl_2_Mm3.nr_pts; p++){
        ac_res_masl_2_Mm3.x_points[p] = res_curve_masl[p];
        ac_res_masl_2_Mm3.y_points[p] = res_curve_Mm3[p];
    }
    ac_res_masl_2_Mm3.initializeArrays();

    //ArrayCurve ac_res_Mm3_2_masl;
    ac_res_Mm3_2_masl.nr_pts = nr_points_res_curve;
    for (int p = 0; p < ac_res_Mm3_2_masl.nr_pts; p++){
        ac_res_Mm3_2_masl.x_points[p] = res_curve_Mm3[p];
        ac_res_Mm3_2_masl.y_points[p] = res_curve_masl[p];
    }
    ac_res_Mm3_2_masl.initializeArrays();
    //---------------------------------------------------------------------------

    //--------------------------------------------------------------------
    // Specify OVERFLOW_CURVE and number of points. If not used specify with "-9999"
    ac_ovefl_masl_2_m3s.nr_pts = this->nr_points_ovefl_curve;
    for (int p = 0; p < ac_ovefl_masl_2_m3s.nr_pts; p++){
        ac_ovefl_masl_2_m3s.x_points[p] = ovefl_curve_masl[p];
        ac_ovefl_masl_2_m3s.y_points[p] = ovefl_curve_m3s[p];
    }
    ac_ovefl_masl_2_m3s.initializeArrays();

    //ArrayCurve ac_ovefl_m3s_2_masl;
    ac_ovefl_m3s_2_masl.nr_pts = this->nr_points_ovefl_curve;
    for (int p = 0; p < ac_ovefl_m3s_2_masl.nr_pts; p++){
        ac_ovefl_m3s_2_masl.x_points[p] = ovefl_curve_m3s[p];
        ac_ovefl_m3s_2_masl.y_points[p] = ovefl_curve_masl[p];
    }
    ac_ovefl_m3s_2_masl.initializeArrays();

    return 0;
}
////////////////////////////////////////////////////////////////
double Reservoir::CalcOverflow() {

    double masl_start_overflow;
    double overflow_m3s;
    double overflow_Mm3;
    double current_filling;
    double max_overflow;

    overflow_m3s = 0.0;
    overflow_Mm3 = 0.0;

    masl_start_overflow = this->ovefl_curve_masl[0];

    // The bottom point in the overflow curve is usually the same as HRW, but not always.
    if(this->res_masl > masl_start_overflow) {
        overflow_m3s = ac_ovefl_masl_2_m3s.x2y(this->res_masl);
        overflow_Mm3 = MACRO_m3s_2_Mm3(overflow_m3s,dt);

        // We cannot allow the overflow to drain more than down to the top of the dam ( for now we assume HRW).
        // This has to do with numerical stability using large timesteps.
        // When the reservoir level is close to one of the points defining the reservoir- or overflow curve, we will get wrong answer
        // when the reservoirlevel drops from one point to below another one.
        // Maybe we need to use the old way of looping over all the points defining the reservoir and overflow curves.
        // WORK IN PROGRESS
        current_filling  = this->res_Mm3; 

        max_overflow = current_filling - filling_at_hrw_Mm3;

        if(overflow_Mm3 > max_overflow){
            overflow_Mm3 = max_overflow;
        }

        if(overflow_Mm3 < 0.0) {
            printf("Negative overflow is not allowed \n");
            printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
            exit(0);
            return -9;
        }
    }
    return overflow_Mm3;
}
////////////////////////////////////////////////////////////////
void Reservoir::InitReservoir(void) {

    if(this->nr_points_res_curve < 2) {
        printf("Reservoir curve not initialized\n");
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
        exit(EXIT_FAILURE);
    }
    
    if(this->reservoir_init_fr < -1.0) {
        printf("ERROR Something wrong with reservoir_init_fr=%.4f \n", this->reservoir_init_fr);
        printf("Leaving - BYE\n");
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
        exit(EXIT_FAILURE);
    }

    filling_at_lrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_LRW);
    filling_at_hrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_HRW);

    res_Mm3 = filling_at_lrw_Mm3 + reservoir_init_fr * (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);

    // Note that reservoir content is the water between HRW and LRW.
    // That volume cannot be used directly to calculate the filling in meters above sea level.
    res_masl = ac_res_Mm3_2_masl.x2y(res_Mm3);

    // Calculate filling at hatch level here so do it only once, and not every timestep.
    if(outlet_hatch_in_use) {
        filling_at_hatchlevel = ac_res_masl_2_Mm3.x2y(this->hatch_masl);
    }
}
////////////////////////////////////////////////////////////////
int Reservoir::Simulate(size_t t) {
    
    // Upstream inflow has already been set to zero or adjusted earlier.
    this->dt     = S->dt;
    this->stps   = S->stps;

    double hatchflow_Mm3;
    double tunnelflow_Mm3;
    double overflow_Mm3;
    double outlet_auto_qmin_flow_Mm3;
    double total_inflow_Mm3;

    double max_hatchflow;
    double current_filling;
    
    current_filling = -999.0; // To void warning

    #ifdef HERSS_DEBUG_ALL
        if( S->inflow[t] < 0.0 || S->inflow[t] > 5000.0) {
            printf("Reservoir::Simulate() There is something wrong with inflow =%.3f\n", S->inflow[t]);
            printf("Node idnr = %d   nodename = %s", int(this->idnr) , this->nodename.c_str() );
            printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
            exit(EXIT_FAILURE);
        }

        if( S->price[t] < 0.0 || S->price[t] > 5000.0) {
            printf("Reservoir::Simulate() There is something wrong with price =%.3f\n", S->price[t]);
            printf("Node idnr = %d   nodename = %s", int(this->idnr) , this->nodename.c_str() );
            printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
            exit(EXIT_FAILURE);
        }
    #endif

    total_inflow_Mm3 = S->inflow[t]+S->up_inflow[t];
    total_inflow_Mm3 = MACRO_m3s_2_Mm3(total_inflow_Mm3,dt);

    // Add local inflow
    this->res_Mm3 += MACRO_m3s_2_Mm3(S->inflow[t],this->dt);    // Mm3

    S->sum_local_inflow_Mm3 += MACRO_m3s_2_Mm3(S->inflow[t],this->dt);    // Mm3

    // Add upstream inflow
    this->res_Mm3 += MACRO_m3s_2_Mm3(S->up_inflow[t],this->dt);  // Mm3   All initialized to zero 

    // Update filling height
    this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);

    //---------------------------------------------------------------------
    // We have maximum four outlets. Tunnel, Hatch, auto_qmin_hatch, Overflow
    // We start with TUNNEL
    // CASE A: Normal production.
    // CASE B: Auto_Qmin. 
    // CASE C: Completely empty reservoir, we shut down both A and B. 

    tunnelflow_Mm3 = 0.0;
    if(outlet_tunnel_in_use) {
        ptr_downstream_node_tunnel->start_of_stp_masl = this->res_masl;
        ptr_downstream_node_tunnel->up_res_Mm3 = this->res_Mm3;
        double tunnelf_m3s = ptr_downstream_node_tunnel->GetTunnelFLow(t);
        ptr_downstream_node_tunnel->S->up_inflow[t] = tunnelf_m3s;
        tunnelflow_Mm3 = MACRO_m3s_2_Mm3(tunnelf_m3s ,this->dt);  // Mm3   All initialized to zero
    }

    this->res_Mm3 -= tunnelflow_Mm3;
    this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);

    //-------------------------------------------------------------------
    // OUTLET HATCH, typically to channel 
    hatchflow_Mm3 = 0.0;
    if(outlet_hatch_in_use){
        if(this->res_masl > this->hatch_masl ) {
            // Some places we need to release water regardless of the actions set 
            // This can be done by setting minQ_hatch to a low level.
            hatchflow_Mm3 = this->minQ_hatch + S->action[t]*(this->maxQ_hatch - this->minQ_hatch);
            hatchflow_Mm3 = MACRO_m3s_2_Mm3(hatchflow_Mm3, dt);  // Mm3
            current_filling = ac_res_masl_2_Mm3.x2y(this->res_masl);
            max_hatchflow = current_filling - filling_at_hatchlevel;
            if (hatchflow_Mm3 > max_hatchflow) {
                hatchflow_Mm3 = max_hatchflow;
            }
        }
        ptr_downstream_node_hatch->S->up_inflow[t] += MACRO_Mm3_2_m3s(hatchflow_Mm3, dt);  // m3/s
    }
    this->res_Mm3 -= hatchflow_Mm3;

    // Update the reservoir masl
    this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);

    
    // AUTO HATCH 
    outlet_auto_qmin_flow_Mm3 = 0.0;
    // Here we simulate the effect of an automatic water release set by the operators.
    if(outlet_auto_qmin_in_use){
        double void_cost;
        outlet_auto_qmin_flow_Mm3 = this->qmin.calcQminRequirement(S->year[t], S->month[t], S->day[t],  &void_cost  );  // m3/s
        this->ptr_downstream_node_auto_qmin->S->up_inflow[t] += outlet_auto_qmin_flow_Mm3;
    }

    outlet_auto_qmin_flow_Mm3 = MACRO_m3s_2_Mm3(outlet_auto_qmin_flow_Mm3, dt);  // Mm3
    this->res_Mm3 -= outlet_auto_qmin_flow_Mm3;
    // Update the reservoir masl
    this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);

    // Overflow is always used
    overflow_Mm3 = this->CalcOverflow();
    ptr_downstream_node_overflow->S->up_inflow[t] += MACRO_Mm3_2_m3s(overflow_Mm3, dt);  // m3/s

    this->res_Mm3 -= overflow_Mm3;
    this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);

    cost_lrw = 0.0;
    if( this->res_masl  < this->res_LRW) {
        cost_lrw = this->res_penalty*dt/3600;
        printf( "LRW COST  :  idnr=%d  nodename=%s\n", int(idnr) , nodename.c_str()  );
        printf( "res_masl = %.3f     res_LRW= %.3f\n", this->res_masl , this->res_LRW);
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
    }

    if(outlet_tunnel_in_use) {
        ptr_downstream_node_tunnel->end_of_stp_masl = this->res_masl;
    }

    // Fractional_filling
    double fract_filling = (res_Mm3  - filling_at_lrw_Mm3) / (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);

    remaining_available_Mm3 = res_Mm3  - filling_at_lrw_Mm3;
    if(remaining_available_Mm3 < 0.0) {
        remaining_available_Mm3 = 0.0; // Used to calculate remaining available energy in system. Cannot be negative.
    }

    if(fract_filling < -1.0) {
        printf("ERROR\n");
        printf("There is obviously something wrong with the fract_filling calculations => NON PHYSICAL SITUATIONS \n");
        printf( "idnr=%d  nodename=%s   timestep=%lu \n", int(idnr) , nodename.c_str() , t );
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        printf("current_filling     = %.5f\n", res_Mm3);
        printf("filling_at_lrw_Mm3  = %.5f\n", filling_at_lrw_Mm3 );
        printf("filling_at_hrw_Mm3  = %.5f\n", filling_at_hrw_Mm3);
        printf("fract_filling       = %.5f\n", fract_filling);
        exit(EXIT_FAILURE);
    }

    // Transfer timeseries 
    S->tot_inflow[t]   = MACRO_Mm3_2_m3s(total_inflow_Mm3,dt);
    S->res_Mm3[t]      = res_Mm3;
    S->res_masl[t]     = res_masl;
    S->res_fr[t]       = fract_filling;
    S->overflow_Mm3[t] = overflow_Mm3;
    S->cost[t]         = cost_lrw;

    double tot_out     = hatchflow_Mm3 + tunnelflow_Mm3 + overflow_Mm3 + outlet_auto_qmin_flow_Mm3;
    S->tot_outflow[t]  = MACRO_Mm3_2_m3s(tot_out, dt);
    S->tunnelflow_m3s[t]  = MACRO_Mm3_2_m3s(tunnelflow_Mm3, dt);
    S->hatchflow_m3s[t]   = MACRO_Mm3_2_m3s(hatchflow_Mm3, dt);
    S->overflow_m3s[t]    = MACRO_Mm3_2_m3s(overflow_Mm3, dt);
    S->auto_qmin_m3s[t]   = MACRO_Mm3_2_m3s(outlet_auto_qmin_flow_Mm3, dt);
    S->income[t]  = 0.0;  // No income in reservoirs 

    return 0;
}
////////////////////////////////////////////////////////////////
int Reservoir::ReadNodeData(string filename){

	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;
    size_t tmp_idnr;
    string token;

	myfile.open(filename.c_str() );

	if (!myfile.is_open()) 	{
		cout << "The topologyfile " << filename << " could not be found/opened. \n";
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		exit(EXIT_FAILURE);
	}

    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {
            // Line is not empty and doesn't start with # (hash/pound sign)
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);

            if ( keyword.compare("NODE") == 0 && value.compare("RESERVOIR") == 0 ) {

                token = line_obj.extractNextElementFromLine(&line);
                tmp_idnr = atoi(token.c_str() );

                if(tmp_idnr == idnr) {
                    nodename = line_obj.extractNextElementFromLine(&line);
                    nodetype = NodeType::RESERVOIR;

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("HRW") != 0 ) {
                        cout << "Could not find token HRW in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    this->res_HRW = atof(value.c_str() );

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("LRW") != 0 ) {
                        cout << "Could not find token LRW in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    this->res_LRW = atof(value.c_str() );

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("RES_PENALTY") != 0 ) {
                        cout << "Could not find token RES_PENALTY in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    this->res_penalty = atof(value.c_str() );
                    // RES_PENALTY

                    getline(myfile, line);  // Scip one line with comments. 

                    getline(myfile, line);  // RESERVOIR_CURVE 6
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("RESERVOIR_CURVE") != 0 ) {
                        cout << "Could not find token RESERVOIR_CURVE in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    this->nr_points_res_curve = atoi(value.c_str() );

                    if( nr_points_res_curve > MAX_NR_POINTS_CURVE) {
                        cout << "nr_points_res_curve > MAX_NR_POINTS_CURVE " << endl;
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }

                    for(size_t p = 0; p < nr_points_res_curve; p++) {
                        getline(myfile, line);
                        keyword = line_obj.extractNextElementFromLine(&line);
                        value   = line_obj.extractNextElementFromLine(&line);
                        res_curve_masl[p] = atof(keyword.c_str());
                        res_curve_Mm3[p]  = atof(value.c_str());
                    }

                    getline(myfile, line);  // Scip line with comments.

                    // # Overflow curve, points, downstream idnr   [masl, m3s]
                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    token   = line_obj.extractNextElementFromLine(&line);

                    if ( keyword.compare("OVERFLOW_CURVE") != 0 ) {
                        cout << "Could not find token OVERFLOW_CURVE in topologyfile " << filename << " ERROR \n";
                        cout << keyword << " " << value << endl; 
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }

                    nr_points_ovefl_curve = atoi(value.c_str());
                    if( nr_points_ovefl_curve > MAX_NR_POINTS_CURVE) {
                        cout << "nr_points_ovefl_curve > MAX_NR_POINTS_CURVE " << endl;
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }

                    downstream_idnr_overflow = atoi(token.c_str());
                    this->outlet_overflow_in_use = true;

                    for(size_t p = 0; p < nr_points_ovefl_curve; p++) {
                        getline(myfile, line);
                        keyword = line_obj.extractNextElementFromLine(&line);
                        value   = line_obj.extractNextElementFromLine(&line);
                        ovefl_curve_masl[p] = atof(keyword.c_str());
                        ovefl_curve_m3s[p]  = atof(value.c_str());
                    }

                    getline(myfile, line);  // Scip line with comments. 

                    // # Outlet hatch downstream_nodeid, qmin_hatch, qmax_hatch, max_nr_adjustments_pr_day, penalty, hatch_masl
                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);

                    if ( keyword.compare("OUTLET_HATCH") != 0) {
                        cout << "Could not find the keyword OUTLET_HATCH in file " << filename << " something is wrong\n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    
                    // # Outlet hatch downstream_nodeid, qmin_hatch, qmax_hatch, hatch_masl
                    if (atoi(value.c_str()) > -1 ) { 
                        downstream_idnr_hatch = atoi(value.c_str());
                        outlet_hatch_in_use            = true;
                        downstream_node_in_use         = true;
                        value      = line_obj.extractNextElementFromLine(&line);
                        minQ_hatch = atof(value.c_str());
                        value      = line_obj.extractNextElementFromLine(&line);
                        maxQ_hatch = atof(value.c_str());
                        value      = line_obj.extractNextElementFromLine(&line);
                        hatch_masl = atof(value.c_str());
                    }

                    // OUTLET_TUNNEL -9
                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    downstream_idnr_tunnel = atoi(value.c_str()  );

                    if(downstream_idnr_tunnel >=0) {
                        outlet_tunnel_in_use           = true;
                        downstream_node_in_use         = true;
                    }

                    // OUTLET_AUTO_QMIN -9999
                    getline(myfile, line);
                    
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);

                    if ( keyword.compare("OUTLET_AUTO_QMIN") != 0) {
                        cout << "Could not find the keyword OUTLET_AUTO_QMIN in file " << filename << " something is wrong\n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }

                    // Number of timeperiods and downstream node idnr
                    // OUTLET_AUTO_QMIN 2 4
                    // 01.10 30.04	5.0
                    // 01.05 30.09	10.5
                    outlet_auto_qmin_in_use = false;
                    if(atoi(value.c_str() ) >= 0) { 
                        outlet_auto_qmin_in_use = true;
                        this->qmin.nr_periods = atoi(value.c_str());
                        // Downstream node
                        value   = line_obj.extractNextElementFromLine(&line);
                        this->downstream_idnr_auto_qmin = atoi(value.c_str());

                        // Now we read in the qmin periods (MAXIMUM 5)
                        for(int q = 0; q < this->qmin.nr_periods; q++) {
                            getline(myfile, line);
                            value   = line_obj.extractNextElementFromLine(&line);
                            qmin.timeperiods[q].start_day = atoi(value.substr(0,2).c_str() );
                            qmin.timeperiods[q].start_month  = atoi(value.substr(3,2).c_str() );
                        
                            value   = line_obj.extractNextElementFromLine(&line);
                            qmin.timeperiods[q].end_day = atoi(value.substr(0,2).c_str() );
                            qmin.timeperiods[q].end_month  = atoi(value.substr(3,2).c_str() );

                            value   = line_obj.extractNextElementFromLine(&line);
                            qmin.timeperiods[q].min_discharge = atof(value.c_str() );
                            qmin.timeperiods[q].penalty_cost = 0.0;  // This is automatic water release. We check actual qmin in channels. 
                        }
                    }
                }
            }
        }
    }
    myfile.close();

    // We need to set the downstream_idnr
    // We always choose Powerstation node if it exists.
    // Every Reservoir has overflow so if we dont have tunnel we set to overflow node. 
    if(outlet_overflow_in_use) { 
        downstream_idnr = downstream_idnr_overflow;
        downstream_node_in_use = true;
    }
    if(outlet_tunnel_in_use) {
        downstream_idnr = downstream_idnr_tunnel;
        downstream_node_in_use = true;
    }
    return 0;
}
//------------------------------------------------------------------------
int Reservoir::ReadStateFile(string filename){

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
		cout << "The statefile " << filename << " could not be found/opened. \n";
        printf("file: %s  linenr: %d  function %s \n", __FILE__ , __LINE__, __FUNCTION__ );
		exit(EXIT_FAILURE);
	}

    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {
            // Line is not empty and doesn't start with # (hash/pound sign)
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);
            if ( keyword.compare("NODE") == 0 && value.compare("RESERVOIR") == 0 ) {
                token = line_obj.extractNextElementFromLine(&line);
                tmp_idnr = atoi(token.c_str() );
                keyword = line_obj.extractNextElementFromLine(&line);
                if(tmp_idnr == idnr && keyword == nodename) {
                    // NODE RESERVOIR
                    value   = line_obj.extractNextElementFromLine(&line);
                    this->reservoir_init_fr = atof( value.c_str() );
                    found_node = true;
                }
            }
        }
    }

    if(!found_node) {
		cout << "There is something wrong in the statefile "<< filename << "\n";
        printf( "Reservoir::ReadStateFile           idnr=%d  nodename=%s\n", int(idnr) , nodename.c_str()  );
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		exit(EXIT_FAILURE);
    }

    myfile.close();
    return 0;
}
//------------------------------------------------------------------------
int Reservoir::CheckWaterBalance(void) { 

    // Starting water volume.
    filling_at_lrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_LRW);
    filling_at_hrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_HRW);
    double start_res_Mm3 = filling_at_lrw_Mm3 + reservoir_init_fr * (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);
    double sum_inflow  = 0.0;
    double sum_outflow = 0.0;

    for(size_t t = 0; t < this->stps; t++) {
        sum_inflow += MACRO_m3s_2_Mm3( (this->S->inflow[t] + this->S->up_inflow[t]) , dt);
        sum_outflow += MACRO_m3s_2_Mm3(this->S->tot_outflow[t], dt);
    }

    // Ending water volume
    double end_res_Mm3 = this->res_Mm3;
    double waterbalance = start_res_Mm3 + sum_inflow - end_res_Mm3 - sum_outflow;

    if(abs(waterbalance) > 0.0001) {
        printf( "---------------------------\n" );
        printf( "WATERBALANCE for idnr=%d  nodename=%s\n", int(idnr), nodename.c_str()  );
        printf("start_res_Mm3 = %.6f\n", start_res_Mm3);
        printf("sum_inflow    = %.6f\n", sum_inflow);
        printf("sum_outflow   = %.6f\n", sum_outflow);
        printf("end_res_Mm3   = %.6f\n", end_res_Mm3);
        printf("waterbalance  = %.6f\n", waterbalance);
        printf( "idnr=%d   nodename=%s\n", int(idnr) , nodename.c_str()  );
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        printf( "---------------------------\n" );
	    exit(EXIT_FAILURE);
    }
    return 0; 
}
//------------------------------------------------------------------------
double Reservoir::GetStartWater_Mm3(void) {
    // Starting water volume.
    filling_at_lrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_LRW);
    filling_at_hrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_HRW);
    double start_res_Mm3 = filling_at_lrw_Mm3 + reservoir_init_fr * (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);
    return start_res_Mm3;
}
//------------------------------------------------------------------------
double Reservoir::GetEndWater_Mm3(void) {
    // Ending water volume
    return this->res_Mm3;
}
//------------------------------------------------------------------------
int Reservoir::WriteNodeOutput(GlobalConfig *gc){

    FILE *fp;
    char outfilename [100];
    sprintf (outfilename, "%snode%lu_%s.txt", gc->outputdir.c_str() , (idnr) , nodename.c_str()  );

    char outstr [100];

    if((fp = fopen(  outfilename ,"w"))==NULL) {
        printf("Cannot open file %s \n", outfilename);
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        exit(EXIT_FAILURE);
    }

    sprintf (outstr, "RESERVOIR node %d %s\n", int(idnr), nodename.c_str()  );
    fprintf(fp, "%s", outstr);
    fprintf(fp, "reservoir_init_fr= %.5f\n", this->reservoir_init_fr);

    fprintf(fp, "yyyy mm dd hh [m3/s] [Euro/MWh] [fr] [m3/s] [Mm3] [masl] [fr] [Euro]         [m3/s]     [m3/s]    [m3/s]   [m3/s]    [m3/s] \n");
    fprintf(fp, "yyyy mm dd hh Inflow Price Action Up_Inflow Res_Mm3 Res_masl Res_fr lrw_cost tunnelflow hatchflow overflow auto_qmin tot_outflow\n");

    for(size_t t = 0; t < this->stps; t++) {
        fprintf(fp, "%d %d %d %d ", S->year[t], S->month[t], S->day[t], S->hour[t]);
        fprintf(fp, "%.4f %.4f %.4f ", S->inflow[t] , S->price[t], S->action[t] );
        fprintf(fp, "%.4f ", S->up_inflow[t]);
        fprintf(fp, "%.4f %.4f %.4f ", S->res_Mm3[t] , S->res_masl[t], S->res_fr[t] );
        fprintf(fp, "%.4f ", S->cost[t]);
        fprintf(fp, "%.4f %.4f %.4f %.4f ",  S->tunnelflow_m3s[t], S->hatchflow_m3s[t], S->overflow_m3s[t] , S->auto_qmin_m3s[t]);
        fprintf(fp, "%.4f ", S->tot_outflow[t]);
        fprintf(fp, "\n");
    }
    fclose(fp);
    return 0;
}
/////////////////////////////////////////////////////////////////////////
double Reservoir::GetTunnelFLow(size_t t) {
    printf("ERROR reservoir cannot use this function. \n");
    printf ("NODE RESERVOIR %d %s\n", int(idnr), nodename.c_str());
    printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
    exit(EXIT_FAILURE);
}
/////////////////////////////////////////////////////////////////////////
int Reservoir::WriteStateFile(FILE *fp) {
    // # NODE RESERVOIR IDNR NAME INIT_RES_FR
    fprintf (fp, "NODE RESERVOIR %d %s %.5f\n", int(idnr), nodename.c_str() , this->S->res_fr[S->stps-1] );
     return 0; 
}
/////////////////////////////////////////////////////////////////////////
