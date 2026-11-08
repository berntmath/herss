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
#include <limits>


Reservoir::Reservoir(){
    reservoir_init_fr              = NOT_INIT;
    reservoir_init_masl            = NOT_INIT;
    reservoir_init_Mm3             = NOT_INIT;

    res_HRW                        = NOT_INIT;
    filling_at_hrw_Mm3             = NOT_INIT;
    filling_at_hatchlevel          = NOT_INIT;
    filling_at_spillway_Mm3        = NOT_INIT;

    res_LRW                        = NOT_INIT;
    filling_at_lrw_Mm3             = NOT_INIT;

    res_Mm3                        = NOT_INIT;
    res_masl                       = NOT_INIT;
    res_fr                         = NOT_INIT;
    nr_points_res_curve            = 0;
    nr_points_ovefl_curve          = 0;

    downstream_node_in_use         = false;

    outlet_hatch_in_use            = false;
    outlet_tunnel_in_use           = false;
    outlet_overflow_in_use         = false;
    
    outlet_auto_qmin_in_use        = false;
 
    // Terje Sandø, 02.07.2026
    // MASL of the outlet_auto_min on the reservoir.
    outlet_auto_qmin_masl          = -1.0 * NOT_INIT;

    use_reservoir_geometry        = false;
    use_reservoir_curve           = false;

    //-----------------------------------------------
    // OVERFLOW 
    use_overflow_curve            = false;
    use_spillway                  = false;
    //-----------------------------------------------

    minQ_hatch                     = NOT_INIT;
    maxQ_hatch                     = NOT_INIT;
    hatch_masl                     = NOT_INIT;
    res_penalty                    = -1.0*NOT_INIT;
    filling_f0                     = 0.0;
    filling_penalty                = 0.0;
    cost_filling                   = 0.0;
    hatch_adjustment_cost          = 0.0;
    cost_hatch_adjust              = 0.0;
    floodlevel_cost                = -1.0*NOT_INIT;

    fast_overflow = false; 
    
    width_m     = -1.0*NOT_INIT;
    length_m    = -1.0*NOT_INIT;
    theta       = -1.0*NOT_INIT; // angle of the sides of the reservoir, in degrees.
    bottom_masl = -1.0*NOT_INIT; // masl of the bottom of the reservoir.
    slope_term  = -1.0*NOT_INIT; // tan((90.0 - theta) * PI / 180.0);
    geo_denom   = -1.0*NOT_INIT; // length_m * (slope_term + width_m);

    ptr_downstream_node            = NULL;
    ptr_downstream_node_tunnel     = NULL;
    ptr_downstream_node_hatch      = NULL;
    ptr_downstream_node_overflow   = NULL; 
    ptr_downstream_node_auto_qmin  = NULL;

    res_Mm3_init_grad              = 0.0;

}
//----------------------------------------------------------------------------------
Reservoir::~Reservoir(){}
//----------------------------------------------------------------------------------
int Reservoir::initArrayCurves(void) {

    if(use_reservoir_curve) {
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

    }
    
    // We need to check wether we use spillway or overflow curve for the overflow calculation. We cannot use both.
    if(this->use_overflow_curve && this->use_spillway) {
        LOG_ERR("ERROR: Cannot use both overflow curve and reservoir geometry for overflow calculation. Node idnr = "
             + std::to_string(int(this->idnr)) + "   nodename = " + this->nodename );
    }
    
    
    if(use_overflow_curve) {    
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
    }

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

    // Added this to void warning - have not checked that it is correct.
    masl_start_overflow = this->res_HRW;


    if(this->use_overflow_curve) {
        masl_start_overflow = this->ovefl_curve_masl[0];  
    } else if(this->use_spillway) {
        masl_start_overflow = this->spillway_level_masl;
    }

    // CHANGE BY OVE: Fast overflow calculation
    if (fast_overflow) {
        if (this->res_masl > masl_start_overflow) {
            overflow_Mm3 = this->res_Mm3 - filling_at_hrw_Mm3;
            if (overflow_Mm3 < 0.0) overflow_Mm3 = 0.0;
        }
        return overflow_Mm3;
    } else {

        // We have two options for overflow calculations. 
        // Overflow curve or spillway. 
        if(this->use_overflow_curve && this->use_spillway) {
            LOG_ERR("ERROR: Cannot use both overflow curve and reservoir geometry for overflow calculation. Node idnr = " + std::to_string(int(this->idnr)) + "   nodename = " + this->nodename );
        }


        if(this->use_overflow_curve) {
            // The bottom point in the overflow curve is usually the same as HRW, but not always.
            if(this->res_masl > masl_start_overflow) {

                // Check if res_masl is above the maximum in the reservoir curve
                double masl_max_overflow = this->ovefl_curve_masl[this->nr_points_ovefl_curve - 1];
                if(this->res_masl > masl_max_overflow) {
                    LOG_WARN("res_masl (" + std::to_string(this->res_masl) + ") is above maximum overflow curve level (" + std::to_string(masl_max_overflow) + ")");
                    LOG_ERR("Please check your overflow curve for node idnr = " + std::to_string(int(this->idnr)) + "   nodename = " + this->nodename );
                }

                overflow_m3s = ac_ovefl_masl_2_m3s.x2y(this->res_masl);
                overflow_Mm3 = MACRO_m3s_2_Mm3(overflow_m3s,S->dt);

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
                    LOG_WARN("Negative overflow is not allowed \n");
                    LOG_WARN("current_filling  = " + std::to_string(this->res_Mm3)); 
                    LOG_WARN("filling_at_hrw_Mm3 = " + std::to_string(filling_at_hrw_Mm3));
                    LOG_WARN("masl_start_overflow= " + std::to_string(masl_start_overflow));
                    LOG_WARN("overflow_Mm3 = " + std::to_string(overflow_Mm3));
                    LOG_WARN("res_masl= " + std::to_string(this->res_masl));
                    LOG_WARN("masl_start_overflow = " + std::to_string(masl_start_overflow));

                    std::string debug_info = std::string(__FILE__) + " linenr: " + std::to_string(__LINE__) + " function: " + std::string(__FUNCTION__);
                    // printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
                    LOG_WARN(debug_info);

                    LOG_ERR("Node idnr = " + std::to_string(int(this->idnr)) + "   nodename = " + this->nodename );
                    return -9;
                }

            }
        } else if(this->use_spillway) {

            // SMOOTH
            // BVM 14 oct 2026.
            // FOund some numerical issues. Outflow can be larger than inflow when reservoir is full. 
            // This is because I am using hte head after I have added water.
            // Using average filling to calculate the head for the spillway.
            double head_m = relu_hi((this->res_masl + spillway_level_masl) / 2.0 - spillway_level_masl, EPS_M);

            overflow_m3s = spillway_C * spillway_L * head_m * sqrt(head_m);
            overflow_Mm3 = MACRO_m3s_2_Mm3(overflow_m3s,S->dt);

            // We can have situations where the spillway is set lower than HRW.
            // Only water above the spillway crest can leave, this goes essentialy all the way down to the bottom of the reservoir
            double available_Mm3 = relu_lo(this->res_Mm3 - filling_at_spillway_Mm3, EPS_MM3);

            return smin_nn(overflow_Mm3, available_Mm3, EPS_REL_FLOW);

        } else {
            LOG_ERR("ERROR: No method for calculating overflow is specified. Node idnr = " + std::to_string(int(this->idnr)) + "   nodename = " + this->nodename );
        }

        return overflow_Mm3;
    }
}
////////////////////////////////////////////////////////////////
double Reservoir::calcResVolume(double masl) {  // Returns Mm3

    double h = masl - bottom_masl;
    if (h < 0.0) {
        LOG_ERR("ERROR: masl (" + std::to_string(masl) + 
        ") is below bottom_masl (" + std::to_string(bottom_masl) + ").\n");
    }

    double area = width_m * h  +  (h * h * slope_term);
    return area * length_m / 1000000.0; // Convert to Mm3

}
////////////////////////////////////////////////////////////////
double Reservoir::calcResMasl(double Mm3) {     // Returns masl
    if (Mm3 < 0.0) {
        LOG_ERR("Error: Mm3 (" + std::to_string(Mm3) + ") is negative in calcResMasl.\n");
    }

    double a = length_m * slope_term;
    double b = length_m * width_m;
    double c = -1*Mm3*1000000.0;
    double h_new = (-b + sqrt(b*b - 4*a*c)) / (2*a);

    return bottom_masl + h_new;  // masl 
}
////////////////////////////////////////////////////////////////
void Reservoir::InitReservoir(void) {

    for(size_t t = 0; t < S->stps; t++ ) {
        S->up_inflow[t]    = 0.0;
        // Terje Sandø, pump-station work, August 2026.
        // Pump nodes accumulate into these with +=, so they must start at zero for every run.
        S->pump_in_m3s[t]  = 0.0;
        S->pump_out_m3s[t] = 0.0;
    }

    if(this->use_reservoir_curve) {

        if(this->nr_points_res_curve < 2) {
            LOG_ERR("Reservoir curve not initialized");
        }
    
        if(this->reservoir_init_fr < -1.0) {
            LOG_ERR("ERROR Something wrong with reservoir_init_fr= " + std::to_string(this->reservoir_init_fr));
        }

        filling_at_lrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_LRW);
        filling_at_hrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_HRW);

        this->active_max_volume_Mm3 = filling_at_hrw_Mm3 - filling_at_lrw_Mm3;

        res_Mm3 = filling_at_lrw_Mm3 + reservoir_init_fr * (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);

        this->reservoir_init_Mm3 = res_Mm3;
        this->reservoir_init_active_Mm3 = res_Mm3 - filling_at_lrw_Mm3;

        // Note that reservoir content is the water between HRW and LRW.
        // That volume cannot be used directly to calculate the filling in meters above sea level.
        res_masl = ac_res_Mm3_2_masl.x2y(res_Mm3);
        this->reservoir_init_masl = res_masl;

        // Calculate filling at hatch level here so do it only once, and not every timestep.
        if(outlet_hatch_in_use) {
            filling_at_hatchlevel = ac_res_masl_2_Mm3.x2y(this->hatch_masl);
        }

        if(use_spillway) {
            filling_at_spillway_Mm3 = ac_res_masl_2_Mm3.x2y(this->spillway_level_masl);
        }
    }


    if(this->use_reservoir_geometry) {

        if(this->width_m <= 0.0 || this->length_m <= 0.0 || this->theta < 0.0 || this->theta >= 90.0) {
            LOG_ERR("Reservoir geometry is not initialized correctly, check width_m, length_m, theta");
        }

        slope_term = tan((90.0 - theta) * PI / 180.0);

        filling_at_lrw_Mm3 = calcResVolume(this->res_LRW);
        filling_at_hrw_Mm3 = calcResVolume(this->res_HRW);

        this->active_max_volume_Mm3 = filling_at_hrw_Mm3 - filling_at_lrw_Mm3;

        res_Mm3 = filling_at_lrw_Mm3 + reservoir_init_fr * (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);

        this->reservoir_init_Mm3 = res_Mm3;
        this->reservoir_init_active_Mm3 = res_Mm3 - filling_at_lrw_Mm3;
       
        // Note that reservoir content is the water between HRW and LRW.
        // That volume cannot be used directly to calculate the filling in meters above sea level.
        res_masl = this->calcResMasl(res_Mm3);
       
        this->reservoir_init_masl = res_masl;

        // Calculate filling at hatch level here so do it only once, and not every timestep.
        if(outlet_hatch_in_use) {
            filling_at_hatchlevel = this->calcResVolume(this->hatch_masl);
        }

        if(use_spillway) {
            filling_at_spillway_Mm3 = this->calcResVolume(this->spillway_level_masl);
        }
    }
}
////////////////////////////////////////////////////////////////
// Check that initial settings are valid and within bounds. 
void Reservoir::ValidateReservoirSettings() {

    // The reservoir must be initialized between 0.0 and 1.5. 
    // Note that with a filling of 1.5, there is substantial flooding in the system . 
    if(this->reservoir_init_fr < -0.000001 || this->reservoir_init_fr > 1.5) {
        LOG_WARN("Warning reservoir_init_fr is out of bounds: " + std::to_string(this->reservoir_init_fr));
        LOG_ERR("Reservoir: " + this->nodename);
    }

    if(this->res_HRW < 0.0 || this->res_HRW > MOUNT_EVEREST_MASL) {
        LOG_WARN("ERROR HRW is out of bounds: " + std::to_string(this->res_HRW));
        LOG_ERR("Reservoir: " + this->nodename);
    }

    if(this->res_LRW < 0.0 || this->res_LRW > MOUNT_EVEREST_MASL) {
        LOG_WARN("ERROR LRW is out of bounds: " + std::to_string(this->res_LRW));
        LOG_ERR("Reservoir: " + this->nodename);
    }

    if(this->res_LRW >= this->res_HRW) {
        LOG_ERR("ERROR: LRW must be lower than HRW: LRW=" + std::to_string(this->res_LRW) + ", HRW=" + std::to_string(this->res_HRW));
        LOG_ERR("Reservoir: " + this->nodename);
    }

    if(this->res_penalty < 0.0 || this->res_penalty > VERY_LARGE_NUMBER) {
        LOG_WARN("ERROR: RES_PENALTY is out of bounds: " + std::to_string(this->res_penalty));
        LOG_ERR("Reservoir: " + this->nodename);
    }

    if(this->res_penalty < 0.0 || this->res_penalty > VERY_LARGE_NUMBER) {
        LOG_WARN("ERROR: RES_PENALTY must be non-negative, and not larger than VERY_LARGE_NUMBER : " + std::to_string(this->res_penalty));
        LOG_ERR("Reservoir: " + this->nodename);
    }

    if(this->use_reservoir_curve) {
        if(this->nr_points_res_curve < 2 || this->nr_points_res_curve > MAX_NR_POINTS_CURVE) {
            LOG_WARN("ERROR: Number of points in reservoir curve is out of bounds: " + std::to_string(this->nr_points_res_curve));
            LOG_ERR("Reservoir: " + this->nodename);
        }
    }


    if(this->nr_points_ovefl_curve < 0 || this->nr_points_ovefl_curve > MAX_NR_POINTS_CURVE) {
        LOG_WARN("ERROR: Number of points in overflow curve is out of bounds: " + std::to_string(this->nr_points_ovefl_curve));
        LOG_ERR("Reservoir: " + this->nodename);
    }

    // Terje Sandø, 02.07.2026
    // Validate outlet_auto_qmin MASL and qmin period definitions.
    // outlet_auto_qmin_masl must be >= reservoir bottom.
    // validatePeriods checks date ranges, year-crossings, coverage gaps, and overlaps.
    if(outlet_auto_qmin_in_use) {
        if(use_reservoir_geometry && outlet_auto_qmin_masl < bottom_masl) {
            LOG_WARN("OUTLET_AUTO_QMIN_MASL (" + std::to_string(outlet_auto_qmin_masl) + ") is below BOTTOM_MASL ("
                + std::to_string(bottom_masl) + ") for reservoir " + nodename);
            LOG_ERR("OUTLET_AUTO_QMIN_MASL must be >= BOTTOM_MASL. Check topology file.");
        }
        if(use_reservoir_curve && outlet_auto_qmin_masl < res_curve_masl[0]) {
            LOG_WARN("OUTLET_AUTO_QMIN_MASL (" + std::to_string(outlet_auto_qmin_masl) + ") is below lowest reservoir curve point ("
                + std::to_string(res_curve_masl[0]) + ") for reservoir " + nodename);
            LOG_ERR("OUTLET_AUTO_QMIN_MASL must be >= lowest reservoir curve point. Check topology file.");
        }
        this->qmin.validatePeriods(this->nodename);
    }

}
////////////////////////////////////////////////////////////////
// We use this to check if the reservoir level is valid.
void Reservoir::ValidateReservoirLevelMm3(size_t t, double level_Mm3) {
    if(this->use_reservoir_curve) {
        if(level_Mm3 > res_curve_Mm3[nr_points_res_curve-1]) {
            LOG_WARN("ERROR: Numerical instability, there is too much water in your system \n");
            LOG_WARN("Reservoir::Simulate nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype));
            LOG_ERR("To fix this add more volume at the top of you reservoir curve");
        }
    }
}
///////////////////////////////////////////////////////////////
// d(masl)/d(Mm3) of calcResMasl: h = (-b + sqrt(b^2 + 4 a V)) / (2 a), so dh/dV = 1 / sqrt(b^2 + 4 a V), V in m3.
double Reservoir::dResMasl_dMm3(double Mm3) {
    const double a = length_m * slope_term;
    const double b = length_m * width_m;
    return 1000000.0 / sqrt(b*b + 4.0*a*Mm3*1000000.0);
}
///////////////////////////////////////////////////////////////
// Backward pass for timestep t, the reverse of Simulate(t). x_grad means d(ValueFunction)/d(x).
//
// Forward stages of Simulate(t), V = res_Mm3:
//   V1 = V0 + (inflow + up_inflow) * dt                 V0 = end volume of step t-1
//   V2 = V1 - tunnel flow          (tunnel flow = GetTunnelFLow, uses up_res = relu_lo(V1 - LRW volume))
//   V3 = V2 - hatch flow           (smin_nn of the desired flow and the water above the hatch level)
//   V4 = V3 - auto-qmin flow       (smin_nn of the required flow and the water above the outlet)
//   V5 = V4 - overflow             (spillway law capped by the water above the crest, uses masl(V4))
//   cost_lrw = penalty * relu_hi(LRW - masl(V5)),  profit = -cost_lrw (- the FILLING_PENALTY cost)
// The flows of the stages are read back from the Scenario arrays that Simulate wrote.
//
// Reads:  res_Mm3_grad[t], res_masl_grad[t], profit_grad[t], and from the downstream nodes up_inflow_grad[t] and,
//         for the tunnel node, Hbrutto_grad[t] (the head uses the level at both ends of the step).
// Writes: res_Mm3_grad[t-1] (or res_Mm3_init_grad at t = 0), up_inflow_grad[t], action_grad[t] (hatch), and through
//         BackwardTunnelFlow the actions of the powerstation.
// Supports RESERVOIR_GEOMETRY with SPILLWAY and no pumps. Everything else stops with an error.
int Reservoir::Backward(size_t t) {

    if(!use_reservoir_geometry || !use_spillway || fast_overflow || use_overflow_curve || use_reservoir_curve) {
        LOG_ERR("Reservoir::Backward requires RESERVOIR_GEOMETRY and SPILLWAY (no curves, no FAST_OVERFLOW). Node idnr = " + std::to_string(int(idnr)) + "   nodename = " + nodename);
    }
    if(!ptr_pumps_as_source.empty() || !ptr_pumps_as_target.empty()) {
        LOG_ERR("Reservoir::Backward does not support pumps yet. Node idnr = " + std::to_string(int(idnr)) + "   nodename = " + nodename);
    }

    const double dt_s = double(S->dt);
    const double c    = dt_s / 1000000.0;        // m3/s -> Mm3
    const double ci   = 1000000.0 / dt_s;        // Mm3 -> m3/s

    // ---- Volumes after each stage, from the start volume and the stored flows ----
    const double V0 = (t == 0) ? reservoir_init_Mm3 : S->res_Mm3[t-1];
    const double V1 = V0 + (S->inflow[t] + S->up_inflow[t]) * c;
    const double V2 = V1 - S->tunnelflow_m3s[t] * c;
    const double V3 = V2 - S->hatchflow_m3s[t]  * c;
    const double V4 = V3 - S->auto_qmin_m3s[t]  * c;
    const double V5 = V4 - S->overflow_Mm3[t];
    const double masl4 = calcResMasl(V4);
    const double masl5 = calcResMasl(V5);

    // ---- End of the step: level of V5 ----
    // The adjoint of the end level comes from the LRW penalty, the head of the tunnel node, and external users of res_masl.
    const double penalty = res_penalty * dt_s / 3600.0;
    double g_masl5 = S->res_masl_grad[t];
    g_masl5 += S->profit_grad[t] * penalty * relu_hi_d(res_LRW - masl5, EPS_M);     // profit = -cost_lrw
    double g_masl0 = 0.0;                                                          // Adjoint of the start level masl(V0)
    if(outlet_tunnel_in_use) {
        const double Hbrutto_grad = ptr_downstream_node_tunnel->S->Hbrutto_grad[t]; // Hbrutto = (start + end) / 2 - z
        g_masl5 += 0.5 * Hbrutto_grad;
        g_masl0 += 0.5 * Hbrutto_grad;
    }
    double g_V5 = S->res_Mm3_grad[t] + g_masl5 * dResMasl_dMm3(V5);

    // FILLING_PENALTY: cost = P dt/3600 r^2, r = relu_hi(f0 - fr), fr = (V5 - V_LRW) / W. profit = -cost.
    if(filling_penalty > 0.0) {
        const double W = filling_at_hrw_Mm3 - filling_at_lrw_Mm3;
        const double x = filling_f0 - (V5 - filling_at_lrw_Mm3) / W;
        g_V5 += S->profit_grad[t] * filling_penalty * dt_s / 3600.0 * 2.0 * relu_hi(x, EPS_FR) * relu_hi_d(x, EPS_FR) / W;
    }

    // ---- Overflow (spillway) ----
    // overflow = smin_nn(o, avail),  o = C L h^1.5 dt,  h = relu_hi((masl(V4) - crest) / 2),  avail = relu_lo(V4 - V_crest)
    const double x_h     = 0.5 * (masl4 - spillway_level_masl);
    const double h       = relu_hi(x_h, EPS_M);
    const double o       = spillway_C * spillway_L * h * sqrt(h) * c;
    const double avail_o = relu_lo(V4 - filling_at_spillway_Mm3, EPS_MM3);
    double dfa = 0.0, dfb = 0.0;
    smin_nn_grad(o, avail_o, EPS_REL_FLOW, dfa, dfb);

    // overflow leaves the reservoir (V5 = V4 - overflow) and arrives downstream as overflow * ci [m3/s]
    const double g_ovf   = -g_V5 + ptr_downstream_node_overflow->S->up_inflow_grad[t] * ci;
    const double do_dmasl4 = spillway_C * spillway_L * 1.5 * sqrt(h) * c * 0.5 * relu_hi_d(x_h, EPS_M);
    double g_V4 = g_V5 + g_ovf * dfb * relu_lo_d(V4 - filling_at_spillway_Mm3, EPS_MM3);
    g_V4 += g_ovf * dfa * do_dmasl4 * dResMasl_dMm3(V4);

    // ---- Auto-qmin ----
    // actual = smin_nn(required, relu_lo(V3 - V_outlet)),  required is a calendar value without gradient
    double g_V3 = g_V4;
    if(outlet_auto_qmin_in_use) {
        double void_cost;
        const double required_Mm3 = qmin.calcQminRequirement(S->year[t], S->month[t], S->day[t], &void_cost) * c;
        const double filling_at_outlet_Mm3 = calcResVolume(outlet_auto_qmin_masl);
        const double avail = relu_lo(V3 - filling_at_outlet_Mm3, EPS_MM3);
        smin_nn_grad(required_Mm3, avail, EPS_REL_FLOW, dfa, dfb);

        const double g_actual = -g_V4 + ptr_downstream_node_auto_qmin->S->up_inflow_grad[t] * ci;
        g_V3 += g_actual * dfb * relu_lo_d(V3 - filling_at_outlet_Mm3, EPS_MM3);
    }

    // ---- Hatch ----
    // hatch = smin_nn(desired, relu_lo(V2 - V_hatch)),  desired = (minQ + action * (maxQ - minQ)) * dt
    double g_V2 = g_V3;
    if(outlet_hatch_in_use) {
        const double desired_Mm3 = (minQ_hatch + S->action[t][idnr] * (maxQ_hatch - minQ_hatch)) * c;
        const double avail = relu_lo(V2 - filling_at_hatchlevel, EPS_MM3);
        smin_nn_grad(desired_Mm3, avail, EPS_REL_FLOW, dfa, dfb);

        const double g_hatch = -g_V3 + ptr_downstream_node_hatch->S->up_inflow_grad[t] * ci;
        S->action_grad[t] += g_hatch * dfa * (maxQ_hatch - minQ_hatch) * c;
        g_V2 += g_hatch * dfb * relu_lo_d(V2 - filling_at_hatchlevel, EPS_MM3);

        // Hatch adjustment cost couples action[t] and action[t-1]: profit = -cost, cost = fee (sqrt(d^2 + eps^2) - eps)
        if(hatch_adjustment_cost > 0.0 && t > 0) {
            const double d  = S->action[t][idnr] - S->action[t-1][idnr];
            const double gd = -S->profit_grad[t] * hatch_adjustment_cost * d / sqrt(d*d + EPS_HATCH_ACTION*EPS_HATCH_ACTION);
            S->action_grad[t]   += gd;
            S->action_grad[t-1] -= gd;
        }
    }

    // ---- Tunnel ----
    // V2 = V1 - tunnel flow, and the flow arrives at the powerstation as up_inflow (assigned, not added).
    double g_V1 = g_V2;
    if(outlet_tunnel_in_use) {
        const double g_flow_m3s = -g_V2 * c + ptr_downstream_node_tunnel->S->up_inflow_grad[t];
        const double up_res     = relu_lo(V1 - filling_at_lrw_Mm3, EPS_MM3);
        const double g_up_res   = ptr_downstream_node_tunnel->BackwardTunnelFlow(t, up_res, g_flow_m3s);
        g_V1 += g_up_res * relu_lo_d(V1 - filling_at_lrw_Mm3, EPS_MM3);
    }

    // ---- Inflow ----
    S->up_inflow_grad[t] += g_V1 * c;
    const double g_V0 = g_V1 + g_masl0 * dResMasl_dMm3(V0);
    if(t > 0) {
        S->res_Mm3_grad[t-1] += g_V0;
    } else {
        res_Mm3_init_grad = g_V0;
    }

    return 0;
}


///////////////////////////////////////////////////////////////
int Reservoir::Simulate(size_t t) {

    // BVM 27 june 2025:
    // We minimize the updating of the MASL in the reservoir. 
    // the function x2y in the ArrayCurve is used alot and is time consuming, 
    // so we try to avoid unnecessary usage 

    
    // Upstream inflow has already been set to zero or adjusted earlier.
    this->dt     = S->dt;
    this->stps   = S->stps;
    double hatchflow_Mm3 = 0.0; // To void warning
    double tunnelflow_Mm3;
    double overflow_Mm3;
    double outlet_auto_qmin_flow_Mm3;
    double total_inflow_Mm3;

    #if HERSS_DEBUG_ALL
        printf("Node idnr = %d   nodename = %s\n ", int(this->idnr) , this->nodename.c_str() );
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
    total_inflow_Mm3 = MACRO_m3s_2_Mm3(total_inflow_Mm3,S->dt);

    // Add local inflow
    this->res_Mm3 += MACRO_m3s_2_Mm3(S->inflow[t],S->dt);    // Mm3

    S->sum_local_inflow_Mm3 += MACRO_m3s_2_Mm3(S->inflow[t],S->dt);    // Mm3

    // Add upstream inflow
    this->res_Mm3 += MACRO_m3s_2_Mm3(S->up_inflow[t],S->dt);  // Mm3   All initialized to zero 

    ValidateReservoirLevelMm3(t, this->res_Mm3);

    // Update filling height - Testing without updating this 27June20205, BVM
    // this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);

    // Terje Sandø, pump-station work, July 2026.
    // Store the start level for connected pumps for later use in pump power calculations.
    for(Pump* p : ptr_pumps_as_source) { p->src_start_masl = this->res_masl; }
    for(Pump* p : ptr_pumps_as_target) { p->tgt_start_masl = this->res_masl; }
    for(Powerstation* ps : ptr_stations_as_tailwater) { ps->tw_start_masl = this->res_masl; }


    tunnelflow_Mm3 = 0.0;

    if(outlet_tunnel_in_use) {

        if(ptr_downstream_node_tunnel    == NULL) {
            LOG_WARN("ERROR IN RESERVOIR:  Something is wrong with the pointer:  ptr_downstream_node_tunnel \n");
            LOG_WARN("idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename + "   timestep=" + std::to_string(t));
            LOG_ERR("res_masl     = " + std::to_string(this->res_masl));
        }

        
        // The real level is used at both ends of the step (no cap at HRW, see end of Simulate).
        ptr_downstream_node_tunnel->start_of_stp_masl = this->res_masl;


        // SMOOTH 
        // Reservoir::Simulate
        ptr_downstream_node_tunnel->up_res_Mm3 =
            relu_lo(this->res_Mm3 - this->filling_at_lrw_Mm3, EPS_MM3);   // 0 when empty, never above true value



        ptr_downstream_node_tunnel->S->dt = S->dt;

        double tunnelf_m3s = ptr_downstream_node_tunnel->GetTunnelFLow(t);
        
        ptr_downstream_node_tunnel->S->up_inflow[t] = tunnelf_m3s;

        // BVM June 2026, 
        // ptr_downstream_node_tunnel->S->tot_outflow[t] = tunnelf_m3s;

        tunnelflow_Mm3 = MACRO_m3s_2_Mm3(tunnelf_m3s ,S->dt);  // Mm3 

        
         if(tunnelflow_Mm3 < 0.0) {
            LOG_WARN("Negative tunnel flow is not allowed \n");
            LOG_WARN("tunnelf_m3s = " + std::to_string(tunnelf_m3s));
            LOG_WARN("tunnelflow_Mm3 = " + std::to_string(tunnelflow_Mm3));
            LOG_WARN("res_masl= " + std::to_string(this->res_masl));
            LOG_WARN("res_HRW = " + std::to_string(this->res_HRW));
            LOG_ERR("Node idnr = " + std::to_string(int(this->idnr)) + "   nodename = " + this->nodename );
        }
    }




    this->res_Mm3 -= tunnelflow_Mm3;

    if(this->use_reservoir_geometry) {

        // Update the reservoir masl using the geometry. This is faster than using the curve, but less accurate. 
        this->res_masl = this->calcResMasl(this->res_Mm3);

    } else if(this->use_reservoir_curve) {
        ValidateReservoirLevelMm3(t, this->res_Mm3);
        // Update the reservoir masl using the curve. This is more accurate, but slower than using the geometry.
        this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);
    }



    

    //-------------------------------------------------------------------
    // OUTLET HATCH
    if(outlet_hatch_in_use) {

        hatchflow_Mm3 = 0.0;

        if(S->action[t][this->idnr] < -0.000001 || S->action[t][this->idnr] > 1.000001) {
            LOG_WARN("ERROR: Action for hatch is out of bounds: " + std::to_string(S->action[t][this->idnr]));
            LOG_WARN("Node idnr = " + std::to_string(int(this->idnr)) + "   nodename = " + this->nodename );
            LOG_ERR("Check your action file, and make sure the action for the hatch is between 0.0 and 1.0");
        }

        // SMOOTH
        // Some places we need to release water regardless of the actions set 
        // This can be done by setting minQ_hatch to a low level.
        double desired_hatch_m3s = this->minQ_hatch + S->action[t][this->idnr]*(this->maxQ_hatch - this->minQ_hatch);
        double desired_hatch_Mm3 = MACRO_m3s_2_Mm3(desired_hatch_m3s, S->dt);  // Mm3

        // Only water above the hatch level can be released. Working directly with volume
        // replaces the "res_masl > hatch_masl" test and the volume->masl->volume round trip.
        double available_hatch_Mm3 = relu_lo(this->res_Mm3 - this->filling_at_hatchlevel, EPS_MM3);
        hatchflow_Mm3 = smin_nn(desired_hatch_Mm3, available_hatch_Mm3, EPS_REL_FLOW);

        ptr_downstream_node_hatch->S->up_inflow[t] += MACRO_Mm3_2_m3s(hatchflow_Mm3, S->dt);  // m3/s
        this->res_Mm3 -= hatchflow_Mm3;

        // SMOOTH. Wear of the hatch: smoothed |change of action|. The first step has no previous action and is free.
        cost_hatch_adjust = 0.0;
        if(hatch_adjustment_cost > 0.0 && t > 0) {
            const double d = S->action[t][this->idnr] - S->action[t-1][this->idnr];
            cost_hatch_adjust = hatch_adjustment_cost * (sqrt(d*d + EPS_HATCH_ACTION*EPS_HATCH_ACTION) - EPS_HATCH_ACTION);
        }

        if(this->use_reservoir_geometry) {
            this->res_masl = this->calcResMasl(this->res_Mm3);
        } else if(this->use_reservoir_curve) {
            ValidateReservoirLevelMm3(t, this->res_Mm3);
            this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);
        }
    }



    // AUTO HATCH 
    outlet_auto_qmin_flow_Mm3 = 0.0;    
    // Here we simulate the effect of an automatic water release set by the operators.
    if(outlet_auto_qmin_in_use){
        double void_cost;
        double required_m3s = this->qmin.calcQminRequirement(S->year[t], S->month[t], S->day[t], &void_cost);  // m3/s
        // Changes by Terje Sandø, 02.07.2026. Adding functionality to assure physical auto_qmin release.
        // Volume stored below the outlet cannot be released through this outlet
        double filling_at_outlet_Mm3 = 0.0;
        if(this->use_reservoir_geometry) {
            filling_at_outlet_Mm3 = this->calcResVolume(this->outlet_auto_qmin_masl);
        } else if(this->use_reservoir_curve) {
            filling_at_outlet_Mm3 = ac_res_masl_2_Mm3.x2y(this->outlet_auto_qmin_masl);
        }

        // Only water above the outlet MASL is available
        // SMOOTH
        double available_Mm3 = relu_lo(this->res_Mm3 - filling_at_outlet_Mm3, EPS_MM3);
        double required_Mm3  = MACRO_m3s_2_Mm3(required_m3s, S->dt);
        double actual_Mm3    = smin_nn(required_Mm3, available_Mm3, EPS_REL_FLOW);
        double actual_m3s    = MACRO_Mm3_2_m3s(actual_Mm3, S->dt);

        if(actual_Mm3 < required_Mm3 - 1e-6 && required_m3s > 1e-3) {
            LOG_WARN("OUTLET_AUTO_QMIN: Cannot meet qmin for node " + std::to_string(int(idnr))
                + " (" + nodename + ") at timestep " + std::to_string(t)
                + ". Required: " + std::to_string(required_m3s)
                + " m3/s, Released: " + std::to_string(actual_m3s) + " m3/s.");
        }

        this->ptr_downstream_node_auto_qmin->S->up_inflow[t] += actual_m3s;
        outlet_auto_qmin_flow_Mm3 = actual_Mm3;
        this->res_Mm3 -= outlet_auto_qmin_flow_Mm3;
        ValidateReservoirLevelMm3(t, this->res_Mm3);
    }

    
    // Update the reservoir masl
    if(this->use_reservoir_geometry) {
        this->res_masl = this->calcResMasl(this->res_Mm3);
    } else if(this->use_reservoir_curve) {
        ValidateReservoirLevelMm3(t, this->res_Mm3);
        this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);
    }


    if(this->res_masl < 1.0 + (-1.0 * VERY_LARGE_NUMBER)   ) {
        LOG_WARN("ERROR: Calling reservoir masl calculation (x2y) for node " 
            + nodename + " at timestep " + std::to_string(t));
        LOG_WARN("There is something wrong with the reservoir masl calculation (x2y) for node " 
            + nodename + " at timestep " + std::to_string(t));
    }


    #if HERSS_DEBUG_ALL
        printf("Node idnr = %d   nodename = %s\n ", int(this->idnr) , this->nodename.c_str() );
        printf("masl= %.4f  Mm3 = %.4f\n", this->res_masl,  this->res_Mm3);
        printf("-----------------------\n");
    #endif




    // Overflow is always used
    overflow_Mm3 = this->CalcOverflow();


    if( ptr_downstream_node_overflow == NULL) {
        LOG_WARN("ERROR IN RESERVOIR:  Something is wrong with the pointer:  ptr_downstream_node_overflow \n");
        LOG_WARN("idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename + "   timestep=" + std::to_string(t));
        LOG_ERR("res_masl     = " + std::to_string(this->res_masl));
    }


    ptr_downstream_node_overflow->S->up_inflow[t] += MACRO_Mm3_2_m3s(overflow_Mm3, this->dt);  // m3/s
    this->res_Mm3 -= overflow_Mm3;


    // Update the reservoir masl
    if(this->use_reservoir_geometry) {
        this->res_masl = this->calcResMasl(this->res_Mm3);
    } else if(this->use_reservoir_curve) {
        ValidateReservoirLevelMm3(t, this->res_Mm3);
        this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);
    }
    
    // There is a treshold here. We make the penalty dependent on how much below LRW we are
    // This is so we can see an improvement when we do gradient optimization.
    // SMOOTH
    cost_lrw = (this->res_penalty*this->dt/3600) * relu_hi(this->res_LRW - this->res_masl, EPS_M);

    if(outlet_tunnel_in_use) {
        // The real level is used at both ends of the step. Above HRW the water sits on the spillway,
        // and the turbines see the full head (no cap at HRW).
        ptr_downstream_node_tunnel->end_of_stp_masl = this->res_masl;
    }

    // Terje Sandø, pump-station work, July 2026.
    // Store the end level for connected pumps.
    for(Pump* p : ptr_pumps_as_source) { p->src_end_masl = this->res_masl; }
    for(Pump* p : ptr_pumps_as_target) { p->tgt_end_masl = this->res_masl; }
    for(Powerstation* ps : ptr_stations_as_tailwater) { ps->tw_end_masl = this->res_masl; }

    // Fractional_filling
    double fract_filling = (res_Mm3  - filling_at_lrw_Mm3) / (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);

    // SMOOTH. A soft reserve: the cost grows with the square of the shortfall below the fractional filling f0.
    cost_filling = 0.0;
    if(filling_penalty > 0.0) {
        const double shortfall = relu_hi(filling_f0 - fract_filling, EPS_FR);
        cost_filling = filling_penalty * double(this->dt) / 3600.0 * shortfall * shortfall;
    }

    remaining_Mm3 = res_Mm3;

    // We only value water up to HRW
    // SMOOTH: smooth clamp of (res_Mm3 - filling_at_lrw_Mm3) to [0, active capacity]
    double active_capacity_Mm3 = filling_at_hrw_Mm3 - filling_at_lrw_Mm3;
    double above_lrw_Mm3       = res_Mm3 - filling_at_lrw_Mm3;
    remaining_active_Mm3 = relu_hi(above_lrw_Mm3, EPS_MM3) - relu_hi(above_lrw_Mm3 - active_capacity_Mm3, EPS_MM3);


    if(fract_filling < -100.0) {
        LOG_WARN("There is obviously something wrong with the fract_filling calculations => NON PHYSICAL SITUATIONS ");
        LOG_WARN("res_Mm3  = " + std::to_string(res_Mm3) + "  Mm3");
        LOG_WARN("filling_at_lrw_Mm3 = " + std::to_string(filling_at_lrw_Mm3) + "  Mm3");
        LOG_WARN("filling_at_hrw_Mm3 = " + std::to_string(filling_at_hrw_Mm3) + "  Mm3");
        LOG_WARN("hatchflow_Mm3 = " + std::to_string(hatchflow_Mm3) + "  Mm3");
        LOG_WARN("tunnelflow_Mm3 = " + std::to_string(tunnelflow_Mm3) + "  Mm3");
        LOG_WARN("overflow_Mm3 = " + std::to_string(overflow_Mm3) + "  Mm3");
        LOG_WARN("outlet_auto_qmin_flow_Mm3 = " + std::to_string(outlet_auto_qmin_flow_Mm3) + "  Mm3");
        LOG_WARN("idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename + "   timestep=" + std::to_string(t));
        LOG_ERR("current_filling     = " + std::to_string(res_Mm3));
    }

    this->res_fr = fract_filling;

    // Transfer timeseries 
    // Terje Sandø, pump-station work, August 2026.
    // tot_inflow/tot_outflow hold ALL physical flow across the reservoir boundary,
    // so pumped water is included alongside the ordinary inflows and outlets.
    S->tot_inflow[t]      = MACRO_Mm3_2_m3s(total_inflow_Mm3,this->dt) + S->pump_in_m3s[t];
    S->res_Mm3[t]         = res_Mm3;
    S->res_active_Mm3[t]  = remaining_active_Mm3;
    S->res_masl[t]        = res_masl;
    S->res_fr[t]          = fract_filling;
    S->overflow_Mm3[t]    = overflow_Mm3;
    S->cost[t]            = cost_lrw + cost_filling + cost_hatch_adjust;
    S->adjust_cost[t]     = cost_hatch_adjust;
    S->Power[t]           = 0.0;  // No power production in reservoirs

    double tot_out        = hatchflow_Mm3 + tunnelflow_Mm3 + overflow_Mm3 + outlet_auto_qmin_flow_Mm3;
    S->tot_outflow[t]     = MACRO_Mm3_2_m3s(tot_out, this->dt) + S->pump_out_m3s[t];
    S->tunnelflow_m3s[t]  = MACRO_Mm3_2_m3s(tunnelflow_Mm3, this->dt);
    S->hatchflow_m3s[t]   = MACRO_Mm3_2_m3s(hatchflow_Mm3, this->dt);
    S->overflow_m3s[t]    = MACRO_Mm3_2_m3s(overflow_Mm3, this->dt);
    S->auto_qmin_m3s[t]   = MACRO_Mm3_2_m3s(outlet_auto_qmin_flow_Mm3, this->dt);
    S->income[t]  = 0.0;  // No income in reservoirs 
    S->profit[t]  = S->income[t] - S->cost[t];

    return 0;
}
////////////////////////////////////////////////////////////////
double Reservoir::GetReservoirFraction(size_t t){
    return S->res_fr[t];
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

        if ( keyword.compare("NODE") == 0 && value.compare("RESERVOIR") == 0 ) {
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

                    if (keyword2.compare("ENDNODE") == 0) {
                        inside_node = false;
                        break;
                    }

                    // Here we can extract the variables for the reservoir, we are still inside the correct node. 
                    // We can use keyword2 and value2 to extract the variables.
                    if (keyword2.compare("HRW") == 0) {
                         this->res_HRW = atof(value2.c_str() );
                    }
                    
                    if (keyword2.compare("LRW") == 0) {
                         this->res_LRW = atof(value2.c_str() );
                    }

                    if (keyword2.compare("RES_PENALTY") == 0) {
                         this->res_penalty = atof(value2.c_str() );
                    }
                    
                    if (keyword2.compare("FLOODLEVEL_PENALTY") == 0) {
                         this->floodlevel_penalty = atof(value2.c_str() );
                    }

                    // FILLING_PENALTY f0 P   [P in Euro per hour]
                    if (keyword2.compare("FILLING_PENALTY") == 0) {
                        if(line_obj.calcNrCols(&tmpline2) != 3) {
                            LOG_ERR("FILLING_PENALTY needs two values: f0 and P. Reservoir: " + this->nodename);
                        }
                        this->filling_f0 = atof(value2.c_str() );
                        value2 = line_obj.extractNextElementFromLine(&line);
                        this->filling_penalty = atof(value2.c_str() );
                        if(this->filling_f0 <= 0.0 || this->filling_f0 > 1.0) {
                            LOG_ERR("FILLING_PENALTY f0 must be in (0, 1]. Reservoir: " + this->nodename);
                        }
                        if(this->filling_penalty < 0.0 || this->filling_penalty > VERY_LARGE_NUMBER) {
                            LOG_ERR("FILLING_PENALTY P must be in [0, VERY_LARGE_NUMBER]. Reservoir: " + this->nodename);
                        }
                    }

                    // HATCH_ADJUSTMENT_COST fee   [Euro per full hatch range of travel]
                    if (keyword2.compare("HATCH_ADJUSTMENT_COST") == 0) {
                        this->hatch_adjustment_cost = atof(value2.c_str() );
                        if(this->hatch_adjustment_cost < 0.0 || this->hatch_adjustment_cost > VERY_LARGE_NUMBER) {
                            LOG_ERR("HATCH_ADJUSTMENT_COST must be in [0, VERY_LARGE_NUMBER]. Reservoir: " + this->nodename);
                        }
                    }

                    if (keyword2.compare("LOCAL_ENERGY_EQUIVALENT") == 0) {
                         this->local_energy_equivalent = atof(value2.c_str() );
                    }


                    if (keyword2.compare("SPILLWAY") == 0) {
                        // SPILLWAY  downstream_idnr  C  L  spillway_level_masl
                        // SPILLWAY 2 2.1 10.0 757.0
                        int tmpidnr = atoi(value2.c_str() );
                        if(tmpidnr < 0) {
                            LOG_INFO("ERROR: SPILLWAY downstream node idnr must be non-negative. Check topology file " + filename);
                            LOG_INFO("Reservoir::ReadNodeData  nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype) + "\n");
                            LOG_ERR("ERROR: SPILLWAY downstream node idnr must be non-negative. Check topology file " + filename);
                        }

                        downstream_idnr_overflow = size_t(tmpidnr);

                        if(downstream_idnr_overflow != this->idnr) {
                            downstream_node_in_use = true;
                            outlet_overflow_in_use = true;
                            this->use_spillway = true;          

                        } else {
                            LOG_INFO("ERROR: SPILLWAY downstream node idnr must be non-negative and cannot point to itself. Check topology file " + filename);
                            LOG_INFO("Reservoir::ReadNodeData  nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype) + "\n");
                            LOG_ERR("ERROR: SPILLWAY downstream node idnr must be non-negative and cannot point to itself. Check topology file " + filename);
                        }

                        value2      = line_obj.extractNextElementFromLine(&line);
                        spillway_C = atof(value2.c_str() );
                        if(spillway_C < 0.0 || spillway_C > 100.0 ) {
                            LOG_WARN("ERROR: SPILLWAY C is out of bounds: " + std::to_string(spillway_C));
                            LOG_ERR("Reservoir: " + this->nodename);
                        }

                        value2      = line_obj.extractNextElementFromLine(&line);
                        spillway_L = atof(value2.c_str() );
                        if(spillway_L < 0.0 || spillway_L > VERY_LARGE_NUMBER) {
                            LOG_WARN("ERROR: SPILLWAY L is out of bounds: " + std::to_string(spillway_L));
                            LOG_ERR("Reservoir: " + this->nodename);
                        }

                        value2      = line_obj.extractNextElementFromLine(&line);
                        spillway_level_masl = atof(value2.c_str() );
                        if(spillway_level_masl < 0.0 || spillway_level_masl > MOUNT_EVEREST_MASL) {
                            LOG_WARN("ERROR: SPILLWAY level masl is out of bounds: " + std::to_string(spillway_level_masl));
                            LOG_ERR("Reservoir: " + this->nodename);
                        }
                    }

                    // FAST_OVERFLOW FALSE
                    if (keyword2.compare("FAST_OVERFLOW") == 0) {
                        if (value2 == "TRUE") {
                            this->fast_overflow = 1;
                        } else if (value2 == "FALSE") {
                            this->fast_overflow = 0;
                        }  else {
                            LOG_ERR("FAST_OVERFLOW must be TRUE or FALSE in topologyfile " + filename + " ERROR");
                        }                  
                    }

                    // OUTLET_HATCH -9999
                    if (keyword2.compare("OUTLET_HATCH") == 0) {

                        // BVM, May 2026
                        if(atoi(value2.c_str() ) >= 0) {
                            
                            downstream_idnr_hatch = atoi(value2.c_str());

                            // We do not allow to point to yourself. This can cause numerical instability and is not physical.
                            if(size_t(downstream_idnr_hatch) == this->idnr) {
                                LOG_INFO("ERROR: OUTLET_HATCH cannot point to itself.");
                                LOG_INFO("This can cause numerical instability and is not physical. Check topology file " + filename);
                                LOG_ERR("ERROR: OUTLET_HATCH cannot point to itself. Reservoir::ReadNodeData  nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype) + "\n");
                            }

                            outlet_hatch_in_use            = true;
                            downstream_node_in_use         = true;

                            // There should be five columns in this line

                            // OUTLET_HATCH downstream_nodeid, qmin_hatch, qmax_hatch, hatch_masl
                            int n_cols = line_obj.calcNrCols(&tmpline2);
                            if(n_cols != 5) {
                                LOG_INFO("ERROR: OUTLET_HATCH line should have 5 columns, but it has " + std::to_string(n_cols) + " columns. Check topology file " + filename);
                                LOG_INFO("Reservoir::ReadNodeData  nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype) + "\n");
                                LOG_ERR("ERROR: OUTLET_HATCH line should have 5 columns, but it has " + std::to_string(n_cols) + " columns. Check topology file " + filename);
                            }

                            value2      = line_obj.extractNextElementFromLine(&line);
                            minQ_hatch = atof(value2.c_str());
                            value2      = line_obj.extractNextElementFromLine(&line);
                            maxQ_hatch = atof(value2.c_str());
                            value2      = line_obj.extractNextElementFromLine(&line);
                            hatch_masl = atof(value2.c_str());

                            if(hatch_masl < 0.0 || hatch_masl > MOUNT_EVEREST_MASL) {
                                LOG_WARN("ERROR: Hatch masl is out of bounds: " + std::to_string(hatch_masl));
                                LOG_ERR("Reservoir: " + this->nodename);
                            }

                            if(minQ_hatch < 0.0 || minQ_hatch > VERY_LARGE_NUMBER) {
                                LOG_WARN("ERROR: minQ_hatch is out of bounds: " + std::to_string(minQ_hatch));
                                LOG_ERR("Reservoir: " + this->nodename);
                            }

                            if(maxQ_hatch < 0.0 || maxQ_hatch > VERY_LARGE_NUMBER) {
                                LOG_WARN("ERROR: maxQ_hatch is out of bounds: " + std::to_string(maxQ_hatch));
                                LOG_ERR("Reservoir: " + this->nodename);
                            }

                        }
                    }


                    // OUTLET_TUNNEL 1
                    if (keyword2.compare("OUTLET_TUNNEL") == 0) {
                        int tmp_idnr = atoi(value2.c_str());   // <-- parse as signed int

                        if (tmp_idnr >= 0) {
                            downstream_idnr_tunnel = (size_t)tmp_idnr;

                        if (downstream_idnr_tunnel == this->idnr) {
                            // self-loop check ...
                            LOG_ERR("ERROR: OUTLET_TUNNEL cannot point to itself. ...");
                        }

                        outlet_tunnel_in_use   = true;
                        downstream_node_in_use = true;
                        }
                        // if tmp_idnr < 0 (-9999), tunnel is simply not used — leave flags false
                    }



                    // // OUTLET_TUNNEL 1
                    // if ( keyword2.compare("OUTLET_TUNNEL") == 0) {
                    //     downstream_idnr_tunnel = atoi(value2.c_str()  );
                    //         if(size_t(downstream_idnr_tunnel) == this->idnr) {
                    //             LOG_INFO("ERROR: OUTLET_TUNNEL cannot point to itself.");
                    //             LOG_INFO("This can cause numerical instability and is not physical. Check topology file " + filename);
                    //             LOG_ERR("ERROR: OUTLET_TUNNEL cannot point to itself. Reservoir::ReadNodeData  nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype) + "\n");
                    //         }
                    //     if(downstream_idnr_tunnel >=0) {
                    //         outlet_tunnel_in_use           = true;
                    //         downstream_node_in_use         = true;
                    //     }
                    // }
                    




                    // Change by Terje Sandø, 02.07.2026
                    // Parse OUTLET_AUTO_QMIN with outlet MASL and seasonal flow periods.
                    // Activates OUTLET_AUTO_QMIN.
                    // Format: OUTLET_AUTO_QMIN <nr_periods> <downstream_idnr> <outlet_masl>
                    // Each following line: DD.MM DD.MM discharge_m3s
                    if (keyword2.compare("OUTLET_AUTO_QMIN") == 0) {
                        outlet_auto_qmin_in_use = false;

                        if(atoi(value2.c_str()) >= 0) {
                            outlet_auto_qmin_in_use = true;
                            downstream_node_in_use  = true;
                            this->qmin.nr_periods = atoi(value2.c_str());

                            // 3rd token: downstream node idnr
                            value = line_obj.extractNextElementFromLine(&line);
                            this->downstream_idnr_auto_qmin = atoi(value.c_str());

                            if(size_t(downstream_idnr_auto_qmin) == this->idnr) {
                                LOG_INFO("ERROR: OUTLET_AUTO_QMIN cannot point to itself.");
                                LOG_INFO("This can cause numerical instability and is not physical. Check topology file " + filename);
                                LOG_ERR("ERROR: OUTLET_AUTO_QMIN cannot point to itself. Reservoir::ReadNodeData  nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype) + "\n");
                            }

                            // 4th token: outlet MASL
                            value = line_obj.extractNextElementFromLine(&line);
                            this->outlet_auto_qmin_masl = atof(value.c_str());

                            if(outlet_auto_qmin_masl < 0.0 || outlet_auto_qmin_masl > MOUNT_EVEREST_MASL) {
                                LOG_WARN("OUTLET_AUTO_QMIN_MASL is out of bounds: " + std::to_string(outlet_auto_qmin_masl));
                                LOG_ERR("Check OUTLET_AUTO_QMIN in topology file " + filename + " for reservoir " + nodename);
                            }

                            // Now we read in the qmin periods (MAXIMUM 5)
                            if(this->qmin.nr_periods < 1 || this->qmin.nr_periods > MAX_NUMBER_OF_QMIN_PERIODS) {
                                LOG_WARN("OUTLET_AUTO_QMIN nr_periods out of bounds: " + std::to_string(this->qmin.nr_periods));
                                LOG_ERR("Check OUTLET_AUTO_QMIN in topology file " + filename + " for reservoir " + nodename);
                            }

                            // Terje Sandø, 03.07.2026
                            // Read seasonal qmin period lines (DD.MM DD.MM discharge_m3s).
                            // Parsing here only guards against a crash in substr() when a token
                            // is too short. All semantic date validation (month/day ranges, year-crossing, gaps, overlaps) is done in Qmin::validatePeriods(),
                            // and is called by ValidateReservoirSettings(). 
                            //Keeping the checks in one place avoids duplicated/inconsistent error messages.
                            for(int q = 0; q < this->qmin.nr_periods; q++) {
                                line = gc->topoparser.getLine(k+q+1);

                                // Start date token: substr(3,2) below requires length >= 3, or it throws.
                                value = line_obj.extractNextElementFromLine(&line);
                                if(value.length() < 3) {
                                    LOG_WARN("Qmin period " + std::to_string(q+1) + " start date token '"
                                        + value + "' is too short to parse. Expected DD.MM format (e.g. 01.04).");
                                    LOG_ERR("Check OUTLET_AUTO_QMIN periods in topology file "
                                        + filename + " for reservoir " + nodename + ".");
                                }
                                qmin.timeperiods[q].start_day   = atoi(value.substr(0,2).c_str());
                                qmin.timeperiods[q].start_month = atoi(value.substr(3,2).c_str());

                                // End date token: same crash guard as above.
                                value = line_obj.extractNextElementFromLine(&line);
                                if(value.length() < 3) {
                                    LOG_WARN("Qmin period " + std::to_string(q+1) + " end date token '"
                                        + value + "' is too short to parse. Expected DD.MM format (e.g. 31.03).");
                                    LOG_ERR("Check OUTLET_AUTO_QMIN periods in topology file "
                                        + filename + " for reservoir " + nodename + ".");
                                }
                                qmin.timeperiods[q].end_day   = atoi(value.substr(0,2).c_str());
                                qmin.timeperiods[q].end_month = atoi(value.substr(3,2).c_str());

                                value = line_obj.extractNextElementFromLine(&line);
                                qmin.timeperiods[q].min_discharge = atof(value.c_str());
                                qmin.timeperiods[q].penalty_cost  = 0.0;
                            }
                        }
                    }

                    if ( keyword2.compare("WIDTH_M") == 0 ) {
                        this->width_m = atof(value2.c_str() );
                    }

                    if ( keyword2.compare("LENGTH_M") == 0 ) {
                        this->length_m = atof(value2.c_str() );
                    }

                    if ( keyword2.compare("THETA") == 0 ) {
                        this->theta = atof(value2.c_str() );
                    }

                    if ( keyword2.compare("BOTTOM_MASL") == 0 ) {
                        this->bottom_masl = atof(value2.c_str() );
                        this->use_reservoir_geometry = true;
                    }

                    if ( keyword2.compare("RESERVOIR_CURVE") == 0 ) {
                        this->use_reservoir_curve = true;
                        this->nr_points_res_curve = atoi(value2.c_str() );
                        if( nr_points_res_curve > MAX_NR_POINTS_CURVE) {
                            LOG_ERR("ERROR: nr_points_res_curve > MAX_NR_POINTS_CURVE ");
                        }
                        for(size_t p = 0; p < nr_points_res_curve; p++) {
                            line = gc->topoparser.getLine(k+p+1);
                            keyword = line_obj.extractNextElementFromLine(&line);
                            value   = line_obj.extractNextElementFromLine(&line);
                            res_curve_masl[p] = atof(keyword.c_str());
                            res_curve_Mm3[p]  = atof(value.c_str());
                        }
                    }

                    // # Overflow curve, points, downstream idnr   [masl, m3s]
                    if (keyword2.compare("OVERFLOW_CURVE") == 0 ) {
                        token   = line_obj.extractNextElementFromLine(&line);
                        nr_points_ovefl_curve = atoi(value2.c_str());
                        if( nr_points_ovefl_curve > MAX_NR_POINTS_CURVE) {
                            LOG_ERR("nr_points_ovefl_curve > MAX_NR_POINTS_CURVE ");
                        }
                        
                        downstream_idnr_overflow = atoi(token.c_str());

                        if(downstream_idnr_overflow < 0) {
                            LOG_ERR("ERROR: downstream node idnr for OUTLET_OVERFLOW must be non-negative in topologyfile " + filename);
                        }
                        
                        this->use_overflow_curve = true;

                        if(size_t(downstream_idnr_overflow) == this->idnr) {
                            LOG_INFO("ERROR: OUTLET_OVERFLOW cannot point to itself.");
                            LOG_INFO("This can cause numerical instability and is not physical. Check topology file " + filename);
                            LOG_ERR("ERROR: OUTLET_OVERFLOW cannot point to itself. Reservoir::ReadNodeData  nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype) + "\n");
                        }
                        this->outlet_overflow_in_use = true;
                        for(size_t p = 0; p < nr_points_ovefl_curve; p++) {
                            line = gc->topoparser.getLine(k+p+1);
                            keyword = line_obj.extractNextElementFromLine(&line);
                            value   = line_obj.extractNextElementFromLine(&line);
                            ovefl_curve_masl[p] = atof(keyword.c_str());
                            ovefl_curve_m3s[p]  = atof(value.c_str());
                        }
                    }
                    
                    k++; // Move to the next line for the next iteration
                }
            }
        }
    }


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


    if(!downstream_node_in_use){

        cout << "Downstream node not set for reservoir: " << nodename << endl;
        cout << "Reservoir name: " << nodename << ", ID: " << idnr << endl;

        cout << "Please check the topology file for the correct downstream node." << endl;
        cout << "Exiting due to missing downstream node." << endl;

        LOG_ERR("ERROR: Downstream node has to be set for reservoir. Please add a channel node downstream of the reservoirs");

    }




    if(this->use_reservoir_geometry){
         // Check if we have the necessary variables to calculate the reservoir geometry.
         if(width_m <= 0.0 || length_m <= 0.0 || theta <= 0.0 || bottom_masl < 0.0) {
             LOG_ERR("ERROR: To use reservoir geometry, you need to set WIDTH_M, LENGTH_M, THETA and BOTTOM_MASL in the topology file for reservoir " + nodename);
         } 
    }



    if(this->use_reservoir_geometry && this->use_reservoir_curve) {
        LOG_ERR("ERROR: You cannot use both reservoir geometry and reservoir curve in the same reservoir " 
            + nodename + ". Please choose one of them in the topology file.");
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
		LOG_ERR("The statefile " + filename + " could not be found/opened");
    }

    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {
            // Line is not empty and doesn't start with # (hash/pound sign)
            string str_tmpline = line;  // Create a copy of the line for parsing
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);
            if ( keyword.compare("NODE") == 0 && value.compare("RESERVOIR") == 0 ) {
                size_t n_cols = line_obj.calcNrCols(&str_tmpline);

                // BVM May 2026. 
                if(n_cols != 5) {
                    LOG_WARN("Invalid line in statefile " + filename + ": " + str_tmpline);
                    LOG_ERR("Expected 5 columns for NODE RESERVOIR, but got " + std::to_string(n_cols));
                }

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
        cout << "Reservoir name: " << nodename << ", ID: " << idnr << " not found in statefile: " << filename << endl;
        cout << "Please check the statefile for the correct reservoir information." << endl;
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);

        LOG_WARN("ReadStateFile     idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename);
		LOG_ERR("There is something wrong with nodes in the statefile " + filename);
    }

    myfile.close();
    return 0;
}
//------------------------------------------------------------------------
double Reservoir::GetStartWater_Mm3(void) {

    double start_res_Mm3 = filling_at_lrw_Mm3 + reservoir_init_fr * (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);
    return start_res_Mm3;
}
//------------------------------------------------------------------------
double Reservoir::GetEndWater_Mm3(void) {
    // Ending water volume
    return this->res_Mm3;  // This is the water in the reservoir at the end of the last timestep
}
//------------------------------------------------------------------------
int Reservoir::WriteNodeOutput(GlobalConfig *gc){

    FILE *fp;
    char outfilename [100];
    sprintf (outfilename, "%snode%lu_%s.txt", gc->outputdir.c_str() , (idnr) , nodename.c_str()  );
    char outstr [100];

    if((fp = fopen(  outfilename ,"w"))==NULL) {
        LOG_ERR("Cannot open file " + std::string(outfilename));
    }

    sprintf (outstr, "RESERVOIR node %d %s\n", int(idnr), nodename.c_str() );
    fprintf(fp, "%s", outstr);
    fprintf(fp, "reservoir_init_fr= %.5f  init_masl=%.3f\n", this->reservoir_init_fr, this->reservoir_init_masl);
    fprintf(fp, "Filling at HRW [Mm3] = %.5f\n", this->filling_at_hrw_Mm3);
    fprintf(fp, "Filling at LRW [Mm3] = %.5f\n", this->filling_at_lrw_Mm3);
    fprintf(fp, "Active reservoir capacity [Mm3] = %.5f\n", this->filling_at_hrw_Mm3 - this->filling_at_lrw_Mm3);
    // Terje Sandø, 02.07.2026
    // Write outlet MASL to output file header for traceability.
    // Makes it easy to verify which outlet level was active when reviewing results.
    if(outlet_auto_qmin_in_use) {
        fprintf(fp, "outlet_auto_qmin_masl [masl] = %.3f\n", this->outlet_auto_qmin_masl);
    }
    fprintf(fp, "yyyy mm dd hh [m3/s] [Euro/MWh] [fr] [m3/s] [Mm3] [masl] [fr] [Euro]         [m3/s]     [m3/s]    [m3/s]   [m3/s]    [m3/s]   [m3/s]    [m3/s]     [m3/s] \n");
    fprintf(fp, "yyyy mm dd hh Inflow Price Action Up_Inflow Res_Mm3 Res_masl Res_fr lrw_cost tunnelflow hatchflow overflow auto_qmin pump_in pump_out tot_inflow tot_outflow\n");

    for(size_t t = 0; t < this->stps; t++) {
        fprintf(fp, "%d %d %d %d ", S->year[t], S->month[t], S->day[t], S->hour[t]);
        fprintf(fp, "%.4f %.4f %.4f ", S->inflow[t] , S->price[t], S->action[t][this->idnr] ); // CHANGE BY OVE: Added outoput for action pr generator
        fprintf(fp, "%.4f ", S->up_inflow[t]);
        fprintf(fp, "%.4f %.4f %.4f ", S->res_Mm3[t] , S->res_masl[t], S->res_fr[t] );
        fprintf(fp, "%.4f ", S->cost[t]);
        fprintf(fp, "%.4f %.4f %.4f %.4f ",  S->tunnelflow_m3s[t], S->hatchflow_m3s[t], S->overflow_m3s[t] , S->auto_qmin_m3s[t]);
        fprintf(fp, "%.4f %.4f ", S->pump_in_m3s[t], S->pump_out_m3s[t]);
        fprintf(fp, "%.4f %.4f ", S->tot_inflow[t], S->tot_outflow[t]);
        fprintf(fp, "\n");
    }
    fclose(fp);
    return 0;
}
/////////////////////////////////////////////////////////////////////////
double Reservoir::GetTunnelFLow(size_t t) {
    LOG_WARN("ERROR reservoir cannot use this function");
    LOG_ERR("NODE RESERVOIR " + std::to_string(int(idnr)) + " " + nodename);
    return -99.0;
}
/////////////////////////////////////////////////////////////////////////
int Reservoir::WriteStateFile(FILE *fp) {
    // # NODE RESERVOIR IDNR NAME INIT_RES_FR
    fprintf (fp, "NODE RESERVOIR %d %s %.5f\n", int(idnr), nodename.c_str() , this->S->res_fr[S->stps-1] );
    return 0; 
}
/////////////////////////////////////////////////////////////////////////
// CHANGE BY OVE: CheckWaterBalance method that uses variable timesteps
int Reservoir::CheckWaterBalance(class Herss *herss_obj) { 

    double start_res_Mm3 = filling_at_lrw_Mm3 + reservoir_init_fr * (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);

    double sum_inflow  = 0.0;
    double sum_outflow = 0.0;

    for(size_t t = 0; t < this->stps; t++) {
        // Use variable timestep for each timestep
        int variable_dt = herss_obj->getDeltaT(t);
        // tot_inflow/tot_outflow already contain every flow crossing the reservoir
        // boundary, pumped water included.
        sum_inflow += MACRO_m3s_2_Mm3(this->S->tot_inflow[t], variable_dt);
        sum_outflow += MACRO_m3s_2_Mm3(this->S->tot_outflow[t], variable_dt);
    }

    // Ending water volume
    double end_res_Mm3 = this->res_Mm3;
    double waterbalance = start_res_Mm3 + sum_inflow - end_res_Mm3 - sum_outflow;

    if(WATERBALANCE_WARNINGS) {
        printf( "WATERBALANCE RESERVOIR (Variable Timesteps) idnr=%d  nodename=%s\n", int(idnr), nodename.c_str()  );
        printf("start_res_Mm3     = %.6f\n", start_res_Mm3);
        printf("sum_inflow_Mm3    = %.6f\n", sum_inflow);
        printf("sum_outflow_Mm3   = %.6f\n", sum_outflow);
        printf("end_res_Mm3       = %.6f\n", end_res_Mm3);
        printf("waterbalance      = %.6f\n", waterbalance);
        printf( "---------------------------\n" );
    }


    if(abs(waterbalance) > 0.0001) {
        LOG_WARN("------ERROR ERROR ERROR (Variable Timesteps) --------------");
        LOG_WARN("WATERBALANCE RESERVOIR idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename);
        LOG_WARN("start_res_Mm3 = " + std::to_string(start_res_Mm3));
        LOG_WARN("sum_inflow    = " + std::to_string(sum_inflow));
        LOG_WARN("sum_outflow   = " + std::to_string(sum_outflow));
        LOG_WARN("end_res_Mm3   = " + std::to_string(end_res_Mm3));
        LOG_WARN("waterbalance  = " + std::to_string(waterbalance));
        LOG_WARN("idnr=" + std::to_string(int(idnr)) + "   nodename=" + nodename);
        LOG_ERR("---------------------------");
    }
    return 0; 
}
/////////////////////////////////////////////////////////////////////////