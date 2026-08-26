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
#include <cmath>
#include "arraycurve.h"
#include "logger.h"
#include <time.h>
#include <vector> 
using namespace std;

// BVM May 2026, we start using the version convention MAJOR.MINOR.PATCH
const string VERSION = "4.0.00"; //Since we now have four node types;)
const string VERSION_DATE = "20260814";

// Maximum number of nodes allowed. // to save coding
#define MAX_NR_NODES 250
// Maximum number of coloumns (words or tokens) in one line
#define MAX_WORDS 200

// To make initialisation of array easier. 
#define MAX_TRAVELTIME_HOURS 240

// Maximum number of points in a point curve
#define MAX_NR_POINTS_CURVE 50

// If MINIMIZE_OUTPUT=1 we write only the the riversystem outputfile with waterbalance and economy. 
// #define MINIMIZE_OUTPUT 0

// Average Earth gravity
#define GRAVITY 9.80665
#define PI 3.14159265358979323846
#define MOUNT_EVEREST_MASL 8848.0

const string DELIMITER = " \n\t";
const string NUMERIC = "0123456789.-";

#define NOT_INIT 9999999
#define STR_NOT_INIT "ERROR_STR_NOT_INIT"

// #include <limits> std::numeric_limits<double>::max();
// More practical number to use in the code.
#define VERY_LARGE_NUMBER 999999999

const std::string DEFAULT_STRING_INIT = "ERROR_STR_NOT_INIT";

#define HERSS_DEBUG_ALL false

#define MAX_NUMBER_OF_QMIN_PERIODS 5

// Turn on and off warnings related to check of waterbalance. 
#define WATERBALANCE_WARNINGS false

// Turn on and off warnings related to check of economy in the system
#define ECONOMY_WARNINGS false

// Maximum nr of generators in a powerstation
#define MAX_NR_GENERATORS 6
#define MAX_NR_CASCADED_RESERVOIRS 10

#define N_UNIFORM_EFF_CURVE_POINTS 11

#define HERSS_AGGRESSIVE_ACTIONS_COST 1000


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

//-----------------------------------------------------------------------------
// Smoothly maps ℝ → [0,1] (differentiable alternative to clamp)
// We avoid "hard"-constraining the range of parameters [min,max] 
// Instead we use the smoothClamp01 function to smoothly constrain the range of parameters to [0,1], 
// and then we can scale and shift this to get the desired range. 
inline double smoothClamp01(double x) {
    double y = 0.5 * (x / (1.0 + std::abs(x)) + 1.0);
    return y * y * (3.0 - 2.0 * y);
}

// We avoid using hard minimum and maximum functions, 
// and instead use smooth approximations
inline double smooth_max(double a, double b) {
    return log(exp(a) + exp(b));
}

inline double smooth_min(double a, double b) {
    return -log(exp(-a) + exp(-b));
}
//-----------------------------------------------------------------------------


// This is the naming convention that needs to be used inside the topolgy file 
// In earlyer version we used POWERSTATION, this has now changed to PSTATION 
// We make it more consistent with class names. 
enum NodeType
{
   RESERVOIR,
   PSTATION,
   CHANNEL,
   PUMP
};

const size_t NODE_TYPE_COUNT = 4;

