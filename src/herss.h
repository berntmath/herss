/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     herss.h                                                        
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

#ifndef _HERSS_H
#define _HERSS_H

#include <string>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <map>
#include "arraycurve.h"
#include <time.h>

using namespace std;

// Version number.
#define VERSION 005
#define VERSION_DATE 20241014

// Maximum number of nodes allowed. // to save coding
#define MAX_NR_NODES 30
// Maximum number of coloumns (words or tokens) in one line
#define MAX_WORDS 200

// To make initialisation of array easier. 
#define MAX_TRAVELTIME_HOURS 200

// Maximum number of points in a point curve
#define MAX_NR_POINTS_CURVE 50

// If MINIMIZE_OUTPUT=1 we write only the the riversystem outputfile with waterbalance and economy. 
// #define MINIMIZE_OUTPUT 0

// Average Earth gravity
#define GRAVITY 9.80665

const string DELIMITER = " \n\t";
const string NUMERIC = "0123456789.-";

#define NOT_INIT 99999
#define STR_NOT_INIT "ERROR_STR_NOT_INIT"

#define HERSS_DEBUG_ALL 1

#define MAX_NUMBER_OF_QMIN_PERIODS 5

// Turn on and off warnings related to check of waterbalance. 
#define WATERBALANCE_WARNINGS false

// Turn on and off warnings related to check of economy in the system
#define ECONOMY_WARNINGS false


/////////////////////////////////////////////////////////////////
#define MACRO_m3s_2_Mm3(q, dt) q*dt/1000000.0
#define MACRO_Mm3_2_m3s(q, dt) q*1000000.0/dt
/////////////////////////////////////////////////////////////////

#ifdef _WIN32
   #define timegm(X) _mktime64(X) - timezone
   #define gmtime_r(X,Y) gmtime_s(Y,X)
#endif

#ifdef _WIN64
   #define timegm(X) _mktime64(X) - timezone
   #define gmtime_r(X,Y) gmtime_s(Y,X)
#endif


enum NodeType
{
   RESERVOIR,
   POWERSTATION,
   CHANNEL
};


inline const char* EnumToString(NodeType v)
{
    switch (v)
    {
        case RESERVOIR:   return "RESERVOIR";
        case POWERSTATION:   return "POWERSTATION";
        case CHANNEL: return "CHANNEL";
    }
    return "VOID";
}

//---------------------------------------------------------------------------
// Forward declarations to make all classes visible before they are defined
class Scenario;
class GlobalConfig;
class Channel;
class SystemState;
//-----------------------------------------------------------------------

// Simple time class
// See Kernighan and Ritchie page 298, ISBN 82-518-2705-1, Norwegian edition. 
// Note that this may be effected by the Y2038 problem. 
class DateTime {
public:
    DateTime(){};
    ~DateTime(){};
    void setDate(int year, int month, int day, int hour, int min, int sec) {
        mytm.tm_sec   = sec;            // 0 to 59
        mytm.tm_min   = min;            // 0 to 59
        mytm.tm_hour  = hour;           // 0 to 23
        mytm.tm_mday  = day;            // 1 to 31
        mytm.tm_mon   = month-1;           // 0 to 11
        mytm.tm_year  = year-1900;      // year-1900
        mytm.tm_isdst = 0;              // 0 to 1, daylight saving or not.
        epoch = timegm(&mytm);
        gmtime_r(&epoch, &mytm);
    }
    time_t getEpoch(){ return epoch;};
private:
    struct tm mytm;
    time_t epoch;       // Essentialy a pointer to an integer value holding EPOCH seconds. After 2038 this may be a problem. 
};

