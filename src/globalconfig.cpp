/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     globalconfig.cpp
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

GlobalConfig::GlobalConfig(){
    this->globalfile         = STR_NOT_INIT;
    this->topologyfile       = STR_NOT_INIT;
    this->actionsfile        = STR_NOT_INIT;
    this->pricefile          = STR_NOT_INIT;
    this->outputfile         = STR_NOT_INIT;
    this->inflowfile         = STR_NOT_INIT;
    this->systemname         = STR_NOT_INIT;
    this->start_statefile    = STR_NOT_INIT;
    this->out_statefile      = STR_NOT_INIT;
    this->outputdir          = STR_NOT_INIT;
    this->inputdir           = STR_NOT_INIT;

    this->found_topologyfilename       = false;
    this->found_actionsfilename        = false;
    this->found_pricefilename          = false;
    this->found_inflowfilename         = false;
    this->found_systemname             = false;
    this->found_start_statefilename    = false;
    this->found_outputfilename         = false;
    this->found_dt                     = false;
    this->write_nodefiles              = false;

    this->dt                 = NOT_INIT;
    this->stps               = NOT_INIT;
    this->discount_rate      = NOT_INIT;
    this->discount_factor    = NOT_INIT;
    this->nr_nodes           = NOT_INIT;
    this->nr_pstations       = NOT_INIT;
    this->nr_reservoirs      = NOT_INIT;
    this->nr_channels        = NOT_INIT;
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

    char infilename [100];
    sprintf (infilename, "%s%s", this->inputdir.c_str() , this->pricefile.c_str() );

    myfile.open(infilename);
	if (myfile.is_open()) 	{
        // Do nothing
	} else {
		cout << "The file " << infilename << " could not be found/opened. \n";
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__ );
		exit(EXIT_FAILURE);
	}

    getline(myfile, line);
    if( line.length()  > 0 && ( line[0] != '#') ) {
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);
        if (!keyword.compare("RESTPRICE") == 0) {
		    cout << "There is an error in the pricefile " << this->pricefile << " please revisit input\n";
            printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		    exit(EXIT_FAILURE);
        }
    }

    getline(myfile, line);
    if( line.length()  > 0 && ( line[0] != '#') ) {
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);
        if (!keyword.compare("Date") == 0) {
		    cout << "There is an error in the pricefile " << this->pricefile << " please revisit input\n";
            printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		    exit(EXIT_FAILURE);
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
void GlobalConfig::diagnoseTopologyFile() { 
	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;

    this->nr_nodes      = 0;
    this->nr_pstations  = 0;
    this->nr_reservoirs = 0; 
    this->nr_channels   = 0;

	myfile.open(this->topologyfile.c_str() );

	if (myfile.is_open()) 	{
        // Do nothing
	} else {
		cout << "Topologyfile " << this->topologyfile << " could not be found/opened. \n";
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		exit(EXIT_FAILURE);
	}

    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {
            // Line is not empty and doesn't start with # (hash/pound sign)
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);

            if (keyword.compare("NODE") == 0) {
                
                if (value.compare("RESERVOIR") == 0) {
                    this->nr_reservoirs++;
                    nodetypes[this->nr_nodes] = NodeType::RESERVOIR;
                }
                if (value.compare("PSTATION") == 0) {
                    this->nr_pstations++;
                    nodetypes[this->nr_nodes] = NodeType::POWERSTATION;
                }
                if (value.compare("CHANNEL") == 0) {
                    this->nr_channels++;
                    nodetypes[this->nr_nodes] = NodeType::CHANNEL;
                }
                this->nr_nodes++;
            }
        }
    }
    myfile.close();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