inline const char* EnumToString(NodeType v)
{
    switch (v)
    {
        case RESERVOIR:   return "RESERVOIR";
        case PSTATION:   return "PSTATION";
        case CHANNEL: return "CHANNEL";
        case PUMP: return "PUMP";
    }
    return "VOID";
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Forward declarations to make all classes visible before they are defined
class Scenario;
class GlobalConfig;
class Channel;
class SystemState;
class Herss;
class Pump;

class CascadedReservoirs;


//-----------------------------------------------------------------------
class TopologyParser {

private:
    std::vector<std::string> lines;
    // size_t currentLine = 0;
    std::string trim(const std::string& str);
    
public:

    bool loadFile(const std::string& filename);
    size_t getLineCount() const { return lines.size(); }

    std::string getLine(size_t index) {
        if (index < lines.size()) {
            return lines[index];
        } else {
            throw std::out_of_range("Index out of range in TopologyParser::getLine");
        }
    }
};
//----------------------------------------------------------

    // We accept the following formats:
    // Supported date formats
    // yyyy-mm-dd
    // yyyy-mm-dd-hh
    // yyyymmdd
    // yyyymmddhh

class Xtime {
public:
    Xtime();
    ~Xtime();
    static bool isValid(std::string &);
    static void getNowString(std::string &);
    
	bool setDate(string str_date);  // Returns true if date format is OK. 
	void getDateString(char *str_buffer); 
    string returnDateString();
    void setDate(int yyyy, int mm, int day, int hour, int min, int sec);
	void printDate();
    int changeDate(int dt);
    int getYear();
    int getMonth();
    int getDay();
    int getHour();
    int getMin();
    int getSec();
    int getJulianDay();
    time_t getEpoch();
    int dateDiff(Xtime *, int);
    void print(FILE *f, char *format);   // Write the current time to file descriptor f on given format
    string currentDateTime();
	bool isValid(int, int, int, int, int, int);

private:
    struct tm my_tm;
    time_t epoch;       /* Essentialy a pointer to an integer value holding EPOCH seconds.
                            After 2038 this may be a problem. */
    
    static int isLeapYear(int);
};
//----------------------------------------------------------

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
// fr       fraction, usually between zero and one. [0,1], but may be slightly above 1 or under 0.
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

    TopologyParser topoparser;

    bool found_topologyfilename;
    bool found_actionsfilename;
    bool found_pricefilename;
    bool found_inflowfilename;
    bool found_systemname;
    bool found_start_statefilename;  
    bool found_outputfilename;
    bool found_dt;
    bool write_nodefiles;
    bool printglobalinfo; 
    bool printeconomicinfo;


    string logfilename;
    size_t nr_nodes;
    size_t nr_pstations;
    size_t nr_reservoirs;
    size_t nr_channels;
    size_t nr_pumps;
    size_t dt;     // Delta time step in seconds
    size_t stps;   // Nr of time steps in the simulation
    size_t dt_last; 

    double discount_rate;  // DISCOUNT_RATE 0.05
    double discount_factor;

    size_t actions_idnrs[MAX_NR_NODES];  // We save the idnrs pointing to nodes with actions (actions inputfile). 
    size_t n_action_nodes;  // Number of nodes were we need to set actions. Could be at PSTATION or RESERVOIRS (hatch_release) 

    // Number of nodes were we need to set actions. 
    // Could be at PSTATION or RESERVOIRS (hatch_release) 
    // We calculate this from the topology file and compare it when reading actions. 
    size_t n_action_nodes_from_topology;

    size_t inflows_idnrs[MAX_NR_NODES];  // We save the idnrs pointing to nodes with inflows 
    size_t n_inflow_nodes;  // Number of nodes were we need to set the inflow (RESERVOIRS) 

    void DiagnoseActionFile(); // We read the header and find number of action nodes and their indexes. 
    void readGlobalFile();              // Reads the global file
    void SetDirectoriesAndFilenames();
    void printGlobalInfo();
    void Diagnose();
    void DiagnoseTopologyFile(); 

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
    std::vector<std::string> action_colnames;  
    std::vector<int> delta_t; 
    
    void readPricefile();
    void readInflowFile();
    void readActionsFile();
    void readAllData();
    void multi_temporal_resolution();
    int getDeltaT(size_t timestep);  // Get delta_t for a specific timestep
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
    double **action;
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
    double *pump_in_m3s;   // Water pumped into this reservoir [m3/s]
    double *pump_out_m3s;  // Water pumped out of this reservoir [m3/s]
    double *channel_storage_Mm3;

    double *res_Mm3;          // Reservoir filling in Mm3
    double *res_active_Mm3;   //  Reservoir filling [Mm3] minus filling at LRW 
    double *res_masl;         // Reservoir filling in meters above sea level (masl)
    double *res_fr;           // Reservoir filling as a fraction of full
    double *profit;
    double *overflow_Mm3;
    double *income;
    double *cost;
    double *cost_qmin;
    double *startStopCost;
    double *cost_lrw;
    double *cost_fake_lrw;

    
    // Use this to penalise number of Power adjustments pr day . Like a soft start-stop cost. 
    double *adjust_cost;  

    // If we try to empty the reservoir. We override the actions and give penalty cost for too agressive actions.  
    double *cost_aggressive_actions;

    double *Hbrutto;  // Hydraulic head brutto
    double *Hnetto;  // Hydraulic head netto
    double *Power;
    double *EstimatedEEKV;  // Estimated energy equivalent

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
    double calcQminRequirement(int year, int month, int day, double *cost);
    // Terje Sandø, 02.07.2026, extended 06.07.2026
    // Validates qmin period definitions at startup. Shared by Reservoir OUTLET_AUTO_QMIN
    // and Channel QMIN. Catches topology file errors (bad dates, gaps, overlaps) with clear
    // error messages before any simulation timestep is computed.
    void validatePeriods(string nodename);
};

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// BVM May 2026, new routing model inside the Channel class.
// We make a routing model that is suited for multi-temporal resolution. 
// It is based on a series of cascaded linear reservoirs. The number of reservoirs can be set by the user.
struct ReservoirState {
    double storage_m3 = 0.0;
};