//////////////////////////////////////////////////
// NAMING OF VARIABLES
// On many variables we specify the units they are in.
// We use the following ending on the variables to indicate unit
// Euro     Euros
// masl     meters above sea level
// MWh      Mega Watt Hours
// Mm3      Million kubic meters
// m3s      square meters pr second
// MW       Mega Watt
// fr       fraction, usually between zero and one. [0,1], byt may be slightly above 1 or under 0.
//////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////
class Line {
public:
    Line();
    ~Line();
    string extractNextElementFromLine(string* line);
    string extractLastElementFromLine(string* line);
    int calcNrCols(string* line);     // Calculates how many coloumns (words/tokens) there are in the string
    int checkDigit(string line);     // Checks if the string contains digits.
    int removeWhites(string* line);
};
///////////////////////////////////////////////////////////////////////////////////////////
class GlobalConfig {
public:
    GlobalConfig();       
    ~GlobalConfig();
    NodeType nodetypes[MAX_NR_NODES];  // We keep track of which nodetype each index 0,1,2,3 etc is. 
    string globalfile;
    string topologyfile;
    string actionsfile;
    string pricefile;
    string outputfile;
    string inflowfile;
    string systemname;
    string start_statefile;  // Reservoir levels and water storage in channels (how to value the water in channels?).
    string out_statefile;
    string outputdir;
    string inputdir;

    bool found_topologyfilename;
    bool found_actionsfilename;
    bool found_pricefilename;
    bool found_inflowfilename;
    bool found_systemname;
    bool found_start_statefilename;  
    bool found_outputfilename;
    bool found_dt;
    bool write_nodefiles;

    size_t nr_nodes;
    size_t nr_pstations;
    size_t nr_reservoirs;
    size_t nr_channels;
    size_t dt;     // Delta time step in seconds
    size_t stps;   // Nr of time steps in the simulation

    double discount_rate;  // DISCOUNT_RATE 0.05
    double discount_factor;

    size_t actions_idnrs[MAX_NR_NODES];  // We save the idnrs pointing to nodes with actions (actions inputfile). 
    size_t n_action_nodes;  // Number of nodes were we need to set actions. Could be at PSTATION or RESERVOIRS (hatch_release) 
    size_t inflows_idnrs[MAX_NR_NODES];  // We save the idnrs pointing to nodes with inflows 
    size_t n_inflow_nodes;  // Number of nodes were we need to set the inflow (RESERVOIRS) 

    void DiagnoseActionFile(); // We read the header and find number of action nodes and their indexes. 
    void readGlobalFile();              // Reads the global file
    void SetDirectoriesAndFilenames();
    void printGlobalInfo();
    void Diagnose();
    void checkNrSteps();  // Checks number of timesteps in the pricefile
};
///////////////////////////////////////////////////////////////////////////////////////////
class Dataset {
public:
    Dataset(GlobalConfig *gconfig);
    ~Dataset();    
    size_t stps;               // Number of timesteps used.
    size_t nr_nodes;           // We allocate one inflow and action series pr node. Not used in all of them , but makes it easyer.  
    GlobalConfig *gc; 

    double *price;          // We assume all nodes located in same price area. So we need only one price series. 
    double restprice;
    double **inflow;        // One series for each node. We can point to these series from othe robjects.
    double **action;     // One series for each node. Could change in the future. 
    int *year;
    int *month;
    int *day;
    int *hour;
    map<string, int> datestring2idx; // Map between string yyyymmdd and an index to the arrays.
    map<int, string> idx2datestring; // Map between index and a date string yyyymmdd.
    string str_startdate;                            // Startdate of data
    string str_enddate;                              // End date of data

    void readPricefile();
    void readInflowFile();
    void readActionsFile();

};
///////////////////////////////////////////////////////////////////////////////////////////
class Scenario {

public:
	Scenario();
    Scenario(size_t stps, size_t dt, size_t idnr);
    ~Scenario();

    size_t stps;
    size_t dt;
    size_t idnr;  // This is the same idnr as used in node.
    double restprice;
    bool broken_lrw; 
    bool broken_qmin;

    double days_with_production;
    double remaining_Mm3;
    double local_remaining_Mm3;  // The remainding volume in the Node.
    double remaining_Euro;
    double remaining_MWh;
    double remaining_upstream_Mm3;
    double sum_prod_MWh;
    double sum_income_Euro;
    double sum_cost_Euro;
    double sum_profit_Euro;
    double sum_incoming_water_Mm3;
    double sum_local_inflow_Mm3;
    double sum_total_energy_MWh;
    double sum_overflow_Mm3;  // We check overflow over the optimization horizon.

