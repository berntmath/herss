/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     dataset.cpp
Developers:   Bernt Viggo Matheussen, Ove Arstad
Organization: Å Energi, www.ae.no

This software is released under the MIT license.

********************************************************************************/

#include "herss.h"


/////////////////////////////////////////////////////////////////////////////////////////////////
Dataset::Dataset(GlobalConfig *gc){

    this->gc = gc;
    this->stps     = gc->stps;
    this->nr_nodes = gc->nr_nodes;
    try {
        inflow  = new double*[stps];
        action = new double*[stps];
        for( size_t t = 0; t < stps; ++t ) {
            inflow[t] = new double[nr_nodes];
            action[t] = new double[nr_nodes];
        }
    }
    catch(std::bad_alloc& exc) { 
        LOG_ERR("Error: memory allocation failed."); 
    }

    for(size_t t = 0; t < stps;  t++) {
        for(size_t n = 0; n < nr_nodes;  n++) {
            inflow[t][n] = 0.0;  // To make things easier and faster 
            action[t][n] = NOT_INIT;
        }
    }

    try {
        price   = new double[stps];
        year    = new int[stps];
        month   = new int[stps];
        day     = new int[stps];
        hour    = new int[stps];
    }
    catch(std::bad_alloc& exc) { 
        LOG_ERR("Error: memory allocation failed."); 
    }
    this->restprice = NOT_INIT;
    for(size_t t = 0; t < stps; t++) {
        price[t] = NOT_INIT;
        year[t]  = NOT_INIT;
        month[t] = NOT_INIT;
        day[t]   = NOT_INIT;
        hour[t]  = NOT_INIT;
    }

}
///////////////////////////////////////////////////////////////////////////////////////////
Dataset::~Dataset(){
    for(size_t t = 0; t < stps; t++) {
        delete [] inflow[t];
        delete [] action[t];
    }
    delete [] inflow;
    delete [] action;
    delete [] price;
    delete [] year;
    delete [] month;
    delete [] day;
    delete [] hour;
    this->gc = NULL;

}
/////////////////////////////////////////////////////////////////////////////////////////
// CHANGE BY OVE: Calculate epochs and delta_t for each timestep.
// BVM May 2026. 
// We do not use DT or DT_LAST anymore.
// DT are calculated from the dates in the price file. This allows for variable time steps.
// We set DT in the last timestep to be the same as in the timestep before.

void Dataset::multi_temporal_resolution() {

    std::vector<time_t> epochs(stps);
    
    DateTime dt;
    
    for (size_t t = 0; t < stps; ++t) {
        dt.setDate(year[t], month[t], day[t], hour[t], 0, 0); 
        epochs[t] = dt.getEpoch();
    }
    
    delta_t.clear();
    for (size_t t = 1; t < stps; ++t) {
        delta_t.push_back(static_cast<int>(epochs[t] - epochs[t-1]));
    }

    // Add the last timestep with the same delta_t as the previous one
    delta_t.push_back(static_cast<int>(epochs[stps-1] - epochs[stps-2]));

}
/////////////////////////////////////////////////////////////////////////////////////////
// CHANGE BY OVE: Get delta_t for a specific timestep
// If the timestep is the last one, we return gc->dt_last

// BVM May 2026, found a minor bug. 
// If the user didnt specify the dt_last in the config file, it failed. 
// I am not sure why,
// We have set dt in the last timestep to be the same as in the second last step.
// In this way the user can run the model without specifying dt, or dt_last.

// Old code 
// int Dataset::getDeltaT(size_t timestep) {
//     if (timestep < stps - 1) {
//         return delta_t[timestep]; 
//     }
//     if (timestep == stps - 1 && !delta_t.empty()) {
//         if (gc->dt_last > 0) {
//             return gc->dt_last;
//         }
//         printf("Error: Not set LAST_DT in global config?\n"); 
//         exit(EXIT_FAILURE);
//     }
//     std::cerr << "Error: timestep out of bounds in getDeltaT: " << timestep << std::endl;
//     exit(EXIT_FAILURE);
// }

