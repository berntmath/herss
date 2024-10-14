/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     dataset.cpp
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
        printf("Error: memory allocation failed. \n"); 
        printf("file: %s  linenr: %d   function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        exit(EXIT_FAILURE);
    }

    for(size_t t = 0; t < stps;  t++) {
        for(size_t n = 0; n < nr_nodes;  n++) {
            inflow[t][n] = 0.0;  // To make things easyer and faster 
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
        printf("Error: memory allocation failed. \n"); 
        printf("file: %s  linenr: %d   function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        exit(EXIT_FAILURE);
    }
    this->restprice = NOT_INIT;
    for(size_t t = 0; t < stps; t++) {
        price[t] = NOT_INIT;
        year[t]  = NOT_INIT;
        month[t] = NOT_INIT;
        day[t]   = NOT_INIT;
        hour[t]  = NOT_INIT;
    }

    readPricefile();
    readInflowFile();
    readActionsFile();

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
void Dataset::readActionsFile() {

	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;
    int idnrs[MAX_NR_NODES];  // We save the idnrs given in the first line in the inputfile. 

	myfile.open(gc->actionsfile.c_str() );
	if (!myfile.is_open()) 	{
		cout << "The actionsfile " << gc->actionsfile << " could not be found/opened. \n";
        printf("file: %s  linenr: %d   function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		exit(EXIT_FAILURE);
	}

    size_t active_nodes = 0;

    getline(myfile, line);
    if( line.length()  > 0 && ( line[0] != '#') ) {
        
        keyword = line_obj.extractNextElementFromLine(&line);
        if (!keyword.compare("Date_NodeID") == 0) {
		    cout << "There is an error in the actionsfile file " << gc->actionsfile << " please revisit input\n";
            printf("file: %s  linenr: %d   function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		    exit(EXIT_FAILURE);
        }
        string tmpline = line;
        active_nodes = line_obj.calcNrCols(&tmpline);
        // Now we read in the idnrs for each coloumn and save it. 
        for(size_t c = 0; c < active_nodes; c++) {
            value = line_obj.extractNextElementFromLine(&line);
            idnrs[c] = stoi(value);
        }
    }

    // Now we read in each line with date in the first coloumn and then the data
    for(size_t t = 0; t < this->stps; t++) {
        getline(myfile, line);
        keyword = line_obj.extractNextElementFromLine(&line);
        for(size_t c = 0; c < active_nodes; c++) {
            value = line_obj.extractNextElementFromLine(&line);
            action[t][idnrs[c]]  = stof(value);
        }
    }
    myfile.close();
}
/////////////////////////////////////////////////////////////////////////////////////////
void Dataset::readInflowFile() {
	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;
    int idnrs[MAX_NR_NODES];  // We save the idnrs given in the first line in the inputfile. 

	myfile.open(gc->inflowfile.c_str() );
	if (!myfile.is_open()) 	{
		cout << "The inflowfile " << gc->inflowfile << " could not be found/opened. \n";
        printf("file: %s  linenr: %d   function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		exit(EXIT_FAILURE);
	}

    size_t active_nodes = 0;

    getline(myfile, line);
    if( line.length()  > 0 && ( line[0] != '#') ) {
        keyword = line_obj.extractNextElementFromLine(&line);
        if (!keyword.compare("Date_NodeID") == 0) {
		    cout << "There is an error in the inflowseries file " << gc->inflowfile << " please revisit input\n";
            printf("file: %s  linenr: %d   function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		    exit(EXIT_FAILURE);
        }
        string tmpline = line;
        active_nodes = line_obj.calcNrCols(&tmpline);
        // Now we read in the idnrs for each coloumn and save it. 
        for(size_t c = 0; c < active_nodes; c++) {
            value = line_obj.extractNextElementFromLine(&line);
            idnrs[c] = stoi(value);
        }
    }

    // Now we read in each line with date in the first coloumn and then the data
    for(size_t t = 0; t < this->stps; t++) {
        getline(myfile, line);
        keyword = line_obj.extractNextElementFromLine(&line);
        for(size_t c = 0; c < active_nodes; c++) {
            value = line_obj.extractNextElementFromLine(&line);
            inflow[t][idnrs[c]]  = stof(value);
        }
    }
    myfile.close();

}
/////////////////////////////////////////////////////////////////////////////////////////
void Dataset::readPricefile() {

	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;

	myfile.open(gc->pricefile.c_str() );
	if (myfile.is_open()) 	{
        // Do nothing
	} else {
		cout << "The file " << gc->pricefile << " could not be found/opened. \n";
        printf("file: %s  linenr: %d   function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		exit(EXIT_FAILURE);
	}

    getline(myfile, line);
    if( line.length()  > 0 && ( line[0] != '#') ) {
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);
        if (keyword.compare("RESTPRICE") == 0) {
            this->restprice = stof(value);
        } else {
		    cout << "There is an error in the pricefile " << gc->pricefile << " please revisit input\n";
            printf("file: %s  linenr: %d   function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		    exit(EXIT_FAILURE);
        }
    }

    getline(myfile, line);
    if( line.length()  > 0 && ( line[0] != '#') ) {
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);
        if (!keyword.compare("Date") == 0) {
		    cout << "There is an error in the pricefile " << gc->pricefile << " please revisit input\n";
            printf("file: %s  linenr: %d   function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		    exit(EXIT_FAILURE);
        }
    }

    // We now read in the timeseries of price data
    for(size_t t = 0; t < this->stps; t++) {
        getline(myfile, line);
        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);

        // Check date format
        if(keyword.length() != 10) {
		    cout << "ERROR: Date format is not YYYYMMDDHH there is something wrong with pricefile: " << gc->pricefile << ", please revisit input\n";
            printf("file: %s  linenr: %d  function %s \n", __FILE__ , __LINE__, __FUNCTION__ );
		    exit(EXIT_FAILURE);
        }

        year[t]  = stoi(keyword.substr (0,4));
        month[t] = stoi(keyword.substr (4,2));
        day[t]   = stoi(keyword.substr (6,2));
        hour[t]  = stoi(keyword.substr (8,2));
        price[t] = stof(value);
    }
    myfile.close();
}
/////////////////////////////////////////////////////////////////////////////////////////