    // Arrays
    double *price;
    double *action;
    double *q_action;
    double *inflow;
    double *tot_outflow;
    double *tot_inflow;
    double *local_inflow;
    double *up_inflow;
    double *tunnelflow_m3s;
    double *hatchflow_m3s;
    double *overflow_m3s;
    double *auto_qmin_m3s;
    double *channel_storage_Mm3;

    double *res_Mm3;        // Reservoir filling in Mm3
    double *res_masl;       // Reservoir filling in meters above sea level (masl)
    double *res_fr;         // Reservoir filling as a fraction of full
    double *profit;
    double *overflow_Mm3;
    double *income;
    double *cost;
    double *cost_qmin;
    double *startStopCost;
    double *cost_lrw;
    double *cost_fake_lrw;
    double *adjust_cost;

    double *Hbrutto;  // Hydraulic head brutto
    double *Hnetto;  // Hydraulic head netto
    double *Power;
    int *year;
    int *month;
    int *day;
    int *hour;
    int *qmin_flag;

};
//////////////////////////////////////////////////////////////////////////////////////////
class QminPeriod {
    public:
    QminPeriod(){};
    ~QminPeriod(){};
    double min_discharge;
    int start_day;
    int start_month;
    int end_day;
    int end_month;
    double penalty_cost;
};
//////////////////////////////////////////////////////////////////////////////////////////
class Qmin {
  public:
    Qmin();
    ~Qmin();
    bool qmin_flag;
    QminPeriod timeperiods[MAX_NUMBER_OF_QMIN_PERIODS];
    int nr_periods;
    double calcQminRequirement(int year, int month, int day, double *cost );
};
//////////////////////////////////////////////////////////////////////////////////////////
class Node {
  public:
    Node();
    virtual ~Node();
    NodeType nodetype;
    size_t idnr; // Specified by the user, muste be correct calculation order (accumulation levels)
    Scenario *S;  // This will point to the correct scenario for the node.
    string nodename;

    Qmin qmin;  // We let all nodes have access to one Qmin object 
    bool qmin_in_use;  // Flag indicatin wether Qmin is used or not. 
    double up_res_Mm3;   // Upstream reservoir volume - used in Powerstation. 
    double remaining_available_Mm3;
    double upstream_remaining_available_Mm3; // We accumulate as we go downward. 

    size_t reservoir_idnr;  // Used so we can go from node idnr to reservoir number. 

    int pstation_idnr; // We use this to index pstations. 
    int max_adjustment_pr_day;
    double max_adjustment_cost;

    double local_energy_equivalent;  // kWh/m3
    double powstat_min_discharge;  // We must place them here so we can accoes them through node pointer. 
    double powstat_max_discharge;
    double auto_qmin;
    double start_of_stp_masl;
    double end_of_stp_masl;
    
    bool downstream_node_in_use;
    bool outlet_hatch_in_use;
    bool outlet_tunnel_in_use;
    bool outlet_overflow_in_use;
    bool outlet_auto_qmin_in_use;

    int downstream_idnr; // Used to keep track of remaining water volumes. 
    int downstream_idnr_tunnel;
    int downstream_idnr_hatch;
    int downstream_idnr_overflow;
    int downstream_idnr_auto_qmin;

    Node *ptr_downstream_node;
    Node *ptr_downstream_node_tunnel;
    Node *ptr_downstream_node_hatch;
    Node *ptr_downstream_node_overflow;
    Node *ptr_downstream_node_auto_qmin;

    virtual int ReadNodeData(string filename);
    virtual int ReadStateFile(string filename);
    virtual int WriteStateFile(FILE *fp);

    virtual int Simulate(size_t t);
    virtual int initArrayCurves(void);
    virtual int CheckWaterBalance(void);
    virtual double GetStartWater_Mm3(void);
    virtual double GetEndWater_Mm3(void);
    virtual int WriteNodeOutput(GlobalConfig *gc);  // Write output for each node 
    virtual double GetTunnelFLow(size_t t); // Used for reservoirs connected to a powerstation. 

