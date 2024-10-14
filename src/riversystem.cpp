/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     riversystem.cpp                                                        
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

Riversystem::Riversystem(){}

Riversystem::Riversystem(GlobalConfig *gc) {

    this->gc = gc;
    this->nr_nodes      = gc->nr_nodes;
    this->nr_reservoirs = gc->nr_reservoirs;
    this->nr_pstations  = gc->nr_pstations;
    this->nr_channels   = gc->nr_channels;
    nodes = new Node*[nr_nodes];
    if(nr_reservoirs > 0) {
        reservoirs = new Reservoir[nr_reservoirs];
    }
    if(nr_pstations > 0) {
        pstations = new Powerstation[nr_pstations];
    }
    if(nr_channels > 0) {
        channels = new Channel[nr_channels];
    }

    size_t reservoirs_used = 0;
    size_t pstations_used = 0;
    size_t channels_used = 0;

    for(size_t n = 0; n < nr_nodes; n++) {
        switch (gc->nodetypes[n])  {
            case RESERVOIR:
                nodes[n] = &reservoirs[reservoirs_used];
                nodes[n]->idnr = n;
                reservoirs_used++;
                break;

            case POWERSTATION:
                nodes[n] = &pstations[pstations_used];
                nodes[n]->pstation_idnr = pstations_used;
                nodes[n]->idnr = n;
                pstations_used++;
                break;

            case CHANNEL:
                nodes[n] = &channels[channels_used];
                nodes[n]->idnr = n;
                channels_used++;
                break;
        }
    }
}
///////////////////////////////////////////////////////////////////
Riversystem::~Riversystem(){
    if(nr_nodes > 0) {
        delete [] nodes;
    }
    if(nr_reservoirs > 0) {
        delete [] reservoirs;
    }
    if(nr_pstations > 0) {
        delete [] pstations;
    }
    if(nr_channels > 0) {
        delete [] channels;
    }
}
///////////////////////////////////////////////////////////////////
int Riversystem::WriteSelectedOutputMatrix() {
    printf("ERROR:    WORK IN PROGRESS\n");
    printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
    exit(EXIT_FAILURE);
   return 0;
}
///////////////////////////////////////////////////////////////////
// Return the ending reservoir level for the given reservoir idnr. 
double Riversystem::GetEndingReservoirLevel(size_t r_idnr) {
    
    if( r_idnr > (gc->nr_reservoirs-1) ) {
        printf("ERROR:  You are asking for data that doesnt exist\n");
        printf("HERSS:  file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        exit(EXIT_FAILURE);
    }
    
    return this->reservoirs[r_idnr].S->res_fr[ gc->stps -1];
}
///////////////////////////////////////////////////////////////////
void Riversystem::PrintReservoirData2Screen() {
    printf("-----   reservoir fractions  -----  \n");
    for(size_t t = 0; t < gc->stps; t++) {
        for(size_t r = 0; r < gc->nr_reservoirs; r++) {
            printf("%.4f ", this->reservoirs[r].S->res_fr[t]);
        }
        printf("\n");
    }
    printf("-----   Actions  for powerstations  -----  \n");
    for(size_t t = 0; t < gc->stps; t++) {
        for(size_t p = 0; p < gc->nr_pstations; p++) {
            printf("%.4f ", this->pstations[p].S->action[t]);
        }
        printf("\n");
    }
}
///////////////////////////////////////////////////////////////////
void Riversystem::WriteReservoirData() {

    FILE *fp;
    char outfilename [100];
    sprintf (outfilename, "%sreservoirs_%s_out.txt",  gc->outputdir.c_str(),  gc->systemname.c_str() );

    if((fp = fopen(  outfilename ,"w"))==NULL) {
        printf("Cannot open file %s \n", outfilename);
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        exit(EXIT_FAILURE);
    }

    fprintf(fp, "Riversystem %s reservoir fractions [fr] \n", gc->systemname.c_str() );

    fprintf(fp,"YYYY MM DD HH ");
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::RESERVOIR) {
            fprintf(fp, "%s ", nodes[n]->nodename.c_str()  );
        }
    }
    fprintf(fp,"\n");

    for(size_t t = 0; t < gc->stps; t++) {
        fprintf(fp, "%d %d %d %d ", nodes[0]->S->year[t], nodes[0]->S->month[t], nodes[0]->S->day[t], nodes[0]->S->hour[t]);
        for(size_t n = 0; n < gc->nr_nodes; n++) {
            if(nodes[n]->nodetype == NodeType::RESERVOIR) {
                fprintf(fp, "%.4f ", nodes[n]->S->res_fr[t] );
            }
        }
        fprintf(fp, "\n");
    }

    fclose(fp);

}
///////////////////////////////////////////////////////////////////
// This function returns income and penalties in the simulation period. 
// Remaining water in the Riversystem is not included.
double Riversystem::CalcSimulationProfit() {
    double sim_profit = 0.0;
    for(size_t n = 0; n < nr_nodes; n++) {
        for(size_t t = 0; t < gc->stps; t++) {
            sim_profit += (nodes[n]->S->income[t] - nodes[n]->S->cost[t]);
        }
    }
    return sim_profit;
}
//////////////////////////////////////////////////////////////////////
double Riversystem::CalcVF(double restprice) {

    tot_remaining_available_Mm3  = 0.0;
    tot_remaining_available_MWh  = 0.0;
    tot_remaining_available_Euro = 0.0;
    tot_income_Euro              = 0.0;
    tot_cost_Euro                = 0.0;
    tot_profit_Euro              = 0.0;
    valuefunction_Euro           = 0.0;
    sum_production               = 0.0;

    // At the most downstream node (OCEAN) the total available water 
    // is the node available water + upstream available (not included DEAD WATER)
    tot_remaining_available_Mm3  = nodes[nr_nodes-1]->upstream_remaining_available_Mm3;
    tot_remaining_available_Mm3 += nodes[nr_nodes-1]->remaining_available_Mm3;
    //printf("tot_remaining_available_Mm3 at outlet= %.4f\n", tot_remaining_available_Mm3);

    // We now loop over the Powerstations and calculate the remaining energy and value. 
    for(size_t n = 0; n < nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::POWERSTATION) { 
            // Powerstations has zero storage so only upstream water is needed. 
            tot_remaining_available_MWh += (nodes[n]->local_energy_equivalent * nodes[n]->upstream_remaining_available_Mm3 * 1000000.0 / 1000.0); // MWh
            for(size_t t = 0; t < gc->stps; t++) {
                sum_production += nodes[n]->S->Power[t];
            }
        }
    }

    for(size_t n = 0; n < nr_nodes; n++) {
        for(size_t t = 0; t < gc->stps; t++) {
            tot_income_Euro += nodes[n]->S->income[t];
            tot_cost_Euro  += nodes[n]->S->cost[t];
        }
    }

    tot_remaining_available_Euro = tot_remaining_available_MWh*restprice;
    tot_profit_Euro = tot_income_Euro - tot_cost_Euro;
    valuefunction_Euro = tot_profit_Euro + tot_remaining_available_Euro;

    sum_qmin_cost = 0.0;
    for(size_t n = 0; n < nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::CHANNEL) { 
            for(size_t t = 0; t < gc->stps; t++) {
                sum_qmin_cost += nodes[n]->S->cost[t];
            }
        }
    }

    sum_lrw_cost = 0.0;
    // We now loop over the RESERVOIRS 
    for(size_t n = 0; n < nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::RESERVOIR) { 
            for(size_t t = 0; t < gc->stps; t++) {
                sum_lrw_cost += nodes[n]->S->cost[t];
            }
        }
    }

    sum_startstopcost = 0.0;
    sum_max_adjustment_cost = 0.0;
    for(size_t n = 0; n < nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::POWERSTATION) { 
            for(size_t t = 0; t < gc->stps; t++) {
                sum_startstopcost += nodes[n]->S->cost[t] - nodes[n]->S->adjust_cost[t];
                sum_max_adjustment_cost += nodes[n]->S->adjust_cost[t];
            }
        }
    }

    avg_price = 0.0;
    for(size_t t = 0; t < gc->stps; t++) {
        avg_price += nodes[0]->S->price[t];
    }
    avg_price = avg_price/double(gc->stps);

    return valuefunction_Euro;
}
///////////////////////////////////////////////////////////////////
int Riversystem::WriteRiverSystemData(double restprice) {

    // Here we write out the aggregated Riversystem data
    FILE *fp;
    char outfilename [100];
    sprintf (outfilename, "%sriversystem_%s_output.txt",  gc->outputdir.c_str(),  gc->systemname.c_str() );

    if((fp = fopen(  outfilename ,"w"))==NULL) {
        printf("Cannot open file %s \n", outfilename);
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        exit(EXIT_FAILURE);
    }

    fprintf(fp, "Riversystem %s\n", gc->systemname.c_str() );

    fprintf(fp, "Node Idnr Nodename          Nodetype int Nodetypename Remaining_Mm3\n");

    for(size_t n = 0; n < nr_nodes; n++) {
        fprintf(fp, "Node %2d %-20s Nodetype %d  %-20s  %.4f \n", 
            int(n), nodes[n]->nodename.c_str(), nodes[n]->nodetype, EnumToString(nodes[n]->nodetype) , nodes[n]->GetEndWater_Mm3() );
    }
  
    tot_remaining_available_Mm3  = 0.0;
    tot_remaining_available_MWh  = 0.0;
    tot_remaining_available_Euro = 0.0;
    tot_income_Euro              = 0.0;
    tot_cost_Euro                = 0.0;
    tot_profit_Euro              = 0.0;
    valuefunction_Euro           = 0.0;
    sum_production               = 0.0;

    // At the most downstream node (OCEAN) the total available water 
    // is the node available water + upstream available (not included DEAD WATER)
    tot_remaining_available_Mm3  = nodes[nr_nodes-1]->upstream_remaining_available_Mm3;
    tot_remaining_available_Mm3 += nodes[nr_nodes-1]->remaining_available_Mm3;

    // Note that Powerstation cannot store water (remaining_available_Mm3 = 0.0), 
    // so downstream accumulation of remaining water is OK. 

    // We now loop over the Powerstations and calculate the remaining energy and value. 
    for(size_t n = 0; n < nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::POWERSTATION) { 
            // Powerstations has zero storage so only upstream water is needed. 
            tot_remaining_available_MWh += (nodes[n]->local_energy_equivalent * nodes[n]->upstream_remaining_available_Mm3 * 1000000.0 / 1000.0); // MWh
            for(size_t t = 0; t < gc->stps; t++) {
                sum_production += nodes[n]->S->Power[t];
            }
        }
    }

    for(size_t n = 0; n < nr_nodes; n++) {
        for(size_t t = 0; t < gc->stps; t++) {
            tot_income_Euro += nodes[n]->S->income[t];
            tot_cost_Euro  += nodes[n]->S->cost[t];
        }
    }

    tot_remaining_available_Euro = tot_remaining_available_MWh*restprice;
    tot_profit_Euro = tot_income_Euro - tot_cost_Euro;
    valuefunction_Euro = tot_profit_Euro + tot_remaining_available_Euro;

    fprintf(fp,"-------------------------------------------\n");
    fprintf(fp,"GLOBAL WATERBALANCE\n");
    fprintf(fp,"start_water_Mm3   = %.6f\n", start_water_Mm3 );
    fprintf(fp,"inflow_volume_Mm3 = %.6f\n", inflow_volume_Mm3); 
    fprintf(fp,"outflow_Mm3       = %.6f\n", outgoing_Mm3);
    fprintf(fp,"end_water_Mm3     = %.6f\n", end_water_Mm3);
    fprintf(fp,"waterbalance      = %.6f\n", waterbalance);
    fprintf(fp,"Note that there might be dead water below LRW in the system\n");
    fprintf(fp,"-------------------------------------------\n");


    if(WATERBALANCE_WARNINGS) {
        printf("GLOBAL WATERBALANCE\n");
        printf("start_water_Mm3   = %.6f\n", start_water_Mm3 );
        printf("inflow_volume_Mm3 = %.6f\n", inflow_volume_Mm3); 
        printf("outflow_Mm3       = %.6f\n", outgoing_Mm3);
        printf("end_water_Mm3     = %.6f\n", end_water_Mm3);
        printf("waterbalance      = %.6f\n", waterbalance);
        printf("Note that there might be dead water below LRW in the system\n");
        printf("-------------------------------------------\n");
    }


    double sum_qmin_cost = 0.0;

    for(size_t n = 0; n < nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::CHANNEL) { 
            for(size_t t = 0; t < gc->stps; t++) {
                sum_qmin_cost += nodes[n]->S->cost[t];
            }
        }
    }

    sum_lrw_cost = 0.0;
    // We now loop over the RESERVOIRS
    for(size_t n = 0; n < nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::RESERVOIR) { 
            for(size_t t = 0; t < gc->stps; t++) {
                sum_lrw_cost += nodes[n]->S->cost[t];
            }
        }
    }

    sum_startstopcost = 0.0;
    sum_max_adjustment_cost = 0.0;
    for(size_t n = 0; n < nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::POWERSTATION) { 
            for(size_t t = 0; t < gc->stps; t++) {
                sum_startstopcost += nodes[n]->S->cost[t] - nodes[n]->S->adjust_cost[t];
                sum_max_adjustment_cost += nodes[n]->S->adjust_cost[t];
            }
        }
    }

    avg_price = 0.0;
    for(size_t t = 0; t < gc->stps; t++) {
        avg_price += nodes[0]->S->price[t];
    }
    avg_price = avg_price/double(gc->stps);

    fprintf(fp, "Average_price_Euro           = %.3f\n", avg_price);
    fprintf(fp, "RestPrice_Euro               = %.3f\n", restprice);
    fprintf(fp, "tot_remaining_available_Mm3  = %.3f\n", tot_remaining_available_Mm3);
    fprintf(fp, "tot_remaining_available_MWh  = %.3f\n", tot_remaining_available_MWh);
    fprintf(fp, "tot_remaining_available_Euro = %.3f\n", tot_remaining_available_Euro);
    fprintf(fp, "Sum_Production_MWh           = %.3f\n", sum_production);
    fprintf(fp, "tot_income_Euro              = %.3f\n", tot_income_Euro);
    fprintf(fp, "Avg_achieved_price_E_MWh     = %.3f\n", tot_income_Euro/sum_production );
    fprintf(fp, "sum_qmin_cost_Euro           = %.3f\n", sum_qmin_cost);
    fprintf(fp, "sum_lrw_cost_Euro            = %.3f\n", sum_lrw_cost);
    fprintf(fp, "sum_startstopcost_Euro       = %.3f\n", sum_startstopcost);
    fprintf(fp, "sum_max_adjustment_cost      = %.3f\n", sum_max_adjustment_cost);
    fprintf(fp, "tot_cost_Euro                = %.3f\n", tot_cost_Euro);
    fprintf(fp, "tot_profit_Euro              = %.3f\n", tot_profit_Euro);
    fprintf(fp, "valuefunction_Euro           = %.3f\n", valuefunction_Euro);

    fclose(fp);

    if(ECONOMY_WARNINGS) {
        printf("Average_price_Euro           = %.3f\n", avg_price);
        printf("RestPrice_Euro               = %.3f\n", restprice);
        printf("tot_remaining_available_Mm3  = %.3f\n", tot_remaining_available_Mm3);
        printf("tot_remaining_available_MWh  = %.3f\n", tot_remaining_available_MWh);
        printf("tot_remaining_available_Euro = %.3f\n", tot_remaining_available_Euro);
        printf("Sum_Production_MWh           = %.3f\n", sum_production);
        printf("tot_income_Euro              = %.3f\n", tot_income_Euro);
        printf("Avg_achieved_price_E_MWh     = %.3f\n", tot_income_Euro/sum_production );
        printf("sum_qmin_cost_Euro           = %.3f\n", sum_qmin_cost);
        printf("sum_lrw_cost_Euro            = %.3f\n", sum_lrw_cost);
        printf("sum_startstopcost_Euro       = %.3f\n", sum_startstopcost);
        printf("sum_max_adjustment_cost      = %.3f\n", sum_max_adjustment_cost);
        printf("tot_cost_Euro                = %.3f\n", tot_cost_Euro);
        printf("tot_profit_Euro              = %.3f\n", tot_profit_Euro);
        printf("valuefunction_Euro           = %.3f\n", valuefunction_Euro);
        printf("-----------------------------------\n");
    }


    return 0;
}
///////////////////////////////////////////////////////////////////