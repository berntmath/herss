/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     manningchannel.h
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

class ManningChannel
{
public:

    ManningChannel() = default;

    ManningChannel(
        int    numReaches,
        double totalLength,
        double bottomWidth,
        double sideSlope,
        double bedSlope,
        double manningN)
    {
        Configure(
            numReaches,
            totalLength,
            bottomWidth,
            sideSlope,
            bedSlope,
            manningN);
    }

    void Configure(
        int    numReaches,
        double totalLength,
        double bottomWidth,
        double sideSlope,
        double bedSlope,
        double manningN)
    {
        m_numReaches = numReaches;
        m_totalLength = totalLength;
        m_bottomWidth = bottomWidth;
        m_sideSlope = sideSlope;   
        m_slope = bedSlope;
        m_manningN = manningN;

        m_volume.assign(numReaches, 0.0);
    }


    // sideSlope,    z = 1.5 means for every 1.0 m of vertical rise, 
    // the wall moves 1.5 m horizontally outward. 
    // often written "1.5:1 H:V"

    //MANNING_SIDE_SLOPE	Shape
    // 0	Rectangular, with vertical walls
    // 1	45° banks
    // 1.5	Moderately sloped banks
    // 3+	Gentle, wide, shallow banks



    // Handbook of hydrology, page 10.25. 
    // Mountain streams n = [0.04 - 0.07]  


    // Channel type                                 Manning n
    // Smooth straight earth channel                0.020 - 0.025
    // Natural stream, clean, straight              0.025 - 0.035
    // Natural stream with stones and pools         0.035 - 0.050
    // Mountain stream with cobbles/boulders        0.050 - 0.080
    // Dense vegetation                             0.070 - 0.150+

    // Channel type	                        Bed slope 
    // Lowland river, near-flat valley      0.00005 – 0.0005
    // Typical lowland/agricultural stream  0.0005 – 0.002
    // Foothill stream                      0.002 – 0.01
    // Mountain stream                      0.01 – 0.04
    // Steep torrent/gully                  0.04+



    //----------------------------------------------------
    // Trapezoid geometry helpers.
    // sideSlope = 0 gives the rectangular special case.
    //----------------------------------------------------

    double AreaToDepth(double area) const
    {
        double depth;
        if (m_sideSlope > 0.0) {
            depth = (-m_bottomWidth + std::sqrt(
                m_bottomWidth * m_bottomWidth + 4.0 * m_sideSlope * area))
                / (2.0 * m_sideSlope);
        } else {
            depth = area / m_bottomWidth;
        }
        return std::max(depth, 0.01);
    }

    double DepthToArea(double depth) const
    {
        return depth * (m_bottomWidth + m_sideSlope * depth);
    }

    double WettedPerimeter(double depth) const
    {
        return m_bottomWidth + 2.0 * depth * std::sqrt(1.0 + m_sideSlope * m_sideSlope);
    }

    //----------------------------------------------------
    // Initialize at steady-state flow
    //----------------------------------------------------

    void SetInitialFlow(
        double Q)
    {
        double reachLength =
            m_totalLength / m_numReaches;

        double depth =
            SolveDepth(Q);

        double area =
            DepthToArea(depth);

        double velocity =
            Q / area;

        double travelTime =
            reachLength / velocity;

        double volume =
            Q * travelTime;

        for(auto& v : m_volume)
        {
            v = volume;
        }
    }

    //----------------------------------------------------
    // Main routing routine
    //----------------------------------------------------

    double Route(
        double Qin,
        double dtSeconds)
    {
        double flowIn = Qin;

        double reachLength =
            m_totalLength /
            m_numReaches;

        for(int i=0;i<m_numReaches;i++)
        {
            //----------------------------------------
            // Volume -> area -> depth (trapezoid)
            //----------------------------------------

            double area = m_volume[i] / reachLength;

            double depth = AreaToDepth(area);

            area = DepthToArea(depth); // recompute after depth clamp

            //----------------------------------------
            // Geometry
            //----------------------------------------

            double perimeter = WettedPerimeter(depth);

            double hydraulicRadius = area / perimeter;

            //----------------------------------------
            // Manning velocity
            //----------------------------------------

            double velocity =
                (1.0 / m_manningN)
                *
                std::pow(
                    hydraulicRadius,
                    2.0/3.0)
                *
                std::sqrt(
                    m_slope);

            //----------------------------------------
            // Outflow
            //----------------------------------------

            double flowOut =
                area *
                velocity;

            //----------------------------------------
            // Mass balance
            //----------------------------------------

            m_volume[i] +=
                (flowIn - flowOut)
                * dtSeconds;

            //----------------------------------------
            // Cannot become negative
            //----------------------------------------

            if(m_volume[i] < 0.0)
            {
                // m_volume[i] is already updated, so the deficit reduces the computed outflow.
                flowOut =
                    flowOut +
                    m_volume[i] /
                    dtSeconds;

                m_volume[i] = 0.0;

                flowOut =
                    std::max(
                        0.0,
                        flowOut);
            }

            //----------------------------------------
            // Feed next reach
            //----------------------------------------

            flowIn = flowOut;
        }

        return flowIn;
    }

    double GetStoredVolume() const
    {
        double sum = 0.0;

        for(double v : m_volume)
        {
            sum += v;
        }

        return sum;
    }

    // Per-reach storage [m3], in reach order, for state-file persistence/hot-start.
    std::vector<double> GetReachStorage() const
    {
        return m_volume;
    }

    void SetReachStorage(const std::vector<double>& storage)
    {
        if (static_cast<int>(storage.size()) != m_numReaches)
            throw std::invalid_argument("SetReachStorage: size mismatch with numReaches");
        m_volume = storage;
    }

private:

    double SolveDepth(
        double Q) const
    {
        double depth = 1.0;

        for(int iter=0; iter<30; iter++)
        {
            double area =
                DepthToArea(depth);

            double perimeter =
                WettedPerimeter(depth);

            double radius =
                area /
                perimeter;

            double qCalc =
                area
                *
                (1.0 / m_manningN)
                *
                std::pow(
                    radius,
                    2.0/3.0)
                *
                std::sqrt(
                    m_slope);

            double ratio =
                Q /
                std::max(
                    qCalc,
                    1e-6);

            depth *=
                std::sqrt(
                    ratio);
        }

        return depth;
    }

private:

    int m_numReaches = 1;

    double m_totalLength = 1.0;
    double m_bottomWidth = 1.0;
    double m_sideSlope = 0.0;  // horizontal run per unit vertical rise; 0 = rectangular
    double m_slope = 0.001;    // channel bed slope
    double m_manningN = 0.04;

    std::vector<double> m_volume;
};