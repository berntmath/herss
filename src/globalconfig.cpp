/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     globalconfig.cpp
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
********************************************************************************/


#include "herss.h"

#include <algorithm>
#include <cctype>


GlobalConfig::GlobalConfig(){

    globalfile         = DEFAULT_STRING_INIT;
    topologyfile       = DEFAULT_STRING_INIT;
    actionsfile        = DEFAULT_STRING_INIT;
    pricefile          = DEFAULT_STRING_INIT;
    outputfile         = DEFAULT_STRING_INIT;
    inflowfile         = DEFAULT_STRING_INIT;
    systemname         = DEFAULT_STRING_INIT;
    start_statefile    = DEFAULT_STRING_INIT;
    out_statefile      = DEFAULT_STRING_INIT;
    outputdir          = DEFAULT_STRING_INIT;
    inputdir           = DEFAULT_STRING_INIT;
    logfilename        = DEFAULT_STRING_INIT;

    this->found_topologyfilename       = false;
    this->found_actionsfilename        = false;
    this->found_pricefilename          = false;
    this->found_inflowfilename         = false;
    this->found_systemname             = false;
    this->found_start_statefilename    = false;
    this->found_outputfilename         = false;
    this->found_dt                     = false;
    this->write_nodefiles              = false;
    this->printglobalinfo              = false;
    this->printeconomicinfo            = false;

    this->dt                 = NOT_INIT;
    this->dt_last            = NOT_INIT;  // Last time step in the simulation
    this->stps               = NOT_INIT;
    this->discount_rate      = NOT_INIT;
    this->discount_factor    = NOT_INIT;
    this->nr_nodes           = NOT_INIT;
    this->nr_pstations       = NOT_INIT;
    this->nr_reservoirs      = NOT_INIT;
    this->nr_channels        = NOT_INIT;
    this->nr_pumps           = NOT_INIT;

    for(size_t n = 0; n < MAX_NR_NODES; n++) {
        actions_idnrs[n] = NOT_INIT;
        inflows_idnrs[n] = NOT_INIT;
    } 
    this->n_action_nodes = NOT_INIT;  // Will make crash if not reset proparly, this is intentionaly   :)
    this->n_inflow_nodes = NOT_INIT;  // Will make crash if not reset proparly  :)


}
///////////////////////////////////////////////////////////////////////////////////////////////////////
GlobalConfig::~GlobalConfig(){}
///////////////////////////////////////////////////////////////////////////////////////////////////////
void GlobalConfig::checkNrSteps() {
	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;
    this->stps      = 0;

    myfile.open(this->pricefile);
	if (myfile.is_open()) 	{
        // Do nothing
	} else {
		LOG_ERR("The file " + this->pricefile + " could not be found/opened.");
	}

    getline(myfile, line);
    if( line.length()  > 0 && ( line[0] != '#') ) {
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);
        if ((!keyword.compare("RESTPRICE")) == 0) {
		    LOG_ERR("There is an error in the pricefile " + this->pricefile + " please revisit input");

        }
    }

    getline(myfile, line);
    if( line.length()  > 0 && ( line[0] != '#') ) {
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);
        if ((!keyword.compare("Date")) == 0) {
		    LOG_ERR("There is an error in the pricefile " + this->pricefile + " please revisit input");
        }
    }
    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {
            this->stps++;
        }
    }
    myfile.close();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
