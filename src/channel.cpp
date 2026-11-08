/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     channel.cpp
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

********************************************************************************/

#include "herss.h"
#include "naturalchannel.h"



//-----------------------------------------------------------------------
Channel::Channel(){

    this->K_traveltime_hours      = NOT_INIT;
    this->num_cascaded_reservoirs = NOT_INIT;
    this->casc_reservoirs         = nullptr;
    this->routing_delay_hours     = NOT_INIT;

    for(size_t t = 0; t < MAX_TRAVELTIME_HOURS; t++) {
        waterflow_m3[t]        = NOT_INIT;
        init_waterflow_m3[t]   = NOT_INIT;
    }
    
    this->n_reaches = NOT_INIT;
    this->referenceFlow = NOT_INIT;
    this->referenceTravelTime = NOT_INIT;
    this->nonlinearity = NOT_INIT;

    this->manning_length_m = NOT_INIT;
    this->manning_bottom_width_m = NOT_INIT;
    this->manning_side_slope = NOT_INIT;
    this->manning_slope = NOT_INIT;
    this->manning_roughness_n = NOT_INIT;
    this->manning_initial_flow = NOT_INIT;


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

    // std::cout << "Channel::ValidateChannelSettings routing method: " << RoutingEnumToString(this->routing_method) << "\n";
    
    if(routing_method == RoutingMethod::A_CASCADEDLINRES || routing_method == RoutingMethod::B_LAGGED_CASCADEDLINRES) {
        // We allocate memory to the cascaded reservoirs if we have not done it already.
        if(this->casc_reservoirs == nullptr) {
            this->casc_reservoirs = 
                 new CascadedReservoirs(this->K_traveltime_hours, this->num_cascaded_reservoirs);
                // Transfer initial conditions to the cascaded linear reservoirs.
                this->casc_reservoirs->setInitialStorage(this->initial_storage_linres_Mm3);
        }
    }
    // RoutingMethod::D_MANNING
        
    // Terje Sandø, 06.07.2026
    // Validate QMIN period definitions (date format, year-crossing, coverage gaps, overlaps, negative values).
    if(qmin_in_use) {
        this->qmin.validatePeriods(this->nodename);
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
// Smooth QMIN penalty. The fee of the period is scaled by the unmet fraction of the requirement:
//   cost = qcost * dt / 3600 * relu_lo(required - outflow, eps) / required
// A requirement that is met costs exactly nothing, and the cost grows gradually as the outflow falls below it,
// so the gradient points towards meeting it. relu_lo(x) is 0 for x <= 0 and x - eps for x >= 2 eps, so the full fee
// is not reached: with nothing released the cost is (1 - eps / required) of the fee. A shortfall of about eps is accepted
// at almost no cost, like the 1e-3 m3/s tolerance that this replaces (the cascade only approaches the inflow asymptotically).
// The smoothing width is EPS_M3S, but at most 5 % of the requirement, which keeps at least 95 % of the fee for small requirements.
static const double QMIN_MIN_REQUIREMENT_M3S = 1e-9;   // Below this there is no requirement (also avoids dividing by zero)

static double qminSmoothingWidth(double required_m3s) {
    return std::min(EPS_M3S, 0.05 * required_m3s);
}
//----------------------------------------------------------------------
// Backward pass for timestep t. x_grad means d(ValueFunction)/d(x).
// A channel has no value of its own (remaining_active_Mm3 = 0), so it matters only when the outflow is
// penalised (QMIN) or feeds a downstream node.
// Reads:  profit_grad[t] and up_inflow_grad[t] of the downstream node, and the storage adjoint of step t+1 (casc_storage_grad).
// Writes: up_inflow_grad[t], casc_storage_grad.
// Implemented for routing method A (cascaded linear reservoirs), B (the same cascade followed by a pure delay) and
// C (cascade of nonlinear reservoirs, whose adjoint uses the reach storages recorded by Simulate).
// Method B: q_out[t] = q_cascade[t - L], so d(VF)/d(q_cascade[s]) = d(VF)/d(q_out[s + L]). Backward runs from the last step to
// the first, so d(VF)/d(q_out[s + L]) has already been computed and stored in lag_q_out_grad. The first L outflows come from the
// initial queue and the last L cascade outflows are still in the queue at the end of the run. Neither has a gradient to a control.
int Channel::Backward(size_t t) {

    if(!qmin_in_use && !downstream_node_in_use) {
        return 0;
    }

    if(routing_method != RoutingMethod::A_CASCADEDLINRES && routing_method != RoutingMethod::B_LAGGED_CASCADEDLINRES
       && routing_method != RoutingMethod::C_NONLINEARRESCASCADE) {
        LOG_ERR("Channel::Backward with QMIN or a downstream node is implemented only for routing methods A_CASCADEDLINRES, B_LAGGED_CASCADEDLINRES and C_NONLINEARRESCASCADE. Node idnr = "
                + std::to_string(int(idnr)) + "   nodename = " + nodename);
    }

    const bool is_method_c = (routing_method == RoutingMethod::C_NONLINEARRESCASCADE);

    // The water left in the channel at the end of the run has no value.
    if(is_method_c) {
        if(t + 1 == S->stps || reach_storage_grad.size() != size_t(n_reaches)) {
            reach_storage_grad.assign(size_t(n_reaches), 0.0);
        }
    } else {
        if(t + 1 == S->stps || casc_storage_grad.size() != num_cascaded_reservoirs) {
            casc_storage_grad.assign(num_cascaded_reservoirs, 0.0);
        }
        if(t + 1 == S->stps || lag_q_out_grad.size() != S->stps) {
            lag_q_out_grad.assign(S->stps, 0.0);
        }
    }

    double q_out_grad = 0.0;      // d(VF)/d(outflow)

    if(downstream_node_in_use) {
        q_out_grad += ptr_downstream_node->S->up_inflow_grad[t];       // downstream up_inflow += outflow
    }

    if(qmin_in_use) {
        double qcost = 0.0;
        const double required = qmin.calcQminRequirement(S->year[t], S->month[t], S->day[t], &qcost);
        if(required > QMIN_MIN_REQUIREMENT_M3S) {
            const double fee = qcost * double(S->dt) / 3600.0;
            // profit = -cost, and cost = fee * relu_lo(required - outflow) / required
            q_out_grad += S->profit_grad[t] * fee * relu_lo_d(required - S->tot_outflow[t], qminSmoothingWidth(required)) / required;
        }
    }

    double q_in_grad = 0.0;
    if(is_method_c) {
        if(reach_storage_tape_m3.size() != S->stps * size_t(n_reaches)) {
            LOG_ERR("Channel::Backward: Simulate must be run before Backward. Node idnr = " + std::to_string(int(idnr)) + "   nodename = " + nodename);
        }
        nonlinear_naturalChannel.RouteBackward(&reach_storage_tape_m3[t * size_t(n_reaches)], q_out_grad, double(S->dt),
                                               reach_storage_grad, q_in_grad);
        S->up_inflow_grad[t] += q_in_grad;
        return 0;
    }

    double q_cascade_grad = q_out_grad;
    if(routing_method == RoutingMethod::B_LAGGED_CASCADEDLINRES) {
        lag_q_out_grad[t] = q_out_grad;
        const size_t s = t + routing_delay_hours;
        q_cascade_grad = (s < S->stps) ? lag_q_out_grad[s] : 0.0;
    }

    casc_reservoirs->routeBackward(q_cascade_grad, double(S->dt), casc_storage_grad, q_in_grad);
    S->up_inflow_grad[t] += q_in_grad;

    return 0;
}
//----------------------------------------------------------------------
int Channel::Simulate(size_t t) {

    // BVM, May 2026, Reimplement the routing model.
    // We use a series of cascaded linear reservoirs to model the routing in the channel.

    this->dt     = S->dt;
    this->stps   = S->stps;

    S->channel_storage_Mm3[t] = 0.0;    // To void warnings. 

    double q_in = S->up_inflow[t]; // m3/s
    double initial_storage_m3 = 0.0;
    double total_in_m3 = 0.0;
    double total_out_m3 = 0.0;
    double q_out = 0.0;
    double final_storage_m3 = 0.0;

    switch (routing_method) {

        case RoutingMethod::A_CASCADEDLINRES: { 
            //--------------------------------------------------------------------
            // Routing method A, as of Sep 2026 
            // CascadedLinRes
            initial_storage_m3 = this->casc_reservoirs->totalStorageM3();
            total_in_m3 = 0.0;
            total_out_m3 = 0.0;
            q_out = this->casc_reservoirs->route(q_in, dt);
            total_in_m3 += q_in * dt;
            total_out_m3 += q_out * dt;
            final_storage_m3 = this->casc_reservoirs->totalStorageM3();
            //--------------------------------------------------------------------
            break;
        }

        case RoutingMethod::B_LAGGED_CASCADEDLINRES: {
            //--------------------------------------------------------------------
            if (dt != 3600) {
                LOG_ERR("Volume queue implementation requires a fixed 1-hour timestep");
            }

            if (lagged_outflow_volume_m3.size() != routing_delay_hours) {
                LOG_ERR("Lag queue size does not match ROUTING_DELAY_HOURS");
            }

            initial_storage_m3 = casc_reservoirs->totalStorageM3();
            for (double volume_m3 : lagged_outflow_volume_m3) {
                initial_storage_m3 += volume_m3;
            }

            const double cascade_q_out_m3s = casc_reservoirs->route(q_in, dt);
            const double cascade_volume_m3 = cascade_q_out_m3s * dt;

            total_in_m3 = q_in * dt;

            if (routing_delay_hours == 0) {
                q_out = cascade_q_out_m3s;
            } else {
                const double released_volume_m3 = lagged_outflow_volume_m3.front();
                lagged_outflow_volume_m3.pop_front();
                lagged_outflow_volume_m3.push_back(cascade_volume_m3);
                q_out = released_volume_m3 / dt;
            }

            total_out_m3 = q_out * dt;

            final_storage_m3 = casc_reservoirs->totalStorageM3();
            for (double volume_m3 : lagged_outflow_volume_m3) {
                final_storage_m3 += volume_m3;
            }
            break;
        }

        case RoutingMethod::C_NONLINEARRESCASCADE: {
            initial_storage_m3 = nonlinear_naturalChannel.GetStoredVolume();

            // The storages at the start of the step are kept for Backward
            const size_t n_reach_state = size_t(n_reaches);
            if(reach_storage_tape_m3.size() != stps * n_reach_state) {
                reach_storage_tape_m3.assign(stps * n_reach_state, 0.0);
            }
            q_out = nonlinear_naturalChannel.Route(q_in, dt, &reach_storage_tape_m3[t * n_reach_state]);

            total_in_m3 = q_in * dt;
            total_out_m3 = q_out * dt;
            final_storage_m3 = nonlinear_naturalChannel.GetStoredVolume();
            break;
        }
        //--------------------------------------------------------------------
        case RoutingMethod::D_MANNING: {
            initial_storage_m3 = manningChannel.GetStoredVolume();

            q_out = manningChannel.Route(q_in, dt);

            total_in_m3 = q_in * dt;
            total_out_m3 = q_out * dt;
            final_storage_m3 = manningChannel.GetStoredVolume();
            break;
        }

        default:
            LOG_ERR("ERROR: Unknown routing method!");
            break;
    }

    
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
         double qcost = 0.0;
         double qmin_requirements = this->qmin.calcQminRequirement(S->year[t], S->month[t], S->day[t], &qcost);  // m3/s
         // SMOOTH. The fee is scaled by the unmet fraction of the requirement, see qminSmoothingWidth.
         // This replaces the fixed fee that was charged below (requirement - 1e-3 m3/s).
         if(qmin_requirements > QMIN_MIN_REQUIREMENT_M3S) {
             S->cost_qmin[t] = qcost * S->dt / 3600
                             * relu_lo(qmin_requirements - S->tot_outflow[t], qminSmoothingWidth(qmin_requirements)) / qmin_requirements;
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

    // std::cout << "Channel::ReadNodeData        nodename: " << nodename << ", idnr: " << idnr << ", nodetype: " << EnumToString(nodetype) << "\n";

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

                    if(keyword2.compare("ROUTING_DELAY_HOURS") == 0 ) {
                        found_keyword = true;
                        this->routing_delay_hours = atoi(value2.c_str() );
                    }


                    if(keyword2.compare("N_REACHES") == 0 ) {
                        found_keyword = true;
                        this->n_reaches = atoi(value2.c_str() );
                    }
                    if(keyword2.compare("REFERENCE_FLOW") == 0 ) {
                        found_keyword = true;
                        this->referenceFlow = atof(value2.c_str() );
                    }
                    if(keyword2.compare("REFERENCE_TRAVEL_TIME") == 0 ) {
                        found_keyword = true;
                        this->referenceTravelTime = atof(value2.c_str() );
                    }
                    if(keyword2.compare("NONLINEAR_COEFFICIENT") == 0 ) {
                        found_keyword = true;
                        this->nonlinearity = atof(value2.c_str() );
                    }

                    if(keyword2.compare("MANNING_LENGTH_M") == 0 ) {
                        found_keyword = true;
                        this->manning_length_m = atof(value2.c_str() );
                    }
                    if(keyword2.compare("MANNING_BOTTOM_WIDTH_M") == 0 ) {
                        found_keyword = true;
                        this->manning_bottom_width_m = atof(value2.c_str() );
                    }
                    if(keyword2.compare("MANNING_SIDE_SLOPE") == 0 ) {
                        found_keyword = true;
                        this->manning_side_slope = atof(value2.c_str() );
                    }
                    if(keyword2.compare("MANNING_SLOPE") == 0 ) {
                        found_keyword = true;
                        this->manning_slope = atof(value2.c_str() );
                    }
                    if(keyword2.compare("MANNING_ROUGHNESS_N") == 0 ) {
                        found_keyword = true;
                        this->manning_roughness_n = atof(value2.c_str() );
                    }
                    if(keyword2.compare("MANNING_INITIAL_FLOW") == 0 ) {
                        found_keyword = true;
                        this->manning_initial_flow = atof(value2.c_str() );
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


    const bool cascade_settings_set = K_traveltime_hours != NOT_INIT && num_cascaded_reservoirs != NOT_INIT;

    const bool delay_set = routing_delay_hours != NOT_INIT;

    const bool nonlinear_set =  nonlinearity != NOT_INIT && 
                                referenceFlow != NOT_INIT && 
                                referenceTravelTime != NOT_INIT && 
                                n_reaches != NOT_INIT;

    const bool manning_set =    manning_length_m != NOT_INIT &&
                                manning_bottom_width_m != NOT_INIT &&
                                manning_side_slope != NOT_INIT &&
                                manning_slope != NOT_INIT &&
                                manning_roughness_n != NOT_INIT &&
                                manning_initial_flow != NOT_INIT &&
                                n_reaches != NOT_INIT;


    if (!cascade_settings_set) {
        if(nonlinear_set) { 
            routing_method = RoutingMethod::C_NONLINEARRESCASCADE;

            if(this->nonlinearity < 1.0) {
                LOG_ERR("ERROR: NONLINEAR_COEFFICIENT must be >= 1 (1 is a linear reservoir). Channel " + nodename + " has " + std::to_string(this->nonlinearity));
            }
            nonlinear_naturalChannel.SetParameters(this->n_reaches, 
                this->referenceFlow,
                this->referenceTravelTime,
                this->nonlinearity);
        } else if(manning_set) {
            routing_method = RoutingMethod::D_MANNING;

            if(this->manning_length_m <= 0 || this->manning_bottom_width_m <= 0 ||
               this->manning_side_slope < 0 || this->manning_slope <= 0 || this->manning_roughness_n <= 0) {
                LOG_ERR("ERROR: MANNING_LENGTH_M, MANNING_BOTTOM_WIDTH_M, MANNING_SLOPE and MANNING_ROUGHNESS_N must be positive, and MANNING_SIDE_SLOPE cannot be negative.");
            }

            manningChannel.Configure(this->n_reaches,
                this->manning_length_m,
                this->manning_bottom_width_m,
                this->manning_side_slope,
                this->manning_slope,
                this->manning_roughness_n);
        } else { 
            LOG_ERR("ERROR: Channel routing method could not be determined from topology file.");
        }
    } else if (delay_set) {
        routing_method = RoutingMethod::B_LAGGED_CASCADEDLINRES;

        if(this->routing_delay_hours < 0) {
            LOG_ERR("ERROR: routing_delay_hours cannot be negative.");
        }

    } else {
        routing_method = RoutingMethod::A_CASCADEDLINRES;

        if(this->K_traveltime_hours < 0 || this->K_traveltime_hours > MAX_TRAVELTIME_HOURS) {
            LOG_ERR("ERROR: K_traveltime_hours is out of valid range (0 - " + to_string(MAX_TRAVELTIME_HOURS) + ").");
        }

        if(this->num_cascaded_reservoirs < 0) {
            LOG_ERR("ERROR: num_cascaded_reservoirs cannot be negative.");
        }

    }

    return 0;
}
////////////////////////////////////////////////////////////////////////////
int Channel::SetStartState(void) {

    for(size_t t = 0; t < S->stps; t++ ) {
         S->up_inflow[t]    = 0.0;
    }
    
    lagged_outflow_volume_m3.clear();


    switch (this->routing_method) {
        //------------------------------------------------------------------------------
        case A_CASCADEDLINRES:  
            this->casc_reservoirs->setInitialStorage(this->initial_storage_linres_Mm3);
            break;

        //------------------------------------------------------------------------------
        case B_LAGGED_CASCADEDLINRES: 
            // Also used for lagged cascaded linear reservoirs initialization
            this->casc_reservoirs->setInitialStorage(this->initial_storage_linres_Mm3);  

            // Use the lag queue loaded from the statefile if it matches the configured delay, else warm-up with zeros.
            if (initial_lag_queue_Mm3.size() == routing_delay_hours) {
                lagged_outflow_volume_m3.assign(initial_lag_queue_Mm3.begin(), initial_lag_queue_Mm3.end());
                for (double &volume_m3 : lagged_outflow_volume_m3) {
                    volume_m3 *= 1000000.0; // Mm3 -> m3
                }
            } else {
                lagged_outflow_volume_m3.assign(routing_delay_hours, 0.0);
            }
            break;

        //------------------------------------------------------------------------------
        case C_NONLINEARRESCASCADE: 
            // Use the reach storage loaded from the statefile if it matches n_reaches, else reset to reference equilibrium.
            if (static_cast<int>(initial_reach_storage_Mm3.size()) == n_reaches) {
                std::vector<double> storage_m3(initial_reach_storage_Mm3.size());
                for (size_t i = 0; i < storage_m3.size(); ++i) {
                    storage_m3[i] = initial_reach_storage_Mm3[i] * 1000000.0; // Mm3 -> m3
                }
                nonlinear_naturalChannel.SetReachStorage(storage_m3);
            } else {
                nonlinear_naturalChannel.ResetToReferenceState();
            }
            break;

        //------------------------------------------------------------------------------
        case D_MANNING: 
            // Use the reach storage loaded from the statefile if it matches n_reaches, else warm-start from manning_initial_flow.
            if (static_cast<int>(initial_reach_storage_Mm3.size()) == n_reaches) {
                std::vector<double> storage_m3(initial_reach_storage_Mm3.size());
                for (size_t i = 0; i < storage_m3.size(); ++i) {
                    storage_m3[i] = initial_reach_storage_Mm3[i] * 1000000.0; // Mm3 -> m3
                }
                manningChannel.SetReachStorage(storage_m3);
            } else {
                manningChannel.SetInitialFlow(manning_initial_flow);
            }
            break;
    }

    start_storage_m3 = GetRoutingStorageM3();
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

                // NODE CHANNEL 2 VANAROSEN A_CASCADEDLINRES N 3 0.1 1.2 2.3
                // NODE CHANNEL 2 VANAROSEN B_LAGGED_CASCADEDLINRES N 3 0.1 1.2 2.3 LAG 2 0.05 0.05
                // NODE CHANNEL 2 VANAROSEN C_NONLINEARRESCASCADE N 10 v0 ... v9
                if(tmp_idnr == idnr && keyword == nodename) {
                    found_node = true;

                    string method_tag = line_obj.extractNextElementFromLine(&line);
                    if (method_tag == "METHOD") {
                        method_tag = line_obj.extractNextElementFromLine(&line);
                    }

                    line_obj.extractNextElementFromLine(&line); // "N"
                    size_t n_values = size_t(atoi(line_obj.extractNextElementFromLine(&line).c_str()));

                    std::vector<double> values;
                    for(size_t s = 0; s < n_values; s++) {
                        value = line_obj.extractNextElementFromLine(&line);
                        values.push_back(atof(value.c_str()));
                    }

                    std::vector<double> lag_values;
                    if(line_obj.extractNextElementFromLine(&line) == "LAG") {
                        size_t n_lag = size_t(atoi(line_obj.extractNextElementFromLine(&line).c_str()));
                        for(size_t s = 0; s < n_lag; s++) {
                            value = line_obj.extractNextElementFromLine(&line);
                            lag_values.push_back(atof(value.c_str()));
                        }
                    }

                    // routing_method for this run comes from the topology file, read before the statefile.
                    if(method_tag != string(RoutingEnumToString(routing_method))) {
                        LOG_WARN("Statefile routing method (" + method_tag + ") for channel " + nodename +
                            " does not match configured routing method (" + RoutingEnumToString(routing_method) +
                            "). Ignoring saved channel state.");
                    } else {
                        switch(routing_method) {
                            case A_CASCADEDLINRES:
                                initial_storage_linres_Mm3 = values;
                                break;
                            case B_LAGGED_CASCADEDLINRES:
                                initial_storage_linres_Mm3 = values;
                                initial_lag_queue_Mm3 = lag_values;
                                break;
                            case C_NONLINEARRESCASCADE:
                                initial_reach_storage_Mm3 = values;
                                break;
                            case D_MANNING:
                                initial_reach_storage_Mm3 = values;
                                break;
                        }
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
double Channel::GetRoutingStorageM3()
{
    switch (routing_method) {
        case RoutingMethod::A_CASCADEDLINRES:
            return casc_reservoirs->totalStorageM3();

        case RoutingMethod::B_LAGGED_CASCADEDLINRES: {
            double storage_m3 = casc_reservoirs->totalStorageM3();
            for (double volume_m3 : lagged_outflow_volume_m3)
                storage_m3 += volume_m3;
            return storage_m3;
        }
        case RoutingMethod::C_NONLINEARRESCASCADE:
            return nonlinear_naturalChannel.GetStoredVolume();

        case RoutingMethod::D_MANNING:
            return manningChannel.GetStoredVolume();
    }

    LOG_ERR("Cannot get storage for an unknown channel routing method.");
    return -9;  // To void warning 
}

//------------------------------------------------------------------------
double Channel::GetStartWater_Mm3(void)
{
    return start_storage_m3 / 1000000.0;
}

double Channel::GetEndWater_Mm3(void)
{
    return GetRoutingStorageM3() / 1000000.0;
}
//------------------------------------------------------------------------


//------------------------------------------------------------------------
int Channel::CheckWaterBalance(Herss *herss_obj) { 

    double sum_inflow_Mm3       = 0.0;
    double sum_outflow_Mm3      = 0.0;

    double start_channel_Mm3 = start_storage_m3 / 1000000.0;

    // Sum inflow and outflow as before.
    double end_channel_Mm3 = GetRoutingStorageM3() / 1000000.0;


    for (size_t t = 0; t < S->stps; ++t) {
        const double dt_seconds = herss_obj->getDeltaT(t);
        sum_inflow_Mm3 += S->up_inflow[t] * dt_seconds / 1000000.0;
        sum_outflow_Mm3 += S->tot_outflow[t] * dt_seconds / 1000000.0;
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
double Channel::GetTunnelFLow(size_t t) {
    printf("ERROR Channel cannot use the function: GetTunnelFLow \n");
    printf("Have you connected a tunnel from a reservoir to a channel? - check input\n");
    printf( "NODE CHANNEL idnr=%d  nodename=%s\n", int(idnr), nodename.c_str()  );
    printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);

    std::string where = std::string("file: ") + __FILE__ + "  linenr: " + std::to_string(__LINE__) + "  function: " + __FUNCTION__;
    LOG_WARN(where);

    exit(EXIT_FAILURE);
}
//////////////////////////////////////////////////////////////////////////////////
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
int Channel::WriteStateFile(FILE *fp) {
    fprintf(fp, "NODE CHANNEL %d %s %s ",
        int(idnr), nodename.c_str(), RoutingEnumToString(routing_method));

    switch (routing_method) {
        case A_CASCADEDLINRES: {
            fprintf(fp, "N %zu ", num_cascaded_reservoirs);
            for (size_t s = 0; s < num_cascaded_reservoirs; s++) {
                fprintf(fp, "%.5f ", casc_reservoirs->getStorageMm3(s));
            }
            break;
        }

        case B_LAGGED_CASCADEDLINRES: {
            fprintf(fp, "N %zu ", num_cascaded_reservoirs);
            for (size_t s = 0; s < num_cascaded_reservoirs; s++) {
                fprintf(fp, "%.5f ", casc_reservoirs->getStorageMm3(s));
            }
            fprintf(fp, "LAG %zu ", lagged_outflow_volume_m3.size());
            for (double volume_m3 : lagged_outflow_volume_m3) {
                fprintf(fp, "%.5f ", volume_m3 / 1000000.0);
            }
            break;
        }

        case C_NONLINEARRESCASCADE: {
            std::vector<double> storage_m3 = nonlinear_naturalChannel.GetReachStorage();
            fprintf(fp, "N %zu ", storage_m3.size());
            for (double v : storage_m3) {
                fprintf(fp, "%.5f ", v / 1000000.0);
            }
            break;
        }

        case D_MANNING: {
            std::vector<double> storage_m3 = manningChannel.GetReachStorage();
            fprintf(fp, "N %zu ", storage_m3.size());
            for (double v : storage_m3) {
                fprintf(fp, "%.5f ", v / 1000000.0);
            }
            break;
        }
    }

    fprintf(fp, "\n");
    return 0;
}
//////////////////////////////////////////////////////////////////////////////////