struct ReservoirStepResult {
    double storage_new_m3;
    double q_out_avg_m3s;
    double q_out_end_m3s;
};

//------------------------------------------------------------------------------------------------
// Class models a series of cascaded reservoirs that will be used in the channel class/routing. 
class CascadedReservoirs {

  public:
    CascadedReservoirs(double k_traveltime_hours, size_t num_reservoirs);
    ~CascadedReservoirs();
    void setInitialStorage(std::vector<double> initial_storage_Mm3);
    double route(double q_in_m3s, double dt_seconds);
    double totalStorageM3();
    double getStorageMm3(size_t idx_linres);

  private:

    ReservoirStepResult routeOneReservoir(
        double storage_old_m3, double q_in_m3s, double k_seconds, double dt_seconds);

    // In each channel the user can choose number of cascaded linear reservoirs
    std::vector<ReservoirState> linreservoirs;
    double k_total_seconds;
    double k_res_seconds; 
    double k_traveltime_hours;
    size_t num_reservoirs;


};
//////////////////////////////////////////////////////////////////////////////////////////
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
    
    double remaining_Mm3;   
    double upstream_remaining_Mm3; // We accumulate as we go downward. 
    
    double remaining_active_Mm3;          // Means only water that can be produced 
    double upstream_remaining_active_Mm3; // We accumulate as we go downward.  Only water that is assumed to available for production. 

    size_t reservoir_idnr;  // Used so we can go from node idnr to reservoir number. 

    int pstation_idnr; // We use this to index pstations. 
    int max_adjustment_pr_day;
    double max_adjustment_cost;

    double local_energy_equivalent;  // kWh/m3
    double powstat_min_discharge;  // We must place them here so we can accoes them through node pointer. 
    double auto_qmin;
    double start_of_stp_masl;
    double end_of_stp_masl;

    bool downstream_node_in_use;

    bool outlet_hatch_in_use;
    bool outlet_tunnel_in_use;
    bool outlet_overflow_in_use;
    bool outlet_auto_qmin_in_use;

    size_t downstream_idnr; // Used to keep track of remaining water volumes.
    size_t downstream_idnr_tunnel;
    size_t downstream_idnr_hatch;
    size_t downstream_idnr_overflow;  // Same idnr for Spillway and overflow_curve
    size_t downstream_idnr_auto_qmin;
   
    Node *ptr_downstream_node;  // This one is essentially a helper pointer.
    // We set this one to one of the outlets that are used.

    Node *ptr_downstream_node_tunnel;
    Node *ptr_downstream_node_hatch;
    Node *ptr_downstream_node_overflow;  // Same pointer for spillway and overflow curve, since we can only use one of them in each reservoir.
    Node *ptr_downstream_node_auto_qmin;

    virtual int ReadNodeData(string filename);
    virtual int ReadStateFile(string filename);
    virtual int WriteStateFile(FILE *fp);

    virtual int Simulate(size_t t);
    virtual int initArrayCurves(void);
    virtual int CheckWaterBalance(class Herss *herss_obj); 
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
    GlobalConfig *gc;
    double floodlevel_penalty;
    double floodlevel_cost;
    double reservoir_init_fr;
    double reservoir_init_masl;
    double reservoir_init_Mm3;
    double reservoir_init_active_Mm3;  // minus the filling at LRW
    double active_max_volume_Mm3;  // Filling at HRW minus the filling at LRW 
    double res_HRW;             //  Highest regulated water level [masl]
    double res_LRW;             //  Lowest regulated water level [masl]
    double filling_at_hrw_Mm3;  // Mm3
    double filling_at_lrw_Mm3;  // Mm3
    double filling_at_hatchlevel;
    double cost_lrw;
    double res_penalty;
    bool fast_overflow;         // If true, we use the fast overflow calculation.
    
    // Dynamic variables that change during the simulation
    double res_Mm3;             //  Reservoir filling [Mm3]
    double res_masl;            //  Reservoir filling [masl]
    double res_fr;              //  Reservoir filling as a fraction of full.

    // We can mix usage of reservoir curve and reservoir geometry, 
    // but we need to make sure that they are consistent.
    // We can only use one of them in each reservoir. 
    // but mix them in a riversystem. 


    // Reservoir geometry, we assume a trapezoidal shape
    // we use this to scip the reservoir curve. 
    bool use_reservoir_geometry;
    double width_m;
    double length_m;
    double theta; // angle of the sides of the reservoir, in degrees.
    double bottom_masl; // masl of the bottom of the reservoir.

    // We precompute this one to save time in the simulation.
    double slope_term; // tan((90.0 - theta) * PI / 180.0);   

    // We precompute this one to save time in the simulation.
    double geo_denom; 
    // geo_denom = length_m * (slope_term + width_m); 


    bool use_reservoir_curve; // If true, we use the reservoir curves for calculating the filling and the masl.
    double res_curve_masl[MAX_NR_POINTS_CURVE];
    double res_curve_Mm3[MAX_NR_POINTS_CURVE];
    size_t nr_points_res_curve;



    bool use_overflow_curve; // If true, we use the overflow curve for calculating the overflow.
    double ovefl_curve_masl[MAX_NR_POINTS_CURVE];
    double ovefl_curve_m3s[MAX_NR_POINTS_CURVE];
    size_t nr_points_ovefl_curve;
 
    double minQ_hatch, maxQ_hatch, hatch_masl;

    // BVM, 5 June 2026. 
    // Spillway, Q = CLH^1.5, 
    // where C is the spillway coefficient, 
    // L is the spillway width, and H is the head above the spillway.
    // H is typically the reservoir level minus the spillway level.
    // We can use this to calculate the overflow when the reservoir level is above the spillway level. 
    double spillway_C;
    double spillway_L;
    double spillway_level_masl;  // The level of the spillway in masl
    bool use_spillway; 

    // Terje Sandø, 02.07.2026
    // MASL of the outlet_auto_min on the reservoir.
    // Water can only flow out when the reservoir level exceeds this threshold.
    // Prevents the model from releasing water that is physically below the pipe.
    double outlet_auto_qmin_masl;


    // Reverse mapping: pumps using this reservoir as source or target (for averaged-head computation).
    std::vector<Pump*> ptr_pumps_as_source;
    std::vector<Pump*> ptr_pumps_as_target;

    ArrayCurve ac_res_masl_2_Mm3;
    ArrayCurve ac_res_Mm3_2_masl;
    ArrayCurve ac_ovefl_masl_2_m3s;
    ArrayCurve ac_ovefl_m3s_2_masl;

    // VIRTUAL FUNCTIONS USED IN RESERVOIR/CHANNEL/PSTATION
    int ReadNodeData(string filename);
    int ReadStateFile(string filename);
    int Simulate(size_t t);
    int initArrayCurves(void);
    //int CheckWaterBalance(void);
    int CheckWaterBalance(class Herss *herss_obj);  
    int GetStartWater(void);
    int WriteStateFile(FILE *fp);

    // Functions used only in Reservoir
    void InitReservoir(void);
    double CalcOverflow();   // Mm3
    double GetStartWater_Mm3(void);
    double GetEndWater_Mm3(void);
    int WriteNodeOutput(GlobalConfig *gc);  // Write output for each node 
    double GetTunnelFLow(size_t t); // Used for reservoirs connected to a powerstation. 
    double GetReservoirFraction(size_t t);

    // We use this to check if the reservoir level is valid.
    void ValidateReservoirLevelMm3(size_t t, double level_Mm3);

    // We check if the settings for the reservoir are valid. 
    void ValidateReservoirSettings();  

    double calcResVolume(double masl);  // Returns Mm3 
    double calcResMasl(double Mm3);     // Returns masl



};
/////////////////////////////////////////////////////////////////////////////////////////
// CHANGE BY OVE - Initialize the Generator struct
struct Generator {
        std::vector<double> turb_virkn_Q;       // Turbine efficiency curve Q values
        std::vector<double> turb_virkn_psnt;    // Turbine efficiency curve psnt values
        ArrayCurve eff_curve; 
        std::vector<double> action;             // Actions for this generator
        double headlosscoef;                    // Head loss coefficient for this generator/penstock
        double max_discharge;                   // Maximum discharge for this generator

