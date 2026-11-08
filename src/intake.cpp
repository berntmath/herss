/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     intake.cpp
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

********************************************************************************/

#include "herss.h"

//-----------------------------------------------------------------------
Intake::Intake(){

    intake_volume_init_Mm3  = NOT_INIT;
    intake_volume_init_fr   = NOT_INIT;

    intake_Mm3   = NOT_INIT;      //  Intake filling [Mm3]
    intake_fr    = NOT_INIT;      //  Intake filling as a fraction of full.

    floodwater_down_idnr    = NOT_INIT;
    capture_capacity_m3s    = NOT_INIT; 
    capcap_down_idnr        = NOT_INIT;
    hatch_release_down_idnr = NOT_INIT;
    const_hatch_release_m3s = NOT_INIT;
    max_volume_Mm3          = NOT_INIT;

    stps = NOT_INIT;
    dt   = NOT_INIT;
}
//-----------------------------------------------------------------------
Intake::~Intake(){ }
//-----------------------------------------------------------------------
// We check if the settings for the channel are valid
void Intake::ValidateIntakeSettings() { }

//-----------------------------------------------------------------------
int Intake::ReadNodeData(string filename){

	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;
    size_t tmp_idnr;
    string token;

    // We have the whole topology file saved in the Topoparser in GlobalConfig. 
    // We first extract single value variables. 
    // Then we extract the reservoir curve and overflow curve.
    bool inside_node = false;


    // Single value variables 
    for (size_t i = 0; i < gc->topoparser.getLineCount(); ++i) {
        line = gc->topoparser.getLine(i);

        string tmpline = line;  // Create a copy of the line for parsing

        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);

        if ( keyword.compare("ENDNODE") == 0 ) {
            inside_node = false;
        }

        if ( keyword.compare("NODE") == 0 && value.compare("INTAKE") == 0 ) {

            token = line_obj.extractNextElementFromLine(&line);
            tmp_idnr = atoi(token.c_str() );

            if(tmp_idnr == idnr) {

                // Now we are inside the correct node, we can extract the variables. 
                // We continue to loop over the lines until we find the next node, then we stop.  
                inside_node = true;

                size_t k = i + 1;  // Start looking from the next line
                while(inside_node) {

                    if (k >= gc->topoparser.getLineCount()) {
                        LOG_ERR("ERROR: Reached end of topology file (" + filename + ") while looking for node data.");
                    }

                    line = gc->topoparser.getLine(k);

                    string tmpline2 = line;  // Create a copy of the line for parsing
                    string keyword2 = line_obj.extractNextElementFromLine(&line);
                    string value2   = line_obj.extractNextElementFromLine(&line);

                    if (keyword2 == "FLOODWATER_IDNR") {
                        floodwater_down_idnr = size_t(atoi(value2.c_str()));
                     }

                    if (keyword2 == "FLOODWATER_IDNR") {
                        size_t parsed_chars = 0;
                        int node_id;
                        try {
                            node_id = std::stoi(value2, &parsed_chars);
                        } catch (const std::exception&) {
                            LOG_ERR("FLOODWATER_IDNR must be an integer: " + value2);
                            return -1;
                        }
                        if (parsed_chars != value2.size() || node_id < 0 || static_cast<size_t>(node_id) >= gc->nr_nodes) {
                            LOG_ERR("FLOODWATER_IDNR is not a valid node ID: " + value2);
                            return -1;
                        }
                        floodwater_down_idnr = static_cast<size_t>(node_id);
                    }

                    // Check for capture capacity
                    if (keyword2 == "CAPTURE_CAPACITY") {
                        capture_capacity_m3s = std::stod(value2);

                        if(capture_capacity_m3s < 0.0) {
                            LOG_ERR("CAPTURE_CAPACITY must be non-negative: " + value2);
                            return -1;
                        }
                    }

                    // Id number to downstream node receiving captured water
                    // CAPCAP_DOWN_IDNR 1 
                    if (keyword2 == "CAPCAP_DOWN_IDNR") {
                        size_t parsed_chars = 0;
                        int node_id;
                        try {
                            node_id = std::stoi(value2, &parsed_chars);
                        } catch (const std::exception&) {
                            LOG_ERR("CAPCAP_DOWN_IDNR must be an integer: " + value2);
                            return -1;
                        }
                        if (parsed_chars != value2.size() || node_id < 0 || static_cast<size_t>(node_id) >= gc->nr_nodes) {
                            LOG_ERR("CAPCAP_DOWN_IDNR is not a valid node ID: " + value2);
                            return -1;
                        }
                        capcap_down_idnr = static_cast<size_t>(node_id);
                    }


                    // Amount of water released until intake is empty [m3/s] 
                    // CONST_HATCH_RELEASE 0.5
                    if (keyword2 == "CONST_HATCH_RELEASE") {
                        const_hatch_release_m3s = std::stod(value2);

                        if(const_hatch_release_m3s < 0.0) {
                            LOG_ERR("CONST_HATCH_RELEASE must be non-negative: " + value2);
                            return -1;
                        }
                    }


                    // Id number of downstream node receiving the constant flow from hatch 
                    // HATCHREL_DOWN_IDNR 3
                    if (keyword2 == "HATCHREL_DOWN_IDNR") {
                        size_t parsed_chars = 0;
                        int node_id;
                        try {
                            node_id = std::stoi(value2, &parsed_chars);
                        } catch (const std::exception&) {
                            LOG_ERR("HATCHREL_DOWN_IDNR must be an integer: " + value2);
                            return -1;
                        }
                        if (parsed_chars != value2.size() || node_id < 0 || static_cast<size_t>(node_id) >= gc->nr_nodes) {
                            LOG_ERR("HATCHREL_DOWN_IDNR is not a valid node ID: " + value2);
                            return -1;
                        }
                        hatch_release_down_idnr = static_cast<size_t>(node_id);
                    }
                    
                    // Volume of intake (reservoir), at a Qmin flowrate at 0.5 m3/s, this is 4-5 days of flow. 
                    // VOLUME 0.2
                    if (keyword2 == "VOLUME") {
                        max_volume_Mm3 = std::stod(value2);

                        if(max_volume_Mm3 < 0.0) {
                            LOG_ERR("VOLUME must be non-negative: " + value2);
                            return -1;
                        }
                    }

                    if (keyword2.compare("ENDNODE") == 0) {
                        inside_node = false;
                        break;
                    }
                    
                    k++; // Move to the next line for the next iteration
                }
            }
        }
    }
    return 0;
}
//------------------------------------------------------------------------

