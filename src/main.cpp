/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     main.cpp
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

BVM May 20206 
********************************************************************************/

#include "herss.h"
#include "logger.h"

#include <stdio.h>
#include <sys/time.h>


//////////////////////////////////////////////////////
int main(int argc, char *argv[]) {

    // Tracking start date and time of the program.
    Xtime xtime_start;

    string xtime_start_str;
    xtime_start.getNowString(xtime_start_str);


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
    
    char logfilename[256];
    snprintf(logfilename, sizeof(logfilename), "herss_%s_%s.log", VERSION.c_str() , VERSION_DATE.c_str());

    #ifndef HERSS_NO_LOG
        if (!Logger::instance().init(logfilename)) {
            std::cerr << "Could not open log file " << logfilename << "\n";
            std::exit(EXIT_FAILURE);
        }
    #endif

    std::chrono::steady_clock::time_point chrono_start;
    chrono_start = std::chrono::steady_clock::now();

    LOG_INFO("#################################################################");
    LOG_INFO("# The Hydraulic Economic River System Simulator (HERSS)");
    LOG_INFO("# VERSION: " + VERSION);
    LOG_INFO("# VERSION_DATE: " + VERSION_DATE);
    LOG_INFO("#################################################################");

    // Note that we dont use newline at the end of the log messages, because the logger adds it. 
    // We can write all logs to screen if needed. 
    // I turned it off in logger.h. Too much flushing of the screen.
    
    LOG_INFO("HERSS started at: " + xtime_start_str);
    LOG_MSG("HERSS started at: " + xtime_start_str);
    LOG_MSG("Starting HERSS with global config file: " + string(argv[1]) );
    LOG_MSG("VERSION: " + VERSION );
    LOG_MSG("VERSION_DATE: " + VERSION_DATE);

    gc->globalfile     = string(argv[1]);
    
    gc->readGlobalFile();
    gc->SetDirectoriesAndFilenames();
    gc->Diagnose();  // Reads topology file into parser 

    gc->checkNrSteps();  // This can be voided if you want to set stps manually before allocation of objects

    if(gc->printglobalinfo) {
        gc->printGlobalInfo();
    }

    // gc->printGlobalInfo();


    Dataset *data;
    data = new Dataset(gc);
    data->readAllData(); 

    Herss *herss;
    herss = new Herss(gc);

    herss->prepaireSimulation(data);


    // Check that basic variables are initialized and sett within aceptable limits. 
    herss->rs->DiagnoseRiversystemConfiguration();

    LOG_MSG("Initialisation looks good. Starting simulation..... ");
    herss->Simulate();
    herss->CheckWaterBalance();
    herss->GlobalWaterBalance();
    
    herss->CalcAdjustmenCosts();

    herss->rs->CalcVF(data->restprice);

    if(gc->printeconomicinfo) {
        herss->rs->PrintEconomicInfo(herss);
    }

    //herss->rs->PrintReservoirData2Screen();
    herss->rs->CalcSimulationProfit();
    //herss->PrintAllInput(); Uncomment to print all input data

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


    // Starttime and runtime of HERSS 
    std::chrono::steady_clock::time_point chrono_end;
    chrono_end = std::chrono::steady_clock::now();

    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(chrono_end - chrono_start).count();

    
    LOG_INFO("Program runtime (s): " + std::to_string(elapsedMs / 1000.0));
    LOG_INFO("Program runtime (min): " + std::to_string(elapsedMs / 60000.0));
    // LOG_INFO("Program runtime (hours): " + std::to_string(elapsedMs / 3600000.0));

    // Terje Sandø, 06.07.2026
    // Warnings only go to the log file, not the screen. 
    // Now the user get a notice to check the log file.
    size_t n_warnings = Logger::instance().getWarningCount();
    if(n_warnings > 0) {
        string warning_word = (n_warnings == 1) ? " warning was" : " warnings were";
        LOG_INFO("NOTE: " + std::to_string(n_warnings) + warning_word + " logged during this run. Check " + string(logfilename) + " for details.");
    }

    LOG_INFO("HERSS ended normally - bye!");
    
    
    LOG_MSG("Program runtime (s): " + std::to_string(elapsedMs / 1000.0));
    LOG_MSG("Program runtime (min): " + std::to_string(elapsedMs / 60000.0));
    // LOG_MSG("Program runtime (hours): " + std::to_string(elapsedMs / 3600000.0));
    LOG_MSG("HERSS ended normally - bye!");

    return 0;
}