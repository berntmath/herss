/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     smoothing.h
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

Smooth, differentiable replacements for hard max/min. Kept in a separate header so that
the routing classes, which are included by herss.h, can use them too.
********************************************************************************/

#pragma once

constexpr double EPS_REL_FLOW = 1.0e-3;   // [-] relative blend width for capped flows (0.1 % of requested flow)

// Never below max(0,x); can be positive up to ε below 0. Use for penalties and "added" quantities.
inline double relu_hi(double x, double eps) {
    if (x <= -eps) return 0.0;
    if (x >=  eps) return x;
    double u = x + eps;  return u*u/(4.0*eps);
}
inline double relu_hi_d(double x, double eps) {
    if (x <= -eps) return 0.0;
    if (x >=  eps) return 1.0;
    return (x + eps)/(2.0*eps);
}
// Never above max(0,x); exactly 0 for x<=0. Use for "available water" and released flows.
inline double relu_lo(double x, double eps)   { return relu_hi(x - eps, eps); }
inline double relu_lo_d(double x, double eps) { return relu_hi_d(x - eps, eps); }

// smooth min/max built from the above
inline double smin(double a, double b, double e) { return a - relu_hi(a - b, e); }   // <= min(a,b): never over-releases
inline double smax(double a, double b, double e) { return a + relu_hi(b - a, e); }   // >= max(a,b)



inline double smin_nn(double a, double b, double d) {
    if (a <= 0.0) return 0.0;
    return a - relu_hi(a - b, d*a);
}
inline void smin_nn_grad(double a, double b, double d, double &dfa, double &dfb) {
    if (a <= 0.0)          { dfa = 1.0; dfb = 0.0; return; }
    double x = a - b, e = d*a;
    if (x <= -e)           { dfa = 1.0; dfb = 0.0; }
    else if (x >= e)       { dfa = 0.0; dfb = 1.0; }
    else {
        double u = (1.0 + d)*a - b;
        dfa = 1.0 - u*((1.0 + d)*a + b)/(4.0*d*a*a);
        dfb = u/(2.0*d*a);
    }
}