//------------------------------------------------------------------------
int Intake::ReadStateFile(string filename) {
    
    // printf("file: %s  line: %d  function: %s\n", __FILE__, __LINE__, HERSS_FUNCTION_SIGNATURE);

    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_ERR("The statefile " + filename + " could not be found/opened");
        return -1;
    }

    std::string line;
    bool found_node = false;

    while (std::getline(file, line)) {
        std::istringstream row(line);
        std::string record;
        std::string type;

        if (!(row >> record >> type) || record != "NODE" || type != "INTAKE") {
            continue;
        }

        long long state_id;
        std::string state_name;
        std::string fraction_text;
        std::string extra;

        if (!(row >> state_id >> state_name >> fraction_text) || (row >> extra)) {
            LOG_ERR("Invalid INTAKE row in statefile: " + line);
            return -1;
        }

        if (state_id < 0) {
            LOG_ERR("Negative INTAKE node ID in statefile: " + line);
            return -1;
        }

        if (static_cast<size_t>(state_id) != idnr || state_name != nodename) {
            continue;
        }

        if (found_node) {
            LOG_ERR("Duplicate INTAKE entry in statefile for node " +
                    std::to_string(idnr));
            return -1;
        }

        size_t parsed_chars = 0;
        double fraction;
        try {
            fraction = std::stod(fraction_text, &parsed_chars);
        } catch (const std::exception&) {
            LOG_ERR("Invalid INTAKE fraction in statefile: " + fraction_text);
            return -1;
        }

        if (parsed_chars != fraction_text.size() ||
            fraction < 0.0 || fraction > 1.0) {
            LOG_ERR("INTAKE fraction must be between 0 and 1: " + fraction_text);
            return -1;
        }

        intake_volume_init_fr = fraction;
        found_node = true;
    }

    if (!found_node) {
        LOG_ERR("No matching INTAKE entry in statefile for node " +
                std::to_string(idnr) + " (" + nodename + ")");
        return -1;
    }

    return 0;
}
//------------------------------------------------------------------------
int Intake::Backward(size_t t) {
    // Implement the backward pass for the intake here.
    // This function should reverse the simulation for timestep t.
    return 0;
}
////////////////////////////////////////////////////////////////
int Intake::Simulate(size_t t) {

    // printf("t=%zu  file: %s  line: %d  function: %s\n", t, __FILE__, __LINE__, HERSS_FUNCTION_SIGNATURE);

    this->dt     = S->dt;
    this->stps   = S->stps;

    S->channel_storage_Mm3[t] = 0.0;    // To void warnings. 

    // Safe guard, put this someweher else to minimize compute.
    if(ptr_downstream_hatch_release_node == nullptr) {
        LOG_ERR("Downstream hatch release node is not set!");
    }

    if(ptr_downstream_capcap_node == nullptr) {
        LOG_ERR("Downstream capture-capacity node is not set!");
    }

    if(ptr_downstream_floodwater_node == nullptr) {
        LOG_ERR("Downstream floodwater node is not set!");
    }


    double initial_storage_m3 = intake_Mm3 * 1.0e6; // Convert Mm3 to m3
    double q_in = S->inflow[t]; // m3/s
    double total_in_m3 = q_in * dt; // Total inflow in m3

    // Add inflow to the intake volume
    intake_Mm3 += MACRO_m3s_2_Mm3(q_in, dt); // Convert m3/s * to Mm3

    // Calc hatch release and subtract from the intake volume
    double hatch_water_need_Mm3 = MACRO_m3s_2_Mm3(const_hatch_release_m3s, dt);
    double hatch_flow_m3s = 0.0;

    if( intake_Mm3 > hatch_water_need_Mm3) {
        hatch_flow_m3s = const_hatch_release_m3s;
    } else {
        hatch_flow_m3s = 0.0; 
    }
    
    double hatch_flow_Mm3 = MACRO_m3s_2_Mm3(hatch_flow_m3s, dt);

    this->ptr_downstream_hatch_release_node->S->up_inflow[t] += hatch_flow_m3s;

    intake_Mm3 -= hatch_flow_Mm3; // Subtract the hatch flow from the intake volume

    if(intake_Mm3 < 0.0) {
        intake_Mm3 = 0.0; // Ensure intake volume does not go negative
    }


    intake_fr = intake_Mm3 / max_volume_Mm3;

    // Now we capture water up to the capture capacity, if there is still more water we have flooding. 
    double capture_capacity_Mm3 = MACRO_m3s_2_Mm3(capture_capacity_m3s, dt);
    
    double available_water_above_max =  intake_Mm3 - max_volume_Mm3;

    if(available_water_above_max < 0.0) {
        available_water_above_max = 0.0;
    }

    double captured_Mm3 = 0.0;

    if(available_water_above_max > 0.0) {
        captured_Mm3 = std::min(capture_capacity_Mm3, available_water_above_max);
        intake_Mm3 -= captured_Mm3;
    }
   
    
    double captured_m3s = MACRO_Mm3_2_m3s(captured_Mm3, dt);
    ptr_downstream_capcap_node->S->up_inflow[t] += captured_m3s; // Add captured water to downstream capture capacity node

    intake_fr = intake_Mm3 / max_volume_Mm3;

    double flooding_Mm3 = std::max(0.0, available_water_above_max - captured_Mm3);
    double flooding_m3s = MACRO_Mm3_2_m3s(flooding_Mm3, dt);

    intake_Mm3 -= flooding_Mm3;
    if(intake_Mm3 < 0.0) {
        intake_Mm3 = 0.0; // Ensure intake volume does not go negative after flooding
    }
    intake_fr = intake_Mm3 / max_volume_Mm3;

    ptr_downstream_floodwater_node->S->up_inflow[t] += flooding_m3s; // Add flooding water to downstream flood node
   
    double total_out_m3 = (hatch_flow_m3s*dt); // Total outflow in m3

    total_out_m3 += (captured_m3s*dt); // Add captured water to total outflow
    total_out_m3 += (flooding_m3s*dt); // Add flooding water to total outflow

    double final_storage_m3 = intake_Mm3 * 1e6; // Convert intake volume from Mm3 to m3
    double water_balance_m3 = initial_storage_m3 + total_in_m3 - total_out_m3 - final_storage_m3;
   
    if(std::abs(water_balance_m3) > 0.0001) {
        LOG_WARN("WARNING: Water balance check FAILED!"); 
        LOG_WARN("WB = " + std::to_string(water_balance_m3));
        printf("t=%zu  file: %s  line: %d  function: %s\n", t, __FILE__, __LINE__, HERSS_FUNCTION_SIGNATURE);
        cout << "Intake: " << nodename << " (IDNR " << idnr << ")\n";
        LOG_ERR("WB = " + std::to_string(water_balance_m3));
    }

    S->tot_outflow[t] = total_out_m3 / dt; // Convert total outflow from m3 to m3/s
    S->intake_hatchflow_m3s[t]     = hatch_flow_m3s;
    S->intake_captured_flow_m3s[t] = captured_m3s;
    S->intake_flooding_m3s[t]      = flooding_m3s;
    S->intake_storage_Mm3[t] = final_storage_m3 / 1000000.0; // Convert m3 to Mm3

    // WORK IN PROGRESS
    // if(this->downstream_node_in_use) {
    //     this->ptr_downstream_node->S->up_inflow[t] += S->tot_outflow[t];
    // }

    // For now we dont put value on the water in the Intake 
    remaining_active_Mm3 = 0.0;

    S->income[t]  = 0.0;  // No income in Intakes
    S->cost[t]    = 0.0;
    S->Power[t]   = 0.0;  // No power production in Intakes
    S->profit[t]  = 0.0;


    return 0;
}
//------------------------------------------------------------------------
void Intake::InitIntake() {
    // Initialize the intake with the initial volume fraction
    intake_volume_init_Mm3 = intake_volume_init_fr * max_volume_Mm3;
    intake_Mm3 = intake_volume_init_Mm3;
    intake_fr = intake_volume_init_fr;
}
//------------------------------------------------------------------------
int Intake::CheckWaterBalance(class Herss *herss_obj) {

    // printf("File: %s  line: %d  function: %s\n", __FILE__, __LINE__, HERSS_FUNCTION_SIGNATURE);

    double sum_inflow_Mm3       = 0.0;
    double sum_outflow_Mm3      = 0.0;
    double start_intake_Mm3 = this->intake_volume_init_Mm3;
    double end_intake_Mm3 = this->intake_Mm3; 

    for (size_t t = 0; t < S->stps; ++t) {
        const double dt_seconds = herss_obj->getDeltaT(t);
        sum_inflow_Mm3 += S->inflow[t] * dt_seconds / 1000000.0;
        sum_outflow_Mm3 += S->tot_outflow[t] * dt_seconds / 1000000.0;
    }

    double waterbalance = start_intake_Mm3 + sum_inflow_Mm3 - end_intake_Mm3 - sum_outflow_Mm3;

    if(WATERBALANCE_WARNINGS) {
        LOG_INFO("INTAKE WATERBALANCE for idnr=" + std::to_string(int(idnr)) + "   nodename=" + nodename);
        LOG_INFO("start_intake_Mm3 = " + std::to_string(start_intake_Mm3));
        LOG_INFO("sum_inflow_Mm3    = " + std::to_string(sum_inflow_Mm3));
        LOG_INFO("sum_outflow_Mm3   = " + std::to_string(sum_outflow_Mm3));
        LOG_INFO("end_intake_Mm3   = " + std::to_string(end_intake_Mm3));
        LOG_INFO("waterbalance      = " + std::to_string(waterbalance));
    }

    if(abs(waterbalance) > 0.0001) {
        LOG_WARN("WATERBALANCE INTAKE ERROR for idnr=" + std::to_string(int(idnr)) + "   nodename=" + nodename);
        LOG_WARN("start_intake_Mm3 = " + std::to_string(start_intake_Mm3));
        LOG_WARN("sum_inflow_Mm3    = " + std::to_string(sum_inflow_Mm3));
        LOG_WARN("sum_outflow_Mm3   = " + std::to_string(sum_outflow_Mm3));
        LOG_WARN("end_intake_Mm3   = " + std::to_string(end_intake_Mm3));
        LOG_WARN("waterbalance      = " + std::to_string(waterbalance));
        LOG_ERR ("INTAKE ERROR idnr=" + nodename + " " + std::to_string(int(idnr)) + nodename.c_str()  );
    }

    return 0;
}
//-------------------------------------------------------------------------------------------------
double Intake::GetStartWater_Mm3() {
    return intake_volume_init_Mm3;
}

