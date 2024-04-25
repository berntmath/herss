/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     node.cpp
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

Node::Node() {
    idnr                     = NOT_INIT;
    S                        = NULL;
    nodename                 = STR_NOT_INIT;
    downstream_node_in_use   = false;
    outlet_hatch_in_use      = false;
    outlet_tunnel_in_use     = false;
    outlet_overflow_in_use   = false;
    outlet_auto_qmin_in_use  = false;
    auto_qmin                = NOT_INIT;
    start_of_stp_masl        = NOT_INIT;
    end_of_stp_masl          = NOT_INIT;
    qmin_in_use              = false;
    remaining_available_Mm3  = NOT_INIT;
    upstream_remaining_available_Mm3 = 0.0; // To make things easier. 

    ptr_downstream_node           = NULL;
    ptr_downstream_node_tunnel    = NULL;
    ptr_downstream_node_hatch     = NULL;
    ptr_downstream_node_overflow  = NULL;
    ptr_downstream_node_auto_qmin = NULL;
}

Node::~Node() {}

// VIRTUAL FUNCTIONS
int Node::ReadNodeData(string filename)             { return 0; }
int Node::ReadStateFile(string filename)            { return 0; }
int Node::Simulate(size_t t)                        { return 0; }
int Node::initArrayCurves(void)                     { return 0; }
int Node::CheckWaterBalance(void)                   { return 0; }
double Node::GetStartWater_Mm3(void)                { return 0; }
double Node::GetEndWater_Mm3(void)                  { return 0; } 
int Node::WriteNodeOutput(GlobalConfig *gc )        { return 0; }
double Node::GetTunnelFLow(size_t t)                { return 0; }
int Node::WriteStateFile(FILE *fp)                  { return 0; }