        double uniform_normalized_curve[N_UNIFORM_EFF_CURVE_POINTS];
        bool use_uniform_normalized_curve;

    };

class Powerstation: public Node {
    public:
    Powerstation();
    ~Powerstation();
    size_t stps;
    size_t dt;
    double init_Power;
    GlobalConfig *gc;


    //--------------------------------------------------------------------------------------
    // Efficiency curves for the generators in the powerstation. 
    // Qn	Pelton	Francis	Kaplan
    // 0	0	0	0
    // 0.1	50	40	55
    // 0.2	75	65	75
    // 0.3	86	80	84
    // 0.4	90	88	89
    // 0.5	92	91	91
    // 0.6	91.5	92	92
    // 0.7	90	91	91.5
    // 0.8	88	88	91
    // 0.9	85	83	90
    // 1	82	78	88

    // One of the problems with these curves is that it is slow to interpolate on them.
    // To solve this we can use two methods.
    // 1. Fit smooth equations to the data.
    // 2. Use the data curve and interpolate on it.
    // If the data are uniformly distributed - and only then, we have a fast lookup method.
    // If the data are non-uniformly, we must use the slow method already implemented in ArrayCurve. 
    
    // Setup
    //const int N = 100;          // table size
    //double eta_table[N];        // precomputed efficiency values

