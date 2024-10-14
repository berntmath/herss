/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     arraycurve.cpp                                                        
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

#include "arraycurve.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <herss.h>

ArrayCurve::ArrayCurve(){}
ArrayCurve::~ArrayCurve(){}


//////////////////////////////////////////////////////////////////
double ArrayCurve::initializeArrays() {
    xmin = ymin =  99999999.9;
    xmax = ymax = -999999999.9;

    for(int i = 0; i < nr_pts; i++) {

        if( x_points[i] > xmax) {
            xmax = x_points[i];
        }

        if( x_points[i] < xmin) {
            xmin = x_points[i];
        }

        if( y_points[i] > ymax) {
            ymax = y_points[i];
        }

        if( y_points[i] < ymin) {
            ymin = y_points[i];
        }
    }

    // Now we normalize both axis [0,1]
    for(int i = 0; i < nr_pts; i++) {
        x_points[i]  = (x_points[i] - xmin) / (xmax - xmin);
        y_points[i]  = (y_points[i] - ymin) / (ymax - ymin);
    }

    xlower[0] = x_points[0];
    ylower[0] = y_points[0];
    xupper[0] = x_points[1];
    yupper[0] = y_points[1];

    int idx_points = 0;
    double dx = (x_points[nr_pts-1] - x_points[0])/double(POINTS_IN_ARRAY);
    double x;
    for(int t = 1; t < POINTS_IN_ARRAY; t++) {
        x = x_points[0] + double(t)*dx;
        if(x >= x_points[idx_points+1]){
            idx_points++;
        }
        xlower[t] = x_points[idx_points];
        ylower[t] = y_points[idx_points];
        xupper[t] = x_points[idx_points+1];
        yupper[t] = y_points[idx_points+1];
    }
    return 0.0;
}
///////////////////////////////////////////////////////////////////////////
//
// BUG: Fix later.
// When the flow is at maximum. We are at the upper end of the efficiency curves.
// The current method have a numerical problem with it.
// The quick solution is to make the curves go to a tiny fraction above max flow.
// We have to look at this later
double ArrayCurve::x2y(double x) {

    double xt = x + 0.0;

    xt = (x-xmin)/(xmax-xmin);

    if(xt > 1.0 || xt < 0.0) {
        printf("ERROR with normalization   [0,1]\n");
        printf("xt=%.8f\n", xt );
        for(int i = 0; i < nr_pts; i++) {
            printf("%d x_points[i]=%.5f  y_points[i]=%.5f\n", i, x_points[i], y_points[i]);
        }
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		exit(EXIT_FAILURE);
    }

    int idx;
    idx = int(  0.5 +  ( xt - x_points[0]) / (x_points[nr_pts-1] - x_points[0]) * double(POINTS_IN_ARRAY)) ;

    if(xt < x_points[0] || xt > x_points[nr_pts-1]) {
        printf("HOUSTON - we have a problem!\n");
        printf("x=%.3f\n", xt);
        printf("idx = %d\n", idx);
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
		exit(EXIT_FAILURE);
    }

    double y;
    double slope;
    slope = (yupper[idx] - ylower[idx]) / ( xupper[idx] - xlower[idx] );
    y = slope * (xt- xlower[idx]) +  ylower[idx];

    // De-Normalize
    y = y * (ymax-ymin) + ymin;
    return y;
}
///////////////////////////////////////////////////////////////////////////
