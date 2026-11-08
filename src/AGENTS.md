# HERSS Agent Guidelines

HERSS-AD is implemented directly in HERSS as a new feature.
The objective is to evolve HERSS into HERSS-AD through incremental changes.
So we want HERSS to use automatic differentiation in the core classes.



Core Principles
---------------
- Preserve the existing HERSS architecture.
- Preserve Reservoir, Powerstation, Channel, Intake and Pump classes.
- Preserve the existing DAG topology.
- Preserve the existing multi-temporal simulation framework.
- Preserve the existing Scenario class design.
- Use double precision throughout HERSS-AD.

Important
---------
- Do not introduce Enzyme dependencies.
- Do not suggest rewrites to plain C versions.
- Do not redesign HERSS.
- Keep the existing object-oriented architecture.
- Extend existing classes.

Primary AD Design
-----------------
Forward pass:
    Simulate(size_t t)

Backward pass:
    Backward(size_t t)

Gradient naming convention:
    variable_grad

Examples:
    profit_grad
    Power_grad
    res_Mm3_grad
    action_grad

Definition:
    x_grad means:

        d(ValueFunction) / d(x)

Preferred changes
-----------------
- Add Backward()
- Add *_grad variables
- Smooth existing equations
- Add gradient propagation incrementally
- Keep forward simulation behaviour unchanged

Avoid
-----
- New simulator architectures
- PyTorch rewrites
- Duplicate node hierarchies
- Parallel DHERSS implementations
- Introducing a second simulator codebase

Current HERSS-AD Roadmap
------------------------
1. Add Backward() to all node types.
2. Add *_grad arrays to Scenario.
3. Implement Riversystem::SeedGradients().
4. Implement Riversystem::Backward().
5. Implement profit -> Power gradients.
6. Implement Power -> action gradients.
7. Implement reservoir state gradients.
8. Implement backpropagation through time.
9. Validate gradients against finite differences.
10. Integrate with Python via CPPYY.

Long-term Vision
----------------
HERSS-AD remains a C++ simulator.

Python may be used through CPPYY for:
- Optimization
- Visualization
- Neural-network value functions

The neural network should consume HERSS outputs and terminal states.
The neural network should not replace HERSS.
