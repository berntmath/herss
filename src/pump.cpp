/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     pump.cpp
Developer:    Terje Sandø (pump-station work, July 2026)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

A PUMP moves water from PUMP_SOURCE_IDNR to PUMP_TARGET_IDNR. Herss::Simulate() runs every
node once per timestep in ascending idnr order, so a PUMP node's idnr must be
LOWER than both its source and target. This is enforced in ValidatePumpSettings() below.
The pump can then mutate src/tgt->res_Mm3 directly, and both reservoirs pick up
the change when they run later in the same timestep.

The work is split in two: Simulate() moves the water, and CalcPowerAndCost() runs
at the end of the timestep to compute head, power and cost from the reservoir
levels once they are final.
********************************************************************************/

#include "herss.h"
#include <algorithm>
#include <vector>

Pump::Pump(){
    stps = 0;
    dt   = 0;

    pump_source_idnr           = NOT_INIT;
    pump_target_idnr           = NOT_INIT;
    pump_source_target_in_use  = false;
    ptr_pump_source             = NULL;
    ptr_pump_target             = NULL;

    pump_inlet_masl             = -1.0 * NOT_INIT;
    max_discharge               = -1.0 * NOT_INIT;
    headlosscoef                 = -1.0 * NOT_INIT;
    static_motor_efficiency     = -1.0 * NOT_INIT;
    pump_startstop              = -1.0 * NOT_INIT;
    init_Power                  = -1.0 * NOT_INIT;

    src_start_masl = NOT_INIT;
    src_end_masl   = NOT_INIT;
    tgt_start_masl = NOT_INIT;
    tgt_end_masl   = NOT_INIT;

    use_uniform_normalized_curve = false;
    for(size_t i = 0; i < N_UNIFORM_EFF_CURVE_POINTS; i++) {
        uniform_normalized_curve[i] = 0.0;
    }
}

Pump::~Pump(){}

