/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     riversystem.cpp                                                        
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:

Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

********************************************************************************/


#include "herss.h"

Riversystem::Riversystem(){}

Riversystem::Riversystem(GlobalConfig *gc) {

    this->gc = gc;
    this->nr_nodes      = gc->nr_nodes;
    this->nr_reservoirs = gc->nr_reservoirs;
    this->nr_pstations  = gc->nr_pstations;
    this->nr_channels   = gc->nr_channels;
    // Terje Sandø, pump-station work, July 2026.
    this->nr_pumps      = gc->nr_pumps;

    nodes = new Node*[nr_nodes];

    if(nr_reservoirs > 0) {
        reservoirs = new Reservoir[nr_reservoirs];
        for(size_t r = 0; r < nr_reservoirs; r++) {
            reservoirs[r].gc = gc;
        }
    }

    if(nr_pstations > 0) {
        pstations = new Powerstation[nr_pstations];
        for(size_t p = 0; p < nr_pstations; p++) {
            pstations[p].gc = gc;
        }
    }
    if(nr_channels > 0) {
        channels = new Channel[nr_channels];
        for(size_t c = 0; c < nr_channels; c++) {
            channels[c].gc = gc;
        }
    }
    
    // Terje Sandø, pump-station work, July 2026.
    if(nr_pumps > 0) {
        pumps = new Pump[nr_pumps];
        for(size_t p = 0; p < nr_pumps; p++) {
            pumps[p].gc = gc;
        }
    }

    
    size_t reservoirs_used = 0;
    size_t pstations_used = 0;
    size_t channels_used = 0;
    size_t pumps_used = 0;

    for(size_t n = 0; n < nr_nodes; n++) {

        switch (gc->nodetypes[n])  {
            case RESERVOIR:
                nodes[n] = &reservoirs[reservoirs_used];
                nodes[n]->idnr = n;
                reservoirs_used++;
                break;

            case PSTATION:
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
            // Terje Sandø, pump-station work, July 2026.
            case PUMP:
                nodes[n] = &pumps[pumps_used];
                nodes[n]->idnr = n;
                pumps_used++;
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
    // Terje Sandø, pump-station work, July 2026.
    if(nr_pumps > 0) {
        delete [] pumps;
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
/*
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
            printf("%.4f ", this->pstations[p].S->action[t][this->pstations[p].idnr]);
        }
        printf("\n");
    }
}
*/
///////////////////////////////////////////////////////////////////
void Riversystem::PrintReservoirData2Screen() {

    printf("Riversystem %s reservoir fractions [fr] \n", gc->systemname.c_str() );

    printf("YYYY MM DD HH ");
    for(size_t n = 0; n < gc->nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::RESERVOIR) {
            printf("%s ", nodes[n]->nodename.c_str()  );
        }
    }
    printf("\n");

    for(size_t t = 0; t < gc->stps; t++) {
        // printf("%d %d %d %d ", nodes[0]->S->year[t], nodes[0]->S->month[t], nodes[0]->S->day[t], nodes[0]->S->hour[t]);
        for(size_t n = 0; n < gc->nr_nodes; n++) {
            if(nodes[n]->nodetype == NodeType::RESERVOIR) {
                // printf("%.4f ", nodes[n]->S->res_fr[t] );
                printf("%.4f ", nodes[n]->S->res_Mm3[t] );
            }
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
void Riversystem::PrintEconomicInfo(class Herss *herss_obj) {

    // THESE ARE USED FOR DETAILED TIMESTEP PRINT
    //double sum_profit;
    //double sum_income;
    //double sum_Power;
    //double sum_cost;
    //double sum_inflow_Mm3;

    printf("##################################################################\n");
    printf("Riversystem %s economic information \n", gc->systemname.c_str() );

    printf("ValueFunction                = %.5f\n",  this->valuefunction_Euro);
    printf("valuefunction_Euro           = %.3f\n",  this->valuefunction_Euro);
    printf("tot_profit_Euro              = %.3f\n",  this->tot_profit_Euro);
    printf("tot_remaining_Euro           = %.3f\n",  this->tot_remaining_Euro);
    printf("tot_remaining_MWh            = %.4f\n",  this->tot_remaining_MWh);
    printf("tot_remaining_Mm3            = %.4f\n",  this->tot_remaining_Mm3);
    printf("tot_remaining_active_Mm3     = %.4f\n",  this->tot_active_remaining_Mm3);  

    // First we calculate the starting reservoir level for the whole system.
    // We want this in Mm3 and in fraction.

    double R_init_active_Mm3 = 0.0;
    double sum_active_max_volue_Mm3 = 0.0;

    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        R_init_active_Mm3 += this->reservoirs[r].reservoir_init_active_Mm3;
        sum_active_max_volue_Mm3 += this->reservoirs[r].active_max_volume_Mm3;
    }

    printf("R_init_active_Mm3         = %.6f\n", R_init_active_Mm3);
    printf("Sum_active_max_volue_Mm3  = %.6f\n", sum_active_max_volue_Mm3);
    printf("R_init_fr                 = %.6f\n", R_init_active_Mm3/sum_active_max_volue_Mm3);
    
    // this->PrintEconomicInfo(herss_obj);
    
    
/*
    // Print HEADER LINE 
    printf("t  SumQ_Mm3   ");
    for(size_t p = 0; p < gc->nr_pstations; p++) {
        for(size_t g = 0; g < this->pstations[p].generators.size(); g++) {
            string outstr = "a" + to_string(p) + "_g" + to_string(g);
            printf("%-8s ", outstr.c_str());
        }
    }



ADD THIS PRINT FOR DETAILED INFORMATION FOR EACH TIME STEP
REMEMBER TO ADD THE DOUBLES ABOVE

    printf(" SumActiveR_Mm3       SumR_fr   SumPower   Price      Income     Cost       Profit     VF_end_of_stp\n");
    printf("----------------------------------------------------------------------------------------------------------------\n");


    for(size_t t = 0; t < gc->stps; t++) {
        sum_profit = 0.0;
        sum_income  = 0.0;
        sum_Power   = 0.0;
        sum_cost    = 0.0;
        sum_inflow_Mm3 = 0.0;

        double sumR_active_Mm3 = 0.0;
        for(size_t r = 0; r < gc->nr_reservoirs; r++) {
            sumR_active_Mm3 += this->reservoirs[r].S->res_active_Mm3[t];
            sum_inflow_Mm3 += MACRO_m3s_2_Mm3(this->reservoirs[r].S->inflow[t], herss_obj->getDeltaT(t));
        }

        for(size_t n = 0; n < nr_nodes; n++) {
            sum_income += nodes[n]->S->income[t];
            sum_cost   += nodes[n]->S->cost[t];
            sum_Power  += nodes[n]->S->Power[t];
            sum_profit += nodes[n]->S->profit[t];
            
        }

        printf("%-2lu ", t);
        printf("%-10.3f ", sum_inflow_Mm3);

        for(size_t p = 0; p < gc->nr_pstations; p++) {
            for(size_t g = 0; g < this->pstations[p].generators.size(); g++) {
                double act;
                if (this->pstations[p].generators[g].action.size() > t) {
                    act = this->pstations[p].generators[g].action[t];
                } else {
                    act = -9999.0;
                }
                printf("%8.3f ", act);
            }
        }
        
        printf("%-10.3f ", sumR_active_Mm3);
        printf("%-10.5f ", sumR_active_Mm3/sum_active_max_volue_Mm3);
        printf("%-10.3f ", sum_Power);
        printf("%-10.3f ", nodes[0]->S->price[t]);
        printf("%-10.3f ", sum_income);
        printf("%-10.3f ", sum_cost);
        printf("%-10.3f ", sum_profit);
        printf("%-10.3f ", this->ValueFunction[t]);
        printf("\n");
    }


*/
    printf("\n");
    printf("tot_remaining_Mm3            = %.2f\n", this->tot_remaining_Mm3);
    printf("tot_active_remaining_Mm3     = %.2f\n", this->tot_active_remaining_Mm3);
    printf("tot_remaining_MWh            = %.2f\n", this->tot_remaining_MWh);
    printf("restprice                    = %.2f\n", this->nodes[0]->S->restprice);
    printf("tot_remaining_Euro           = %.2f\n", this->tot_remaining_Euro);
    printf("tot_income_Euro              = %.2f\n", this->tot_income_Euro);
    printf("tot_cost_Euro                = %.2f\n", this->tot_cost_Euro);
    printf("tot_profit_Euro              = %.2f\n", this->tot_profit_Euro);
    printf("valuefunction_Euro           = %.2f\n", this->valuefunction_Euro);


//    printf("Reservoir content time Mm3 fr \n");
//    for(size_t t = 0; t < gc->stps; t++) {
//        printf("t= %lu ", t);
//        for(size_t r = 0; r < gc->nr_reservoirs; r++) {
//            printf("%.6f ", this->reservoirs[r].S->res_Mm3[t]);
//            printf("%.6f ", this->reservoirs[r].S->res_active_Mm3[t]);
//            printf("%.6f ", this->reservoirs[r].S->res_fr[t]);
//            printf("%.6f ", sum_active_max_volue_Mm3);
//            printf("%.6f ", this->reservoirs[r].S->res_Mm3[t] / sum_active_max_volue_Mm3  );
//
//        }
//        printf("\n");
//    }

}
//////////////////////////////////////////////////////////////////////
// Calculate the value function at the END of a specified timestep
double Riversystem::CalcVF_atEndOfStp(double restprice, size_t stp) {

    // We need to run simulation and CalcVF before this ufunction. 
    if(stp < 0 || stp > (gc->stps-1)) {
        printf("ERROR:  stp is out of range.  stp= %lu \n", stp );
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        exit(EXIT_FAILURE);
    }   

    double my_income_Euro = 0.0;
    double my_cost_Euro = 0.0;

    // We have already calculated the total VF0 for the whole period. 
    // We can now sum the profit from the start until the current timestep. 
    // We subtract this value from VF0, and we have the value function at the end of the timestep.
    double VF0 = valuefunction_Euro;

    //printf("VF0 = %.4f\n", VF0);
    //printf("------------ CalcVF_atStp --------t = %lu  -----------------------\n", stp );
    //printf("restprice = %.4f\n", restprice);
    //printf("income_Euro = %.4f\n", my_income_Euro);

    for(size_t n = 0; n < nr_nodes; n++) {
        for(size_t t = 0; t <= stp; t++) {
            my_income_Euro += nodes[n]->S->income[t];
            my_cost_Euro  += nodes[n]->S->cost[t];
        }
    }

    //printf("income_Euro = %.4f\n", my_income_Euro);
    //printf("cost_Euro   = %.4f\n", my_cost_Euro);
    double profit_stp = my_income_Euro - my_cost_Euro;

    double VF_stp = VF0 - profit_stp;
    //printf("profit_stp = %.4f\n", profit_stp);
    //printf("VF_stp = %.4f\n", VF_stp);

    return VF_stp;
    //printf("------------ CalcVF_atStp ------------------------------\n");

};  
//////////////////////////////////////////////////////////////////////
// Run some checks to see if the configuration of the riversystem is correct.
void Riversystem::DiagnoseRiversystemConfiguration() {   

    // Loop over all reservoirs and check sinits, constraints and reservoir curve.
    for(size_t r = 0; r < gc->nr_reservoirs; r++) {
        reservoirs[r].ValidateReservoirSettings();
    }

    for(size_t p = 0; p < gc->nr_pstations; p++) {
        pstations[p].ValidatePowerstationSettings();
    }

    for(size_t c = 0; c < gc->nr_channels; c++) {
       channels[c].ValidateChannelSettings();
    }

    // Terje Sandø, pump-station work, July 2026.
    for(size_t p = 0; p < gc->nr_pumps; p++) {
       pumps[p].ValidatePumpSettings();
    }

}
//////////////////////////////////////////////////////////////////////
double Riversystem::CalcVF(double restprice) {

    this->tot_active_remaining_Mm3  = 0.0;
    tot_remaining_MWh               = 0.0;
    tot_remaining_Euro              = 0.0;
    tot_income_Euro                 = 0.0;
    tot_cost_Euro                   = 0.0;
    tot_profit_Euro                 = 0.0;
    valuefunction_Euro              = 0.0;
    sum_production                  = 0.0;

    tot_active_remaining_Mm3   = nodes[nr_nodes-1]->upstream_remaining_active_Mm3;  // Bottom node is the most downstream node.
    tot_remaining_Mm3          = nodes[nr_nodes-1]->upstream_remaining_Mm3;

    sum_total_MWh = 0.0;    // Production pluss remaining in whole riversystem

    // At the most downstream node (OCEAN) the total available water 
    // is the node available water + upstream available (not included DEAD WATER)
    // tot_remaining_available_Mm3  = nodes[nr_nodes-1]->upstream_remaining_available_Mm3;
    // tot_remaining_available_Mm3 += nodes[nr_nodes-1]->remaining_Mm3;
    //printf("tot_remaining_available_Mm3 at outlet= %.4f\n", tot_remaining_available_Mm3);


    // We now loop over the Powerstations and calculate the remaining energy and value. 
    // Note that we only count the water up to HRW, even though in flooding situations we can have water levels above this. 
    for(size_t n = 0; n < nr_nodes; n++) {

        if(nodes[n]->nodetype == NodeType::PSTATION) { 
            // Powerstations has zero storage so only upstream water is needed. 
            // tot_remaining_available_MWh += (nodes[n]->local_energy_equivalent * nodes[n]->upstream_remaining_available_Mm3 * 1000000.0 / 1000.0); // MWh
            tot_remaining_MWh += (nodes[n]->local_energy_equivalent * nodes[n]->upstream_remaining_active_Mm3 * 1000000.0 / 1000.0); // MWh


            for(size_t t = 0; t < gc->stps; t++) {
                sum_production += nodes[n]->S->Power[t];
                sum_total_MWh += nodes[n]->S->Power[t];
            }

            if(nodes[n]->local_energy_equivalent < 0.0 ) { 
                printf("ERROR: local_energy_equivalent is negative for node %d (%s) \n", int(n), nodes[n]->nodename.c_str() );
                printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
                exit(EXIT_FAILURE);
            }

            // cout << "Node " << n << " " << nodes[n]->nodename << " upstream_remaining_active_Mm3 = " << nodes[n]->upstream_remaining_active_Mm3 << endl;
            // cout << "local energy equibalent = " << nodes[n]->local_energy_equivalent << endl;
            // cout << "tot_remaining_MWh = " << tot_remaining_MWh << endl;
            // cout << "sum_production = " << sum_production << endl;
            // cout << "sum_total_MWh = " << sum_total_MWh << endl;


        }
    }

  


    sum_total_MWh += tot_remaining_MWh;


    for(size_t n = 0; n < nr_nodes; n++) {
        for(size_t t = 0; t < gc->stps; t++) {
            tot_income_Euro += nodes[n]->S->income[t];
            tot_cost_Euro  += nodes[n]->S->cost[t];
        }
    }


    tot_remaining_Euro = tot_remaining_MWh*restprice;
    tot_profit_Euro = tot_income_Euro - tot_cost_Euro;
    valuefunction_Euro = tot_profit_Euro + tot_remaining_Euro;

    /*
    printf("------------ CalcVF -------------------------------\n");
    printf("tot_remaining_available_MWh                   = %.4f\n", tot_remaining_available_MWh);
    printf("restprice                                     = %.4f\n", restprice);
    printf("valuefunction_Euro                            = %.4f\n", valuefunction_Euro);
    printf("tot_income_Euro                               = %.4f\n", tot_income_Euro);
    printf("tot_cost_Euro                                 = %.4f\n", tot_cost_Euro);
    printf("tot_profit_Euro                               = %.4f\n", tot_profit_Euro);
    printf("tot_remaining_available_Euro                  = %.4f\n", tot_remaining_available_Euro);
    printf("tot_remaining_available_Mm3                   = %.4f\n", tot_remaining_available_Mm3);
    printf("total_effective_remaining_available_Mm3       = %.4f\n", total_effective_remaining_available_Mm3);
    printf("------------- CalcVF ------------------------------\n");
    */


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
        if(nodes[n]->nodetype == NodeType::PSTATION) { 
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
  
    tot_remaining_Mm3  = 0.0;
    tot_remaining_MWh  = 0.0;
    tot_remaining_Euro = 0.0;
    tot_income_Euro              = 0.0;
    tot_cost_Euro                = 0.0;
    tot_profit_Euro              = 0.0;
    valuefunction_Euro           = 0.0;
    sum_production               = 0.0;

    // At the most downstream node (OCEAN) the total available water 
    // is the node available water + upstream available (not included DEAD WATER)
    tot_remaining_Mm3  = nodes[nr_nodes-1]->upstream_remaining_Mm3;
    tot_remaining_Mm3 += nodes[nr_nodes-1]->remaining_Mm3;

    // Note that Powerstation cannot store water (remaining_available_Mm3 = 0.0), 
    // so downstream accumulation of remaining water is OK. 

    // We now loop over the Powerstations and calculate the remaining energy and value. 
    for(size_t n = 0; n < nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::PSTATION) { 
            // Powerstations has zero storage so only upstream water is needed. 
            tot_remaining_MWh += (nodes[n]->local_energy_equivalent * nodes[n]->upstream_remaining_active_Mm3 * 1000000.0 / 1000.0); // MWh

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

    tot_remaining_Euro = tot_remaining_MWh*restprice;
    tot_profit_Euro = tot_income_Euro - tot_cost_Euro;
    valuefunction_Euro = tot_profit_Euro + tot_remaining_Euro;

    // Terje Sandø, pump-station work, August 2026.
    // A pump stores its consumption as negative Power [MWh], so the sign is flipped here
    // to report the consumed energy as a positive number.
    double sum_pump_consumption = 0.0;
    for(size_t n = 0; n < nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::PUMP) {
            for(size_t t = 0; t < gc->stps; t++) {
                sum_pump_consumption -= nodes[n]->S->Power[t];
            }
        }
    }

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
        if(nodes[n]->nodetype == NodeType::PSTATION || nodes[n]->nodetype == NodeType::PUMP) { 
            for(size_t t = 0; t < gc->stps; t++) {
                sum_startstopcost += nodes[n]->S->startStopCost[t] ;
                sum_max_adjustment_cost += nodes[n]->S->adjust_cost[t];
            }
        }
    }


    // Summarize the aggressive actions cost for all powerstations
    double sum_aggressive_actions_cost = 0.0;
    for(size_t n = 0; n < nr_nodes; n++) {
        if(nodes[n]->nodetype == NodeType::PSTATION) { 
            for(size_t t = 0; t < gc->stps; t++) {
                sum_aggressive_actions_cost += nodes[n]->S->cost_aggressive_actions[t];
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
    fprintf(fp, "tot_remaining_Mm3            = %.3f\n", tot_remaining_Mm3);
    fprintf(fp, "tot_active_remaining_Mm3     = %.3f\n", this->tot_active_remaining_Mm3);
    fprintf(fp, "tot_remaining_MWh            = %.3f\n", tot_remaining_MWh);
    fprintf(fp, "tot_remaining_Euro           = %.3f\n", tot_remaining_Euro);
    fprintf(fp, "Sum_Production_MWh           = %.3f\n", sum_production);
    fprintf(fp, "Sum_Pump_Consumption_MWh     = %.3f\n", sum_pump_consumption);
    fprintf(fp, "tot_income_Euro              = %.3f\n", tot_income_Euro);

    if(sum_production > 1.0) {
        fprintf(fp, "Avg_achieved_price_E_MWh     = %.3f\n", tot_income_Euro/sum_production );
    } else {
        fprintf(fp, "Avg_achieved_price_E_MWh     = %.3f\n", restprice);
    }
    fprintf(fp, "sum_qmin_cost_Euro           = %.3f\n", sum_qmin_cost);
    fprintf(fp, "sum_lrw_cost_Euro            = %.3f\n", sum_lrw_cost);
    fprintf(fp, "sum_startstopcost_Euro       = %.3f\n", sum_startstopcost);
    fprintf(fp, "sum_max_adjustment_cost      = %.3f\n", sum_max_adjustment_cost);
    fprintf(fp, "sum_aggressive_actions_cost  = %.3f\n", sum_aggressive_actions_cost);
    fprintf(fp, "tot_cost_Euro                = %.3f\n", tot_cost_Euro);
    fprintf(fp, "tot_profit_Euro              = %.3f\n", tot_profit_Euro);
    fprintf(fp, "valuefunction_Euro           = %.3f\n", valuefunction_Euro);

    fclose(fp);

    if(ECONOMY_WARNINGS) {
        printf("Average_price_Euro           = %.3f\n", avg_price);
        printf("RestPrice_Euro               = %.3f\n", restprice);
        printf("tot_remaining_Mm3            = %.3f\n", tot_remaining_Mm3);
        printf("tot_active_remaining_Mm3     = %.3f\n", tot_active_remaining_Mm3);
        printf("tot_remaining_MWh            = %.3f\n", tot_remaining_MWh);
        printf("tot_remaining_Euro           = %.3f\n", tot_remaining_Euro);
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