void GlobalConfig::readGlobalFile() {
	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;
	myfile.open(globalfile.c_str() );

	if (myfile.is_open()) 	{
        // Do nothing
	} else {
		cout << "The file " << globalfile << " could not be found/opened. \n";
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		exit(EXIT_FAILURE);
	}

    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {
            // Line is not empty and doesn't start with # (hash/pound sign)
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);

            if (keyword.compare("ACTIONFILE") == 0) {
                this->actionsfile = value;
                this->found_actionsfilename  = true;
            }

            if (keyword.compare("INFLOWFILE") == 0) {
                this->inflowfile = value;
                this->found_inflowfilename = true;
            }

            if (keyword.compare("PRICEFILE") == 0) {
                this->pricefile = value;
                this->found_pricefilename = true;
            }

            if (keyword.compare("TOPOLOGYFILE") == 0) {
                this->topologyfile = value;
                this->found_topologyfilename  = true;
            }

            if (keyword.compare("OUTPUTFILE") == 0) {
                this->outputfile = value;
                this->found_outputfilename = true;

            }

            if (keyword.compare("SYSTEMNAME") == 0) {
                this->systemname = value;
                this->found_systemname = true;                
            }

            if (keyword.compare("STARTSTATEFILE") == 0) {
                this->start_statefile = value;
                this->found_start_statefilename = true;
            }

            if (keyword.compare("OUTSTATEFILE") == 0) {
                this->out_statefile = value;
            }

            if (keyword.compare("DT") == 0) {
                this->dt = stoi(value);
                this->found_dt = true;
            }

            if (keyword.compare("WRITE_NODEFILES") == 0) {
                this->write_nodefiles  = stoi(value);
            }

            if (keyword.compare("OUTPUTDIR") == 0) {
                this->outputdir = value;
            }

            if (keyword.compare("INPUTDIR") == 0) {
                this->inputdir = value;
            }
        }
    }
    myfile.close();

	if (!found_topologyfilename ) 	{
		cout << "The global configfile was read but we couldnt find a topolyfilename\n";
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		exit(EXIT_FAILURE);
	}

	if (!found_actionsfilename) 	{
		cout << "The global configfile was read but we couldnt find an actionsfilename \n";
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		exit(EXIT_FAILURE);
	}

	if (!found_pricefilename) 	{
		cout << "The global configfile was read but we couldnt find  pricefilename\n";
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		exit(EXIT_FAILURE);
	}
    
	if (!found_inflowfilename) 	{
		cout << "The global configfile was read but we couldnt find inflowfilename\n";
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		exit(EXIT_FAILURE);
	}

	if (!found_systemname) 	{
		cout << "The global configfile was read but we couldnt find found_systemname\n";
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		exit(EXIT_FAILURE);
	}

	if (!found_start_statefilename){
		cout << "The global configfile was read but we couldnt find start_statefilename\n";
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		exit(EXIT_FAILURE);
	}

	if (!found_outputfilename){
		cout << "The global configfile was read but we couldnt find found_outputfilename\n";
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		exit(EXIT_FAILURE);
	}

	if (!found_dt){
		cout << "The global configfile was read but we couldnt find DT value\n";
        printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
		exit(EXIT_FAILURE);
	}
}
///////////////////////////////////////////////////////////////////////////
void GlobalConfig::printGlobalInfo() {
    printf("ACTIONFILE          %s\n", this->actionsfile.c_str() );
    printf("INFLOWFILE          %s\n", this->inflowfile.c_str() );
    printf("PRICEFILE           %s\n", this->pricefile.c_str() );
    printf("TOPOLOGYFILE        %s\n", this->topologyfile.c_str() );
    printf("OUTPUTFILE          %s\n", this->outputfile.c_str() );
    printf("SYSTEMNAME          %s\n", this->systemname.c_str() );
    printf("STARTSTATEFILE      %s\n", this->start_statefile.c_str() );
    printf("OUTSTATEFILE        %s\n", this->out_statefile.c_str() );
    printf("NR_NODES            %d\n", int(this->nr_nodes));
    printf("NR_RESERVOIRS       %d\n", int(this->nr_reservoirs));
    printf("NR_CHANNELS         %d\n", int(this->nr_channels));
    printf("NR_PSTATIONS        %d\n", int(this->nr_pstations));
    printf("DT                  %d\n", int(this->dt));
    printf("STPS                %d\n", int(this->stps));
    printf("WRITE_NODEFILES     %d\n", this->write_nodefiles ); 
    printf("OUTPUTDIR           %s\n", this->outputdir.c_str() );
}
///////////////////////////////////////////////////////////////////////////////
