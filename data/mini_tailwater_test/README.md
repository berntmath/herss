# mini_tailwater_test

Test dataset for `TAILWATER_IDNR`. `PUMP` (0) pumps water from `Reservoir_B` (3) up to `Reservoir_A` (1) in
time step 0, and `HYDRO_POWERSTATION_A` (2) releases the same water back down in time step 1. There is no
inflow and the hatch is closed. `POWSTAT_MASL` is 500, the LRW of `Reservoir_B`, which starts at its HRW (540).

The pump, motor, turbine and generator efficiencies give 0.90 × 0.98 × 0.92 × 0.98 = 0.7952, so the power
station should produce at most 0.7952 of the energy the pump used.

- `TAILWATER_IDNR -9` (old behaviour): Hbrutto goes down to `POWSTAT_MASL`, 60.6 m against the pump's 20.6 m,
  and the power station produces 2.34 times the energy the pump used. This is wrong: water that is pumped up
  and released again can never give back more energy than the pump used.
- `TAILWATER_IDNR 3` (as shipped): Hbrutto goes down to the water surface elevation of `Reservoir_B`, as for
  the pump, and the power station produces 0.7952 of the energy the pump used.

## Run

    cd data/mini_tailwater_test

    # 1. old behaviour (TAILWATER_IDNR -9)
    sed -i 's/^TAILWATER_IDNR 3/TAILWATER_IDNR -9/' topology.txt
    ../../src/herss.exe global.txt

    awk 'FNR==NR && FNR==4 {used=$6} FNR!=NR && $1==2026 && $4==1 {produced=$12} END {printf "energy used by the pump: %.6f MWh, energy produced by the power station: %.4f MWh, produced divided by used: %.4f\n", used, produced, produced/used}' output/node0_PUMP.txt output/node2_HYDRO_POWERSTATION_A.txt

    # 2. as shipped (TAILWATER_IDNR 3)
    sed -i 's/^TAILWATER_IDNR -9/TAILWATER_IDNR 3/' topology.txt
    ../../src/herss.exe global.txt

    awk 'FNR==NR && FNR==4 {used=$6} FNR!=NR && $1==2026 && $4==1 {produced=$12} END {printf "energy used by the pump: %.6f MWh, energy produced by the power station: %.4f MWh, produced divided by used: %.4f\n", used, produced, produced/used}' output/node0_PUMP.txt output/node2_HYDRO_POWERSTATION_A.txt

Expected: `2.3387`, then `0.7952`. The `awk` line divides `Power` at hour 1 in
`node2_HYDRO_POWERSTATION_A.txt` by `Power_MW` at hour 0 in `node0_PUMP.txt`. After step 2 the folder is
as shipped again.