//-------------------------------------------------------------------------------------------------
double Intake::GetEndWater_Mm3() {
    return intake_Mm3;
}
//-------------------------------------------------------------------------------------------------


int Intake::WriteNodeOutput(GlobalConfig *gc) {  // Write output for each node 

    const std::string filename =
        gc->outputdir + "node" + std::to_string(idnr) + "_" + nodename + ".txt";

    FILE* file = fopen(filename.c_str(), "w");
    if (file == nullptr) {
        LOG_ERR("Cannot open file " + filename);
        return -1;
    }

    fprintf(file, "# INTAKE node %zu %s\n", idnr, nodename.c_str());
    fprintf(file, "# Initial_storage_Mm3 = %.5f\n", intake_volume_init_Mm3);
    fprintf(file, "# Initial_fraction = %.5f\n", intake_volume_init_fr);
    fprintf(file, "# Maximum_volume_Mm3 = %.5f\n", max_volume_Mm3);
    fprintf(file, "# Capture_capacity_m3s = %.5f\n", capture_capacity_m3s);
    fprintf(file, "# Constant_hatch_release_m3s = %.5f\n", const_hatch_release_m3s);

    auto write_downstream = [file](const char* label, const Node* node) {
        if (node != nullptr) {
            fprintf(file, "# %s downstream = %s (IDNR %zu)\n",
                    label, node->nodename.c_str(), node->idnr);
        } else {
            fprintf(file, "# %s downstream = not set\n", label);
        }
    };

    write_downstream("Floodwater", ptr_downstream_floodwater_node);
    write_downstream("Capture capacity", ptr_downstream_capcap_node);
    write_downstream("Hatch release", ptr_downstream_hatch_release_node);

    fprintf(file,
            "yyyy mm dd hh inflow_m3s storage_Mm3 fill_fraction "
            "hatch_release_m3s captured_m3s flooding_m3s total_outflow_m3s\n");

    for (size_t t = 0; t < S->stps; ++t) {
        const double storage_Mm3 = S->intake_storage_Mm3[t];
        const double fill_fraction =
            max_volume_Mm3 > 0.0 ? storage_Mm3 / max_volume_Mm3 : 0.0;

        fprintf(file, "%d %02d %02d %02d ", S->year[t], S->month[t], S->day[t], S->hour[t]);

        fprintf(file, "%.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
                S->inflow[t],
                storage_Mm3,
                fill_fraction,
                S->intake_hatchflow_m3s[t],
                S->intake_captured_flow_m3s[t],
                S->intake_flooding_m3s[t],
                S->tot_outflow[t]);
    }

    fclose(file);
 
    return 0;
}

//-------------------------------------------------------------------------------------------------
int Intake::WriteStateFile(FILE* fp) {
    if (fp == nullptr) {
        LOG_ERR("File pointer is null for " + nodename);
        return -1;
    }

    if (!std::isfinite(intake_fr) || intake_fr < 0.0 || intake_fr > 1.0) {
        LOG_ERR("Invalid final intake fill fraction for " + nodename);
        return -1;
    }

    fprintf(fp, "NODE INTAKE %zu %s %.5f\n", idnr, nodename.c_str(), intake_fr);
    return 0;
}
//-------------------------------------------------------------------------------------------------