    // At runtime — no search needed
    // double Qn = Q / Q_rated;    // normalize to 0-1
    // int i = (int)(Qn * (N-1));  // direct index calculation
    // double t = Qn * (N-1) - i;  // fractional part for interpolation
    // double eta = eta_table[i] + t * (eta_table[i+1] - eta_table[i]);
    //--------------------------------------------------------------------------------------


    double static_gen_efficiency;
    double headlosscoef;
    double powstat_masl;
    double powstat_startstop;
    bool shared_penstock;  // CHANGE BY OVE: true = shared penstock, false = separate penstocks

    size_t nr_generators;  // We can get this out of the vector, but its nice to have. 
    std::vector<Generator> generators; // CHANGE BY OVE: Implemented vectors to hold generators in a powerstation.

    // BVM 15 oct 2024.
    // In a river runoff powerstation, it is possible to empty the intake in one timestep. 
    // Since we do not allow this, and we override the action.
    // We give a marginal penalty so that we see a change in the VF.
    double aggressive_actions_cost;

    int ReadNodeData(string filename);
    int ReadStateFile(string filename);
    int Simulate(size_t t);
    // int initArrayCurves(void);
    //int CheckWaterBalance(void);
    int CheckWaterBalance(class Herss *herss_obj);
    double GetStartWater_Mm3(void);
    double GetEndWater_Mm3(void);
    int WriteNodeOutput(GlobalConfig *gc);  // Write output for each node 
    double GetTunnelFLow(size_t t); 
    int WriteStateFile(FILE *fp);
    double CalcAdjustmenCosts(void); // Only for Powerstation 
    void ValidatePowerstationSettings();  // We check if the settings for the powerstation are valid. For example, that the number of generators is not higher than the maximum allowed, and that the headloss coefficient is not negative.

