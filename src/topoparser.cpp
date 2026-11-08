/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     dataset.cpp
Developers:   Bernt Viggo Matheussen
Organization: Å Energi, www.ae.no

This software is released under the MIT license.

********************************************************************************/


#include "herss.h"
#include <fstream>
#include <sstream>
#include <iostream>


string TopologyParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return ""; // String is all whitespace
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}


bool TopologyParser::loadFile(const std::string& filename) {

    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {

        std::string trimmed = trim(line);
        if (!trimmed.empty() && trimmed[0] != '#') {
            lines.push_back(trimmed);
        }

    }
    file.close();
    return true;
}
//--------------------------------------------------------------------------------