    // Defining a function as virtual, means that it can be redefined in the child classes
    // This is an important feature since we can use the same function name, but execute different
    // taks depending on the child class.
};
/////////////////////////////////////////////////////////////////////////////////////////
class Reservoir: public Node {

    public:
    Reservoir();
    ~Reservoir();
    size_t stps;
    size_t dt;

    double reservoir_init_fr;
    double reservoir_init_masl;
    double reservoir_init_Mm3;
    double res_HRW;             //  Highest regulated water level [masl]
    double filling_at_hrw_Mm3;  // Mm3

    double filling_at_hatchlevel;
    double cost_lrw;
    double res_LRW;             //  Lowest regulated water level [masl]
    double filling_at_lrw_Mm3;  // Mm3
    double res_penalty;
    double res_Mm3;             //  Reservoir filling [Mm3]
    double res_masl;            //  Reservoir filling [masl]
    double res_fr;              //  Reservoir filling as a fraction of full.
    double res_curve_masl[MAX_NR_POINTS_CURVE];
    double res_curve_Mm3[MAX_NR_POINTS_CURVE];
    size_t nr_points_res_curve;

    double ovefl_curve_masl[MAX_NR_POINTS_CURVE];
    double ovefl_curve_m3s[MAX_NR_POINTS_CURVE];
    size_t nr_points_ovefl_curve;
 
    double minQ_hatch, maxQ_hatch, hatch_masl;

    ArrayCurve ac_res_masl_2_Mm3;
    ArrayCurve ac_res_Mm3_2_masl;
    ArrayCurve ac_ovefl_masl_2_m3s;
    ArrayCurve ac_ovefl_m3s_2_masl;

    // VIRTUAL FUNCTIONS USED IN RESERVOIR/CHANNEL/PSTATION
    int ReadNodeData(string filename);
    int ReadStateFile(string filename);
    int Simulate(size_t t);
    int initArrayCurves(void);
    int CheckWaterBalance(void);
    int GetStartWater(void);
    int WriteStateFile(FILE *fp);

    // Functions used only in Reservoir
    void InitReservoir(void);
    double CalcOverflow();   // Mm3
    double GetStartWater_Mm3(void);
    double GetEndWater_Mm3(void);
    int WriteNodeOutput(GlobalConfig *gc);  // Write output for each node 
    double GetTunnelFLow(size_t t); // Used for reservoirs connected to a powerstation. 

};
/////////////////////////////////////////////////////////////////////////////////////////
class Powerstation: public Node {
    public:
    Powerstation();
    ~Powerstation();
    size_t stps;
    size_t dt;
    double init_Power;

    // Turbinvirkningsgrad - arrays
    double turb_virkn_Q[MAX_NR_POINTS_CURVE];
    double turb_virkn_psnt[MAX_NR_POINTS_CURVE];
    size_t nr_points_turb_virkn;
    ArrayCurve ac_turbvirkn_curve;

    double static_gen_efficiency;
    double headlosscoef;
    double powstat_masl;
    double powstat_startstop;

    int ReadNodeData(string filename);
    int ReadStateFile(string filename);
    int Simulate(size_t t);
    int initArrayCurves(void);
    int CheckWaterBalance(void);
    double GetStartWater_Mm3(void);
    double GetEndWater_Mm3(void);
    int WriteNodeOutput(GlobalConfig *gc);  // Write output for each node 
    double GetTunnelFLow(size_t t); 
    int WriteStateFile(FILE *fp);
    double CalcAdjustmenCosts(void); // Only for Powerstation 
};
/////////////////////////////////////////////////////////////////////////////////////////
class Channel: public Node {
    public:
    Channel();
    ~Channel();
    size_t stps;
    size_t dt;

    size_t traveltime;
    double decay;
    double waterflow_m3[MAX_TRAVELTIME_HOURS];  // Keeps track of how much water that is stored in the channel. [m3]
    double init_waterflow_m3[MAX_TRAVELTIME_HOURS];  // Keeps track of how much water that is stored in the channel. [m3]