////////////////////////////////////////////////////////////////
void Pump::ValidatePumpSettings() {

    if(pump_source_idnr == NOT_INIT || pump_target_idnr == NOT_INIT) {
        LOG_WARN("ERROR: PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") is missing PUMP_SOURCE_IDNR and/or PUMP_TARGET_IDNR.");
        LOG_ERR("Check your topology file - a PUMP node must define both PUMP_SOURCE_IDNR and PUMP_TARGET_IDNR.");
    }

    if(pump_source_idnr >= gc->nr_nodes || pump_target_idnr >= gc->nr_nodes) {
        LOG_WARN("ERROR: PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") PUMP_SOURCE_IDNR (" + std::to_string(pump_source_idnr) + ") or PUMP_TARGET_IDNR ("
            + std::to_string(pump_target_idnr) + ") is out of range (nr_nodes=" + std::to_string(gc->nr_nodes) + ").");
        LOG_ERR("Check your topology file - PUMP_SOURCE_IDNR/PUMP_TARGET_IDNR must point to valid node idnrs.");
    }

    if(pump_source_idnr == idnr || pump_target_idnr == idnr) {
        LOG_WARN("ERROR: PUMP node " + std::to_string(int(idnr)) + " (" + nodename + ") points to itself.");
        LOG_ERR("Check your topology file - PUMP_SOURCE_IDNR/PUMP_TARGET_IDNR cannot equal the pump's own idnr.");
    }

    if(pump_source_idnr == pump_target_idnr) {
        LOG_WARN("ERROR: PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") has identical PUMP_SOURCE_IDNR and PUMP_TARGET_IDNR (" + std::to_string(pump_source_idnr) + ").");
        LOG_ERR("Check your topology file - a pump cannot pump a reservoir into itself.");
    }

    if(gc->nodetypes[pump_source_idnr] != NodeType::RESERVOIR) {
        LOG_WARN("ERROR: PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") PUMP_SOURCE_IDNR " + std::to_string(pump_source_idnr) + " is not a RESERVOIR node.");
        LOG_ERR("Check your topology file - PUMP can only draw from a RESERVOIR node.");
    }

    if(gc->nodetypes[pump_target_idnr] != NodeType::RESERVOIR) {
        LOG_WARN("ERROR: PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") PUMP_TARGET_IDNR " + std::to_string(pump_target_idnr) + " is not a RESERVOIR node.");
        LOG_ERR("Check your topology file - PUMP can only deliver to a RESERVOIR node.");
    }

    if(pump_source_idnr <= idnr) {
        LOG_WARN("ERROR: PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") has PUMP_SOURCE_IDNR (" + std::to_string(pump_source_idnr) + ") <= its own idnr.");
        LOG_ERR("Check your topology file - a PUMP's idnr must be strictly lower than PUMP_SOURCE_IDNR. Renumber the topology so the PUMP is numbered before its source reservoir.");
    }

    if(pump_target_idnr <= idnr) {
        LOG_WARN("ERROR: PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") has PUMP_TARGET_IDNR (" + std::to_string(pump_target_idnr) + ") <= its own idnr.");
        LOG_ERR("Check your topology file - a PUMP's idnr must be strictly lower than PUMP_TARGET_IDNR. Renumber the topology so the PUMP is numbered before its target reservoir.");
    }

    if(max_discharge <= 0.0 || max_discharge > VERY_LARGE_NUMBER) {
        LOG_WARN("ERROR: MAX_DISCHARGE for PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") is out of bounds: " + std::to_string(max_discharge));
        LOG_ERR("Check your topology file.");
    }

    if(headlosscoef < 0.0 || headlosscoef > VERY_LARGE_NUMBER) {
        LOG_WARN("ERROR: HEADLOSSCOEF for PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") is out of bounds: " + std::to_string(headlosscoef));
        LOG_ERR("Check your topology file.");
    }

    if(static_motor_efficiency <= 0.0 || static_motor_efficiency > 1.0) {
        LOG_WARN("ERROR: STATIC_MOTOR_EFFICIENCY for PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") is out of bounds: " + std::to_string(static_motor_efficiency));
        LOG_ERR("Check your topology file - STATIC_MOTOR_EFFICIENCY must be in (0.0, 1.0].");
    }

    if(pump_inlet_masl < -1000.0 || pump_inlet_masl > MOUNT_EVEREST_MASL) {
        LOG_WARN("ERROR: PUMP_INLET_MASL for PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") is out of bounds: " + std::to_string(pump_inlet_masl));
        LOG_ERR("Check your topology file.");
    }

    if(pump_startstop < 0.0 || pump_startstop > VERY_LARGE_NUMBER) {
        LOG_WARN("ERROR: PUMP_STARTSTOP for PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") is out of bounds: " + std::to_string(pump_startstop));
        LOG_ERR("Check your topology file - a PUMP node must define PUMP_STARTSTOP, and it cannot be negative.");
    }

    if(!use_uniform_normalized_curve && pump_curve_Q.empty()) {
        LOG_WARN("ERROR: PUMP node " + std::to_string(int(idnr)) + " (" + nodename
            + ") has no efficiency curve defined (neither UNIFORM_NORMALIZED_CURVE nor PUMP_CURVE).");
        LOG_ERR("Check your topology file.");
    }

    if(use_uniform_normalized_curve) {
        for(size_t p = 0; p < N_UNIFORM_EFF_CURVE_POINTS; ++p) {
            if(uniform_normalized_curve[p] < 0.0 || uniform_normalized_curve[p] > 100.0) {
                LOG_WARN("ERROR: Invalid efficiency value in UNIFORM_NORMALIZED_CURVE for PUMP node "
                    + std::to_string(int(idnr)) + " (" + nodename + ") at point " + std::to_string(p)
                    + ": " + std::to_string(uniform_normalized_curve[p]) + ". Efficiency should be between 0 and 100.");
                LOG_ERR("Check your topology file.");
            }
        }
    }

    for(size_t p = 0; p < pump_curve_Q.size(); ++p) {
        if(pump_curve_Q[p] < 0.0) {
            LOG_WARN("ERROR: Negative flow value in PUMP_CURVE for PUMP node " + std::to_string(int(idnr)) + " (" + nodename + ")");
            LOG_ERR("Check your topology file.");
        }
        if(pump_curve_psnt[p] < 0.0 || pump_curve_psnt[p] > 100.0) {
            LOG_WARN("ERROR: Invalid efficiency value in PUMP_CURVE for PUMP node " + std::to_string(int(idnr))
                + " (" + nodename + "). Efficiency should be between 0 and 100.");
            LOG_ERR("Check your topology file.");
        }
    }
}
////////////////////////////////////////////////////////////////
double Pump::calcEfficiency(double q_m3s) {

    if(q_m3s < 0.0) {
        LOG_WARN("ERROR: Discharge is negative for PUMP node " + std::to_string(int(idnr)) + " (" + nodename + "): Q = " + std::to_string(q_m3s));
        LOG_ERR("Check your action file, and make sure the requested discharge for PUMP node " + std::to_string(int(idnr)) + " (" + nodename + ") is not negative");
    }

    if(q_m3s > max_discharge * 1.000001) {
        LOG_WARN("ERROR: Discharge is above maximum for PUMP node " + std::to_string(int(idnr)) + " (" + nodename + "): Q = " + std::to_string(q_m3s) + ", max_discharge = " + std::to_string(max_discharge));
        LOG_ERR("Check your action file, and make sure the discharge for PUMP node " + std::to_string(int(idnr)) + " (" + nodename + ") is not above MAX_DISCHARGE");
    }

    if(q_m3s < 0.000001) {
        return 0.0;
    }

    if(use_uniform_normalized_curve) {
        double Qn = q_m3s / max_discharge;    // normalize to 0-1
        int i = (int)(Qn * (N_UNIFORM_EFF_CURVE_POINTS-1));
        if(i > int(N_UNIFORM_EFF_CURVE_POINTS)-2) {
            i = int(N_UNIFORM_EFF_CURVE_POINTS)-2;  // guard against Qn==1.0 landing exactly on the last point
        }
        double t = Qn * (N_UNIFORM_EFF_CURVE_POINTS-1) - i;
        double eta = uniform_normalized_curve[i] + t * (uniform_normalized_curve[i+1] - uniform_normalized_curve[i]);
        return eta / 100.0;
    }

    return pump_eff_curve.x2y(q_m3s) / 100.0;
}
////////////////////////////////////////////////////////////////
int Pump::Simulate(size_t t) {

    this->dt   = S->dt;
    this->stps = S->stps;

    Reservoir* src = static_cast<Reservoir*>(ptr_pump_source);
    Reservoir* tgt = static_cast<Reservoir*>(ptr_pump_target);

    double action_val = S->action[t][this->idnr];

    if(action_val < -0.000001 || action_val > 1.000001) {
        LOG_WARN("ERROR: Action for PUMP node " + std::to_string(int(idnr)) + " (" + nodename + ") is out of bounds: " + std::to_string(action_val));
        LOG_ERR("Check your action file, and make sure the action for the PUMP is between 0.0 and 1.0");
    }

    double requested_m3s = std::max(0.0, action_val) * max_discharge;
    double requested_Mm3 = MACRO_m3s_2_Mm3(requested_m3s, S->dt);

    // Physical limit: water below PUMP_INLET_MASL in the source reservoir cannot be reached.
    double filling_at_inlet_Mm3 = 0.0;
    if(src->use_reservoir_geometry) {
        filling_at_inlet_Mm3 = src->calcResVolume(this->pump_inlet_masl);
    } else if(src->use_reservoir_curve) {
        filling_at_inlet_Mm3 = src->ac_res_masl_2_Mm3.x2y(this->pump_inlet_masl);
    }

    double available_Mm3 = std::max(0.0, src->res_Mm3 - filling_at_inlet_Mm3);
    double actual_Mm3    = std::min(requested_Mm3, available_Mm3);
    double actual_m3s    = MACRO_Mm3_2_m3s(actual_Mm3, S->dt);

    if(actual_Mm3 < requested_Mm3 - 1e-6 && requested_Mm3 > 1e-6) {
        LOG_WARN("PUMP_INLET_MASL: Cannot deliver requested pumping for PUMP node " + std::to_string(int(idnr))
            + " (" + nodename + ") at timestep " + std::to_string(t)
            + ". Requested: " + std::to_string(MACRO_Mm3_2_m3s(requested_Mm3, S->dt))
            + " m3/s, actual: " + std::to_string(actual_m3s)
            + " m3/s (limited by PUMP_INLET_MASL / available water above it in the source reservoir).");
    }

    // Draw water from the source
    src->res_Mm3 -= actual_Mm3;
    src->S->pump_out_m3s[t] += actual_m3s;

    // Fill the water to the target
    tgt->res_Mm3 += actual_Mm3;
    tgt->S->pump_in_m3s[t] += actual_m3s;

    S->income[t]       = 0.0;
    S->tot_outflow[t]  = actual_m3s;
    S->inflow[t]       = 0.0;

    remaining_Mm3        = 0.0;  // A pump can never store water.
    remaining_active_Mm3 = 0.0;

    if(t < actual_pumped_Mm3.size()) {
        actual_pumped_Mm3[t] = actual_Mm3;
    }

    return 0;
}
////////////////////////////////////////////////////////////////
// Run at the end of timestep t, when the source and target reservoirs have
// finished updating their levels for that timestep.
int Pump::CalcPowerAndCost(size_t t) {

    if(t >= actual_pumped_Mm3.size()) {
        return 0;
    }

    double power_MW   = 0.0;
    double actual_m3s = MACRO_Mm3_2_m3s(actual_pumped_Mm3[t], S->dt);

    // Average of the start and end level gives the head representing the timestep.
    double src_avg_masl = (src_start_masl + src_end_masl) / 2.0;
    double tgt_avg_masl = (tgt_start_masl + tgt_end_masl) / 2.0;

    double Hbrutto = tgt_avg_masl - src_avg_masl;
    double Hnetto  = Hbrutto + this->headlosscoef * actual_m3s * actual_m3s;

    // A negative net head means the target reservoir is already lower than the source, so
    // the pump would run for free. That is not a physically meaningful pump setup.
    if(Hnetto < 0.0) {
        LOG_WARN("ERROR: Negative net head (" + std::to_string(Hnetto) + " m) for PUMP node "
            + std::to_string(int(idnr)) + " (" + nodename + ") at timestep " + std::to_string(t)
            + " - the target reservoir is lower than the source.");
        LOG_ERR("Check your topology file - PUMP node " + std::to_string(int(idnr))
            + " (" + nodename + ") must lift water to a higher reservoir.");
    }

    double turbine_efficiency = this->calcEfficiency(actual_m3s);

    if(turbine_efficiency <= 0.0 && actual_m3s > 0.000001) {
        LOG_WARN("ERROR: Pump efficiency is not working properly for PUMP node " + std::to_string(int(idnr)) + " (" + nodename + ")\n");
        LOG_WARN("Please revisit your topology file (PUMP_CURVE / UNIFORM_NORMALIZED_CURVE)\n");
        LOG_ERR("In timestep t = " + std::to_string(t));
    }

    double P_hydraulic_MW = 1000.0 * GRAVITY * Hnetto * actual_m3s / 1000000.0;  // MW
    if(turbine_efficiency > 0.0) {
        power_MW = P_hydraulic_MW / (turbine_efficiency * this->static_motor_efficiency);  // MW drawn from the grid
    }

    double energy_MWh = power_MW * S->dt / 3600.0;
    double cost = energy_MWh * S->price[t];

    // Now we check for start and stop costs
    // We penalise when starting and stopping.
    double startstopCost = 0.0;

    double previous_action = 0.0;
    if (t > 0) {
        previous_action = S->action[t-1][this->idnr];
    } else {
        // At t = 0 the previous state comes from the statefile, so a pump that was already
        // running at the end of the previous run is not charged a start cost.
        previous_action = (init_Power > 0.01) ? 1.0 : 0.0;
    }
    double current_action = S->action[t][this->idnr];

    if ((previous_action > 0.01) != (current_action > 0.01)) {
        startstopCost += this->pump_startstop / 2.0;
    }

    S->cost[t]          = cost + startstopCost;
    S->profit[t]        = S->income[t] - S->cost[t];
    S->startStopCost[t] = startstopCost;
    S->Power[t]         = -energy_MWh;  // Energy consumption [MWh], same unit as a powerstation.
    S->Hbrutto[t]       = Hbrutto;
    S->Hnetto[t]        = Hnetto;

    pump_cost_euro[t]  = cost;
    pump_power_MW[t]   = power_MW;

    return 0;
}
////////////////////////////////////////////////////////////////
int Pump::initArrayCurves(void) {
    this->stps = S->stps;
    actual_pumped_Mm3.assign(stps, 0.0);
    pump_cost_euro.assign(stps, 0.0);
    pump_power_MW.assign(stps, 0.0);
    return 0;
}
////////////////////////////////////////////////////////////////
double Pump::GetStartWater_Mm3(void) { return 0.0; }
double Pump::GetEndWater_Mm3(void)   { return 0.0; }
////////////////////////////////////////////////////////////////
int Pump::CheckWaterBalance(class Herss *herss_obj) {
    return 0;
}
////////////////////////////////////////////////////////////////
int Pump::ReadStateFile(string filename) {

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
            // Line is not empty and doesn't start with # (hash/pound sign)
            string str_tmpline = line;  // Create a copy of the line for parsing
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);

            if ( keyword.compare("NODE") == 0 && value.compare("PUMP") == 0 ) {

                size_t n_cols = line_obj.calcNrCols(&str_tmpline);

                // There must be 5 columns in the line for it to be valid.
                if(n_cols != 5) {
                    LOG_WARN("Invalid line in statefile " + filename + ": " + str_tmpline);
                    LOG_WARN("Pump::ReadStateFile  nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype));
                    LOG_ERR("Expected 5 columns for NODE PUMP, but got " + std::to_string(n_cols));
                }

                token = line_obj.extractNextElementFromLine(&line);
                tmp_idnr = atoi(token.c_str() );
                keyword = line_obj.extractNextElementFromLine(&line);

                if(tmp_idnr == idnr && keyword == nodename) {
                    value   = line_obj.extractNextElementFromLine(&line);
                    // NODE PUMP
                    init_Power = atof( value.c_str() );
                    found_node = true;
                }
            }
        }
    }
    myfile.close();
    if(!found_node) {
        LOG_WARN("Pump::ReadStateFile     idnr= " + std::to_string(int(idnr)) + "  nodename=" + nodename);
        LOG_INFO("This could have been caused by indexing of your nodes.");
        LOG_INFO("Start with zero at the top, and work your way down, to the outlet.");
        LOG_WARN("This could have been caused by indexing of your nodes.");
        LOG_WARN("Start with zero at the top, and work your way down, to the outlet.");
        LOG_ERR("There is something wrong in the statefile " + filename);
    }
    return 0;
}
////////////////////////////////////////////////////////////////
int Pump::WriteStateFile(FILE *fp) {
    // Keep statefile row format aligned with other node types.
    // S->Power is negative for a pump because it consumes, so the sign is flipped here to
    // give the same positive "was it running?" convention [MWh] as a powerstation.
    fprintf(fp, "NODE PUMP %d %s %.5f\n", int(idnr), nodename.c_str(), -1.0 * this->S->Power[S->stps-1]);
    return 0;
}
////////////////////////////////////////////////////////////////
int Pump::WriteNodeOutput(GlobalConfig *gc) {

    string filename = gc->outputdir + "node" + std::to_string(int(idnr)) + "_" + nodename + ".txt";
    FILE *fp;
    if((fp = fopen(filename.c_str(), "w")) == NULL) {
        LOG_ERR("Cannot open file " + filename);
    }

    fprintf(fp, "# PUMP node %d (%s)\n", int(idnr), nodename.c_str());
    fprintf(fp, "# PUMP_SOURCE_IDNR = %d, PUMP_TARGET_IDNR = %d\n", int(pump_source_idnr), int(pump_target_idnr));
    fprintf(fp, "yyyy mm dd hh Pumped_m3s Power_MW Cost_Euro startstopCost\n");

    for(size_t t = 0; t < stps; t++) {
        fprintf(fp, "%d %02d %02d %02d %.6f %.6f %.6f %.6f\n",
            S->year[t], S->month[t], S->day[t], S->hour[t],
            MACRO_Mm3_2_m3s(actual_pumped_Mm3[t], S->dt),
            pump_power_MW[t],
            pump_cost_euro[t],
            S->startStopCost[t]);
    }

    fclose(fp);
    return 0;
}
////////////////////////////////////////////////////////////////
int Pump::ReadNodeData(string filename) {

    ifstream myfile;
    string line;
    string keyword;
    string value;
    Line line_obj;
    size_t tmp_idnr;
    string token;

    bool inside_node = false;

    for (size_t i = 0; i < gc->topoparser.getLineCount(); ++i) {

        line = gc->topoparser.getLine(i);
        string tmpline = line;

        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);

        if (keyword.compare("ENDNODE") == 0) {
            inside_node = false;
        }

        if (keyword.compare("NODE") == 0 && value.compare("PUMP") == 0) {
            token = line_obj.extractNextElementFromLine(&line);
            tmp_idnr = atoi(token.c_str());

            if(tmp_idnr == idnr) {

                inside_node = true;
                size_t k = i + 1;

                while(inside_node) {

                    if (k >= gc->topoparser.getLineCount()) {
                        LOG_ERR("ERROR: Reached end of topology file (" + filename + ") while looking for node data.");
                    }

                    line = gc->topoparser.getLine(k);
                    string tmpline2 = line;
                    string keyword2 = line_obj.extractNextElementFromLine(&line);
                    string value2   = line_obj.extractNextElementFromLine(&line);

                    if(keyword2.compare("ENDNODE") == 0) {
                        inside_node = false;
                        break;
                    }

                    // PUMP_SOURCE_IDNR
                    if(keyword2.compare("PUMP_SOURCE_IDNR") == 0) {
                        int tmp = atoi(value2.c_str());
                        if(tmp < 0) {
                            LOG_WARN("ERROR: PUMP_SOURCE_IDNR is negative for PUMP node " + std::to_string(int(idnr)) + " (" + nodename + ").");
                            LOG_ERR("Check your topology file - PUMP_SOURCE_IDNR must be a valid, non-negative node idnr.");
                        }
                        pump_source_idnr = size_t(tmp);
                    }

                    // PUMP_TARGET_IDNR
                    if(keyword2.compare("PUMP_TARGET_IDNR") == 0) {
                        int tmp = atoi(value2.c_str());
                        if(tmp < 0) {
                            LOG_WARN("ERROR: PUMP_TARGET_IDNR is negative for PUMP node " + std::to_string(int(idnr)) + " (" + nodename + ").");
                            LOG_ERR("Check your topology file - PUMP_TARGET_IDNR must be a valid, non-negative node idnr.");
                        }
                        pump_target_idnr = size_t(tmp);
                    }

                    // PUMP_INLET_MASL
                    if(keyword2.compare("PUMP_INLET_MASL") == 0) {
                        pump_inlet_masl = atof(value2.c_str());
                    }

                    // PUMP_STARTSTOP
                    if(keyword2.compare("PUMP_STARTSTOP") == 0) {
                        this->pump_startstop = atof(value2.c_str());
                    }

                    // MAX_DISCHARGE
                    if(keyword2.compare("MAX_DISCHARGE") == 0) {
                        max_discharge = atof(value2.c_str());
                    }

                    // HEADLOSSCOEF
                    if(keyword2.compare("HEADLOSSCOEF") == 0) {
                        headlosscoef = atof(value2.c_str());
                    }

                    // STATIC_MOTOR_EFFICIENCY
                    if(keyword2.compare("STATIC_MOTOR_EFFICIENCY") == 0) {
                        static_motor_efficiency = atof(value2.c_str());
                    }

                    // UNIFORM_NORMALIZED_CURVE <n_points>
                    // <followed by n_points lines of "Qn  efficiency_percent">
                    if(keyword2.compare("UNIFORM_NORMALIZED_CURVE") == 0) {

                        if(stoi(value2) != N_UNIFORM_EFF_CURVE_POINTS) {
                            LOG_WARN("ERROR: Expected " + to_string(N_UNIFORM_EFF_CURVE_POINTS)
                                + " points for UNIFORM_NORMALIZED_CURVE for PUMP node " + std::to_string(int(idnr)));
                            LOG_ERR("Check your topology file.");
                        }

                        use_uniform_normalized_curve = true;

                        // Keep k unchanged; the outer loop skips these data lines.
                        for (size_t p = 0; p < N_UNIFORM_EFF_CURVE_POINTS; ++p) {
                            line = gc->topoparser.getLine(k + p + 1);
                            string kw3 = line_obj.extractNextElementFromLine(&line);
                            string v3  = line_obj.extractNextElementFromLine(&line);
                            uniform_normalized_curve[p] = atof(v3.c_str());
                        }
                    } // End if UNIFORM_NORMALIZED_CURVE

                    // PUMP_CURVE <n_points>
                    // <followed by n_points lines of "Q_m3s  efficiency_percent">
                    if(keyword2.compare("PUMP_CURVE") == 0) {

                        size_t n_points = size_t(atoi(value2.c_str()));
                        pump_curve_Q.resize(n_points);
                        pump_curve_psnt.resize(n_points);

                        for (size_t p = 0; p < n_points; ++p) {
                            line = gc->topoparser.getLine(k + p + 1);
                            string kw3 = line_obj.extractNextElementFromLine(&line);
                            string v3  = line_obj.extractNextElementFromLine(&line);
                            pump_curve_Q[p]    = atof(kw3.c_str());
                            pump_curve_psnt[p] = atof(v3.c_str());
                        }

                        pump_eff_curve.nr_pts = int(n_points);
                        for (size_t p = 0; p < n_points; ++p) {
                            pump_eff_curve.x_points[p] = pump_curve_Q[p];
                            pump_eff_curve.y_points[p] = pump_curve_psnt[p];
                        }
                        pump_eff_curve.initializeArrays();
                    } // End if PUMP_CURVE

                    k++;
                }
            }
        }
    }

    if(pump_source_idnr != NOT_INIT && pump_target_idnr != NOT_INIT) {
        pump_source_target_in_use = true;
    }

    return 0;
}
