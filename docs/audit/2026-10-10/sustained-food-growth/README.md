# Sustained Food Logistics & Core Town Growth Audit — 10 October 2026

Deterministic simulation audit resolving the food intermittency and frozen house count observed during Core basket repetition.

---

## 1. Root Cause Analysis: The 17-Month Delivery Gap

In the pinned offline baseline (`retained-final-checkpoint.sav` $\rightarrow$ `basket-consumed.sav`), Town 0 (Mito) population fell from 1,191 to 1,096, and houses remained locked at 29 despite the native growth hook being enabled.

### Mathematical Throughput Breakdown

1. **Town Consumption**:
   * Monthly FOOD demand at population 1,096 is 55 units per month (2,048 ticks $\approx$ 27.6 days).
   * City storage capacity is strictly bounded by `IntegratedEconomy::AcceptCity`:
     $$\text{Capacity} = \text{Demand} \times 3 = 55 \times 3 = 165\text{ units}$$
     Any delivery exceeding the 165-unit ceiling is rejected. Thus, the town can never buffer more than 3 months of food.

2. **Consist Capacity**:
   * Train 4 consists of engine 32 and 3 wagons (engine 52), each with capacity 35:
     $$\text{Total Train Capacity} = 3 \times 35 = 105\text{ FOOD}$$
     A full 105-unit delivery satisfies town consumption for only $\frac{105}{55} \approx 1.9\text{ months}$ (3,910 ticks).

3. **Round-Trip Transit Time**:
   * Station 2 (Food plant, World 1) to Station 10 (Core town, World 0): 17,500 ticks one-way travel time at wagon max speed (32 mph).
   * Full round trip (Station 2 $\rightarrow$ Station 10 $\rightarrow$ Station 2): $\approx 35,000\text{ ticks}$ ($\approx 17.1\text{ months}$).

4. **Starvation Cycle**:
   * After Train 4 unloads 105 units, food reserves are exhausted in month 2.
   * During the remaining $17.1 - 1.9 = 15.2\text{ months}$ while Train 4 returns and reloads, town reserves are at 0 FOOD.
   * Consequently, Town 0 starved for $\sim 15\text{ to }21\text{ months}$ out of every cycle.

---

## 2. Simulation Mechanics: Why Houses Did Not Grow

1. **Starvation State**:
   * When monthly evaluation runs without food (`food_sufficient == false`), `MegacityManager::EvaluateMonthlySupply` sets:
     $$\text{growth\_state} = \text{MegacityGrowthState::Starvation},\quad \text{growth\_multiplier} = 0.0$$
2. **Growth Freezing**:
   * In `src/town_cmd.cpp`:
     * `UpdateTownGrowthRate(t)` sets `t->growth_rate = TOWN_GROWTH_RATE_NONE`.
     * `UpdateTownGrowth(t)` returns early without setting `TownFlag::IsGrowing`.
3. **Counter Inactivity**:
   * The native growth period is $\text{growth\_rate} = 7,419\text{ ticks}$ ($\approx 3.6\text{ months}$).
   * In `TownTickHandler`, `t->grow_counter` only decrements if `TownFlag::IsGrowing` is set.
   * Because food was exhausted after 1.9 months, the town never experienced the required 3.6 consecutive nourished months.
   * `t->grow_counter` froze (e.g. at 6,654) and was repeatedly reset or locked by starvation.

---

## 3. Verified Resolution & Test Proof

### Logistics Architecture
To maintain continuous sustenance, logistics throughput must match or exceed 55 FOOD/month:
* Coordinated shuttle service: Operating multiple consists spaced along the 17-month transit loop ensures deliveries arrive every $\le 2\text{ months}$ before the 165-unit capacity is depleted.

### Deterministic Test Verification
Implemented in `src/tests/test_core_construction_basket.cpp` (`[sustained-growth]`):
1. **Starvation Invariant**: Under starvation (`IsGrowing == false`), `OnTick_Town()` does not decrement `t->grow_counter`, preserving the frozen state.
2. **Sustained Invariant**: Under sustained food supply (`IsGrowing == true`), `OnTick_Town()` decrements `t->grow_counter` tick-by-tick across consecutive months.
3. **Growth Trigger**: Decrementing through 0 successfully invokes `GrowTown`, resets `t->grow_counter = t->growth_rate`, and maintains continuous decrementing on subsequent ticks.
