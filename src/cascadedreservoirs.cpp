/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     cascadedreservoirs.cpp
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
********************************************************************************/

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <array>
#include <cmath>
#include <iomanip>
#include <stdexcept>

#include "herss.h"


//------------------------------------------------------------------------------------------------
CascadedReservoirs::CascadedReservoirs(double k_traveltime_hours, size_t num_reservoirs) {

    // Traveltime should be 0 or more.
    // We set maximum to be #define MAX_TRAVELTIME_HOURS 240 => 10 days 
    if (k_traveltime_hours < 0.0 || k_traveltime_hours > MAX_TRAVELTIME_HOURS) {
        LOG_ERR("Travel time must be between 0 and " + std::to_string(MAX_TRAVELTIME_HOURS) + " hours");
    }

    // Check if the number of reservoirs exceeds the maximum allowed
    if (num_reservoirs < 1 || num_reservoirs > MAX_NR_CASCADED_RESERVOIRS) {
        LOG_ERR("Number of cascaded reservoirs must be between 1 and " 
            + std::to_string(MAX_NR_CASCADED_RESERVOIRS));
    }

    this->k_traveltime_hours = k_traveltime_hours;
    this->k_total_seconds = k_traveltime_hours * 3600.0;
    this->k_res_seconds = k_total_seconds / num_reservoirs; 
    this->num_reservoirs = num_reservoirs;
    linreservoirs.resize(num_reservoirs);

};
//------------------------------------------------------------------------------------------------
CascadedReservoirs::~CascadedReservoirs() {
    // Destructor logic if needed
}
//------------------------------------------------------------------------------------------------
double CascadedReservoirs::getStorageMm3(size_t idx_linres) {
    if (idx_linres >= linreservoirs.size()) {
        LOG_ERR("Index out of range in getStorageMm3");
    }
    return linreservoirs[idx_linres].storage_m3 / 1e6; // Convert m3 to Mm3
}
//------------------------------------------------------------------------------------------------
double CascadedReservoirs::totalStorageM3() {

    double total_storage_m3 = 0.0;

    for (const auto& reservoir : linreservoirs) {
        total_storage_m3 += reservoir.storage_m3;
    }

    return total_storage_m3;
}
//------------------------------------------------------------------------------------------------
void CascadedReservoirs::setInitialStorage(std::vector<double> initial_storage_Mm3) {

    if (initial_storage_Mm3.size() != linreservoirs.size()) {
        LOG_ERR("Initial storage size does not match the number of reservoirs");
    }

    for (size_t i = 0; i < linreservoirs.size(); ++i) {
        linreservoirs[i].storage_m3 = initial_storage_Mm3[i] * 1e6; // Convert Mm3 to m3
        if (linreservoirs[i].storage_m3 < 0.0) {
            LOG_ERR("Initial storage cannot be negative");
        }
    }
}
//------------------------------------------------------------------------------------------------
double CascadedReservoirs::route(double q_in_m3s, double dt_seconds) {
    double q_for_next = q_in_m3s;
    for (size_t i = 0; i < linreservoirs.size(); ++i) {
        ReservoirStepResult result =
             routeOneReservoir(linreservoirs[i].storage_m3, q_for_next, k_res_seconds, dt_seconds);
        linreservoirs[i].storage_m3 = result.storage_new_m3;
        q_for_next = result.q_out_avg_m3s;
    }
    return q_for_next; // average outlet discharge over the step
};
//------------------------------------------------------------------------------------------------
// Per linear reservoir, with f = exp(-dt/k):
//   S_new = f * S_old + k (1 - f) * q_in
//   q_out = (S_old + q_in dt - S_new) / dt = ((1 - f) * S_old + (dt - k (1 - f)) * q_in) / dt
// The clipping of q_out at zero in routeOneReservoir only removes rounding errors (the expression is >= 0), so it has no gradient.
void CascadedReservoirs::routeBackward(double q_out_grad, double dt_seconds, std::vector<double> &storage_grad, double &q_in_grad) {

    if(storage_grad.size() != linreservoirs.size()) {
        storage_grad.assign(linreservoirs.size(), 0.0);
    }

    const double f  = std::exp(-dt_seconds / k_res_seconds);
    const double kf = k_res_seconds * (1.0 - f);
    const double a  = (1.0 - f) / dt_seconds;          // d(q_out)/d(S_old)
    const double b  = (dt_seconds - kf) / dt_seconds;  // d(q_out)/d(q_in)

    double q_grad = q_out_grad;                        // Adjoint of the flow leaving linear reservoir i, then entering it
    for (size_t i = linreservoirs.size(); i-- > 0; ) {
        const double S_new_grad = storage_grad[i];
        storage_grad[i] = a * q_grad + f * S_new_grad;
        q_grad          = b * q_grad + kf * S_new_grad;
    }
    q_in_grad = q_grad;
}
//------------------------------------------------------------------------------------------------
ReservoirStepResult CascadedReservoirs::routeOneReservoir(
    double storage_old_m3, double q_in_m3s, double k_seconds, double dt_seconds) {

    double factor = std::exp(-dt_seconds / k_seconds);


	//This can be seen directly from the exact discrete-time update
    //  of a linear reservoir under constant inflow over a timestep:
	//\begin{equation}
	//	S_{t+\Delta t} = S_t e^{-\Delta t/K} + K\left(1-e^{-\Delta t/K}\right) I_t.
	//\end{equation}
    // See actual equation in user manual. 
    double storage_new_m3 = storage_old_m3 * factor + k_seconds * (1.0 - factor) * q_in_m3s;

    double volume_out_m3 =  storage_old_m3 + q_in_m3s * dt_seconds - storage_new_m3;

    double q_out_avg_m3s = volume_out_m3 / dt_seconds;
    double q_out_end_m3s = storage_new_m3 / k_seconds;

    if (q_out_avg_m3s < 0.0) q_out_avg_m3s = 0.0;
    if (q_out_end_m3s < 0.0) q_out_end_m3s = 0.0;

    return {storage_new_m3, q_out_avg_m3s, q_out_end_m3s};
};
//------------------------------------------------------------------------------------------------