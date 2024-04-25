/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     scenario.cpp                                                        
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

Scenario::Scenario(){}

Scenario::Scenario(size_t stps, size_t dt, size_t idnr){

    this->stps = stps;
    this->dt   = dt;
    this->idnr = idnr;
    restprice               = NOT_INIT;
    broken_lrw  = false;
    broken_qmin = false;
    days_with_production    = NOT_INIT;
    remaining_Mm3           = NOT_INIT;
    local_remaining_Mm3     = NOT_INIT;
    remaining_Euro          = NOT_INIT;
    remaining_MWh           = NOT_INIT;
    remaining_upstream_Mm3  = NOT_INIT;
    sum_prod_MWh            = NOT_INIT;
    sum_income_Euro         = NOT_INIT;
    sum_cost_Euro           = NOT_INIT;
    sum_profit_Euro         = NOT_INIT;
    sum_incoming_water_Mm3  = NOT_INIT;
    sum_local_inflow_Mm3    = NOT_INIT;
    sum_total_energy_MWh    = NOT_INIT;
    sum_overflow_Mm3        = NOT_INIT;

    try {
        price           = new double[stps];
        action          = new double[stps];
        q_action        = new double[stps];
        inflow          = new double[stps];
        tot_outflow     = new double[stps];
        tot_inflow      = new double[stps];
        local_inflow    = new double[stps];
        up_inflow       = new double[stps];
        res_Mm3         = new double[stps];
        res_masl        = new double[stps];
        res_fr          = new double[stps];
        profit          = new double[stps];
        overflow_Mm3    = new double[stps];
        income          = new double[stps];
        cost            = new double[stps];
        cost_qmin       = new double[stps];
        startStopCost   = new double[stps];
        cost_lrw        = new double[stps];
        cost_fake_lrw   = new double[stps];
        Hbrutto         = new double[stps];
        Hnetto          = new double[stps];
        Power           = new double[stps];

        tunnelflow_m3s  = new double[stps];
        hatchflow_m3s   = new double[stps];
        overflow_m3s    = new double[stps];
        auto_qmin_m3s   = new double[stps];
        channel_storage_Mm3 = new double[stps];
        adjust_cost         = new double[stps];

        year            = new int[stps];
        month           = new int[stps];
        day             = new int[stps];
        hour            = new int[stps];
        qmin_flag       = new int[stps];
    }
    catch(std::bad_alloc& exc) {
        printf("Error: memory allocation failed. \n"); 
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		exit(EXIT_FAILURE);
    }

    for(size_t t = 0; t < this->stps; t++) {
        price[t]           = NOT_INIT;
        action[t]          = NOT_INIT;
        q_action[t]        = NOT_INIT;
        inflow[t]          = 0.0;     // To make things easy and faster 
        tot_outflow[t]     = NOT_INIT;
        tot_inflow[t]      = NOT_INIT;
        local_inflow[t]    = NOT_INIT;
        up_inflow[t]       = 0.0;        // To make things easy and faster 
        res_Mm3[t]         = NOT_INIT;
        res_masl[t]        = NOT_INIT;
        res_fr[t]          = NOT_INIT;
        profit[t]          = NOT_INIT;
        overflow_Mm3[t]    = NOT_INIT;
        income[t]          = NOT_INIT;
        cost[t]            = NOT_INIT;
        cost_qmin[t]       = NOT_INIT;
        startStopCost[t]   = NOT_INIT;
        cost_lrw[t]        = NOT_INIT;
        cost_fake_lrw[t]   = NOT_INIT;
        Hbrutto[t]         = NOT_INIT;
        Hnetto[t]          = NOT_INIT;
        Power[t]           = NOT_INIT;
        year[t]            = NOT_INIT;
        month[t]           = NOT_INIT;
        day[t]             = NOT_INIT;
        hour[t]            = NOT_INIT;
        qmin_flag[t]       = NOT_INIT;
        adjust_cost[t]     = 0.0;

        tunnelflow_m3s[t]  = NOT_INIT;
        hatchflow_m3s[t]   = NOT_INIT;
        overflow_m3s[t]    = NOT_INIT;
        auto_qmin_m3s[t]   = NOT_INIT;
        channel_storage_Mm3[t] = NOT_INIT;
    }
}
///////////////////////////////////////////////////////////////////////////////
Scenario::~Scenario(){

    delete [] price;
    delete [] action;
    delete [] q_action;
    delete [] inflow;
    delete [] tot_outflow;
    delete [] tot_inflow;
    delete [] local_inflow;
    delete [] up_inflow;
    delete [] res_Mm3;
    delete [] res_masl;
    delete [] res_fr;
    delete [] profit;
    delete [] overflow_Mm3;
    delete [] income;
    delete [] cost;
    delete [] cost_qmin;
    delete [] startStopCost;
    delete [] cost_lrw;
    delete [] cost_fake_lrw;
    delete [] Hbrutto;
    delete [] Hnetto;
    delete [] Power;
    delete [] year;
    delete [] month;
    delete [] day;
    delete [] hour;
    delete [] qmin_flag;
    delete [] tunnelflow_m3s;
    delete [] hatchflow_m3s;
    delete [] overflow_m3s;
    delete [] auto_qmin_m3s;
    delete [] channel_storage_Mm3;
    delete [] adjust_cost;

}
///////////////////////////////////////////////////////////////////////////////