    double calcEfficiency(size_t gen_idx, double q_m3s);  // Calculate the efficiency for a specific generator and discharge.



};
/////////////////////////////////////////////////////////////////////////////////////////
class Channel: public Node {
    public:
    Channel();
    ~Channel();
    size_t stps;
    size_t dt;
    GlobalConfig *gc;

    CascadedReservoirs *casc_reservoirs;
    // We use a series of cascaded reservoirs to model the routing in the channel.

    std::vector<double> initial_storage_linres_Mm3; 

    double K_traveltime_hours;  // Travel time in hours, used in the cascaded reservoir routing model.
    size_t num_cascaded_reservoirs;  // Number of cascaded reservoirs used in the routing model.

    double decay;
    double waterflow_m3[MAX_TRAVELTIME_HOURS];  // Keeps track of how much water that is stored in the channel. [m3]
    double init_waterflow_m3[MAX_TRAVELTIME_HOURS];  // Keeps track of how much water that is stored in the channel. [m3]

    int ReadNodeData(string filename);
    int ReadStateFile(string filename);
    int Simulate(size_t t);
    int initArrayCurves(void);
    int CheckWaterBalance(class Herss *herss_obj);  
    double GetStartWater_Mm3(void);
    double GetEndWater_Mm3(void);
    int WriteNodeOutput(GlobalConfig *gc);  // Write output for each node 
    double GetTunnelFLow(size_t t);
    int WriteStateFile(FILE *fp);
    int SetStartState(void);
    void PrintChannelWater(void);
    void ValidateChannelSettings();  // We check if the settings for the channel are valid. For example, that the travel time is not longer than the maximum allowed.

};
/////////////////////////////////////////////////////////////////////////////////////////
// Terje Sandø, pump-station work, July 2026.
class Pump: public Node {
    public:
    Pump();
    ~Pump();
    size_t stps;
    size_t dt;
    GlobalConfig *gc;

    // --- Topology-configured parameters (PUMP_SOURCE_IDNR / PUMP_TARGET_IDNR / etc.) ---
    size_t pump_source_idnr;
    size_t pump_target_idnr;
    bool pump_source_target_in_use;  // Both keywords found and validated at ReadNodeData time.
    Node *ptr_pump_source;
    Node *ptr_pump_target;

    double pump_inlet_masl;          // PUMP_INLET_MASL - water below this in source is inaccessible.
    double max_discharge;
    double headlosscoef;
    double static_motor_efficiency;  // STATIC_MOTOR_EFFICIENCY, combined motor/transformer losses, (0,1].
    double pump_startstop;           // PUMP_STARTSTOP - cost of one start or one stop event.
    double init_Power;               // Energy [MWh] consumed in the last timestep of the previous run, read from the statefile.

    // Source and target reservoir levels at the start and end of the current
    // timestep, written by those reservoirs during their own Simulate().
    double src_start_masl;
    double src_end_masl;
    double tgt_start_masl;
    double tgt_end_masl;

    // Efficiency curve (Q-dependent), mirrors Generator::eff_curve / uniform_normalized_curve.
    ArrayCurve pump_eff_curve;
    double uniform_normalized_curve[N_UNIFORM_EFF_CURVE_POINTS];
    bool use_uniform_normalized_curve;
    std::vector<double> pump_curve_Q;      // Non-uniform PUMP_CURVE points, if used instead.
    std::vector<double> pump_curve_psnt;

