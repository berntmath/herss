/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     main.cpp
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
#include <stdio.h>
#include <sys/time.h>

//////////////////////////////////////////////////////
int main(int argc, char *argv[]) {

    GlobalConfig *gc;
    gc     = new GlobalConfig();

    if (argc != 2) {
        cout << "#################################################################\n";
        cout << "# The Hydraulic Economic River System Simulator (HERSS)\n";
        cout << "# VERSION: " << VERSION << endl;
        cout << "# VERSION_DATE: " << VERSION_DATE << endl;
        cout << "# Not correct number of commandline arguments\n";
        cout << "# USAGE:  herss.exe globalconfigfile.txt \n";
        cout << "#################################################################\n";
        exit(EXIT_FAILURE);
    }

    gc->globalfile     = string(argv[1]);
    gc->readGlobalFile();
    gc->SetDirectoriesAndFilenames();
    gc->Diagnose();
    gc->checkNrSteps();  // This can be voided if you want to set stps manually before allocation of objects
    gc->printGlobalInfo();

    Dataset *data;
    data = new Dataset(gc);

    Herss *herss;
    herss = new Herss(gc);
    herss->prepaireSimulation(data);
    herss->Simulate();
    herss->CheckWaterBalance();
    herss->GlobalWaterBalance(data);
    herss->CalcAdjustmenCosts();
    printf("ValueFunction = %.5f\n", herss->rs->CalcVF(data->restprice));
    
    // Now we need to write output to files
    herss->rs->WriteRiverSystemData(data->restprice);
    herss->rs->WriteReservoirData();
    herss->WriteStateFile();

    if(gc->write_nodefiles) {
        herss->WriteNodeOutput();
    }  


    delete herss;
    delete data;
    delete gc;

    printf("THE-END\n");

    return 0;
}
