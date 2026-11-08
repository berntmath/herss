/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     powerstation.cpp
Developer:    Bernt Viggo Matheussen, Ove Arstad
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
********************************************************************************/

#include "herss.h"
#include <vector>

Powerstation::Powerstation(){
    stps = 0;
    dt   = 0;
    static_gen_efficiency   = -1.0 * NOT_INIT;
    headlosscoef            = -1.0 * NOT_INIT;
    powstat_masl            = -1.0 * NOT_INIT;
    powstat_min_discharge   = -1.0 * NOT_INIT;
    powstat_startstop       = -1.0 * NOT_INIT;
    init_Power              = -1.0 * NOT_INIT;
    aggressive_actions_cost = -1.0 * NOT_INIT;
    shared_penstock         = false;   
    nr_generators           = 0;       
    
}

Powerstation::~Powerstation(){}

// We check if the settings for the powerstation are valid. 
void Powerstation::ValidatePowerstationSettings() {

    if (nr_generators < 1 || nr_generators > MAX_NR_GENERATORS) {

        LOG_ERR("ERROR: Number of generators in powerstation " 
            + std::to_string(int(idnr)) 
            + " (" + nodename + ") is out of bounds: " 
            + std::to_string(nr_generators));

    }

    if (headlosscoef < 0.0 || headlosscoef > VERY_LARGE_NUMBER) {
        LOG_ERR("ERROR: Head loss coefficient in powerstation " 
            + std::to_string(int(idnr)) 
            + " (" + nodename + ") is out of bounds: " 
            + std::to_string(headlosscoef));
    }

    if (powstat_masl < -1000.0 || powstat_masl > MOUNT_EVEREST_MASL) {
        LOG_ERR("ERROR: Powerstation elevation (masl) in powerstation " 
            + std::to_string(int(idnr)) 
            + " (" + nodename + ") is out of bounds: " 
            + std::to_string(powstat_masl));
    }

    if(local_energy_equivalent < 0.0 || local_energy_equivalent > VERY_LARGE_NUMBER) {

        LOG_ERR("ERROR: Local energy equivalent in powerstation " 
            + std::to_string(int(idnr)) 
            + " (" + nodename + ") is out of bounds: " 
            + std::to_string(local_energy_equivalent));
                
    }

}
////////////////////////////////////////////////////////////////
double Powerstation::calcEfficiency(size_t gen_idx, double q_m3s) {  // Calculate the efficiency for a specific generator and discharge.

    if(q_m3s < 0.0) {
        LOG_WARN("ERROR: Discharge is negative for generator " + std::to_string(gen_idx) + " in powerstation " + std::to_string(int(idnr)) + " (" + nodename + "): Q = " + std::to_string(q_m3s));
        LOG_ERR("Check your action file, and make sure the discharge for generator " + std::to_string(gen_idx) + " in powerstation " + std::to_string(int(idnr)) + " (" + nodename + ") is not negative");
    }

    if(q_m3s > generators[gen_idx].max_discharge * 1.000001) {
        LOG_WARN("ERROR: Discharge is above maximum for generator " + std::to_string(gen_idx) + " in powerstation " + std::to_string(int(idnr)) + " (" + nodename + "): Q = " + std::to_string(q_m3s) + ", max_discharge = " + std::to_string(generators[gen_idx].max_discharge));
        LOG_ERR("Check your action file, and make sure the discharge for generator " + std::to_string(gen_idx) + " in powerstation " + std::to_string(int(idnr)) + " (" + nodename + ") is not above the maximum discharge of the generator");
    }

    if(gen_idx < 0) {
        LOG_ERR("ERROR: Generator index is negative in powerstation " + std::to_string(int(idnr)) + " (" + nodename + "): gen_idx = " + std::to_string(gen_idx));
    }

    if(q_m3s < 0.000001) {
        return 0.0;
    }


    if (generators[gen_idx].use_quadratic_eff_curve) {
        const Generator &gen = generators[gen_idx];
        double Qn  = q_m3s / gen.max_discharge;                 // normalised 0-1
        double eta = gen.a_quad*Qn*Qn + gen.b_quad*Qn + gen.c_quad;
        return std::max(eta, 0.0);                              // returned as a fraction, not %
    }




    if(generators[gen_idx].use_uniform_normalized_curve) {
        // We use the fast lookup method for uniform curves. 
        double Qn = q_m3s / generators[gen_idx].max_discharge;    // normalize to 0-1
        int i = (int)(Qn * (N_UNIFORM_EFF_CURVE_POINTS-1));  // direct index calculation
        double t = Qn * (N_UNIFORM_EFF_CURVE_POINTS-1) - i;  // fractional part for interpolation
        double eta = generators[gen_idx].uniform_normalized_curve[i] + t * (generators[gen_idx].uniform_normalized_curve[i+1] - generators[gen_idx].uniform_normalized_curve[i]);
        // cout << "Uniform normalized curve: gen_idx = " << gen_idx << ", Q = " << q_m3s << ", Qn = " << Qn << ", i = " << i << ", t = " << t << ", eta = " << eta << "\n";
        return eta / 100.0;
    }

    if (gen_idx >= generators.size()) {
        LOG_ERR("ERROR: Generator index out of bounds in powerstation " + std::to_string(int(idnr)) + " (" + nodename + ")");
        return 0.0;
    }

    // Default is to use old code with array curves. 
    return generators[gen_idx].eff_curve.x2y(q_m3s) / 100.0;

}
////////////////////////////////////////////////////////////////
// Derivative of calcEfficiency with respect to the discharge. Mirrors the branches in calcEfficiency.
double Powerstation::calcEfficiencyGrad(size_t gen_idx, double q_m3s) {

    if(q_m3s < 0.000001) {
        return 0.0;
    }

    const Generator &gen = generators[gen_idx];

    if (gen.use_quadratic_eff_curve) {
        double Qn  = q_m3s / gen.max_discharge;
        double eta = gen.a_quad*Qn*Qn + gen.b_quad*Qn + gen.c_quad;
        if (eta <= 0.0) {
            return 0.0;     // calcEfficiency returns max(eta, 0)
        }
        return (2.0*gen.a_quad*Qn + gen.b_quad) / gen.max_discharge;
    }

    if (gen.use_uniform_normalized_curve) {
        double Qn = q_m3s / gen.max_discharge;
        int i = (int)(Qn * (N_UNIFORM_EFF_CURVE_POINTS-1));
        if (i > N_UNIFORM_EFF_CURVE_POINTS-2) {
            i = N_UNIFORM_EFF_CURVE_POINTS-2;
        }
        return (gen.uniform_normalized_curve[i+1] - gen.uniform_normalized_curve[i]) * (N_UNIFORM_EFF_CURVE_POINTS-1) / gen.max_discharge / 100.0;
    }

    LOG_ERR("Backward is only implemented for QUADRATIC_EFF_EQUATION and UNIFORM_NORMALIZED_CURVE. Powerstation " + std::to_string(int(idnr)) + " (" + nodename + ")");
    return 0.0;
}
////////////////////////////////////////////////////////////////
// Backward pass for timestep t. x_grad means d(ValueFunction)/d(x).
// Reads:   profit_grad[t], Power_grad[t], up_inflow_grad[t] of the downstream node.
// Writes:  Power_grad[t], generators[g].action_grad[t] and [t-1] (start/stop cost), Hbrutto_grad[t], up_inflow_grad[t].
// Hbrutto_grad is consumed by the upstream reservoir, and up_inflow_grad by the tunnel flow of the upstream reservoir.
// The aggressive-actions cost belongs to the tunnel flow and is differentiated in BackwardTunnelFlow.
int Powerstation::Backward(size_t t) {

    const size_t G = generators.size();

    // Recompute the quantities of Simulate(t) that the gradients depend on.
    std::vector<double> Q_req(G, 0.0);   // Requested flow per generator, action * max_discharge
    std::vector<double> Q(G, 0.0);       // Flow per generator after the water-availability scaling
    double T_req = 0.0;
    for (size_t g = 0; g < G; ++g) {
        if (generators[g].action.size() > t) {
            Q_req[g] = generators[g].action[t] * generators[g].max_discharge;
        }
        T_req += Q_req[g];
    }

    const double up_inflow = S->up_inflow[t];
    const bool   scaled    = (T_req > up_inflow);                 // Simulate scales all generators by up_inflow / T_req
    const double scale     = scaled ? up_inflow / T_req : 1.0;
    const double total_Q   = scaled ? up_inflow : T_req;
    for (size_t g = 0; g < G; ++g) {
        Q[g] = Q_req[g] * scale;
    }

    // The head of step t as stored by Simulate. start_of_stp_masl/end_of_stp_masl are overwritten every step,
    // so they hold the values of the last step when Backward runs.
    const double Hbrutto = S->Hbrutto[t];
    const double dt_s    = double(S->dt);
    const double k       = 1000.0 * GRAVITY / 1000000.0 * static_gen_efficiency * dt_s / 3600.0;   // Power[MWh] = k * eta * Hnetto * Q

    // income = sum_g Power_g * price, and profit = income - costs
    S->Power_grad[t] += S->profit_grad[t] * S->price[t];
    const double Power_grad = S->Power_grad[t];

    std::vector<double> Q_grad(G, 0.0);   // Adjoint of the flow per generator after scaling
    double Hbrutto_grad  = 0.0;
    double total_Q_grad  = 0.0;           // Adjoint of total_Q: flow into the downstream node (+ head loss below)

    if (ptr_downstream_node != NULL) {
        total_Q_grad += ptr_downstream_node->S->up_inflow_grad[t];   // downstream up_inflow += total_Q
    }

    if (shared_penstock) {
        // Hnetto = Hbrutto - headlosscoef * total_Q^2 is common to all generators
        const double Hnetto = Hbrutto - headlosscoef * total_Q * total_Q;
        double dPower_dHnetto = 0.0;
        for (size_t g = 0; g < G; ++g) {
            const double eta  = calcEfficiency(g, Q[g]);
            const double deta = calcEfficiencyGrad(g, Q[g]);
            Q_grad[g]       = Power_grad * k * (deta * Hnetto * Q[g] + eta * Hnetto);
            dPower_dHnetto += k * eta * Q[g];
        }
        const double Hnetto_grad = Power_grad * dPower_dHnetto;
        Hbrutto_grad  = Hnetto_grad;
        total_Q_grad += Hnetto_grad * (-2.0 * headlosscoef * total_Q);
    } else {
        // Separate penstocks: Hnetto_g = Hbrutto - headlosscoef * Q_g^2
        for (size_t g = 0; g < G; ++g) {
            const double Hnetto = Hbrutto - headlosscoef * Q[g] * Q[g];
            const double eta    = calcEfficiency(g, Q[g]);
            const double deta   = calcEfficiencyGrad(g, Q[g]);
            Q_grad[g]     = Power_grad * k * (deta * Hnetto * Q[g] + eta * Hnetto - eta * Q[g] * 2.0 * headlosscoef * Q[g]);
            Hbrutto_grad += Power_grad * k * eta * Q[g];
        }
    }

    // Back through the scaling of Simulate.
    std::vector<double> Q_req_grad(G, 0.0);
    if (!scaled) {
        // Q_g = Q_req_g and total_Q = sum_g Q_g
        for (size_t g = 0; g < G; ++g) {
            Q_req_grad[g] = Q_grad[g] + total_Q_grad;
        }
    } else {
        // Q_g = Q_req_g * up_inflow / T_req, total_Q = up_inflow
        double sum_Qgrad_Qreq = 0.0;
        for (size_t g = 0; g < G; ++g) {
            sum_Qgrad_Qreq += Q_grad[g] * Q_req[g];
        }
        for (size_t g = 0; g < G; ++g) {
            Q_req_grad[g] = Q_grad[g] * scale - (up_inflow / (T_req * T_req)) * sum_Qgrad_Qreq;
        }
        S->up_inflow_grad[t] += sum_Qgrad_Qreq / T_req + total_Q_grad;
    }

    for (size_t g = 0; g < G; ++g) {
        if (generators[g].action_grad.size() <= t) {
            generators[g].action_grad.resize(S->stps, 0.0);
        }
        generators[g].action_grad[t] += Q_req_grad[g] * generators[g].max_discharge;
    }

    // Start/stop cost: profit = ... - (fee / 2) * sum_g (s_prev - s_cur)^2, with s = smooth_on(action).
    // It couples the action of step t with the action of step t-1. At t = 0 the previous state is a constant.
    for (size_t g = 0; g < G; ++g) {
        if (generators[g].action.size() <= t) continue;
        const double a_cur  = generators[g].action[t];
        const double s_cur  = smooth_on(a_cur);
        const double s_prev = (t > 0) ? smooth_on(generators[g].action[t-1]) : ((init_Power > 0.01) ? 1.0 : 0.0);

        generators[g].action_grad[t] += S->profit_grad[t] * powstat_startstop * (s_prev - s_cur) * smooth_on_d(a_cur);
        if (t > 0) {
            generators[g].action_grad[t-1] -= S->profit_grad[t] * powstat_startstop * (s_prev - s_cur) * smooth_on_d(generators[g].action[t-1]);
        }
    }

    S->Hbrutto_grad[t] += Hbrutto_grad;

    return 0;
}
////////////////////////////////////////////////////////////////
int Powerstation::Simulate(size_t t) {

    // CHANGE BY OVE: Initialize all parameters as 0.0
    this->dt     = S->dt;
    this->stps   = S->stps;
    double Q = 0.0;
    double headloss = 0.0;
    double Hbrutto = 0.0;
    double Hnetto = 0.0;
    double turbine_efficiency = 0.0;
    double P = 0.0;
    double Power = 0.0;
    double total_Q = 0.0;
    double total_Power = 0.0;
    double income = 0.0;
    double startstopCost = 0.0;
    std::vector<double> Q_gen(generators.size(), 0.0); // Flow for each generator

    // CHANGES BY OVE: Calculate flow for each generator and powerstation total flow
    for (size_t g = 0; g < generators.size(); ++g) {
        Q = 0.0;
        if (generators[g].action.size() > t) {
            Q += generators[g].action[t] * generators[g].max_discharge;
        }
        Q_gen[g] = Q;
        total_Q += Q;
        if( generators[g].action[t] < -0.000001 || generators[g].action[t] > 1.000001) {
            LOG_WARN("ERROR: Action for generator " + std::to_string(g) + " in powerstation " + std::to_string(int(idnr)) + " (" + nodename + ") is out of bounds: " + std::to_string(generators[g].action[t]));
            LOG_ERR("Check your action file, and make sure the action for generator " + std::to_string(g) + " in powerstation " + std::to_string(int(idnr)) + " (" + nodename + ") is between 0.0 and 1.0");
        }
    }


    // BVM June 2026. 
    // Fixing bug related to aggressive actions
    // The tunnel has already capped the flow smoothly at the available water, so the station
    // delivers exactly what it receives: all generators are scaled by up_inflow / requested flow.
    if (total_Q > S->up_inflow[t]) {
        if (total_Q > S->up_inflow[t] * 1.000001) {
            LOG_WARN("Agressive actions " + std::to_string(total_Q) + " m3/s) exceeds inflow (" + std::to_string(S->up_inflow[t]) + " m3/s) at timestep " + std::to_string(t) + " for node " + std::to_string(int(idnr)) + " (" + nodename + ")\n");
        }
        double scale = S->up_inflow[t] / total_Q;
        for (size_t g = 0; g < generators.size(); ++g) { 
            Q_gen[g] *= scale;
        }
        total_Q = S->up_inflow[t];
    }


    // BVM May 2026.
    // At some point revisit this to check if we should use the minimum discharge as a hard constrain or soft constrain.
    // For now we use it as a soft constrain.
    if (total_Q > 0.001 && total_Q < powstat_min_discharge) {
        LOG_WARN("WARNING: Total powerstation flow " 
            + std::to_string(total_Q) + " m3/s) below minimum discharge (" + std::to_string(powstat_min_discharge) 
            + " m3/s) at timestep " + std::to_string(t) 
            + " for node " + std::to_string(int(idnr)) + " (" + nodename + ")\n");
    }
    
    Hbrutto  = ((this->start_of_stp_masl + this->end_of_stp_masl)/2.0 ) - this->powstat_masl;
    
    // CHANGES BY OVE: Handle both shared and separate penstock configurations
    // SHARED PENSTOCK: All generators share the same penstock, head loss based on combined flow
    // BVM June 2026, modified for several type of efficiency curves. 

    if (shared_penstock) {
        headloss = this->headlosscoef * total_Q * total_Q;
        Hnetto = Hbrutto - headloss; 
        //Terje Sandø, August 2026.
        if(Hnetto < 0.0) {
            LOG_WARN("WARNING: Negative net head (Hnetto = " + std::to_string(Hnetto) + " m) for POWSTATION node "
                + std::to_string(int(idnr)) + " (" + nodename + ") at timestep " + std::to_string(t)
                + " (shared penstock, total_Q = " + std::to_string(total_Q) + " m3/s, Hbrutto = " + std::to_string(Hbrutto)
                + " m, headloss = " + std::to_string(headloss) + " m). HEADLOSSCOEF may be too large for this head/discharge - this will produce a negative Power estimate.");
        }
        
        for (size_t g = 0; g < generators.size(); ++g) {
            Q = Q_gen[g];
            turbine_efficiency = calcEfficiency(g, Q);
            
            if(turbine_efficiency < 0.0) {
                LOG_WARN("ERROR:  Turbine efficiency is not working properly \n");
                LOG_WARN("ERROR:  Please revisit your topology file \n");
                LOG_WARN("Generator " + std::to_string(g) + ": Q = " + std::to_string(Q));
                LOG_WARN("Shared penstock: total_Q = " + std::to_string(total_Q) + ", headloss = " + std::to_string(headloss));
                LOG_WARN("Hbrutto  = " + std::to_string(Hbrutto));
                LOG_WARN("Shared Hnetto = " + std::to_string(Hnetto));
                LOG_WARN("Generator " + std::to_string(g) + ": turbine_efficiency = " + std::to_string(turbine_efficiency));
                LOG_ERR("In timestep  t= " + std::to_string(t));
            }
            
            P = turbine_efficiency * 1000 * GRAVITY * Hnetto * Q;  // Watt
            P = P /1000000.0; // MW

            P = P * static_gen_efficiency; 
            Power = P * this->dt / 3600.0; // MWh 
            
            //if(Q < powstat_min_discharge) {
            //    Power = 0.0;
            //}

            total_Power += Power;
            income += Power * S->price[t];
        }
    } else {
        // SEPARATE PENSTOCKS: Each generator has individual penstock, head loss is calculated for each generator
        for (size_t g = 0; g < generators.size(); ++g) {
            Q = Q_gen[g];
            headloss = this->headlosscoef * Q * Q; 
            Hnetto   = Hbrutto - headloss; 

            //Terje Sandø, August 2026.
            if(Hnetto < 0.0) {
                LOG_WARN("WARNING: Negative net head (Hnetto = " + std::to_string(Hnetto) + " m) for POWSTATION node "
                    + std::to_string(int(idnr)) + " (" + nodename + "), generator " + std::to_string(g)
                    + " at timestep " + std::to_string(t) + " (Q = " + std::to_string(Q) + " m3/s, Hbrutto = " + std::to_string(Hbrutto)
                    + " m, headloss = " + std::to_string(headloss) + " m). HEADLOSSCOEF may be too large for this head/discharge - this will produce a negative Power estimate.");
            }
            turbine_efficiency = calcEfficiency(g, Q);
            
            if(turbine_efficiency < 0.0) {
                LOG_WARN("ERROR:  Turbine efficiency is not working properly \n");
                LOG_WARN("ERROR:  Please revisit your topology file \n");
                LOG_WARN("file: " + std::string(__FILE__) + "  linenr: " + std::to_string(__LINE__) + "  function: " + std::string(__FUNCTION__));
                LOG_WARN("Generator " + std::to_string(g) + ": Q = " + std::to_string(Q));
                LOG_WARN("Generator " + std::to_string(g) + ": headloss = " + std::to_string(headloss));
                LOG_WARN("Generator " + std::to_string(g) + ": headlosscoef = " + std::to_string(headlosscoef));
                LOG_WARN("Hbrutto  = " + std::to_string(Hbrutto));
                LOG_WARN("Generator " + std::to_string(g) + ": Hnetto   = " + std::to_string(Hnetto));
                LOG_WARN("Generator " + std::to_string(g) + ": turbine_efficiency = " + std::to_string(turbine_efficiency));
                LOG_ERR("In timestep  t= " + std::to_string(t));
            }
            
            P = turbine_efficiency * 1000 * GRAVITY * Hnetto * Q; // Watt
            P = P /1000000.0; // MW

            P = P * static_gen_efficiency; 
            Power = P * this->dt / 3600.0; // MWh

            //if(Q < powstat_min_discharge) {
            //    Power = 0.0;
            //}
            total_Power += Power;
            income += Power * S->price[t];
        }
    }

    // Now we check for start and stop costs
    // We penalise when starting and stopping. 
    // SMOOTH. A generator is running when the smooth indicator smooth_on(action) is 1, and stopped when it is 0.
    // The cost is (fee / 2) * (s_previous - s_current)^2: the old fee / 2 for every full start or stop, and a
    // partial cost with a gradient for actions in the narrow zone above the threshold (see smooth_on in herss.h).
    startstopCost = 0.0;
    
    for (size_t g = 0; g < generators.size(); ++g) {
        double previous_state = 0.0;
        if (t > 0) {
            previous_state = smooth_on(generators[g].action[t-1]);
        } else {
            // Terje Sandø, August 2026.
            // At t = 0 the previous state comes from the statefile, so a station that was
            // already running at the end of the previous run is not charged a start cost.
            previous_state = (init_Power > 0.01) ? 1.0 : 0.0;
        }
        double current_state = 0.0;
        if (generators[g].action.size() > t) {
            current_state = smooth_on(generators[g].action[t]);
        }
        startstopCost += this->powstat_startstop / 2.0 * (previous_state - current_state) * (previous_state - current_state);
    }

    
    // We do allow for a powerstation to be the most downstream node in the riversystem. 
    if(this->ptr_downstream_node != NULL) {
        this->ptr_downstream_node->S->up_inflow[t] += total_Q;
    }

    // Estimating the local EEKV.
    double est_eekv = 0.0;
    double est_GWh = total_Power / 1000;
    double q_Mm3 = MACRO_m3s_2_Mm3(total_Q, S->dt);  // Use variable timestep

    if(total_Power > 0.0) {
        est_eekv = est_GWh / q_Mm3;
    }

    // Save timeseries 
    S->income[t]           = income;
    S->cost[t]             = startstopCost + aggressive_actions_cost;

    // BVM , 15 oct 2024, 
    S->profit[t]           = income - startstopCost - aggressive_actions_cost;
    S->Hnetto[t]           = Hnetto;
    S->Hbrutto[t]          = Hbrutto;
    S->Power[t]            = total_Power;
    S->EstimatedEEKV[t]    = est_eekv;
    S->startStopCost[t]    = startstopCost;

    S->adjust_cost[t]      = 0.0;  // We do not use this for now, but we keep it for later when we want to penalise adjustments.
    S->cost_aggressive_actions[t] = aggressive_actions_cost;

    S->tot_outflow[t]      = total_Q;
    S->inflow[t]           = 0.0;  

    remaining_Mm3 = 0.0;  // The powerstation can never store water.
    remaining_active_Mm3 = 0.0;

    double mydiff = abs(S->up_inflow[t] - S->tot_outflow[t]);
    if(mydiff > 0.001 ) {
        LOG_INFO("Warning: There is a difference between inflow and outflow in a powerstations.");
        LOG_WARN("Warning: There is a difference between inflow and outflow in a powerstations.");
        LOG_WARN("Node idnr = " + std::to_string(int(this->idnr)) + "   nodename = " + this->nodename );
        LOG_INFO("t = " + std::to_string(t) + "  inflow = " + std::to_string(S->up_inflow[t]) + "  outflow = " + std::to_string(S->tot_outflow[t]) );
        LOG_WARN("t = " + std::to_string(t) + "  inflow = " + std::to_string(S->up_inflow[t]) + "  outflow = " + std::to_string(S->tot_outflow[t]) );
        LOG_ERR("Node idnr = " + std::to_string(int(this->idnr)) + "   nodename = " + this->nodename );
    }


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

    bool inside_node = false;
    
    for (size_t i = 0; i < gc->topoparser.getLineCount(); ++i) {

        line = gc->topoparser.getLine(i);
        string tmpline = line;  // Create a copy of the line for parsing

        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);

        if ( keyword.compare("ENDNODE") == 0 ) {
            inside_node = false;
        }

        if ( keyword.compare("NODE") == 0 && value.compare("PSTATION") == 0 ) {
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

                    if(keyword2.compare("ENDNODE") == 0) {
                        inside_node = false;
                        break;
                    }

                    // DOWNLINK_IDNR -9
                    if(keyword2.compare("DOWNLINK_IDNR") == 0 ) {
                        downstream_idnr = atoi(value2.c_str());
                        if(downstream_idnr  >= 0) {
                            downstream_node_in_use = true;
                        }   
                    }

                    // STATIC_GENERATOR_EFFICIENCY 0.96
                    if(keyword2.compare("STATIC_GENERATOR_EFFICIENCY") == 0 ) {
                        static_gen_efficiency = atof(value2.c_str());
                    }

                    // HEADLOSSCOEF 0.145
                    if( keyword2.compare("HEADLOSSCOEF") == 0 ) {
                        this->headlosscoef = atof(value2.c_str());
                    }

                    // SHARED_PENSTOCK	TRUE
                    if( keyword2.compare("SHARED_PENSTOCK") == 0 ) {
                        if (value2 == "TRUE") {
                            this->shared_penstock = 1;
                        } else if (value2 == "FALSE") {
                            this->shared_penstock = 0;
                        } else {
                            LOG_ERR("SHARED_PENSTOCK must be TRUE or FALSE in topologyfile " + filename + " ERROR\n");
                        }
                    }

                    // POWSTAT_MASL 218.0
                    if(keyword2.compare("POWSTAT_MASL") == 0 ) {
                        this->powstat_masl = atof(value2.c_str());
                    }

                    // POWSTAT_MIN_DISCHARGE 0.0
                    if(keyword2.compare("POWSTAT_MIN_DISCHARGE") == 0 ) {
                        this->powstat_min_discharge = atof(value2.c_str());
                    }

                    // POWSTAT_STARTSTOP	2.0
                    if(keyword2.compare("POWSTAT_STARTSTOP") == 0 ) {
                        this->powstat_startstop = atof(value2.c_str());
                    }

                    // LOCAL_ENERGY_EQUIVALENT 0.11
                    if(keyword2.compare("LOCAL_ENERGY_EQUIVALENT") == 0 ) {
                        this->local_energy_equivalent = atof(value2.c_str());
                    }

                    // AUTO_QMIN -9999
                    if(keyword2.compare("AUTO_QMIN") == 0 ) {
                        this->auto_qmin = atof(value2.c_str());
                    }

                    // MAX_ADJUST -9999
                    if(keyword2.compare("MAX_ADJUST") == 0 ) {
                        max_adjustment_pr_day = atoi(value2.c_str());
                        if(max_adjustment_pr_day > -1) {
                            LOG_ERR("MAX_ADJUST  WORK IN PROGRESS - This feature has not been quality controlled yet.  topologyfile " + filename);
                            //value   = line_obj.extractNextElementFromLine(&line);
                            //max_adjustment_cost = atof(value.c_str());
                        }
                    }

                    //---------------------------------------------------------------------------------
                    // --------   START GENERATORS      -----------------------------------------------
                    // Multi-generator feature was implemented by Ove Arstad, summer 2025.
                    // It has been modified here to be part of a more robust topology parser/reader.
                    // BVM May, 2026

                    if (keyword2.compare("NR_GENERATORS") == 0 ) {
                         this->nr_generators = size_t(atoi(value2.c_str()));

                        if(this->nr_generators > size_t(MAX_NR_GENERATORS)) {
                            string outstr = "ERROR: Maximum " + to_string(MAX_NR_GENERATORS) 
                                + " generators allowed per powerstation (input: " 
                                + to_string(this->nr_generators) + ")";
                            LOG_ERR(outstr);
                        }
                        generators.resize(this->nr_generators);


                        // We will now loop over the generators and extract the relevant data for each generator.
                        size_t gen_data_lines = k+1; // Counter for the number of lines read for generator data

                        for(size_t g = 0; g < this->nr_generators; ++g) {
                            line = gc->topoparser.getLine(gen_data_lines);
                            keyword = line_obj.extractNextElementFromLine(&line);
                            value   = line_obj.extractNextElementFromLine(&line);
                            gen_data_lines++;

                            int gen_id = int(MAX_NR_GENERATORS) + 1;
                            if(keyword == "GENERATOR") {
                                gen_id = atoi(value.c_str());
                            }

                            if (keyword != "GENERATOR" || size_t(gen_id) != g) {
                                LOG_ERR("ERROR: Expected GENERATOR " + to_string(g));
                            }
                            
                            line = gc->topoparser.getLine(gen_data_lines);
                            keyword = line_obj.extractNextElementFromLine(&line);
                            value   = line_obj.extractNextElementFromLine(&line);
                            gen_data_lines++;
                            

                            //------------------------------- QUADRATIC_EFF_EQUATION ---------------------------------
                            if(keyword == "QUADRATIC_EFF_EQUATION") {

                                generators[g].use_quadratic_eff_curve = true;
                                generators[g].a_quad = atof(value.c_str());
                                
                                value   = line_obj.extractNextElementFromLine(&line);
                                generators[g].b_quad = atof(value.c_str());

                                value   = line_obj.extractNextElementFromLine(&line);
                                generators[g].c_quad = atof(value.c_str());

                                //cout << "Generator " << g << ": a_quad = " << generators[g].a_quad << ", b_quad = " <<
                                //    generators[g].b_quad << ", c_quad = " << generators[g].c_quad << "\n";

                                // --- ADD: read GENERATOR_MAX_DISCHARGE (was missing) ---
                                line    = gc->topoparser.getLine(gen_data_lines);
                                keyword = line_obj.extractNextElementFromLine(&line);
                                value   = line_obj.extractNextElementFromLine(&line);
                                if(keyword != "GENERATOR_MAX_DISCHARGE") {
                                    LOG_ERR("ERROR: Expected GENERATOR_MAX_DISCHARGE for generator " + to_string(g));
                                }
                                generators[g].max_discharge = atof(value.c_str());
                                gen_data_lines++;
                            }
                            //------------------------------- END QUADRATIC_EFF_EQUATION ---------------------------------


                            if(keyword == "UNIFORM_NORMALIZED_CURVE") {

                                if(stoi(value) != N_UNIFORM_EFF_CURVE_POINTS) {
                                    LOG_ERR("ERROR: Expected " + to_string(N_UNIFORM_EFF_CURVE_POINTS) + " points for UNIFORM_NORMALIZED_CURVE for generator " + to_string(g));
                                }

                                generators[g].use_uniform_normalized_curve = true;

                                for (size_t p = 0; p < N_UNIFORM_EFF_CURVE_POINTS; ++p) {
                                    line = gc->topoparser.getLine(gen_data_lines);
                                    keyword = line_obj.extractNextElementFromLine(&line);
                                    value   = line_obj.extractNextElementFromLine(&line);
                                    generators[g].uniform_normalized_curve[p]    = atof(value.c_str());
                                    // cout << "generators[g].uniform_normalized_curve[p] = " << generators[g].uniform_normalized_curve[p] << "\n";
                                    gen_data_lines++;
                                }

                                line = gc->topoparser.getLine(gen_data_lines);
                                keyword = line_obj.extractNextElementFromLine(&line);
                                value   = line_obj.extractNextElementFromLine(&line);
                            
                                if(keyword != "GENERATOR_MAX_DISCHARGE") {
                                    LOG_INFO("Powerstation::ReadNodeData   nodename: " + nodename + ", idnr: " + std::to_string(int(idnr)) + ", nodetype: " + EnumToString(nodetype));
                                    LOG_WARN("Powerstation::ReadNodeData   nodename: " + nodename + ", idnr: " + std::to_string(int(idnr)) + ", nodetype: " + EnumToString(nodetype));
                                    LOG_ERR("ERROR: Expected GENERATOR_MAX_DISCHARGE for generator " + to_string(g));
                                }
                                generators[g].max_discharge = atof(value.c_str());
                                gen_data_lines++;

                            } // End if UNIFORM_NORMALIZED_CURVE

                            if(keyword == "TURBINE_CURVE") {

                                size_t n_points = size_t(atoi(value.c_str()));
                                generators[g].turb_virkn_Q.resize(n_points);
                                generators[g].turb_virkn_psnt.resize(n_points);

                                for (size_t p = 0; p < n_points; ++p) {
                                    line = gc->topoparser.getLine(gen_data_lines);
                                    keyword = line_obj.extractNextElementFromLine(&line);
                                    value   = line_obj.extractNextElementFromLine(&line);
                                    generators[g].turb_virkn_Q[p]    = atof(keyword.c_str());
                                    generators[g].turb_virkn_psnt[p] = atof(value.c_str());
                                    gen_data_lines++;
                                }


                                generators[g].eff_curve.nr_pts = n_points;
                                for (size_t p = 0; p < n_points; ++p) {
                                    generators[g].eff_curve.x_points[p] = generators[g].turb_virkn_Q[p];
                                    generators[g].eff_curve.y_points[p] = generators[g].turb_virkn_psnt[p];
                                }
                                generators[g].eff_curve.initializeArrays();

                                line = gc->topoparser.getLine(gen_data_lines);
                                keyword = line_obj.extractNextElementFromLine(&line);
                                value   = line_obj.extractNextElementFromLine(&line);
                                gen_data_lines++;
                            
                                if(keyword != "GENERATOR_MAX_DISCHARGE") {
                                    LOG_INFO("Powerstation::ReadNodeData   nodename: " + nodename + ", idnr: " + std::to_string(int(idnr)) + ", nodetype: " + EnumToString(nodetype));
                                    LOG_WARN("Powerstation::ReadNodeData   nodename: " + nodename + ", idnr: " + std::to_string(int(idnr)) + ", nodetype: " + EnumToString(nodetype));
                                    LOG_ERR("ERROR: Expected GENERATOR_MAX_DISCHARGE for generator " + to_string(g));
                                }
                                generators[g].max_discharge = atof(value.c_str());
                            }
                            
                        }
                    }   
                    // --------   END GENERATORS      -----------------------------------------------
                    //---------------------------------------------------------------------------------

                    k++; // Move to the next line for the next iteration
                }
            }
        }
    }


    // Quality controle of the generator values after reading the topology file.
    for(size_t g = 0; g < generators.size(); ++g) {
        for(size_t p = 0; p < generators[g].turb_virkn_Q.size(); ++p) {
            if(generators[g].turb_virkn_Q[p] < 0.0) {
                LOG_ERR("ERROR: Negative flow value in turbine curve for generator " + std::to_string(g) + " in powerstation " + std::to_string(int(idnr)) + " (" + nodename + ")");
            }
            if(generators[g].turb_virkn_psnt[p] < 0.0 || generators[g].turb_virkn_psnt[p] > 100.0) {
                LOG_ERR("ERROR: Invalid efficiency value in turbine curve for generator " + std::to_string(g) + " in powerstation " + std::to_string(int(idnr)) + " (" + nodename + "). Efficiency should be between 0 and 100.");
            }

        }
        if(generators[g].max_discharge < 0.0) {
            LOG_ERR("ERROR: Negative max discharge for generator " + std::to_string(g) + " in powerstation " + std::to_string(int(idnr)) + " (" + nodename + ")");
        }
    }


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
        LOG_ERR("The statefile " + filename + " could not be found/opened.");
    }

    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {
            // Line is not empty and doesn't start with # (hash/pound sign)
            string str_tmpline = line;  // Create a copy of the line for parsing
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);

            if ( keyword.compare("NODE") == 0 && value.compare("PSTATION") == 0 ) {

                size_t n_cols = line_obj.calcNrCols(&str_tmpline);

                // BVM May 2026. 
                // For now we only accept one init value for PSTATION. 
                // When we have more than one generator, this could be a problem. Fix later 
                // There must be 5 columns in the line for it to be valid. 
                if(n_cols != 5) {
                    LOG_WARN("Invalid line in statefile " + filename + ": " + str_tmpline);
                    LOG_WARN("Powerstation::ReadStateFile  nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype));
                    LOG_ERR("Expected 5 columns for NODE PSTATION, but got " + std::to_string(n_cols));
                }

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
        LOG_WARN("Powerstation::ReadStateFile     idnr= " + std::to_string(int(idnr)) + "  nodename=" + nodename);
        LOG_INFO("This could have been caused by indexing of your nodes.");
        LOG_INFO("Start with zero at the top, and work your way down, to the outlet.");
        LOG_WARN("This could have been caused by indexing of your nodes.");
        LOG_WARN("Start with zero at the top, and work your way down, to the outlet.");
        LOG_ERR("There is something wrong in the statefile " + filename);
    }
    return 0;
}
///////////////////////////////////////////////////////////////////////