    // Per-timestep bookkeeping, sized to stps in initArrayCurves(). Needed for
    // WriteNodeOutput and CheckWaterBalance.
    std::vector<double> actual_pumped_Mm3;
    std::vector<double> pump_cost_euro;
    std::vector<double> pump_power_MW;

    int ReadNodeData(string filename);
    int ReadStateFile(string filename);
    int Simulate(size_t t);
    int CalcPowerAndCost(size_t t);  // Head, power and cost for timestep t. Run after the reservoirs have updated their levels.
    int initArrayCurves(void);
    int CheckWaterBalance(class Herss *herss_obj);
    double GetStartWater_Mm3(void);
    double GetEndWater_Mm3(void);
    int WriteNodeOutput(GlobalConfig *gc);
    int WriteStateFile(FILE *fp);

    void ValidatePumpSettings();  // Mirrors Powerstation::ValidatePowerstationSettings.
    double calcEfficiency(double q_m3s);

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
    size_t nr_pumps;

    double start_water_Mm3;
    double end_water_Mm3;
    double inflow_volume_Mm3;
    double outgoing_Mm3;
    double waterbalance;
    int nodes_idnrs[MAX_NR_NODES];
    double sum_prod_MWh;
    double sum_total_MWh; // Production pluss remaining in whole riversystem
    double adjust_cost;   // Adjustment cost

    // For use in ValueFunction calculations
    //double effective_remaining_available_Mm3;  // Means only water that can be produced 
    //double total_effective_remaining_available_Mm3;  // Means only water that can be produced 
    //double effective_upstream_remaining_available_Mm3; // We accumulate as we go downward.  Only water that is assumed to available for production. 

    double tot_remaining_Mm3;
    double tot_remaining_MWh;
    double tot_remaining_Euro;
    double tot_active_remaining_Mm3;  // Total upstream is included 
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
    double ValueFunction[500];  // We store the value function for each timestep.
    double WaterValue[500];  // We store the water value for each timestep.

    // Array of Nodes (reservoirs, powerstations, channels, pumps)
    Node **nodes;
    Reservoir *reservoirs;
    Powerstation *pstations;
    Channel *channels;
    Pump *pumps;
    double Simulate(int id);
    double CalcVF(double restprice);
    double CalcVF_atEndOfStp(double restprice, size_t stp);  // Calculate the value function at a specific timestep
    double CalcSimulationProfit();
    int WriteRiverSystemData(double restprice);
    void WriteReservoirData();
    void PrintReservoirData2Screen();
    int WriteSelectedOutputMatrix();
    double GetEndingReservoirLevel(size_t r_idnr);
    void PrintEconomicInfo(class Herss *herss_obj);
    void DiagnoseRiversystemConfiguration();   // RUn some checks to see if the configuration of the riversystem is correct.

    
};
/////////////////////////////////////////////////////////////////
// A class that models Input, Scenarios and riversystem
class Herss {
public:
    Herss();
    Herss(GlobalConfig *gc);
    GlobalConfig *gc;
    Dataset *data;  // Store reference to dataset for variable timesteps
    ~Herss();
    size_t dt;          // Delta time step in seconds
    size_t stps;        // How many time steps used in each scenario, and in the optimization step.
    size_t nr_nodes;    
    Riversystem *rs;
    Scenario  **scen;

    int prepaireSimulation(Dataset *data); // Read in final data and set pointers.
    void SetPointers();
    void ReadTopologyFile(string filename);
    int Simulate();
    int CheckWaterBalance();
    int GlobalWaterBalance();
    int WriteNodeOutput();  // Write output for each node
    int WriteStateFile();  // Write output for each node
    int CalcAdjustmenCosts();

    void SetAction(size_t node_idnr, size_t gen_idnr, size_t t, double value);

    double GetAction(size_t node_idnr, size_t gen_idnr, size_t t);
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
    double CalcWaterValue_atEndofStp(size_t t);  // Calculate the water value in the system at a specific timestep
    double GetValueFunction_atStp(size_t t); 
    void SetDate(size_t t, int Y, int M, int D, int H);
    int getDeltaT(size_t timestep);  
    
};
/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////


#endif