int Dataset::getDeltaT(size_t timestep) {
    if (timestep < stps && !delta_t.empty() && timestep >= 0) {
        return delta_t[timestep]; 
    }
    std::cerr << "Error: timestep out of bounds in getDeltaT: " << timestep << std::endl;
    printf("file: %s  linenr: %d   function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
    exit(EXIT_FAILURE);
}


/////////////////////////////////////////////////////////////////////////////////////////
void Dataset::readAllData() {
    readPricefile();
    readInflowFile();
    readActionsFile();
    multi_temporal_resolution();
}

/////////////////////////////////////////////////////////////////////////////////////////
void Dataset::readActionsFile() {

    Xtime xtime;
    ifstream myfile;
    string line;
    string keyword;
    string value;
    Line line_obj;


    // CHANGE BY OVE: Enable reading of multiple generators in the 
    // actions file (ex. 1_0 for generator 0 in node 1, 1_1 for generator 1 in node 1)
    
    // Modified by BVM, May 2026. 
    

    std::vector<std::string> colnames;  

    myfile.open(gc->actionsfile.c_str() );
    if (!myfile.is_open()) 	{
        LOG_ERR("The actionsfile " + gc->actionsfile + " could not be found/opened");
    }   

    // The actions can be hatch release from reservoirs
    // or powerproduction (multi-generator). 

    size_t active_nodes = 0;

    getline(myfile, line);

    if( line.length()  > 0 && ( line[0] != '#') ) {

        keyword = line_obj.extractNextElementFromLine(&line);
        if ((!keyword.compare("Date_NodeID")) == 0) {
            LOG_ERR("There is an error (missing Date_NodeID) in the actionsfile file " + gc->actionsfile + " please revisit input");
        }
        string tmpline = line;
        active_nodes = line_obj.calcNrCols(&tmpline);

        if(active_nodes != gc->n_action_nodes_from_topology) {
            LOG_INFO("Number of action nodes in the actionsfile " + gc->actionsfile + " does not match number of action nodes in the topology file " + gc->topologyfile);
            LOG_INFO("Please revisit input");
            LOG_WARN("Number of action nodes in the actionsfile " + gc->actionsfile + " does not match number of action nodes in the topology file " + gc->topologyfile);
            LOG_WARN("Number of action nodes in the actionsfile: " + std::to_string(active_nodes));
            LOG_WARN("Number of action nodes in the topology file: " + std::to_string(gc->n_action_nodes_from_topology));
            LOG_ERR("Please revisit input");
        }

        // Stations with several generators can give more action columns than nodes, and
        // action[t] was allocated with one slot per node.
        if(active_nodes > nr_nodes) {
            for(size_t t = 0; t < stps; t++) {
                delete [] action[t];
                action[t] = new double[active_nodes];
            }
        }

        // Read in the generator names for each column and save them
        for(size_t c = 0; c < active_nodes; c++) {
            value = line_obj.extractNextElementFromLine(&line);
            colnames.push_back(value); // Store "1_0", "1_1", etc.
        }
    }   

    // Read each line with date in the first column and then the data
    for(size_t t = 0; t < this->stps; t++) {
        getline(myfile, line);
        keyword = line_obj.extractNextElementFromLine(&line);

        if( !xtime.setDate(keyword) ) {
            LOG_ERR("ERROR: Date format is not valid in actionsfile: " + gc->actionsfile + ", please revisit input");
        }

        int actions_year = xtime.getYear(); 
        int actions_month = xtime.getMonth();
        int actions_day = xtime.getDay();
        int actions_hour = xtime.getHour();

        if(actions_year != year[t] || actions_month != month[t] || actions_day != day[t] || actions_hour != hour[t]) {
            LOG_ERR("ERROR: Date mismatch in actionfile at timestep " + to_string(t) + " between price and actions files.\n");
        }

        for(size_t c = 0; c < active_nodes; c++) {
            value = line_obj.extractNextElementFromLine(&line);
            action[t][c]  = stof(value);
        }
    }
    myfile.close();
    this->action_colnames = colnames;


}
/////////////////////////////////////////////////////////////////////////////////////////
void Dataset::readInflowFile() {
    
    Xtime xtime;
    ifstream myfile;
    string line;
    string keyword;
    string value;
    Line line_obj;
    size_t idnrs[MAX_NR_NODES];  // We save the idnrs given in the first line in the inputfile. 

    // The number of columns should be the same as number of reservoirs + 1 (the date column). 

    myfile.open(gc->inflowfile.c_str() );
    if (!myfile.is_open()) 	{
        LOG_ERR("The inflowfile " + gc->inflowfile + " could not be found/opened.");
    }

    size_t active_nodes = 0;
    getline(myfile, line);

    if( line.length()  > 0 && ( line[0] != '#') ) {
        keyword = line_obj.extractNextElementFromLine(&line);
        if ((!keyword.compare("Date_NodeID")) == 0) {
            LOG_ERR("There is an error (missing Date_NodeID) in the inflowseries file " + gc->inflowfile + " please revisit input");
        }
        string tmpline = line;
        active_nodes = line_obj.calcNrCols(&tmpline);

        // We have scipped the first column already 
        if(active_nodes != (gc->nr_reservoirs)) {
            LOG_INFO("There is an error in the inflowfile " + gc->inflowfile);
            LOG_INFO("Please revisit input. ");
            LOG_INFO("Number of columns should be number of reservoirs + 1 (date column).");
            LOG_ERR("There is an error in the inflowfile " + gc->inflowfile + " please revisit input. Number of columns should be number of reservoirs + 1 (date column).");
        }

        // Now we read in the idnrs for each coloumn and save it. 
        for(size_t c = 0; c < active_nodes; c++) {
            value = line_obj.extractNextElementFromLine(&line);
            int tmp_idnr = stoi(value);
            if(tmp_idnr < 0 || tmp_idnr >= MAX_NR_NODES) {
                LOG_ERR("Error: idnr " + std::to_string(tmp_idnr) + " in inflowfile " + gc->inflowfile + " is out of bounds. It should be between 0 and " + std::to_string(MAX_NR_NODES-1));
            }
            idnrs[c] = size_t(tmp_idnr);
        }
    }

    // Until now we only accept inflow to reservoirs.
    // This means that the idnrs in the inflow file should match the idnrs of the reservoirs in the topology file.
    // We check this and give an error if they do not match.
    for(size_t c = 0; c < active_nodes; c++) {
        if(gc->nodetypes[idnrs[c]] != NodeType::RESERVOIR) {
            LOG_INFO("The idnr " + std::to_string(idnrs[c]) + " in the inflowfile " + gc->inflowfile + " does not match a reservoir node in the topology file " + gc->topologyfile);
            LOG_INFO("Please revisit input. The idnr " + std::to_string(idnrs[c]) + " in the inflowfile should match a reservoir node in the topology file.");
            LOG_WARN("The idnr " + std::to_string(idnrs[c]) + " in the inflowfile " + gc->inflowfile + " does not match a reservoir node in the topology file " + gc->topologyfile);
            LOG_ERR("Please revisit input. The idnr " + std::to_string(idnrs[c]) + " in the inflowfile should match a reservoir node in the topology file.");
        }
    }


    // Now we read in each line with date in the first column and then the data, and parse the date
    for(size_t t = 0; t < this->stps; t++) {
        getline(myfile, line);
        string str_line = line;
        size_t n_cols = line_obj.calcNrCols(&str_line);

        if(n_cols != (active_nodes+1)) {
            LOG_ERR("There is something wrong with the inflowfile: " + gc->inflowfile + ". Please check number of columns.");
        }

        keyword = line_obj.extractNextElementFromLine(&line);
        for(size_t c = 0; c < active_nodes; c++) {
            value = line_obj.extractNextElementFromLine(&line);
            inflow[t][idnrs[c]]  = stof(value);
        }        
            
        if( !xtime.setDate(keyword) ) {
            LOG_ERR("Date format is not valid in inflowfile: " + gc->inflowfile + ", please revisit input");
        }

        int inflow_year = xtime.getYear(); 
        int inflow_month = xtime.getMonth();
        int inflow_day = xtime.getDay();
        int inflow_hour = xtime.getHour();

        if(inflow_year != year[t] || inflow_month != month[t] || inflow_day != day[t] || inflow_hour != hour[t]) {
            LOG_ERR("ERROR: Date mismatch at timestep " + to_string(t) + " between price and inflow files.\n");
        }

    }

    myfile.close();

}
/////////////////////////////////////////////////////////////////////////////////////////
void Dataset::readPricefile() {

    Xtime xtime;
    ifstream myfile;
    string line;
    string keyword;
    string value;
    Line line_obj;

    myfile.open(gc->pricefile.c_str() );
    if (myfile.is_open()) 	{
        // Do nothing
    } else {
        LOG_ERR("The file " + gc->pricefile + " could not be found/opened.");
    }

    getline(myfile, line);
    if( line.length()  > 0 && ( line[0] != '#') ) {
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);
        if (keyword.compare("RESTPRICE") == 0) {
            this->restprice = stof(value);
        } else {
            LOG_ERR("There is an error in the pricefile " + gc->pricefile + " please revisit input");
        }
    }

    getline(myfile, line);
    if( line.length()  > 0 && ( line[0] != '#') ) {
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);
        if ((!keyword.compare("Date")) == 0) {
            LOG_ERR("There is an error in the pricefile " + gc->pricefile + " please revisit input");
        }
    }

    // We now read in the timeseries of price data
    for(size_t t = 0; t < this->stps; t++) {
        getline(myfile, line);
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);

        // Check date format
        if( !xtime.setDate(keyword) ) {
            LOG_ERR("Date format is not valid in pricefile: " + gc->pricefile + ", please revisit input");
        }

        year[t]  = xtime.getYear();
        month[t] = xtime.getMonth();
        day[t]   = xtime.getDay();
        hour[t]  = xtime.getHour();
        price[t] = stof(value);

    }
    myfile.close();
    
}
/////////////////////////////////////////////////////////////////////////////////////////