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

        // Changes by Terje Sandø, 02.07.2026
        // Determine whether a period crosses the year boundary (e.g. Oct → Mar).
        // The original AND-logic always failed for winter periods because
        // epoch(Oct) > epoch(Mar) in year 2000, making the AND impossible.
        // Instead, use OR-logic when start_epoch > end_epoch.
        bool crosses_year = (qmin_startdate.getEpoch() > qmin_enddate.getEpoch());
        bool in_period;
        if(!crosses_year)
            in_period = (qmindate.getEpoch() >= qmin_startdate.getEpoch() &&
                         qmindate.getEpoch() <= qmin_enddate.getEpoch());
        else
            in_period = (qmindate.getEpoch() >= qmin_startdate.getEpoch() ||
                         qmindate.getEpoch() <= qmin_enddate.getEpoch());

        if(in_period) {
            *cost = this->timeperiods[p].penalty_cost;
            return this->timeperiods[p].min_discharge;
        }
    }
    return 0.0;
}
////////////////////////////////////////////////////////////////
void Qmin::validatePeriods(string nodename) {
    // Terje Sandø, 03.07.2026, extended 06.07.2026
    // Single, central place for ALL Qmin period validation (format, dates, coverage) -
    // shared by Reservoir OUTLET_AUTO_QMIN (hard physical release) and Channel QMIN
    // (economic penalty). Reservoir::ReadNodeData()/Channel::ReadNodeData() only guard
    // against a substr() crash on very short tokens; every other check (valid month/day
    // ranges, wrong DD.MM format, year-crossing, gaps, overlaps, negative values) is
    // validated here, so there is one place to look and no duplicate checks.
    //
    // Year 2000 is a leap year (29 days in February), consistent with calcQminRequirement.
    const int month_days[12] = {31,29,31,30,31,30,31,31,30,31,30,31};

    // Check 0: valid month/day ranges and non-negative discharge ---
    // Why: Catches typos like month=13, day=32, negative flow, and wrong date format
    //      (e.g. "1.4" instead of "01.04" produces month=0 via substr parsing).
    for(int p = 0; p < nr_periods; p++) {
        // Start month
        if(timeperiods[p].start_month < 1 || timeperiods[p].start_month > 12) {
            LOG_WARN("Qmin period " + std::to_string(p+1) + " has invalid start month ("
                + std::to_string(timeperiods[p].start_month) + ") for node " + nodename + ".");
            LOG_ERR("Warning: Invalid date. Neither the Gregorian calendar, the Roman calendar, nor a Norwegian primstav recognizes this combination. Month must be 1-12. Use zero-padded DD.MM format e.g. 01.04. "
                "Check topology file for node " + nodename + ".");
        }
        // Start day (only safe after month is validated)
        if(timeperiods[p].start_day < 1 ||
           timeperiods[p].start_day > month_days[timeperiods[p].start_month - 1]) {
            LOG_WARN("Qmin period " + std::to_string(p+1) + " has invalid start day ("
                + std::to_string(timeperiods[p].start_day) + ") for month "
                + std::to_string(timeperiods[p].start_month) + " for node " + nodename + ".");
            LOG_ERR("Good luck finding this day in a more recent calendar than the Viking Primstav. Check DD.MM date format in topology file for node " + nodename + ".");
        }
        // End month
        if(timeperiods[p].end_month < 1 || timeperiods[p].end_month > 12) {
            LOG_WARN("Qmin period " + std::to_string(p+1) + " has invalid end month ("
                + std::to_string(timeperiods[p].end_month) + ") for node " + nodename + ".");
            LOG_ERR("Warning: Invalid date. Neither the Gregorian calendar, the Roman calendar, nor a Norwegian primstav recognizes this combination. Month must be 1-12. Use zero-padded DD.MM format e.g. 31.03. "
                "Check topology file for node " + nodename + ".");
        }
        // End day
        if(timeperiods[p].end_day < 1 ||
           timeperiods[p].end_day > month_days[timeperiods[p].end_month - 1]) {
            LOG_WARN("Qmin period " + std::to_string(p+1) + " has invalid end day ("
                + std::to_string(timeperiods[p].end_day) + ") for month "
                + std::to_string(timeperiods[p].end_month) + " for node " + nodename + ".");
            LOG_ERR("Good luck finding this day in a more recent calendar than the Viking Primstav. Check DD.MM date format in topology file for node " + nodename + ".");
        }
        // Discharge must be zero or positive
        if(timeperiods[p].min_discharge < 0.0) {
            LOG_WARN("Qmin period " + std::to_string(p+1) + " has negative discharge ("
                + std::to_string(timeperiods[p].min_discharge) + " m3/s) for node " + nodename + ".");
            LOG_ERR("Discharge must be >= 0.0. Check topology file for node " + nodename + ".");
        }
        // Penalty cost must be zero or positive.
        // Only for Channel QMIN (economic penalty in the value function)
        // OUTLET_AUTO_QMIN always sets this to 0.0, so this check never trips for reservoirs.
        if(timeperiods[p].penalty_cost < 0.0) {
            LOG_WARN("Qmin period " + std::to_string(p+1) + " has negative penalty cost ("
                + std::to_string(timeperiods[p].penalty_cost) + ") for node " + nodename + ".");
            LOG_ERR("Penalty cost must be >= 0.0. Check topology file for node " + nodename + ".");
        }
    }

    // --- Check 1: at most one period may cross the year boundary ---
    //      One winter period (e.g. 01.10 31.03) plus normal summer periods is sufficient.
    int year_crossing_count = 0;
    for(int p = 0; p < nr_periods; p++) {
        DateTime s, e;
        s.setDate(2000, timeperiods[p].start_month, timeperiods[p].start_day, 0,0,0);
        e.setDate(2000, timeperiods[p].end_month,   timeperiods[p].end_day,   0,0,0);
        if(s.getEpoch() > e.getEpoch())
            year_crossing_count++;
    }
    if(year_crossing_count > 1) {
        LOG_WARN("Qmin: " + std::to_string(year_crossing_count)
            + " qmin periods cross the year boundary for node " + nodename + ".");
        LOG_ERR("Only 1 year-crossing period is allowed (e.g. one winter period 01.10 31.03). "
            "Check topology file for node " + nodename + ".");
    }

    // --- Check 2: warn about days with no coverage (qmin defaults to 0.0 m3/s) ---
    // Uncovered days are legal (qmin = 0) but usually indicate a missing period.
    // Simulation continues; use LOG_WARN so the user can decide to fix or ignore.
    bool any_gap = false;
    bool in_gap  = false;
    int  gap_d = 0, gap_m = 0;
    string gaps = "";

    for(int m = 1; m <= 12; m++) {
        for(int d = 1; d <= month_days[m-1]; d++) {
            DateTime check;
            check.setDate(2000, m, d, 0,0,0);
            bool covered = false;
            for(int p = 0; p < nr_periods && !covered; p++) {
                DateTime s, e;
                s.setDate(2000, timeperiods[p].start_month, timeperiods[p].start_day, 0,0,0);
                e.setDate(2000, timeperiods[p].end_month,   timeperiods[p].end_day,   0,0,0);
                bool cy = (s.getEpoch() > e.getEpoch());
                if(!cy)
                    covered = (check.getEpoch() >= s.getEpoch() && check.getEpoch() <= e.getEpoch());
                else
                    covered = (check.getEpoch() >= s.getEpoch() || check.getEpoch() <= e.getEpoch());
            }
            if(!covered && !in_gap) {
                in_gap = true;  gap_d = d;  gap_m = m;  any_gap = true;
            } else if(covered && in_gap) {
                in_gap = false;
                int pd = d - 1, pm = m;
                if(pd == 0) { pm = m - 1;  if(pm == 0) pm = 12;  pd = month_days[pm-1]; }
                gaps += std::to_string(gap_d) + "." + std::to_string(gap_m)
                      + "-" + std::to_string(pd) + "." + std::to_string(pm) + "  ";
            }
        }
    }
    if(in_gap)
        gaps += std::to_string(gap_d) + "." + std::to_string(gap_m) + "-31.12";

    if(any_gap) {
        LOG_WARN("Qmin periods do not cover all days of the year for node " + nodename + ".");
        LOG_WARN("Days without requirement (qmin = 0.0 m3/s): " + gaps);
        LOG_WARN("Simulation continues. Extend periods to cover all 365 days to suppress this warning.");
    }

    // --- Check 3: warn about overlapping periods ---
    //calcQminRequirement returns the FIRST matching period. If period B is completely submerged in period A, period B will never be used.
    bool any_overlap = false;
    string overlap_info = "";
    for(int m = 1; m <= 12 && !any_overlap; m++) {
        for(int d = 1; d <= month_days[m-1] && !any_overlap; d++) {
            DateTime check;
            check.setDate(2000, m, d, 0,0,0);
            int cover_count = 0;
            for(int p = 0; p < nr_periods; p++) {
                DateTime s, e;
                s.setDate(2000, timeperiods[p].start_month, timeperiods[p].start_day, 0,0,0);
                e.setDate(2000, timeperiods[p].end_month,   timeperiods[p].end_day,   0,0,0);
                bool cy = (s.getEpoch() > e.getEpoch());
                bool cov;
                if(!cy)
                    cov = (check.getEpoch() >= s.getEpoch() && check.getEpoch() <= e.getEpoch());
                else
                    cov = (check.getEpoch() >= s.getEpoch() || check.getEpoch() <= e.getEpoch());
                if(cov) cover_count++;
            }
            if(cover_count > 1) {
                any_overlap = true;
                overlap_info = std::to_string(d) + "." + std::to_string(m);
            }
        }
    }
    if(any_overlap) {
        LOG_WARN("Overlapping qmin periods detected for node " + nodename + ".");
        LOG_WARN("Check topology file for node " + nodename
            + ". Even small hatches will not tolerate double shifts, violation may result in mutiny against the supreme architect of the topology.");
    }
}