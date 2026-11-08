/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     naturalchannel.h
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

BVM Sep 2026 
********************************************************************************/


#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include "smoothing.h"


// Routing method C: a cascade of nonlinear reservoirs, Qout = a * S^p per reach.
//
// Per reach, with S the storage at the start of the step and Qin the inflow:
//     Qout  = smin_nn(a S^p, S / dt, EPS_REL_FLOW)      smooth version of min(a S^p, S / dt)
//     S_new = S + (Qin - Qout) dt
// The smooth minimum never exceeds either argument, so a reach never releases more than it holds and the
// storage stays non-negative. It equals a S^p exactly unless S / dt is within EPS_REL_FLOW of it.
// The nonlinearity p must be >= 1. For p < 1 the travel time would grow with the flow, which no river does,
// and d(Qout)/dS = a p S^(p-1) would be infinite for an empty reach.
class NaturalChannel
{
public:

    NaturalChannel(
        int    numReaches_,
        double referenceFlow_,
        double referenceTravelTimeHours_,
        double nonlinearity_)
        :
        m_numReaches(numReaches_),
        m_referenceFlow(referenceFlow_),
        m_referenceTravelTimeHours(referenceTravelTimeHours_),
        m_nonlinearity(nonlinearity_),
        m_storage(numReaches_, 0.0)
    {
        Initialise();
    }


    void SetParameters(
        int numReaches,
        double referenceFlow,
        double referenceTravelTime,
        double nonlinearity)
    {
        m_numReaches = numReaches;
        m_referenceFlow = referenceFlow;
        m_referenceTravelTimeHours = referenceTravelTime;
        m_nonlinearity = nonlinearity;
        Initialise();
    }

    void ResetToReferenceState()
    {
        Initialise();
    }

    // Per-reach storage [m3], in reach order, for state-file persistence/hot-start.
    std::vector<double> GetReachStorage() const
    {
        return m_storage;
    }

    void SetReachStorage(const std::vector<double>& storage)
    {
        if (storage.size() != static_cast<std::size_t>(m_numReaches))
            throw std::invalid_argument("SetReachStorage: size mismatch with numReaches");
        m_storage = storage;
    }

    //---------------------------------------------------------------------
    // Route one timestep.
    //
    // Qin       : inflow [m3/s]
    // dtSeconds : timestep [s]
    //
    // Returns:
    //     Qout [m3/s]
    //---------------------------------------------------------------------

    // If storageBefore is not null, the storage of every reach at the start of the step is written to
    // storageBefore[0 .. numReaches-1]. RouteBackward needs them, since the routing is nonlinear.
    double Route(double Qin, double dtSeconds, double *storageBefore = nullptr) {
        double flow = Qin;

        for (int i = 0; i < m_numReaches; i++)
        {
            const double S = m_storage[i];
            if (storageBefore != nullptr) storageBefore[i] = S;

            // Smooth cap: a reach cannot release more than it holds
            const double Qout = ReachOutflow(S, dtSeconds);

            // Mass balance. Qout <= S / dt, so the storage cannot become negative. The max only removes rounding errors.
            m_storage[i] = std::max(0.0, S + (flow - Qout) * dtSeconds);

            flow = Qout;
        }
        return flow;
    }

    //---------------------------------------------------------------------
    // Adjoint of Route() for one time step. x_grad means d(ValueFunction)/d(x).
    //
    // storageBefore : the storages recorded by Route() for this step
    // qOutGrad      : d(VF)/d(outflow of the last reach)
    // storageGrad   : on entry d(VF)/d(storage of each reach at the end of the step),
    //                 on exit the same for the start of the step (resized and zeroed if its size is wrong)
    // qInGrad       : d(VF)/d(inflow of the first reach)
    //
    // The outflow of a reach depends on its storage at the start of the step only, not on its inflow.
    // The rounding guard (max(0, .)) in Route() has no gradient.
    //---------------------------------------------------------------------
    void RouteBackward(const double *storageBefore, double qOutGrad, double dtSeconds,
                       std::vector<double> &storageGrad, double &qInGrad) const
    {
        if (storageGrad.size() != static_cast<std::size_t>(m_numReaches))
            storageGrad.assign(static_cast<std::size_t>(m_numReaches), 0.0);

        // Inflow of reach i is the outflow of reach i-1, and an inflow q adds q dt to the storage of the reach
        // that receives it, so d(VF)/d(inflow of reach i) = dt * d(VF)/d(end storage of reach i).
        double qOutGradDownstream = qOutGrad;   // d(VF)/d(flow leaving reach i) through the next reach and the outside
        for (int i = m_numReaches - 1; i >= 0; i--)
        {
            const double endStorageGrad = storageGrad[i];
            const double qGradTotal = qOutGradDownstream - dtSeconds * endStorageGrad;

            storageGrad[i] = endStorageGrad + qGradTotal * ReachOutflowDerivative(storageBefore[i], dtSeconds);
            qOutGradDownstream = dtSeconds * endStorageGrad;    // This is the inflow of reach i, the outflow of reach i-1
        }
        qInGrad = qOutGradDownstream;
    }
    //---------------------------------------------------------------------
    // Total water currently stored in channel
    //---------------------------------------------------------------------

    double GetStoredVolume() const  {
        double sum = 0.0;

        for (double v : m_storage)
            sum += v;

        return sum;
    }

private:

    // Outflow of one reach [m3/s] for the storage S [m3] at the start of the step
    double ReachOutflow(double S, double dtSeconds) const {
        return smin_nn(m_a * std::pow(S, m_nonlinearity), S / dtSeconds, EPS_REL_FLOW);
    }

    // d(Qout)/dS
    double ReachOutflowDerivative(double S, double dtSeconds) const {
        if (S <= 0.0) {
            // smin_nn is homogeneous of degree one, so Qout = S * smin_nn(a S^(p-1), 1/dt) and the slope of an
            // empty reach is 0 for p > 1 and smin_nn(a, 1/dt) for p = 1.
            return m_nonlinearity > 1.0 ? 0.0 : smin_nn(m_a, 1.0 / dtSeconds, EPS_REL_FLOW);
        }
        double dfa = 0.0, dfb = 0.0;
        smin_nn_grad(m_a * std::pow(S, m_nonlinearity), S / dtSeconds, EPS_REL_FLOW, dfa, dfb);
        return dfa * m_a * m_nonlinearity * std::pow(S, m_nonlinearity - 1.0) + dfb / dtSeconds;
    }

    static void ValidateParameters(int numReaches, double nonlinearity) {
        if (numReaches <= 0)
            throw std::invalid_argument("numReaches must be greater than zero");
        if (!(nonlinearity >= 1.0))
            throw std::invalid_argument("nonlinearity must be >= 1 (1 is a linear reservoir)");
    }

    void Initialise() {
        ValidateParameters(m_numReaches, m_nonlinearity);

        m_storage.resize(static_cast<std::size_t>(m_numReaches));

        constexpr double secondsPerHour = 3600.0;

        double totalStorageAtReference = m_referenceFlow * m_referenceTravelTimeHours * secondsPerHour;

        double storagePerReach = totalStorageAtReference / m_numReaches;

        m_a = m_referenceFlow / std::pow(storagePerReach, m_nonlinearity);

        std::fill(m_storage.begin(), m_storage.end(), storagePerReach);
    }


private:

    int m_numReaches;
    double m_referenceFlow;
    double m_referenceTravelTimeHours;
    double m_nonlinearity;

    double m_a;

    std::vector<double> m_storage;
};