void GlobalConfig::SetDirectoriesAndFilenames() {

    topologyfile    = inputdir + topologyfile;
    pricefile       = inputdir + pricefile;
    inflowfile      = inputdir + inflowfile;
    actionsfile     = inputdir + actionsfile;
    start_statefile = inputdir + start_statefile;
    out_statefile   = outputdir + out_statefile;
    outputfile      = outputdir + outputfile;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
void GlobalConfig::DiagnoseTopologyFile() {
	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;

    this->nr_nodes      = 0;
    this->nr_pstations  = 0;
    this->nr_reservoirs = 0; 
    this->nr_channels   = 0;
    this->nr_pumps      = 0;

	myfile.open(this->topologyfile.c_str() );

	if (myfile.is_open()) 	{
        // Do nothing
	} else {
		cout << "Topologyfile " << this->topologyfile << " could not be found/opened. \n";
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__ );
		exit(EXIT_FAILURE);
	}

    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {
            // Line is not empty and doesn't start with # (hash/pound sign)
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);
            if (keyword.compare("NODE") == 0) {
                if(this->nr_nodes >= MAX_NR_NODES) {
                    LOG_ERR("The topology file " + this->topologyfile + " contains more than the maximum supported number of nodes ("
                        + std::to_string(MAX_NR_NODES) + "). If the model indeed contains more nodes, increase MAX_NR_NODES in herss.h and rebuild the software.");
                }
                if (value.compare("RESERVOIR") == 0) {
                    this->nr_reservoirs++;
                    nodetypes[this->nr_nodes] = NodeType::RESERVOIR;
                }
                if (value.compare("PSTATION") == 0) {
                    this->nr_pstations++;
                    nodetypes[this->nr_nodes] = NodeType::PSTATION;
                }
                if (value.compare("CHANNEL") == 0) {
                    this->nr_channels++;
                    nodetypes[this->nr_nodes] = NodeType::CHANNEL;
                }
                if (value.compare("PUMP") == 0) {
                    this->nr_pumps++;
                    nodetypes[this->nr_nodes] = NodeType::PUMP;
                }
                this->nr_nodes++;
            }
        }
    }
    myfile.close();

};
///////////////////////////////////////////////////////////////////////////////////////////////////////
void GlobalConfig::Diagnose() { 

	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;

    this->nr_nodes      = 0;
    this->nr_pstations  = 0;
    this->nr_reservoirs = 0; 
    this->nr_channels   = 0;
    this->nr_pumps      = 0;

    topoparser.loadFile(this->topologyfile);

    this->n_action_nodes_from_topology = 0;

    int hatch_idnr; 

    for (size_t i = 0; i < topoparser.getLineCount(); ++i) {
        line = topoparser.getLine(i);
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);

        if (keyword.compare("NODE") == 0) {
            //Terje Sandø, nr_of_nodes check, August 2026.
            if(this->nr_nodes >= MAX_NR_NODES) {
                LOG_ERR("The topology file " + this->topologyfile + " contains more than the maximum supported number of nodes ("
                    + std::to_string(MAX_NR_NODES) + "). If the model indeed contains more nodes, increase MAX_NR_NODES in herss.h and rebuild the software.");
            }
             if (value.compare("RESERVOIR") == 0) {
                nodetypes[this->nr_nodes] = NodeType::RESERVOIR;
                this->nr_reservoirs++;
            }
            if (value.compare("PSTATION") == 0) {
                nodetypes[this->nr_nodes] = NodeType::PSTATION;
                this->nr_pstations++;
            }
            if (value.compare("CHANNEL") == 0) {
                nodetypes[this->nr_nodes] = NodeType::CHANNEL;
                this->nr_channels++;
            }
            if (value.compare("PUMP") == 0) {
                nodetypes[this->nr_nodes] = NodeType::PUMP;
                this->nr_pumps++;
                // A PUMP has only one action node, and this step counts 
                this->n_action_nodes_from_topology++;
            }
            this->nr_nodes++;
        }
        
        int tmp_n_generators; 
        if(keyword.compare("NR_GENERATORS") == 0) {
            tmp_n_generators = stoi(value);
            if(tmp_n_generators >= 0 &&  tmp_n_generators <= MAX_NR_GENERATORS) {
                 this->n_action_nodes_from_topology += tmp_n_generators;
            }
        }
    }

    // Bug BVM 4 June 2026.
    // We cannot check OUTLET_HATCH in the same loop as we count nr of nodes.
    // This is because we may point a hatch to a node with higher idnr, than nr_nodes
    for (size_t i = 0; i < topoparser.getLineCount(); ++i) {
        line = topoparser.getLine(i);
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);
        if(keyword.compare("OUTLET_HATCH") == 0) {
            // OUTLET_HATCH -9999
            hatch_idnr = stoi(value);
            if(hatch_idnr >= 0 && hatch_idnr <= int(this->nr_nodes) ) {
                this->n_action_nodes_from_topology++;
            }
        }
    }

    if(this->n_action_nodes_from_topology < 1 ) {
        LOG_INFO("There are no action nodes in the topology file " + this->topologyfile);
        LOG_INFO("Number of action nodes in the topology file: " + std::to_string(this->n_action_nodes_from_topology));
        LOG_INFO("This is weird - have you not used NR_GENERATORS ?!");
        LOG_WARN("There are no action nodes in the topology file " + this->topologyfile);
        LOG_WARN("Number of action nodes in the topology file: " + std::to_string(this->n_action_nodes_from_topology));
        LOG_WARN("This is weird - have you not used NR_GENERATORS ?!");
    }

    if (this->nr_nodes <= 0) {
        LOG_ERR("Number of nodes zero or lower !?. See topologyfile " + this->topologyfile + " revisit input");
    }


    //-------------------------------------------------------------------------------
    // Get information about node actions and idnrs 
	myfile.open(this->actionsfile.c_str() );
	if (myfile.is_open()) 	{
        // Do nothing
	} else {
		LOG_ERR("Actionsfile " + this->actionsfile + " could not be found/opened.");
	}

    getline(myfile, line);  // Read first line
    keyword = line_obj.extractNextElementFromLine(&line);

    if ((!keyword.compare("Date_NodeID")) == 0) {
		LOG_ERR("There is an error in the actionsfile file " + this->actionsfile + " please revisit input");
    }
    
    string tmpline = line;
    this->n_action_nodes = line_obj.calcNrCols(&tmpline);

    // Now we read in the idnrs for each coloumn and save it. 
    for(size_t c = 0; c < this->n_action_nodes; c++) {
        value = line_obj.extractNextElementFromLine(&line);
        actions_idnrs[c] = stoi(value);
    }
    myfile.close();
    //-------------------------------------------------------------------------------



    //-------------------------------------------------------------------------------
    // Read header of inflow file and get idnrs. 
	myfile.open(this->inflowfile.c_str() );
	if (myfile.is_open()) 	{
        // Do nothing
	} else {
		LOG_ERR("Inflowfile:  " + this->inflowfile + " could not be found/opened.");
	}

    getline(myfile, line);  // Read first line
    keyword = line_obj.extractNextElementFromLine(&line);

    if ((!keyword.compare("Date_NodeID")) == 0) {
		LOG_ERR("There is an error in the inflowfile file " + this->inflowfile + " please revisit input");
		exit(EXIT_FAILURE);
    }
    
    tmpline = line;
    this->n_inflow_nodes = line_obj.calcNrCols(&tmpline);

    // The number of inflow nodes must match the number of reservoirs in the topology file.
    if(this->n_inflow_nodes != this->nr_reservoirs) {
        LOG_INFO("Number of inflow nodes in the inflowfile " + this->inflowfile + " does not match number of reservoirs in the topology file " + this->topologyfile);
        LOG_INFO("Number of inflow nodes in the inflowfile: " + std::to_string(this->n_inflow_nodes));
        LOG_INFO("Number of reservoirs in the topology file: " + std::to_string(this->nr_reservoirs));
        LOG_INFO("Please revisit input");
        LOG_WARN("Number of inflow nodes in the inflowfile " + this->inflowfile + " does not match number of reservoirs in the topology file " + this->topologyfile);
        LOG_WARN("Number of inflow nodes in the inflowfile: " + std::to_string(this->n_inflow_nodes));
        LOG_WARN("Number of reservoirs in the topology file: " + std::to_string(this->nr_reservoirs));
        LOG_ERR("Please revisit input");
    }

    // Now we read in the idnrs for each coloumn and save it. 
    for(size_t c = 0; c < this->n_inflow_nodes; c++) {
        value = line_obj.extractNextElementFromLine(&line);
        inflows_idnrs[c] = stoi(value);
        // cout << "Checking inflow node idnr " << inflows_idnrs[c] << " against reservoir idnrs in topology file...\n";
    }
    myfile.close();
    //-------------------------------------------------------------------------------


}
///////////////////////////////////////////////////////////////////////////////////////////////////////
void GlobalConfig::readGlobalFile() {

	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;

	myfile.open(globalfile.c_str());

	if (myfile.is_open()) 	{
        // Do nothing
	} else {
        LOG_ERR("The file "  + globalfile + " could not be found/opened.");
	}

    while(!myfile.eof()){
        getline(myfile, line);

        if( line.length() > 0 && (line[0] != '#') &&
            std::any_of(line.begin(), line.end(), ::isalnum) ) {

            // Line is not empty and doesn't start with # (hash/pound sign)
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);

            bool found_ok_keyword = false;

            if (keyword.compare("ACTIONFILE") == 0) {
                this->actionsfile = value;
                this->found_actionsfilename  = true;
                found_ok_keyword = true;
            }

            if (keyword.compare("INFLOWFILE") == 0) {
                this->inflowfile = value;
                this->found_inflowfilename = true;
                found_ok_keyword = true;
            }

            if (keyword.compare("PRICEFILE") == 0) {
                this->pricefile = value;
                this->found_pricefilename = true;
                found_ok_keyword = true;
            }

            if (keyword.compare("TOPOLOGYFILE") == 0) {
                this->topologyfile = value;
                this->found_topologyfilename  = true;
                found_ok_keyword = true;
            }

            if (keyword.compare("PRINT_GLOBAL_INFO") == 0) {
                this->printglobalinfo = (value == "TRUE");
                found_ok_keyword = true;
            }

            if (keyword.compare("PRINT_ECONOMIC_INFO") == 0) {
                this->printeconomicinfo = (value == "TRUE");
                found_ok_keyword = true;
            }

            if (keyword.compare("SYSTEMNAME") == 0) {
                this->systemname = value;
                this->found_systemname = true;                
                found_ok_keyword = true;
            }

            if (keyword.compare("STARTSTATEFILE") == 0) {
                this->start_statefile = value;
                this->found_start_statefilename = true;
                found_ok_keyword = true;
            }

            if (keyword.compare("OUTSTATEFILE") == 0) {
                this->out_statefile = value;
                found_ok_keyword = true;
            }

            if (keyword.compare("DT") == 0) {
                LOG_INFO("------------------------------------------------------------");
                LOG_INFO("HERSS MESSAGE:");
                LOG_INFO("The feature " + keyword + " " + value +
                     " has been deprecated since May 2026. ");
                LOG_INFO("DT is now calculated automatically from the ");
                LOG_INFO("datetimes given in the pricefile\n");
                LOG_INFO("------------------------------------------------------------");
                found_ok_keyword = true;
            }

            if (keyword == "DT_LAST") {
                LOG_INFO("------------------------------------------------------------");
                LOG_INFO("HERSS MESSAGE:");
                LOG_INFO("The feature " + keyword + " " + value + " has been deprecated since May 2026. ");
                LOG_INFO("DT_LAST is now calculated automatically from the datetimes given in the pricefile\n");
                LOG_INFO("DT_LAST is set to the last DT calculated in the last timestep\n");
                LOG_INFO("------------------------------------------------------------");
                found_ok_keyword = true;
            }

            if(keyword.compare("WRITE_NODEFILES") == 0) {
                this->write_nodefiles  = stoi(value);
                found_ok_keyword = true;
            }

            if(keyword.compare("OUTPUTDIR") == 0) {
                this->outputdir = value;
                found_ok_keyword = true;
            }

            if(keyword.compare("INPUTDIR") == 0) {
                this->inputdir = value;
                found_ok_keyword = true;
            }

            if(!found_ok_keyword) {
                LOG_INFO("There is an error in the global configfile " + this->globalfile);
                LOG_INFO("Could not recognize the keyword: " + keyword + " and value: " + value);
                LOG_INFO("Please revisit the global configfile input");

                LOG_WARN("There is an error in the global configfile " + this->globalfile);
                LOG_WARN("Could not recognize the keyword: " + keyword + " and value: " + value);
                LOG_ERR("Please revisit the global configfile input");
            }
        }
    }
    myfile.close();


    /*

	Due to using CPPYY, we cannot use this code here. 
    Move this to somewhere else, maybe in main.cpp after reading the global file.


    if (!found_topologyfilename ) 	{
		LOG_ERR("HERSS could not find the topology filename in " + this->globalfile + " please revisit input\n");
	}
	if (found_topologyfilename && this->topologyfile != DEFAULT_STRING_INIT) {
		std::ifstream topo_file(this->topologyfile);
		if (!topo_file.good()) {
			LOG_ERR("The topology file does not exist: " + this->topologyfile + "\n");
		}
		topo_file.close();
	}


	if (!found_actionsfilename) 	{
		LOG_ERR("HERSS could not find the actions filename in " + this->globalfile + " please revisit input\n");
	}
    if(found_actionsfilename && this->actionsfile != DEFAULT_STRING_INIT) {
        std::ifstream actions_file(this->actionsfile);
        if (!actions_file.good()) {
            LOG_ERR("The actions file does not exist: " + this->actionsfile + "\n");
        }
        actions_file.close();
    }


	if (!found_pricefilename) 	{
		LOG_ERR("HERSS could not find the price filename in " + this->globalfile + " please revisit input\n");
	}
    if(found_pricefilename && this->pricefile != DEFAULT_STRING_INIT) {
        std::ifstream price_file(this->pricefile);
        if (!price_file.good()) {
            LOG_ERR("The price file does not exist: " + this->pricefile + "\n");
        }
        price_file.close();
    }


	if (!found_inflowfilename) 	{
		LOG_ERR("HERSS could not find the inflow filename in " + this->globalfile + " please revisit input\n");
	}
    if(found_inflowfilename && this->inflowfile != DEFAULT_STRING_INIT) {
        std::ifstream inflow_file(this->inflowfile);
        if (!inflow_file.good()) {
            LOG_ERR("The inflow file does not exist: " + this->inflowfile + "\n");
        }
        inflow_file.close();
    }

	if (!found_systemname) 	{
		LOG_ERR("HERSS could not find the system name in " + this->globalfile + " please revisit input\n");
	}


	if (!found_start_statefilename){
		LOG_ERR("HERSS could not find the start state filename in " + this->globalfile + " please revisit input\n");
	}

    
    if(found_start_statefilename && this->start_statefile != DEFAULT_STRING_INIT) {
        std::ifstream start_state_file(this->start_statefile);
        if (!start_state_file.good()) {
            LOG_ERR("The start state file does not exist: " + this->start_statefile + "\n");
        }
        start_state_file.close();
    }

    */



}
///////////////////////////////////////////////////////////////////////////
void GlobalConfig::printGlobalInfo() {
    printf("################################################################\n");
    printf("Global System Information \n");
    printf("SYSTEMNAME          %s\n", this->systemname.c_str() );
    printf("ACTIONFILE          %s\n", this->actionsfile.c_str() );
    printf("INFLOWFILE          %s\n", this->inflowfile.c_str() );
    printf("PRICEFILE           %s\n", this->pricefile.c_str() );
    printf("TOPOLOGYFILE        %s\n", this->topologyfile.c_str() );
    printf("STARTSTATEFILE      %s\n", this->start_statefile.c_str() );
    printf("OUTSTATEFILE        %s\n", this->out_statefile.c_str() );
    printf("NR_NODES            %d\n", int(this->nr_nodes));
    printf("NR_RESERVOIRS       %d\n", int(this->nr_reservoirs));
    printf("NR_CHANNELS         %d\n", int(this->nr_channels));
    printf("NR_PSTATIONS        %d\n", int(this->nr_pstations));
    printf("STPS                %d\n", int(this->stps));
    printf("WRITE_NODEFILES     %d\n", this->write_nodefiles ); 
    printf("OUTPUTDIR           %s\n", this->outputdir.c_str() );
    printf("n_action_nodes = %lu  [ ", n_action_nodes);

    if(n_action_nodes != NOT_INIT) {
        for(size_t n = 0; n < n_action_nodes; n++) {
            printf("%lu ", actions_idnrs[n]);
        }
        printf("]\n");
    } 

    printf("n_inflow_nodes = %lu  [ ", n_inflow_nodes);
    if(n_inflow_nodes) {
        for(size_t n = 0; n < n_inflow_nodes; n++) {
            printf("%lu ", inflows_idnrs[n]);
        }
        printf("]\n");
    }

    printf("###########################################################\n");

}
///////////////////////////////////////////////////////////////////////////////