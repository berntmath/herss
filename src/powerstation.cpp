/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     powerstation.cpp
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

Powerstation::Powerstation(){
    stps = 0;
    dt   = 0;
    nr_points_turb_virkn = 0;
    static_gen_efficiency   = NOT_INIT;
    headlosscoef            = NOT_INIT;
    powstat_masl            = NOT_INIT;
    powstat_min_discharge   = NOT_INIT;
    powstat_max_discharge   = NOT_INIT;
    powstat_startstop       = NOT_INIT;
    init_Power              = NOT_INIT;
}

Powerstation::~Powerstation(){}

int Powerstation::initArrayCurves(void) {
    // Now we initialize the turbine efficiency curve (Turbinvirkningsgrad)
    ac_turbvirkn_curve.nr_pts = this->nr_points_turb_virkn;
    for (int p = 0; p < ac_turbvirkn_curve.nr_pts; p++){
        ac_turbvirkn_curve.x_points[p] = turb_virkn_Q[p];
        ac_turbvirkn_curve.y_points[p] = turb_virkn_psnt[p];
    }
    ac_turbvirkn_curve.initializeArrays();
    return 0;
}
////////////////////////////////////////////////////////////////
int Powerstation::Simulate(size_t t) {
    this->dt     = S->dt;
    this->stps   = S->stps;
    double Q;
    double headloss;
    double Hbrutto;
    double Hnetto;
    double turbine_efficiency;
    double P;
    double Power;
    double income;
    double startstopCost;
    double previous_power = 0.0;

    if( t ==0 ) {
        previous_power = this->init_Power;
    } else {
        previous_power = S->Power[t-1];
    }

    Q = S->up_inflow[t];

    headloss = this->headlosscoef * Q * Q;
    Hbrutto  = ((this->start_of_stp_masl + this->end_of_stp_masl)/2.0 ) - this->powstat_masl;
    Hnetto   = Hbrutto - headloss;
    turbine_efficiency = ac_turbvirkn_curve.x2y(Q)/100.0;

    P = turbine_efficiency * 1000 * GRAVITY * Hnetto * Q;  // Watt
    P = P /1000000.0; // MW
    P = P * static_gen_efficiency; 
    Power = P * dt / 3600.0; // MWh

    if(Q < this->powstat_min_discharge) {
        Power = 0.0;
    }   

    income = Power * S->price[t];

    // Now we check for start and stop costs
    // We penalise when starting and stopping. 
    startstopCost = 0.0;
    
    if(previous_power > 0.001 and Power < 0.001) {
        startstopCost = this->powstat_startstop/2.0;
    }

    if(previous_power < 0.001 and Power > 0.001) {
        startstopCost = this->powstat_startstop/2.0;
    }

    this->ptr_downstream_node->S->up_inflow[t] += Q;
    // Save timeseries 
    S->income[t]           = income;
    S->cost[t]             = startstopCost;
    S->profit[t]           = income - startstopCost;
    S->Hnetto[t]           = Hnetto;
    S->Hbrutto[t]          = Hbrutto;
    S->Power[t]            = Power;
    S->tot_outflow[t]      = Q;
    remaining_available_Mm3 = 0.0;  // The powerstation can never store water.
    return 0;
}
////////////////////////////////////////////////////////////////
int Powerstation::ReadNodeData(string filename) {

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

            if ( keyword.compare("NODE") == 0 && value.compare("PSTATION") == 0 ) {
                token = line_obj.extractNextElementFromLine(&line);
                tmp_idnr = atoi(token.c_str() );
                if(tmp_idnr == idnr) {

                    nodename = line_obj.extractNextElementFromLine(&line);
                    nodetype = NodeType::POWERSTATION;

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("DOWNLINK_IDNR") != 0 ) {
                        cout << "Could not find token DOWNLINK_IDNR in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    downstream_idnr = atoi(value.c_str());

                    if(downstream_idnr  >= 0) {
                        downstream_node_in_use = true;
                    }   
                    
                    getline(myfile, line);
                    // # Turbine efficiency curve [M3s, %]

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("TURBINE_CURVE") != 0 ) {
                        cout << "Could not find token DOWNLINK_IDNR in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    nr_points_turb_virkn = atoi(value.c_str());
                    // TURBINE_CURVE 8
                    for(size_t p = 0; p < nr_points_turb_virkn; p++) {
                        getline(myfile, line);
                        keyword             = line_obj.extractNextElementFromLine(&line);
                        value               = line_obj.extractNextElementFromLine(&line);
                        turb_virkn_Q[p]     = atof(keyword.c_str());
                        turb_virkn_psnt[p]  = atof(value.c_str());
                    }

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("STATIC_GENERATOR_EFFICIENCY") != 0 ) {
                        cout << "Could not find token STATIC_GENERATOR_EFFICIENCY in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    static_gen_efficiency = atof(value.c_str());
                    // STATIC_GENERATOR_EFFICIENCY

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("HEADLOSSCOEF") != 0 ) {
                        cout << "Could not find token HEADLOSSCOEF in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    this->headlosscoef = atof(value.c_str());
                    // HEADLOSSCOEF
                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("POWSTAT_MASL") != 0 ) {
                        cout << "Could not find token POWSTAT_MASL in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    this->powstat_masl = atof(value.c_str());
                    // POWSTAT_MASL

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("POWSTAT_MIN_DISCHARGE") != 0 ) {
                        cout << "Could not find token POWSTAT_MIN_DISCHARGE in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    this->powstat_min_discharge = atof(value.c_str());
                    // POWSTAT_QMIN
                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("POWSTAT_MAX_DISCHARGE") != 0 ) {
                        cout << "Could not find token POWSTAT_MAX_DISCHARGE in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    this->powstat_max_discharge = atof(value.c_str());
                    // POWSTAT_QMAX

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("POWSTAT_STARTSTOP") != 0 ) {
                        cout << "Could not find token POWSTAT_STARTSTOP in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    this->powstat_startstop = atof(value.c_str());
                    // POWSTAT_STARTSTOP - the user of the model needs to specify the start/stop cost for machinery. 

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("LOCAL_ENERGY_EQUIVALENT") != 0 ) {
                        cout << "Could not find token LOCAL_ENERGY_EQUIVALENT in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    this->local_energy_equivalent = atof(value.c_str());
                    // LOCAL_ENERGY_EQUIVALENT

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("AUTO_QMIN") != 0 ) {
                        cout << "Could not find token AUTO_QMIN in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    this->auto_qmin = atof(value.c_str());
                    // AUTO_QMIN -9999

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    value   = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("MAX_ADJUST") != 0 ) {
                        cout << "Could not find token MAX_ADJUST in topologyfile " << filename << " ERROR \n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    max_adjustment_pr_day = atoi(value.c_str());
                    if(max_adjustment_pr_day > -1) {
                        value   = line_obj.extractNextElementFromLine(&line);
                        max_adjustment_cost = atof(value.c_str());
                    }
                }
            }
        }
    }

    myfile.close();
    return 0;
}
///////////////////////////////////////////////////////////////////////////////
int Powerstation::ReadStateFile(string filename){

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
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		exit(EXIT_FAILURE);
	}

    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {
            // Line is not empty and doesn't start with # (hash/pound sign)
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);
            if ( keyword.compare("NODE") == 0 && value.compare("PSTATION") == 0 ) {
                token = line_obj.extractNextElementFromLine(&line);
                tmp_idnr = atoi(token.c_str() );
                keyword = line_obj.extractNextElementFromLine(&line);
               
                if(tmp_idnr == idnr && keyword == nodename) {
                    value   = line_obj.extractNextElementFromLine(&line);
                    // NODE PSTATION
                    init_Power = atof( value.c_str() );
                    found_node = true;
                }
            }
        }
    }
    myfile.close();
    if(!found_node) {
		cout << "There is something wrong in the statefile "<< filename << "\n";
        printf( "Powerstation::ReadStateFile           idnr=%d  nodename=%s\n", int(idnr) , nodename.c_str()  );
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		exit(EXIT_FAILURE);
    }
    return 0;
}
///////////////////////////////////////////////////////////////////////
int Powerstation::CheckWaterBalance(void) {

    double sum_inflow  = 0.0;
    double sum_outflow = 0.0;

    for(size_t t = 0; t < this->stps; t++) {
        sum_inflow += MACRO_m3s_2_Mm3( (this->S->inflow[t] + this->S->up_inflow[t]) , dt);
        sum_outflow += MACRO_m3s_2_Mm3(this->S->tot_outflow[t], dt);
    }

    double waterbalance = sum_inflow - sum_outflow;

    if(abs(waterbalance) > 0.0001) {
        printf( "WATERBALANCE POWERSTATION idnr=%d  nodename=%s\n", int(idnr), nodename.c_str()  );
        sum_inflow  = 0.0;
        sum_outflow = 0.0;
        for(size_t t = 0; t < this->stps; t++) {
            sum_inflow += MACRO_m3s_2_Mm3( (this->S->inflow[t] + this->S->up_inflow[t]) , dt);
            sum_outflow += MACRO_m3s_2_Mm3(this->S->tot_outflow[t], dt);

            printf("%d %d %d %d %d %.5f %.5f %.5f  action %.5f  sum_in= %.6f  sum_out= %.6f diff= %.6f \n", int(t), 
            S->year[t], S->month[t], S->day[t], S->hour[t], 
            MACRO_m3s_2_Mm3(this->S->inflow[t], dt), 
            MACRO_m3s_2_Mm3(this->S->up_inflow[t], dt ),
            MACRO_m3s_2_Mm3(this->S->tot_outflow[t], dt), S->action[t] ,
            sum_inflow, sum_outflow , sum_inflow - sum_outflow );
    }

        printf( "WATERBALANCE POWERSTATION idnr=%d   nodename=%s\n", int(idnr), nodename.c_str()  );
        printf("sum_inflow    = %.6f\n", sum_inflow);
        printf("sum_outflow   = %.6f\n", sum_outflow);
        printf("waterbalance  = %.6f\n", waterbalance);
        printf( "idnr=%d  nodename=%s\n", int(idnr), nodename.c_str()  );
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
	    exit(EXIT_FAILURE);
    }

    return 0; 
}
///////////////////////////////////////////////////////////////////////
double Powerstation::GetStartWater_Mm3(void) {  // Powerstation have no storage. 
    return 0.0;
}
//------------------------------------------------------------------------
double Powerstation::GetEndWater_Mm3(void) {
    return 0.0;
}
//------------------------------------------------------------------------
int Powerstation::WriteNodeOutput(GlobalConfig *gc) {

    FILE *fp;
    char outfilename [100];
    char outstr [100];

    sprintf (outfilename, "%snode%lu_%s.txt", gc->outputdir.c_str() , (idnr) , nodename.c_str()  );

    if((fp = fopen(  outfilename ,"w"))==NULL) {
        printf("Cannot open file %s \n", outfilename);
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        exit(EXIT_FAILURE);
    }

    sprintf (outstr, "POWERSTATION node %d %s\n", int(idnr) , nodename.c_str()  );
    fprintf(fp, "%s", outstr);
    fprintf(fp, "init_Power = %.5f\n", this->init_Power);

    fprintf(fp, "yyyy mm dd hh [m3/s]    [Euro/MWh] [fr]   [m3/s]      [m3/s]    [Euro] [Euro]        [m] [m]    [MWh] [Euro]\n");
    fprintf(fp, "yyyy mm dd hh Up_Inflow Price      Action tot_outflow auto_qmin income startstopCost Hnetto Hbrutto Power adjust_cost\n");

    for(size_t t = 0; t < this->stps; t++) {
        fprintf(fp, "%d %d %d %d ", S->year[t], S->month[t], S->day[t], S->hour[t]);
        fprintf(fp, "%.4f %.4f %.4f ", S->up_inflow[t] , S->price[t], S->action[t] );
        fprintf(fp, "%.4f ", S->tot_outflow[t]);
        fprintf(fp, "%.4f ", S->auto_qmin_m3s[t]);
        fprintf(fp, "%.4f ", S->income[t]);
        fprintf(fp, "%.4f ", S->cost[t] - S->adjust_cost[t]);
        fprintf(fp, "%.4f ", S->Hnetto[t]);
        fprintf(fp, "%.4f ", S->Hbrutto[t]);
        fprintf(fp, "%.4f ", S->Power[t]);
        fprintf(fp, "%.4f ", S->adjust_cost[t]);
        fprintf(fp, "\n");
    }

    fclose(fp);
    return 0;
}
//////////////////////////////////////////////////////////////////////////////////
double Powerstation::GetTunnelFLow(size_t t) {

    // WORK IN PROGRESS
    double flow = 0.0;
       
    S->auto_qmin_m3s[t] = 0.0; 

    if(S->action[t] < -0.000001) {
        printf("ERROR: action is negative \n");
        printf ("NODE PSTATION %d %s action= %.5f\n", int(idnr), nodename.c_str(), this->S->action[t]);
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        exit(EXIT_FAILURE);
    }

    if(S->action[t] < 0.01) {
        flow = 0.0;
    } else { 
        flow =  this->powstat_min_discharge + S->action[t] * (this->powstat_max_discharge - this->powstat_min_discharge);  // m3/s
    } 

    // Here we check the auto qmin water release in downstream connected Powerstation
    if(auto_qmin > 0.0 && flow < auto_qmin) {
        flow = auto_qmin;
        S->auto_qmin_m3s[t] = flow; 
    }   

    double Q_Mm3 = MACRO_m3s_2_Mm3(flow, S->dt);

    // We shut down production and auto_qmin if the reservoir is dry or water level below tunnel 
    if(Q_Mm3 > up_res_Mm3) {
        flow = 0.0;
    }

    return flow;
}
//////////////////////////////////////////////////////////////////////////////////
int Powerstation::WriteStateFile(FILE *fp) {
    fprintf (fp, "NODE PSTATION %d %s %.5f\n", int(idnr), nodename.c_str(), this->S->Power[S->stps-1]);
    return 0;
}
//////////////////////////////////////////////////////////////////////////////////
double Powerstation::CalcAdjustmenCosts(void) {

    double prev_power = init_Power;
    double diff;
    int nr_changes_pr_day = 0;
    double sum_cost = 0.0;
    for(size_t t = 0; t < S->stps; t++) {
        diff = abs(prev_power - S->Power[t]);

        // We define a change of 0.1 MW as significant
        if(diff > 0.1) {
            nr_changes_pr_day++;
        }

        if(t > 2) {
            if((t+1) % 24 == 0) {
                if(nr_changes_pr_day > this->max_adjustment_pr_day) {
                    sum_cost += this->max_adjustment_cost;
                    S->adjust_cost[t]  = this->max_adjustment_cost;
                    S->cost[t]        += this->max_adjustment_cost;
                    S->profit[t]      -= this->max_adjustment_cost;
                } 
                nr_changes_pr_day = 0;
            }
        }
        prev_power = S->Power[t];
    }
    return sum_cost;
}
//////////////////////////////////////////////////////////////////////////////////
