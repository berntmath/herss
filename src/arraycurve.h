/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     arraycurve.h                                                        
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

#ifndef __ARRAYCURVE_h__
#define __ARRAYCURVE_h__

#define POINTS_IN_ARRAY 1000
//---------------------------------------------
// The idea here is that we want to make a super fast calculation of Y from a curve
// defined by pairs of (x,y).
// We already have a method to do this with PointCurve, but a profiling of the HERSS code showed
// that more than 30 % of the calculations were spent doing interpolation on the point curves.
// This class tries to model the same thing using static curves.
// We do this by defining an upper and a lower array.
// We discretisize the x axes into small steps. For each of these we point to the lower and upper (nearest values of Y and X. )
// The trick is to normalize both axis to values between zero and one.
// WORK IN PROGRESS, BVM Feb 2024. 

class ArrayCurve {

public:
	ArrayCurve();
	~ArrayCurve();

	double xmin, xmax;
	double ymin, ymax;
	double x_points[POINTS_IN_ARRAY];  // We copy over the data from the OVERFLOW CURVE, etc.
	double y_points[POINTS_IN_ARRAY];
	int nr_pts;  // Number of points used in the array
	double xupper[POINTS_IN_ARRAY];
	double xlower[POINTS_IN_ARRAY];
	double yupper[POINTS_IN_ARRAY];
	double ylower[POINTS_IN_ARRAY];
	double initializeArrays();
	double x2y(double x);  // We use this to get y from x for the curve that was used to initialize.
};

#endif