// CHANGE BY OVE:  CheckWaterBalance method that uses variable timesteps
int Powerstation::CheckWaterBalance(class Herss *herss_obj) {
    double sum_inflow  = 0.0;
    double sum_outflow = 0.0;
    for(size_t t = 0; t < this->stps; t++) {
        int variable_dt = herss_obj->getDeltaT(t);
        sum_inflow += MACRO_m3s_2_Mm3( (this->S->inflow[t] + this->S->up_inflow[t]) , variable_dt);
        sum_outflow += MACRO_m3s_2_Mm3(this->S->tot_outflow[t], variable_dt);
    }

    double waterbalance = sum_inflow - sum_outflow;

    if(WATERBALANCE_WARNINGS) {
        printf( "WATERBALANCE POWERSTATION (Variable Timesteps) idnr=%d   nodename=%s\n", int(idnr), nodename.c_str()  );
        printf("sum_inflow_Mm3    = %.6f\n", sum_inflow);
        printf("sum_outflow_Mm3   = %.6f\n", sum_outflow);
        printf("waterbalance      = %.6f\n", waterbalance);
        printf("-------------------------------------------\n");
    }

    if(abs(waterbalance) > 0.0001) {
        printf( "WATERBALANCE POWERSTATION (Variable Timesteps) idnr=%d  nodename=%s\n", int(idnr), nodename.c_str()  );
        sum_inflow  = 0.0;
        sum_outflow = 0.0;

        printf("t year month day hour inflow up_inflow tot_outflow ");
        for (size_t g = 0; g < generators.size(); ++g) {
            printf(" action_g%zu ", g);
        }
        printf(" sum_inflow sum_outflow diff\n");

        for(size_t t = 0; t < this->stps; t++) {
            int variable_dt = herss_obj->getDeltaT(t);
            sum_inflow += MACRO_m3s_2_Mm3( (this->S->inflow[t] + this->S->up_inflow[t]) , variable_dt);
            sum_outflow += MACRO_m3s_2_Mm3(this->S->tot_outflow[t], variable_dt);

            printf("%d %d %d %d %d %.5f %.5f %.5f ", int(t), S->year[t], S->month[t], S->day[t], S->hour[t], 
                MACRO_m3s_2_Mm3(this->S->inflow[t], variable_dt), 
                MACRO_m3s_2_Mm3(this->S->up_inflow[t], variable_dt ),
                MACRO_m3s_2_Mm3(this->S->tot_outflow[t], variable_dt));

            for (size_t g = 0; g < generators.size(); ++g) {
                printf(" %.5f ", generators[g].action[t]);
            }

            printf(" %.6f  %.6f %.6f \n", sum_inflow, sum_outflow , sum_inflow - sum_outflow );
        }

        LOG_WARN("WATERBALANCE POWERSTATION  idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename);
        LOG_WARN("Action larger than available water?");
        LOG_WARN("sum_inflow    = " + std::to_string(sum_inflow));
        LOG_WARN("sum_outflow   = " + std::to_string(sum_outflow));
        LOG_WARN("waterbalance  = " + std::to_string(waterbalance));
        LOG_ERR("idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename);
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

    // CHANGES BY OVE: Added print for generator actions
    FILE *fp;
    char outfilename [100];
    char outstr [100];

    sprintf (outfilename, "%snode%lu_%s.txt", gc->outputdir.c_str() , (idnr) , nodename.c_str()  );

    if((fp = fopen(  outfilename ,"w"))==NULL) {
        LOG_ERR("Cannot open file " + std::string(outfilename));
    }

    sprintf (outstr, "POWERSTATION node %d %s\n", int(idnr) , nodename.c_str() );
    fprintf(fp, "%s", outstr);
    fprintf(fp, "init_Power = %.5f\n", this->init_Power);
    fprintf(fp, "penstock_config = %s\n", shared_penstock ? "SHARED" : "SEPARATE");
    fprintf(fp, "headloss_coef = %.6f\n", this->headlosscoef);

    fprintf(fp, "yyyy mm dd hh [m3/s]    [Euro/MWh] ");

    for (size_t g = 0; g < generators.size(); ++g) {
        fprintf(fp, " [fr_g%zu]", g);
    }
    fprintf(fp, " [m3/s] [m3/s] [m] [m] [MWh] [GWh/Mm3] [Euro] [Euro] [Euro] [Euro] [Euro] [Euro]\n");

    fprintf(fp, "yyyy mm dd hh Up_Inflow Price");
    for (size_t g = 0; g < generators.size(); ++g) {
        fprintf(fp, " Action_g%zu", g);
    }
    fprintf(fp, " tot_outflow auto_qmin Hnetto Hbrutto Power est_eekv income tot_cost startstopCost adjust_cost cost_aggressive_actions profit\n");

    for(size_t t = 0; t < this->stps; t++) {
        fprintf(fp, "%d %d %d %d ", S->year[t], S->month[t], S->day[t], S->hour[t]);
        fprintf(fp, "%.4f %.4f", S->up_inflow[t], S->price[t]);
        for (size_t g = 0; g < generators.size(); ++g) {
            double act = (generators[g].action.size() > t) ? generators[g].action[t] : -9999.0;
            fprintf(fp, " %.4f", act);
        }
        fprintf(fp, " %.4f %.4f %.4f %.4f %.4f %.4f ",
            S->tot_outflow[t], S->auto_qmin_m3s[t], S->Hnetto[t], S->Hbrutto[t], S->Power[t], S->EstimatedEEKV[t]);
        fprintf(fp, " %.4f %.4f %.4f %.4f %.4f %.4f\n",
            S->income[t], S->cost[t] , S->startStopCost[t], S->adjust_cost[t], S->cost_aggressive_actions[t], S->profit[t]);
    }

    fclose(fp);
    return 0;
}
//////////////////////////////////////////////////////////////////////////////////
double Powerstation::GetTunnelFLow(size_t t) {

    aggressive_actions_cost = 0.0;
    S->auto_qmin_m3s[t] = 0.0;

    double flow = 0.0;
    for (size_t g = 0; g < generators.size(); ++g) {
        if (generators[g].action.size() <= t) continue;
        if (generators[g].action[t] < -0.000001) {
            LOG_WARN("ERROR: action is negative for generator " + std::to_string(g));
            LOG_ERR("NODE PSTATION " + std::to_string(int(idnr)) + " " + nodename + " action= " + std::to_string(generators[g].action[t]));
        }
        flow += generators[g].action[t] * generators[g].max_discharge;
    }

    if (auto_qmin > 0.0) {
        double raw = flow;
        flow = smax(flow, auto_qmin, EPS_M3S);
        S->auto_qmin_m3s[t] = flow - raw;     // contribution of the floor
    }

    // Penalty is exactly zero when the request is feasible.
    double excess = MACRO_m3s_2_Mm3(flow, S->dt) - up_res_Mm3;
    aggressive_actions_cost = relu_lo(excess, EPS_MM3) * HERSS_AGGRESSIVE_ACTIONS_COST;
    S->cost_aggressive_actions[t] = aggressive_actions_cost;

    // Cap at available water. Never negative, never above available.
    return smin_nn(flow, MACRO_Mm3_2_m3s(up_res_Mm3, S->dt), EPS_REL_FLOW);
}
//////////////////////////////////////////////////////////////////////////////////
// Adjoint of GetTunnelFLow(t) for the given up_res_Mm3 (the reservoir recomputes it, the member is overwritten every step).
//   flow_grad = d(VF)/d(tunnel flow [m3/s]), as seen by the reservoir and by the water arriving at this station.
// Adds d(VF)/d(action) to generators[g].action_grad[t] and returns d(VF)/d(up_res_Mm3).
// The aggressive-actions cost is part of this station's profit, so it uses profit_grad[t].
double Powerstation::BackwardTunnelFlow(size_t t, double up_res_Mm3, double flow_grad) {

    const double dt_s        = double(S->dt);
    const double Mm3_per_m3s = dt_s / 1000000.0;
    const double m3s_per_Mm3 = 1000000.0 / dt_s;

    double flow_req = 0.0;     // Sum of action * max_discharge
    for (size_t g = 0; g < generators.size(); ++g) {
        if (generators[g].action.size() <= t) continue;
        flow_req += generators[g].action[t] * generators[g].max_discharge;
    }

    double flow         = flow_req;     // After the auto_qmin floor
    double dflow_dreq   = 1.0;
    if (auto_qmin > 0.0) {
        flow       = smax(flow_req, auto_qmin, EPS_M3S);
        dflow_dreq = 1.0 - relu_hi_d(auto_qmin - flow_req, EPS_M3S);
    }

    // aggressive_actions_cost = relu_lo(flow [Mm3] - up_res_Mm3) * COST, and profit = ... - aggressive_actions_cost
    const double excess      = flow * Mm3_per_m3s - up_res_Mm3;
    const double aggr_grad   = -S->profit_grad[t] * HERSS_AGGRESSIVE_ACTIONS_COST * relu_lo_d(excess, EPS_MM3);

    // tunnel flow = smin_nn(flow, up_res_Mm3 [m3/s])
    double dfa = 0.0, dfb = 0.0;
    smin_nn_grad(flow, up_res_Mm3 * m3s_per_Mm3, EPS_REL_FLOW, dfa, dfb);

    const double flow_total_grad = flow_grad * dfa + aggr_grad * Mm3_per_m3s;
    const double up_res_grad     = flow_grad * dfb * m3s_per_Mm3 - aggr_grad;

    for (size_t g = 0; g < generators.size(); ++g) {
        if (generators[g].action_grad.size() <= t) {
            generators[g].action_grad.resize(S->stps, 0.0);
        }
        generators[g].action_grad[t] += flow_total_grad * dflow_dreq * generators[g].max_discharge;
    }

    return up_res_grad;
}
//////////////////////////////////////////////////////////////////////////////////
int Powerstation::WriteStateFile(FILE *fp) {
    fprintf (fp, "NODE PSTATION %d %s %.5f\n", int(idnr), nodename.c_str(), this->S->Power[S->stps-1]);
    return 0;
}
//////////////////////////////////////////////////////////////////////////////////
double Powerstation::CalcAdjustmenCosts(void) {

    // BVM, June 2026. 
    // We are using multi-temporal resolution, 
    // At daily and weekly resolutions, this method doesnt make sense.
    // Should be used when we have sub-daily tempral resolution 

    double prev_power = init_Power;
    double diff;
    int nr_changes_pr_day = 0;
    double sum_cost = 0.0;

    // cout << " this->max_adjustment_cost = " << this->max_adjustment_cost << "\n";

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
////////////////////////////////////////////////////////////////////////////////