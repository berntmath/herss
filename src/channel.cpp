/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     channel.cpp
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

Channel::Channel(){
    traveltime         = NOT_INIT;
    decay              = NOT_INIT;
    for(size_t t = 0; t < MAX_TRAVELTIME_HOURS; t++) {
        waterflow_m3[t]        = NOT_INIT;
        init_waterflow_m3[t]   = NOT_INIT;
    }
}

Channel::~Channel(){}

int Channel::initArrayCurves(void) {
    return 0;
}

//----------------------------------------------------------------------
void Channel::PrintChannelWater(void){
    printf ("NODE CHANNEL %d %s\n", int(idnr) , nodename.c_str() );
    for(size_t t = 0; t <  this->traveltime; t++ ) {  
        printf("waterflow_m3[%lu] = %.5f\n", t, waterflow_m3[t]);
    }
}
//----------------------------------------------------------------------


int Channel::Simulate(size_t t) {

    double sum_storage_m3;
    this->dt     = S->dt;
    this->stps   = S->stps;

    double out[MAX_TRAVELTIME_HOURS];  // Helpers 
    double in[MAX_TRAVELTIME_HOURS];   // Helpers
    S->channel_storage_Mm3[t] = 0.0; // To void warnings. 

    if(this->traveltime < 0) {
        printf("CHANNEL   traveltime < 0   ERROR\n");
        printf("CHANNEL     idnr=%d   nodename=%s\n", int(idnr) , nodename.c_str()  );
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		exit(EXIT_FAILURE);
    }

    // We have to cases.  A: no storage or decay. B: Storage and decay. 
    if(this->traveltime == 0) {
        sum_storage_m3 = 0.0;
        waterflow_m3[0] = 0.0;
        S->tot_outflow[t] = S->up_inflow[t];

        if(this->downstream_node_in_use) {
            this->ptr_downstream_node->S->up_inflow[t] += S->tot_outflow[t];
        }

        S->channel_storage_Mm3[t] = 0.0;

    } else {

        sum_storage_m3 = 0.0;
        for(size_t s = 0; s <  this->traveltime; s++ ) {  
            sum_storage_m3 += waterflow_m3[s];  
        }
    
        S->tot_outflow[t] = waterflow_m3[traveltime-1]*decay/S->dt; // m3/s

        // Calculate the incoming water and do not update yet. 
        for(size_t s = 0; s < traveltime; s++) {
            in[s] = S->up_inflow[t] * dt;
            if(s > 0) {
                in[s] = waterflow_m3[s-1] * decay;
            }
        }

        // Calculate the outgoing water and do not update yet. 
        for(size_t s = 0; s < traveltime; s++) {
            out[s] = waterflow_m3[s] * decay;
        }

        // Now we update 
        for(size_t s = 0; s < traveltime; s++) {
            waterflow_m3[s]  = waterflow_m3[s] + in[s] - out[s];
        }

        if(this->downstream_node_in_use) {
            this->ptr_downstream_node->S->up_inflow[t] += S->tot_outflow[t];
        }

        sum_storage_m3 = 0.0;
        for(size_t s = 0; s <  this->traveltime; s++ ) {
            sum_storage_m3 += waterflow_m3[s];
        }

        S->channel_storage_Mm3[t] = sum_storage_m3 / 1000000.0;  // Mm3
    }

    S->cost_qmin[t]  = 0.0;
    S->income[t]  = 0.0;  // No income in Channels 

    if(this->qmin_in_use) {
        double qcost;
        double qmin_requirements = this->qmin.calcQminRequirement(S->year[t], S->month[t], S->day[t], &qcost);  // m3/s
        if(S->tot_outflow[t]  < qmin_requirements) {
            S->cost_qmin[t]  = qcost*S->dt/3600;
        }
    }
    S->cost[t] = S->cost_qmin[t];

    remaining_available_Mm3 = S->channel_storage_Mm3[t];
    if(remaining_available_Mm3 < 0.0) {
        remaining_available_Mm3 = 0.0; // Used to calculate remaining available energy in system. Cannot be negative.
    }

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

	myfile.open(filename.c_str() );

	if (!myfile.is_open()) 	{
		cout << "The topologyfile " << filename << " could not be found/opened. \n";
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		exit(EXIT_FAILURE);
	}

    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {
            // Line is not empty and doesn't start with # (hash/pound sign)
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);

            if ( keyword.compare("NODE") == 0 && value.compare("CHANNEL") == 0 ) {
                token = line_obj.extractNextElementFromLine(&line);
                tmp_idnr = atoi(token.c_str() );
                if(tmp_idnr == idnr) {
                    nodename = line_obj.extractNextElementFromLine(&line);
                    nodetype = NodeType::CHANNEL;

                    token = line_obj.extractNextElementFromLine(&line);
                    this->downstream_idnr = atoi(token.c_str() );
                    if(this->downstream_idnr >= 0) {
                        downstream_node_in_use = true;
                    }

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("TRAVELTIME") != 0) {
                        cout << "Could not find the keyword TRAVELTIME in file " << filename << " something is wrong\n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    value   = line_obj.extractNextElementFromLine(&line);
                    traveltime = atoi(value.c_str() );

                    getline(myfile, line);
                    keyword = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("DECAY") != 0) {
                        cout << "Could not find the keyword TRAVELTIME in file " << filename << " something is wrong\n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    value   = line_obj.extractNextElementFromLine(&line);
                    decay = atof(value.c_str() );

                    getline(myfile, line);

                    keyword = line_obj.extractNextElementFromLine(&line);
                    if ( keyword.compare("QMIN") != 0) {
                        cout << "Could not find the keyword QMIN in file " << filename << " something is wrong\n";
                        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		                exit(EXIT_FAILURE);
                    }
                    token = line_obj.extractNextElementFromLine(&line);

                    this->qmin.nr_periods = atoi(token.c_str());

                    if(this->qmin.nr_periods  <= 0) {
                        this->qmin_in_use = false;
                    } else {
                        this->qmin_in_use = true;
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

                            value   = line_obj.extractNextElementFromLine(&line);
                            qmin.timeperiods[q].penalty_cost = atof(value.c_str() );   
                        }
                    }
                }
            }
        }
    }
    myfile.close();
    return 0;
}
////////////////////////////////////////////////////////////////////////////
int Channel::SetStartState(void) {

    for(size_t t = 0; t < S->stps; t++ ) {
        S->up_inflow[t]    = 0.0;
        //S->inflow[t]       = 0.0;
    }

    for(size_t t = 0; t <  this->traveltime; t++ ) {
        waterflow_m3[t]    = init_waterflow_m3[t];
    }

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

            if ( keyword.compare("NODE") == 0 && value.compare("CHANNEL") == 0 ) {
                token = line_obj.extractNextElementFromLine(&line);
                tmp_idnr = atoi(token.c_str() );
                keyword = line_obj.extractNextElementFromLine(&line);

                if(tmp_idnr == idnr && keyword == nodename) {
                    found_node = true;
                    if(this->traveltime > 0) {
                        getline(myfile, line);
                        for(size_t t = 0; t <  this->traveltime; t++ ) {
                            value   = line_obj.extractNextElementFromLine(&line);
                            waterflow_m3[t]        = atof( value.c_str() );
                            init_waterflow_m3[t]   = atof( value.c_str() );
                        }
                    }
                }
            }
        }
    }
    myfile.close();

    if(!found_node) {
		cout << "There is something wrong in the statefile "<< filename << "\n";
        printf( "Channel::ReadStateFile           idnr=%d nodename=%s\n", int(idnr) , nodename.c_str()  );
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		exit(EXIT_FAILURE);
    }

    return 0;
}
/////////////////////////////////////////////////////////
int Channel::CheckWaterBalance(void) { 

    double start_channel_m3 = 0.0;
    double end_channel_m3   = 0.0;
    double sum_inflow       = 0.0;
    double sum_outflow      = 0.0;

    for(size_t t = 0; t <  this->traveltime; t++ ) {
        start_channel_m3 += init_waterflow_m3[t];
        end_channel_m3   += waterflow_m3[t];
    }

    for(size_t t = 0; t < this->stps; t++) {
        sum_inflow += MACRO_m3s_2_Mm3( (this->S->inflow[t] + this->S->up_inflow[t]) , dt);
        sum_outflow += MACRO_m3s_2_Mm3(this->S->tot_outflow[t], dt);
    }

    double waterbalance = (start_channel_m3/1000000) + sum_inflow - (end_channel_m3/1000000) - sum_outflow;

    if(WATERBALANCE_WARNINGS) {
        printf( "WATERBALANCE CHANNEL for idnr=%d   nodename=%s\n", int(idnr) , nodename.c_str()  );
        printf("start_channel_Mm3 = %.6f\n", start_channel_m3/1000000);
        printf("sum_inflow_Mm3    = %.6f\n", sum_inflow);
        printf("sum_outflow_Mm3   = %.6f\n", sum_outflow);
        printf("end_channel_Mm3   = %.6f\n", end_channel_m3/1000000);
        printf("waterbalance      = %.6f\n", waterbalance);
        //printf( "idnr=%d   nodename=%s\n", int(idnr), nodename.c_str()  );
        //printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        // this->PrintChannelWater();
    }

    if(abs(waterbalance) > 0.0001) {
        printf( "WATERBALANCE CHANNEL for idnr=%d   nodename=%s\n", int(idnr) , nodename.c_str()  );
        printf("start_channel_Mm3 = %.6f\n", start_channel_m3/1000000);
        printf("sum_inflow        = %.6f\n", sum_inflow);
        printf("sum_outflow       = %.6f\n", sum_outflow);
        printf("end_channel_Mm3   = %.6f\n", end_channel_m3/1000000);
        printf("waterbalance      = %.6f\n", waterbalance);
        printf( "idnr=%d   nodename=%s\n", int(idnr), nodename.c_str()  );
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
	    exit(EXIT_FAILURE);
    }

    return 0; 
}
//------------------------------------------------------------------------
double Channel::GetStartWater_Mm3(void) {
    double start_channel_m3 = 0.0;
    for(size_t t = 0; t <  this->traveltime; t++ ) {
        start_channel_m3 += init_waterflow_m3[t];
    }
    return start_channel_m3/1000000; // Mm3
}
//------------------------------------------------------------------------
double Channel::GetEndWater_Mm3(void) {
    double end_channel_m3   = 0.0;
    for(size_t t = 0; t <  this->traveltime; t++ ) {
        end_channel_m3   += waterflow_m3[t];
    }
    return end_channel_m3/1000000.0;
}
//------------------------------------------------------------------------
int Channel::WriteNodeOutput(GlobalConfig *gc){

    FILE *fp;
    char outfilename [100];
    sprintf (outfilename, "%snode%lu_%s.txt", gc->outputdir.c_str() , (idnr) , nodename.c_str()  );
    char outstr [100];

    if((fp = fopen(  outfilename ,"w"))==NULL) {
        printf("Cannot open file %s \n", outfilename);
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        exit(EXIT_FAILURE);
    }

    sprintf (outstr, "CHANNEL node %d %s\n", int(idnr), nodename.c_str()  );
    fprintf(fp, "%s", outstr);
    fprintf(fp, "TRAVELTIME= %d\n", int( this->traveltime)  );
    fprintf(fp, "DECAY= %.3f\n", this->decay);
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
    for( size_t s = 0; s < this->traveltime; s++) { 
        fprintf(fp, "%.5f ", this->waterflow_m3[s]);
    }
    fprintf(fp, "\n");
    return 0;
}
//////////////////////////////////////////////////////////////////////////////////
