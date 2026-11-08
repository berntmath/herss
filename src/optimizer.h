/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     optimizer.h

HERSS-AD: gradient based optimization of the actions of a river system.

  AdamOptimizer    Adam (Kingma and Ba, 2015) for minimization with box bounds. It knows nothing about HERSS.
  HerssObjective   L(x) = -valuefunction_Euro for the actions x of the generators and/or the reservoir hatches, and its gradient
                   dL/dx = -d(VF)/d(action), from the forward pass Simulate and the backward pass Backward.
  RunAdam          The loop: evaluate, update, keep the best iterate, stop on convergence.

We minimize L = -VF. The value function in Euro is reported in the results, so that it is seen to increase.
********************************************************************************/

#pragma once

#include "herss.h"

#include <vector>

enum class ControlKind { GENERATOR, HATCH };

// One optimized variable at time step `t`: the action of generator `gen` of powerstation `node` (idnr), or the hatch
// action of reservoir `node` (idnr, `gen` is not used).
struct Control {
    ControlKind kind;
    size_t node;
    size_t gen;
    size_t t;
};

//////////////////////////////////////////////////////////////////////
// Adam for minimization, with box constraints handled by projection (clipping).
//   m = b1 m + (1 - b1) g,  v = b2 v + (1 - b2) g^2,  m_hat = m / (1 - b1^k),  v_hat = v / (1 - b2^k)
//   x = clip(x - lr m_hat / (sqrt(v_hat) + eps), lo, hi)
class AdamOptimizer {
public:
    double learning_rate = 0.02;
    double beta1         = 0.9;
    double beta2         = 0.999;
    double epsilon       = 1.0e-8;

    void reset(size_t n);                       // Zero the moments and the step counter
    size_t steps() const { return k_; }

    // One update of x (in place) with the gradient g of the function that is minimized.
    void step(std::vector<double> &x, const std::vector<double> &g, double lower, double upper);

private:
    std::vector<double> m_, v_;
    size_t k_ = 0;
};

//////////////////////////////////////////////////////////////////////
class HerssObjective {
public:
    // The simulation must be prepared (Herss::prepaireSimulation) and restprice is read from the dataset.
    explicit HerssObjective(Herss *herss);

    size_t size() const { return controls_.size(); }
    const std::vector<Control> &controls() const { return controls_; }

    std::vector<double> getActions() const;                 // The actions that are in the simulation now
    void setActions(const std::vector<double> &x);

    // Runs the whole pipeline for the actions x. Returns L = -valuefunction_Euro.
    // If grad is not NULL it is filled with dL/dx (the backward pass is skipped when it is NULL).
    double evaluate(const std::vector<double> &x, std::vector<double> *grad);

    double valueFunction() const;                           // valuefunction_Euro of the last evaluation
    size_t evaluations() const { return n_evaluations_; }

private:
    Herss *herss_;
    double restprice_;
    std::vector<Control> controls_;
    size_t n_evaluations_ = 0;
};

//////////////////////////////////////////////////////////////////////
struct AdamOptions {
    size_t max_iterations    = 300;
    double learning_rate     = 0.02;
    double lr_final_fraction = 0.1;     // The learning rate decays with a cosine to this fraction of its start value. 1 = constant
    double lower             = 0.0;     // Bounds of the actions
    double upper             = 1.0;
    double rel_tolerance     = 1.0e-9;  // Stop when the best value has improved less than this (relative) over `patience` iterations
    size_t patience          = 100;
    double grad_tolerance    = 0.0;     // Stop when the norm of the gradient of the free actions is below this. 0 = off
    bool   verbose           = false;
};

struct AdamResult {
    std::vector<double> x;              // The best actions found
    double best_value_Euro = 0.0;       // valuefunction_Euro of x
    double start_value_Euro = 0.0;      // valuefunction_Euro of the start actions
    size_t iterations = 0;
    size_t evaluations = 0;
    std::vector<double> history_Euro;   // valuefunction_Euro of every iterate
    std::vector<double> history_gradient_norm;   // Projected gradient norm at the same iterates as history_Euro
    double projected_gradient_norm = 0.0;   // Norm of the gradient of the free (not clipped) actions at x
};

// Starts from x0 (clipped to the bounds). The simulation is left with the best actions.
AdamResult RunAdam(HerssObjective &objective, std::vector<double> x0, const AdamOptions &options);

// Writes an actions file in the format HERSS reads: the file `in_file` is copied, and the columns of the optimized controls
// (header names node_generator, like 1_0, for generators and the reservoir idnr, like 1, for hatches) get the values x.
// Other columns are kept as they are.
// Returns false (with a message in `error`) if the files cannot be read or written, or a column is missing.
bool WriteOptimizedActionsFile(const HerssObjective &objective, const std::vector<double> &x,
                               const std::string &in_file, const std::string &out_file, std::string &error);

//////////////////////////////////////////////////////////////////////
// Optimizes the actions of the prepared simulation with the settings of the global file:
//   ADAM_OPTIMIZER TRUE         switch it on
//   OPTIMIZE_CONTROLS GENERATORS  which actions: GENERATORS (default), HATCH (reservoir hatches) or ALL
//   LEARNING_RATE 0.02          start learning rate, the actions are in [0,1] (cosine decay to 10 %)
//   MAX_ITERATIONS 300          maximum number of iterations
//   PATIENCE 100                stop after this many iterations without improvement
//   GRAD_TOLERANCE 0            stop when the gradient norm of the free actions is below this, 0 = off
//   START_ACTIONS FILE          FILE: start from the actions file. A number X in [0,1]: every action starts at X
//   OPTIMIZED_ACTIONFILE name   result, written to OUTPUTDIR (default actions_optimized.txt)
//   ADAM_VERBOSE FALSE          TRUE prints the value function of every iteration
// Prints a summary, writes the optimized actions file and leaves the simulation with the best actions, so that the
// normal run that follows (output files, gradients) uses them.
// Writes OUTPUTDIR/adam_history_<system>.txt after optimization: zero-based iteration, current VF [Euro], and
// projected gradient norm before each update. The final reevaluation of the best actions is not a history row.
void RunAdamOptimization(Herss *herss, GlobalConfig *gc);
