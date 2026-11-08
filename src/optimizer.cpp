/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     optimizer.cpp

See optimizer.h.
********************************************************************************/

#include "optimizer.h"
#include "logger.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <cstdlib>
#include <chrono>

//////////////////////////////////////////////////////////////////////
void AdamOptimizer::reset(size_t n) {
    m_.assign(n, 0.0);
    v_.assign(n, 0.0);
    k_ = 0;
}
//////////////////////////////////////////////////////////////////////
void AdamOptimizer::step(std::vector<double> &x, const std::vector<double> &g, double lower, double upper) {

    if(g.size() != x.size()) {
        LOG_ERR("AdamOptimizer::step: x and g have different sizes");
    }
    if(m_.size() != x.size()) {
        reset(x.size());
    }

    ++k_;
    const double c1 = 1.0 - std::pow(beta1, double(k_));
    const double c2 = 1.0 - std::pow(beta2, double(k_));

    for(size_t i = 0; i < x.size(); ++i) {
        m_[i] = beta1 * m_[i] + (1.0 - beta1) * g[i];
        v_[i] = beta2 * v_[i] + (1.0 - beta2) * g[i] * g[i];
        const double m_hat = m_[i] / c1;
        const double v_hat = v_[i] / c2;
        x[i] = std::min(upper, std::max(lower, x[i] - learning_rate * m_hat / (std::sqrt(v_hat) + epsilon)));
    }
}
//////////////////////////////////////////////////////////////////////
HerssObjective::HerssObjective(Herss *herss) : herss_(herss) {

    if(herss_ == NULL || herss_->data == NULL) {
        LOG_ERR("HerssObjective: the simulation must be prepared (Herss::prepaireSimulation) first");
    }
    restprice_ = herss_->data->restprice;

    Riversystem *rs = herss_->rs;
    const std::string &mode = herss_->gc->adam_controls;

    if(mode == "GENERATORS" || mode == "ALL") {
        for(size_t p = 0; p < herss_->gc->nr_pstations; ++p) {
            Powerstation &ps = rs->pstations[p];
            for(size_t g = 0; g < ps.generators.size(); ++g) {
                for(size_t t = 0; t < ps.generators[g].action.size(); ++t) {
                    controls_.push_back({ControlKind::GENERATOR, size_t(ps.idnr), g, t});
                }
            }
        }
    }

    if(mode == "HATCH" || mode == "ALL") {
        for(size_t r = 0; r < herss_->gc->nr_reservoirs; ++r) {
            Reservoir &res = rs->reservoirs[r];
            if(!res.outlet_hatch_in_use) continue;
            for(size_t t = 0; t < herss_->gc->stps; ++t) {
                controls_.push_back({ControlKind::HATCH, size_t(res.idnr), 0, t});
            }
        }
    }
}
//////////////////////////////////////////////////////////////////////
static Powerstation *findStation(Herss *herss, size_t idnr) {
    for(size_t p = 0; p < herss->gc->nr_pstations; ++p) {
        if(size_t(herss->rs->pstations[p].idnr) == idnr) {
            return &herss->rs->pstations[p];
        }
    }
    LOG_ERR("HerssObjective: no powerstation with idnr " + std::to_string(idnr));
    return NULL;
}
//////////////////////////////////////////////////////////////////////
static Reservoir *findReservoir(Herss *herss, size_t idnr) {
    for(size_t r = 0; r < herss->gc->nr_reservoirs; ++r) {
        if(size_t(herss->rs->reservoirs[r].idnr) == idnr) {
            return &herss->rs->reservoirs[r];
        }
    }
    LOG_ERR("HerssObjective: no reservoir with idnr " + std::to_string(idnr));
    return NULL;
}
//////////////////////////////////////////////////////////////////////
static double &actionRef(Herss *herss, const Control &c) {
    if(c.kind == ControlKind::HATCH) {
        return findReservoir(herss, c.node)->S->action[c.t][c.node];
    }
    return findStation(herss, c.node)->generators[c.gen].action[c.t];
}
//////////////////////////////////////////////////////////////////////
static double gradRef(Herss *herss, const Control &c) {
    if(c.kind == ControlKind::HATCH) {
        return findReservoir(herss, c.node)->S->action_grad[c.t];
    }
    return findStation(herss, c.node)->generators[c.gen].action_grad[c.t];
}
//////////////////////////////////////////////////////////////////////
std::vector<double> HerssObjective::getActions() const {
    std::vector<double> x(controls_.size());
    for(size_t i = 0; i < controls_.size(); ++i) {
        x[i] = actionRef(herss_, controls_[i]);
    }
    return x;
}
//////////////////////////////////////////////////////////////////////
void HerssObjective::setActions(const std::vector<double> &x) {
    if(x.size() != controls_.size()) {
        LOG_ERR("HerssObjective::setActions: expected " + std::to_string(controls_.size()) + " values, got " + std::to_string(x.size()));
    }
    for(size_t i = 0; i < controls_.size(); ++i) {
        actionRef(herss_, controls_[i]) = x[i];
    }
}
//////////////////////////////////////////////////////////////////////
// The same sequence as main.cpp. Herss::Simulate resets the reservoirs and channels, and the other nodes are overwritten
// step by step, so the evaluations are independent of each other (checked by the tests).
double HerssObjective::evaluate(const std::vector<double> &x, std::vector<double> *grad) {

    setActions(x);

    herss_->Simulate();
    herss_->CheckWaterBalance();
    herss_->GlobalWaterBalance();
    herss_->CalcAdjustmenCosts();
    herss_->rs->CalcVF(restprice_);
    ++n_evaluations_;

    if(grad != NULL) {
        herss_->rs->Backward(herss_);

        grad->assign(controls_.size(), 0.0);
        for(size_t i = 0; i < controls_.size(); ++i) {
            (*grad)[i] = -gradRef(herss_, controls_[i]);
        }
    }

    return -valueFunction();
}
//////////////////////////////////////////////////////////////////////
double HerssObjective::valueFunction() const {
    return herss_->rs->valuefunction_Euro;
}
//////////////////////////////////////////////////////////////////////
// Norm of the gradient of the free actions. Minimization: an action at its lower bound is free to move only if g < 0,
// at the upper bound only if g > 0.
static double projectedGradientNorm(const std::vector<double> &x, const std::vector<double> &g, double lower, double upper) {
    double sum = 0.0;
    for(size_t i = 0; i < x.size(); ++i) {
        const bool blocked = (x[i] <= lower && g[i] >= 0.0) || (x[i] >= upper && g[i] <= 0.0);
        if(!blocked) sum += g[i] * g[i];
    }
    return std::sqrt(sum);
}
//////////////////////////////////////////////////////////////////////
AdamResult RunAdam(HerssObjective &objective, std::vector<double> x, const AdamOptions &opt) {

    for(double &xi : x) {
        xi = std::min(opt.upper, std::max(opt.lower, xi));
    }

    AdamOptimizer adam;
    adam.beta1 = 0.9;
    adam.beta2 = 0.999;
    adam.reset(x.size());

    AdamResult res;
    res.x = x;
    res.best_value_Euro = -1.0e300;

    std::vector<double> g;
    size_t last_improvement = 0;

    for(size_t k = 0; k < opt.max_iterations; ++k) {

        const double L = objective.evaluate(x, &g);
        const double VF = -L;
        const double grad_norm = projectedGradientNorm(x, g, opt.lower, opt.upper);
        res.history_Euro.push_back(VF);
        res.history_gradient_norm.push_back(grad_norm);
        res.iterations = k + 1;

        if(k == 0) {
            res.start_value_Euro = VF;
        }

        if(k > 0) {
            if(k%100 == 0) {
                printf("iter %4zu   VF = %.6f Euro   best = %.6f\n", k, VF, res.best_value_Euro);
            }
        }
        

        if(VF > res.best_value_Euro) {
            const double gain = VF - res.best_value_Euro;
            if(k == 0 || gain > opt.rel_tolerance * std::max(1.0, std::fabs(res.best_value_Euro))) {
                last_improvement = k;
            }
            res.best_value_Euro = VF;
            res.x = x;
        }

        if(opt.verbose) {
            printf("iter %4zu   VF = %.6f Euro   best = %.6f\n", k, VF, res.best_value_Euro);
        }

        if(k - last_improvement >= opt.patience) {
            break;
        }
        if(opt.grad_tolerance > 0.0 && grad_norm < opt.grad_tolerance) {
            break;
        }

        const double progress = (opt.max_iterations > 1) ? double(k) / double(opt.max_iterations - 1) : 1.0;
        adam.learning_rate = opt.learning_rate
            * (opt.lr_final_fraction + (1.0 - opt.lr_final_fraction) * 0.5 * (1.0 + std::cos(PI * progress)));
        adam.step(x, g, opt.lower, opt.upper);
    }

    // Leave the simulation with the best actions, and report the gradient there.
    objective.evaluate(res.x, &g);
    res.projected_gradient_norm = projectedGradientNorm(res.x, g, opt.lower, opt.upper);
    res.evaluations = objective.evaluations();

    return res;
}
//////////////////////////////////////////////////////////////////////
static std::vector<std::string> splitWhitespace(const std::string &line) {
    std::istringstream is(line);
    std::vector<std::string> tokens;
    std::string tok;
    while(is >> tok) tokens.push_back(tok);
    return tokens;
}
//////////////////////////////////////////////////////////////////////
bool WriteOptimizedActionsFile(const HerssObjective &objective, const std::vector<double> &x,
                               const std::string &in_file, const std::string &out_file, std::string &error) {

    std::ifstream in(in_file);
    if(!in.is_open()) {
        error = "Cannot open the actions file " + in_file;
        return false;
    }

    std::string line;
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;
    while(std::getline(in, line)) {
        std::vector<std::string> tokens = splitWhitespace(line);
        if(tokens.empty()) continue;
        if(header.empty()) header = tokens;
        else rows.push_back(tokens);
    }
    in.close();

    std::map<std::string, size_t> column;
    for(size_t c = 0; c < header.size(); ++c) column[header[c]] = c;

    const std::vector<Control> &controls = objective.controls();
    for(size_t i = 0; i < controls.size(); ++i) {
        const Control &ctl = controls[i];
        const std::string name = (ctl.kind == ControlKind::HATCH) ? std::to_string(ctl.node)
                                                                  : std::to_string(ctl.node) + "_" + std::to_string(ctl.gen);
        if(!column.count(name)) {
            error = "Column " + name + " is not in the actions file " + in_file;
            return false;
        }
        if(ctl.t >= rows.size() || rows[ctl.t].size() != header.size()) {
            error = "The actions file " + in_file + " has no complete row for time step " + std::to_string(ctl.t);
            return false;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%.8f", x[i]);
        rows[ctl.t][column[name]] = buf;
    }

    std::ofstream out(out_file);
    if(!out.is_open()) {
        error = "Cannot write the actions file " + out_file;
        return false;
    }
    for(size_t c = 0; c < header.size(); ++c) out << (c ? "\t" : "") << header[c];
    out << "\n";
    for(const auto &row : rows) {
        for(size_t c = 0; c < row.size(); ++c) out << (c ? "\t" : "") << row[c];
        out << "\n";
    }
    return true;
}
//////////////////////////////////////////////////////////////////////
void RunAdamOptimization(Herss *herss, GlobalConfig *gc) {

    AdamOptions options;
    options.learning_rate  = gc->adam_learning_rate;
    options.max_iterations = gc->adam_max_iterations;
    options.patience       = gc->adam_patience;
    options.grad_tolerance = gc->adam_grad_tolerance;
    options.verbose        = gc->adam_verbose;


    HerssObjective objective(herss);
    if(objective.size() == 0) {
        LOG_ERR("ADAM_OPTIMIZER: no " + gc->adam_controls + " actions to optimize. Check OPTIMIZE_CONTROLS (GENERATORS, HATCH or ALL) in the global file");
    }

    std::vector<double> x0 = gc->adam_start_from_file ? objective.getActions() : std::vector<double>(objective.size(), gc->adam_start_constant);

    printf("HERSS-AD optimization (Adam): %zu %s actions, start = %s, max %zu iterations, learning rate %g\n",
           objective.size(), gc->adam_controls.c_str(), gc->adam_start_from_file ? "actions file" : std::to_string(gc->adam_start_constant).c_str(),
           options.max_iterations, options.learning_rate);

    // The warnings of the simulation would be written again at every iteration.
    const bool was_quiet = Logger::instance().isQuiet();
    Logger::instance().setQuiet(true);
    const auto t0 = std::chrono::steady_clock::now();
    const AdamResult r = RunAdam(objective, x0, options);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    Logger::instance().setQuiet(was_quiet);

    if(r.history_Euro.size() != r.history_gradient_norm.size() || r.history_Euro.size() != r.iterations) {
        LOG_ERR("ADAM_OPTIMIZER: inconsistent optimizer history sizes");
    }
    const std::string history_file = gc->outputdir + "adam_history_" + gc->systemname + ".txt";
    std::ofstream history(history_file);
    if(!history.is_open()) {
        LOG_ERR("Cannot open optimizer history file " + history_file);
    }
    history << std::setprecision(17);
    history << "# iteration valuefunction_Euro projected_gradient_norm\n";
    for(size_t k = 0; k < r.iterations; ++k) {
        history << k << ' ' << r.history_Euro[k] << ' ' << r.history_gradient_norm[k] << '\n';
    }
    history.close();
    if(!history) {
        LOG_ERR("Cannot write optimizer history file " + history_file);
    }
    printf("  optimizer history written to %s\n", history_file.c_str());

    size_t at_lower = 0, at_upper = 0;
    for(double v : r.x) {
        if(v <= options.lower) ++at_lower;
        if(v >= options.upper) ++at_upper;
    }

    printf("  valuefunction_Euro   start %.3f   best %.3f   change %+.3f\n", r.start_value_Euro, r.best_value_Euro, r.best_value_Euro - r.start_value_Euro);
    printf("  iterations %zu, simulations %zu, %.2f s\n", r.iterations, r.evaluations, seconds);
    printf("  gradient norm of the free actions at the best point: %.4g\n", r.projected_gradient_norm);
    printf("  actions at the lower bound: %zu, at the upper bound: %zu, in between: %zu\n", at_lower, at_upper, r.x.size() - at_lower - at_upper);

    const std::string out_file = gc->adam_actions_out;
    std::string error;
    if(!WriteOptimizedActionsFile(objective, r.x, gc->actionsfile, out_file, error)) {
        LOG_ERR("ADAM_OPTIMIZER: " + error);
    }
    printf("  optimized actions written to %s\n", out_file.c_str());
    printf("  The output files of this run are for the optimized actions.\n");
}