    int ReadNodeData(string filename);
    int ReadStateFile(string filename);
    int Simulate(size_t t);
    int initArrayCurves(void);
    int CheckWaterBalance(void);
    double GetStartWater_Mm3(void);
    double GetEndWater_Mm3(void);
    int WriteNodeOutput(GlobalConfig *gc);  // Write output for each node 
    double GetTunnelFLow(size_t t);
    int WriteStateFile(FILE *fp);
    int SetStartState(void);
    void PrintChannelWater(void);

};
/////////////////////////////////////////////////////////////////////////////////////////
// This class models a hydropower system
class Riversystem {

public:
    Riversystem();                   // Default constructor
    Riversystem(GlobalConfig *gc);
    ~Riversystem();                  // Default destructor
    GlobalConfig *gc;
    size_t nr_nodes;
    size_t nr_reservoirs;
    size_t nr_pstations;
    size_t nr_channels;
    double start_water_Mm3;
    double end_water_Mm3;
    double inflow_volume_Mm3;
    double outgoing_Mm3;
    double waterbalance;
    int nodes_idnrs[MAX_NR_NODES];
    double sum_prod_MWh;
    double sum_total_MWh; // Production pluss remaining in whole riversystem
    double adjust_cost;   // Adjustment cost 
    
    double tot_remaining_available_Mm3;
    double tot_remaining_available_MWh;
    double tot_remaining_available_Euro;
    double tot_income_Euro;
    double tot_cost_Euro;
    double tot_profit_Euro;
    double valuefunction_Euro;
    double sum_production;
    double avg_price;
    double sum_startstopcost;
    double sum_max_adjustment_cost;
    double sum_lrw_cost;
    double sum_qmin_cost;


    // Array of Nodes (reservoirs, powerstations, channels)
    Node **nodes;
    Reservoir *reservoirs;
    Powerstation *pstations;
    Channel *channels;
    double Simulate(int id);
    double CalcVF(double restprice);
    double CalcSimulationProfit();
    int WriteRiverSystemData(double restprice);
    void WriteReservoirData();
    void PrintReservoirData2Screen();
    int WriteSelectedOutputMatrix();
    double GetEndingReservoirLevel(size_t r_idnr);
    
};
/////////////////////////////////////////////////////////////////
// A class that models Input, Scenarios and riversystem
class Herss {
public:
    Herss();
    Herss(GlobalConfig *gc);
    GlobalConfig *gc;
    ~Herss();
    size_t dt;          // Delta time step in seconds
    size_t stps;        // How many time steps used in each scenario, and in the optimization step.
    size_t nr_nodes;    
    Riversystem *rs;
    Scenario  **scen;

    int prepaireSimulation(Dataset *data); // Read in final data and set pointers.
    int Simulate();
    int CheckWaterBalance();
    int GlobalWaterBalance(Dataset *data);
    int WriteNodeOutput();  // Write output for each node
    int WriteStateFile();  // Write output for each node
    int CalcAdjustmenCosts();
    
    void SetAction(size_t node_idnr, size_t t, double value);
    double GetAction(size_t node_idnr, size_t t);

    void PrintActions();
    void PrintReservoirLevels_fr();
    void PrintRemainingChannelWater_Mm3();
    double GetRestPrice();
    double GetPrice(size_t t);
    void SetPrice(size_t t, double price, double restprice);
    double GetReservoir_Init_fr(size_t idnr);  // Get starting reservoir fraction.

    void SetReservoir_Init_fr(size_t idnr, double value);  // Set starting reservoir fraction.


    double GetReservoirLevel_fr(size_t node_idnr, size_t t);
    void PrintInflowSeries(size_t t);
    void PrintState();

    void SetInflowInNode(size_t t, size_t nodenr, double value);
    double GetInflowInNode(size_t t, size_t nodenr);
    void PrintAllInput();
};
/////////////////////////////////////////////////////////////////




#endif
