/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     qmin.cpp
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

Qmin::Qmin(){}
Qmin::~Qmin(){}

double Qmin::calcQminRequirement(int year, int month, int day, double *cost) {

    DateTime qmin_startdate;
    DateTime qmin_enddate;
    DateTime qmindate;

    for(int p = 0; p < this->nr_periods; p++) {
        qmin_startdate.setDate(2000, this->timeperiods[p].start_month, this->timeperiods[p].start_day, 0,0,0);
        qmin_enddate.setDate(2000, this->timeperiods[p].end_month, this->timeperiods[p].end_day, 0,0,0);
        qmindate.setDate(2000, month, day, 0,0,0);

        if(  qmindate.getEpoch() >=  qmin_startdate.getEpoch() &&   qmindate.getEpoch() <= qmin_enddate.getEpoch()) {
            *cost = this->timeperiods[p].penalty_cost;
            return this->timeperiods[p].min_discharge;
        }
    }
    return 0.0;
}
