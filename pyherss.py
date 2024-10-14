#!/usr/bin/python3
# -*- coding: utf-8 -*-


"""
 * Project:       HERSS
 * Filename:      pyherss.py
 * Developers:    Bernt Viggo Matheussen
 * Original date: 14 October, 2024
 *
 * Demonstrates how to use CPPYY to run the C++ herss model.  
 *
"""

import sys
import numpy as np
import time
import os
import cppyy

cppyy.load_library("./src/herss.so")
cppyy.include("./src/herss.h")

#---------------------------------------------------
# MAIN 
#---------------------------------------------------
if __name__ == "__main__":

    # We need to set the inputdir and outputdir inside python code
    herss_inputdir="./mini_utahps/"
    
    gc            = cppyy.gbl.GlobalConfig()    
    gc.globalfile = herss_inputdir + "global.txt";
    gc.readGlobalFile()
    
    gc.inputdir = herss_inputdir
    gc.outputdir = herss_inputdir + "output/"
    gc.SetDirectoriesAndFilenames()
    gc.Diagnose()
    gc.checkNrSteps()
    data = cppyy.gbl.Dataset(gc)
    herss = cppyy.gbl.Herss(gc)
    herss.prepaireSimulation(data)
    herss.Simulate()
    print("Water Value (price) at end of planning horizon = ", data.restprice)

    vf = herss.rs.CalcVF(data.restprice)

    print("ValueFunction = ", vf)

    n_reservoirs = herss.gc.nr_reservoirs
    print("n_reservoirs = ", n_reservoirs)
    print("Nr of nodes in the riversystem = ", herss.nr_nodes)
    print("Timesteps in planning horizon = ", herss.stps)

    # Get the price, inflow, initial reservoir levels, ending reservoir levels, actions, from HERSS or the DATA class. 
    print("Price in timestep 3 (from herss) = ", herss.GetPrice(2) )
    print("Price in timestep 3 (from data)  = ", data.price[2] )

    # Node zero [0] is a reservoir,
    t = 3
    node_idnr = 0
    print("Inflow to node 0 (reservoir) in timestep 4 = ", herss.GetInflowInNode(t, node_idnr) )
    print("Initial reservoir level in node 0  = ", herss.GetReservoir_Init_fr(0)  )
    print("Simulated reservoir level at end of timestep 4 = ", herss.GetReservoirLevel_fr(node_idnr, t) )
    print("Action in Powerstation (node_idnr 1), at timestep t   = ", herss.GetAction(1, t) )

    # Set new price and restprice 
    new_price = 99.9
    restprice = data.restprice + 5.0
    t = 1
    herss.SetPrice(t , new_price, restprice)

    # Setting Action in Powerstation (node_idnr = 1) 
    herss.SetAction(1, t, 0.75)

    # Set new inflow in reservoir at t  [m3/s]
    herss.SetInflowInNode(t, 0, 3.76)
    herss.SetReservoir_Init_fr(0, 0.33)
    
    # Now we can simulate again with the altered Price, Inflow, etc. 
    # Note that we dont need to calll herss.prepaireSimulation(data)
    # This is taken care of within herss.Simulate()
    herss.Simulate()
    vf = herss.rs.CalcVF(restprice)

    print("New ValueFunction = ", vf)
    print("THE-END")
#---------------------------------------------------
