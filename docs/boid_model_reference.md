# Boid Flocking Model — Formal Mathematical Reference

This document formally defines two canonical boid flocking models from the literature and our hybrid implementation used in the COMP6216 pandemic simulation. It includes mathematical derivations of key behavioral differences and a frame-rate independence analysis.

---

## 1. Model A — "Simple Boids" (V. Hunter Adams / Cornell ECE)

**Source:** [V. Hunter Adams, Boids Algorithm](https://vanhunteradams.com/Pico/Animal_Movement/Boids-algorithm.html), based on [Cornell ECE4760](https://people.ece.cornell.edu/land/courses/ece4760/labs/s2021/Boids/Boids.html).

This model applies forces **directly to velocity** with no steering abstraction and no force clamp. It operates in **frame-based units** (no $\Delta t$ multiplier).

### 1.1 Definitions

Let boid $i$ have position $\mathbf{p}_i$ and velocity $\mathbf{v}_i$. Define:
- $r_p$ — protected range (separation radius)
- $r_v$ — visual range (alignment/cohesion radius)
- $N_p(i) = \{j \neq i : \|\mathbf{p}_i - \mathbf{p}_j\| < r_p\}$ — neighbors in protected range
- $N_v(i) = \{j \neq i : \|\mathbf{p}_i - \mathbf{p}_j\| < r_v\}$ — neighbors in visual range

### 1.2 Separation

$$\mathbf{f}_{\text{sep}} = \sum_{j \in N_p(i)} (\mathbf{p}_i - \mathbf{p}_j)$$

Applied as:

$$\mathbf{v}_i \mathrel{+}= \mathbf{f}_{\text{sep}} \cdot w_{\text{avoid}}$$

where $w_{\text{avoid}}$ is the avoidance weight (e.g., 0.05).

**Note:** This is raw positional-difference accumulation. No normalization, no averaging. Each neighbor contributes a vector whose magnitude equals the Euclidean distance $d_{ij} = \|\mathbf{p}_i - \mathbf{p}_j\|$ (see Section 4 for implications).

### 1.3 Alignment

$$\bar{\mathbf{v}} = \frac{1}{|N_v(i)|} \sum_{j \in N_v(i)} \mathbf{v}_j$$

$$\mathbf{v}_i \mathrel{+}= (\bar{\mathbf{v}} - \mathbf{v}_i) \cdot w_{\text{match}}$$

where $w_{\text{match}}$ is the matching factor (e.g., 0.05).

### 1.4 Cohesion

$$\mathbf{c} = \frac{1}{|N_v(i)|} \sum_{j \in N_v(i)} \mathbf{p}_j$$

$$\mathbf{v}_i \mathrel{+}= (\mathbf{c} - \mathbf{p}_i) \cdot w_{\text{center}}$$

where $w_{\text{center}}$ is the centering factor (e.g., 0.0005).

### 1.5 Update Rule (Frame-Based)

$$\mathbf{v}_i \leftarrow \text{clamp}(\mathbf{v}_i,\; v_{\min},\; v_{\max})$$
$$\mathbf{p}_i \leftarrow \mathbf{p}_i + \mathbf{v}_i$$

No $\Delta t$. One step = one frame. Speed is clamped to $[v_{\min}, v_{\max}]$, but there is **no force clamp**.

### 1.6 Reference Parameters

| Parameter | Symbol | Value | Unit |
|-----------|--------|-------|------|
| Protected range | $r_p$ | 8 | px |
| Visual range | $r_v$ | 40 | px |
| Avoid factor | $w_{\text{avoid}}$ | 0.05 | — |
| Matching factor | $w_{\text{match}}$ | 0.05 | — |
| Centering factor | $w_{\text{center}}$ | 0.0005 | — |
| Max speed | $v_{\max}$ | 6 | px/frame |
| Min speed | $v_{\min}$ | 3 | px/frame |

---

## 2. Model B — "Reynolds Steering" (Shiffman / Processing.org / Reynolds GDC'99)

**Sources:** [Processing.org Flocking](https://processing.org/examples/flocking.html), [Reynolds GDC'99](https://www.red3d.com/cwr/steer/gdc99/), [Reynolds 1987](https://www.red3d.com/cwr/boids/).

This model uses the **Reynolds steering paradigm**: each behavior computes a *desired velocity*, and the steering force is the error between desired and current velocity, individually truncated to $f_{\max}$.

### 2.1 Definitions

Same neighbor sets as Model A, plus:
- $v_{\max}$ — maximum speed
- $f_{\max}$ — maximum steering force (per behavior)
- $\text{truncate}(\mathbf{x}, m) = \mathbf{x} \cdot \min\!\left(1,\; \frac{m}{\|\mathbf{x}\|}\right)$ — clamp vector magnitude to $m$

### 2.2 Separation (Inverse-Distance Weighted)

$$\mathbf{s} = \frac{1}{|N_p(i)|} \sum_{j \in N_p(i)} \frac{\hat{\mathbf{d}}_{ij}}{d_{ij}}$$

where $\hat{\mathbf{d}}_{ij} = \frac{\mathbf{p}_i - \mathbf{p}_j}{\|\mathbf{p}_i - \mathbf{p}_j\|}$ is the unit direction away from neighbor $j$, and $d_{ij} = \|\mathbf{p}_i - \mathbf{p}_j\|$.

Then:

$$\mathbf{f}_{\text{sep}} = \text{truncate}\!\left(\frac{\hat{\mathbf{s}}}{\|\mathbf{s}\|} \cdot v_{\max} - \mathbf{v}_i,\; f_{\max}\right)$$

**Note:** The $1/d_{ij}$ weighting means closer neighbors produce stronger repulsion (see Section 4).

### 2.3 Alignment

$$\bar{\mathbf{v}} = \frac{1}{|N_v(i)|} \sum_{j \in N_v(i)} \mathbf{v}_j$$

$$\mathbf{f}_{\text{ali}} = \text{truncate}\!\left(\frac{\bar{\mathbf{v}}}{\|\bar{\mathbf{v}}\|} \cdot v_{\max} - \mathbf{v}_i,\; f_{\max}\right)$$

### 2.4 Cohesion

$$\mathbf{c} = \frac{1}{|N_v(i)|} \sum_{j \in N_v(i)} \mathbf{p}_j$$

$$\mathbf{f}_{\text{coh}} = \text{truncate}\!\left(\frac{\mathbf{c} - \mathbf{p}_i}{\|\mathbf{c} - \mathbf{p}_i\|} \cdot v_{\max} - \mathbf{v}_i,\; f_{\max}\right)$$

### 2.5 Update Rule (Frame-Based)

Each force is individually truncated, then weighted and summed:

$$\mathbf{a} = w_{\text{sep}} \cdot \mathbf{f}_{\text{sep}} + w_{\text{ali}} \cdot \mathbf{f}_{\text{ali}} + w_{\text{coh}} \cdot \mathbf{f}_{\text{coh}}$$

$$\mathbf{v}_i \leftarrow \text{truncate}(\mathbf{v}_i + \mathbf{a},\; v_{\max})$$
$$\mathbf{p}_i \leftarrow \mathbf{p}_i + \mathbf{v}_i$$

From Reynolds' GDC'99 formulation:
```
steering_force = truncate(steering_direction, max_force)
acceleration = steering_force / mass
velocity = truncate(velocity + acceleration, max_speed)
position = position + velocity
```

No $\Delta t$. Frame-based, like Model A.

### 2.6 Reference Parameters (Processing.org)

| Parameter | Symbol | Value | Unit |
|-----------|--------|-------|------|
| Separation radius | $r_p$ | 25 | px |
| Alignment radius | $r_v$ | 50 | px |
| Cohesion radius | $r_v$ | 50 | px |
| Max speed | $v_{\max}$ | 2 | px/frame |
| Max force | $f_{\max}$ | 0.03 | px/frame |
| Separation weight | $w_{\text{sep}}$ | 1.5 | — |
| Alignment weight | $w_{\text{ali}}$ | 1.0 | — |
| Cohesion weight | $w_{\text{coh}}$ | 1.0 | — |

**Key ratio:** $f_{\max} / v_{\max} = 0.03 / 2 = 0.015 = 1.5\%$ per frame.

---

## 3. Our Hybrid Model (COMP6216)

Our implementation combines elements of both models with **frame-rate independent** Euler integration.

### 3.1 Separation (Model A — Raw Accumulation, Cross-Swarm)

$$\mathbf{f}_{\text{sep}} = \sum_{j \in N_p(i)} (\mathbf{p}_i - \mathbf{p}_j)$$

Applies to **all** nearby boids regardless of swarm type (prevents collisions across swarms).

### 3.2 Alignment (Model B — Reynolds Steering, Same-Swarm Only)

Let $S(i) = \{j \in N_v(i) : \text{swarm}(j) = \text{swarm}(i)\}$ be the same-swarm neighbors within alignment radius.

$$\bar{\mathbf{v}} = \frac{1}{|S(i)|} \sum_{j \in S(i)} \mathbf{v}_j$$

$$\mathbf{f}_{\text{ali}} = \left(\frac{\bar{\mathbf{v}}}{\|\bar{\mathbf{v}}\|} \cdot v_{\max} - \mathbf{v}_i\right) \cdot w_{\text{ali}}$$

### 3.3 Cohesion (Model B — Reynolds Steering, Same-Swarm Only)

$$\mathbf{c} = \frac{1}{|S(i)|} \sum_{j \in S(i)} \mathbf{p}_j$$

$$\mathbf{f}_{\text{coh}} = \left(\frac{\mathbf{c} - \mathbf{p}_i}{\|\mathbf{c} - \mathbf{p}_i\|} \cdot v_{\max} - \mathbf{v}_i\right) \cdot w_{\text{coh}}$$

### 3.4 Force Accumulation and Clamping

Forces are accumulated first, then the **total** is clamped (not per-behavior):

$$\mathbf{F} = \mathbf{f}_{\text{sep}} \cdot w_{\text{sep}} + \mathbf{f}_{\text{ali}} + \mathbf{f}_{\text{coh}}$$

$$\mathbf{F} \leftarrow \text{truncate}(\mathbf{F},\; F_{\max})$$

**Note:** In Model B, each force is individually truncated to $f_{\max}$ before summation. In our model, only the total is clamped. This means a large separation force can completely suppress alignment and cohesion after clamping.

### 3.5 Update Rule (Frame-Rate Independent)

$$\mathbf{v}_i \leftarrow \text{truncate}(\mathbf{v}_i + \mathbf{F} \cdot \Delta t,\; v_{\max})$$
$$\mathbf{p}_i \leftarrow \mathbf{p}_i + \mathbf{v}_i \cdot \Delta t$$

where $\Delta t$ is the real time elapsed since the last frame (seconds). This is forward Euler integration.

A minimum speed $v_{\min}$ is also enforced: if $\|\mathbf{v}_i\| < v_{\min}$, velocity is rescaled to $v_{\min}$.

### 3.6 Parameters

| Parameter | Symbol | Old Default | New Default | Unit |
|-----------|--------|-------------|-------------|------|
| Max speed | $v_{\max}$ | 180.0 | 180.0 | px/s |
| **Max force** | $F_{\max}$ | **6.0** | **180.0** | px/s$^2$ |
| Min speed | $v_{\min}$ | 54.0 | 54.0 | px/s |
| **Separation radius** | $r_p$ | **25.0** | **12.0** | px |
| Alignment radius | $r_v$ | 50.0 | 50.0 | px |
| Cohesion radius | $r_v$ | 50.0 | 50.0 | px |
| Separation weight | $w_{\text{sep}}$ | 1.5 | 1.5 | — |
| Alignment weight | $w_{\text{ali}}$ | 1.0 | 1.0 | — |
| Cohesion weight | $w_{\text{coh}}$ | 1.0 | 1.0 | — |

---

## 4. Derivation: Separation Force Asymmetry

This section proves that Model A and Model B produce **opposite** distance-scaling behaviors for separation.

### 4.1 Model A — Force Magnitude Proportional to Distance

Consider boid $i$ at the origin and a single neighbor $j$ at position $\mathbf{p}_j$ with $\|\mathbf{p}_j\| = d < r_p$.

The separation contribution from $j$ is:

$$\mathbf{f}_j = \mathbf{p}_i - \mathbf{p}_j = -\mathbf{p}_j$$

$$\|\mathbf{f}_j\| = \|\mathbf{p}_j\| = d$$

Therefore:

$$\boxed{\|\mathbf{f}_j\|_{\text{Model A}} = d}$$

**The force magnitude increases linearly with distance.** A neighbor at the edge of the protected range ($d \approx r_p$) contributes a force $r_p$ times stronger than a neighbor at distance 1.

For $n$ neighbors at equal distances $d$, the accumulated force magnitude is $n \cdot d$.

### 4.2 Model B — Force Magnitude Proportional to Inverse Distance

In Shiffman's separation, each neighbor contributes:

$$\mathbf{f}_j = \frac{\hat{\mathbf{d}}_{ij}}{d_{ij}} = \frac{\mathbf{p}_i - \mathbf{p}_j}{d_{ij}^2}$$

$$\|\mathbf{f}_j\| = \frac{1}{d_{ij}}$$

Therefore:

$$\boxed{\|\mathbf{f}_j\|_{\text{Model B}} = \frac{1}{d}}$$

**The force magnitude decreases with distance.** A neighbor at distance 1 contributes a force $r_p$ times stronger than a neighbor at the edge of the protected range.

### 4.3 Comparison and Implications

| Property | Model A (Adams) | Model B (Shiffman) |
|----------|:---:|:---:|
| Force per neighbor at distance $d$ | $d$ | $1/d$ |
| Closer neighbor → | Weaker push | Stronger push |
| Farther neighbor (within zone) → | Stronger push | Weaker push |
| Intuitive? | Counterintuitive | Intuitive |

**Why Model A still works:** In Adams' implementation, the protected range $r_p = 8$ is very small (20% of visual range $r_v = 40$). Within such a small zone, all neighbors are approximately equidistant, so the asymmetry has minimal practical effect. The accumulation count $n$ provides the primary scaling — more crowded = stronger repulsion.

**Why this matters for our model:** Our separation radius (originally 25, now 12) is larger relative to the alignment radius than Adams' protected range. At $r_p = 25$ with $r_v = 50$, boids at the far edge of the separation zone contribute forces 25x stronger than near-collision boids. Reducing to $r_p = 12$ limits the maximum single-neighbor contribution and restores the count-driven scaling behavior that Model A relies upon.

### 4.4 Formal Ratio

The force ratio between a neighbor at the zone edge ($d = r_p$) and a near-collision neighbor ($d = \epsilon$):

$$\text{Model A:} \quad \frac{\|\mathbf{f}(r_p)\|}{\|\mathbf{f}(\epsilon)\|} = \frac{r_p}{\epsilon} \gg 1$$

$$\text{Model B:} \quad \frac{\|\mathbf{f}(r_p)\|}{\|\mathbf{f}(\epsilon)\|} = \frac{\epsilon}{r_p} \ll 1$$

These are exact inverses. Model A is edge-dominated; Model B is collision-dominated.

---

## 5. Frame-Rate Independence Analysis

### 5.1 Reynolds' Original Model is Frame-Rate Dependent

Reynolds' GDC'99 update step:

$$\mathbf{v} \leftarrow \text{truncate}(\mathbf{v} + \mathbf{a},\; v_{\max})$$
$$\mathbf{p} \leftarrow \mathbf{p} + \mathbf{v}$$

At frame rate $\text{fps}$, each frame advances by $\Delta t = 1/\text{fps}$ real seconds.

**Position change per second:**

$$\frac{\Delta \mathbf{p}}{\Delta t_{\text{real}}} = \mathbf{v} \cdot \text{fps}$$

If $\mathbf{v}$ is constant at $v_{\max}$ px/frame:
- At 60 fps: $180 \cdot 60 = 10{,}800$ px/s... wait, that's wrong.

Actually, $v_{\max} = 3$ px/frame in Adams' model. At 60 fps: $3 \times 60 = 180$ px/s. At 30 fps: $3 \times 30 = 90$ px/s.

$$\boxed{\text{Real speed} = v_{\max}^{\text{frame}} \cdot \text{fps}}$$

**Boids move twice as fast at 60fps vs 30fps.** This is the fundamental limitation of frame-dependent models.

**Velocity change per second (turning rate):**

$$\frac{\Delta \mathbf{v}}{\Delta t_{\text{real}}} = \mathbf{a} \cdot \text{fps}$$

At 60 fps: $0.1 \times 60 = 6.0$ px/frame/s. At 30 fps: $0.1 \times 30 = 3.0$ px/frame/s.

**Boids turn twice as fast at 60fps vs 30fps.**

### 5.2 Our Model is Frame-Rate Independent

Our update step:

$$\mathbf{v} \leftarrow \text{truncate}(\mathbf{v} + \mathbf{F} \cdot \Delta t,\; v_{\max})$$
$$\mathbf{p} \leftarrow \mathbf{p} + \mathbf{v} \cdot \Delta t$$

**Position change per second:**

At any frame rate: $\frac{\Delta \mathbf{p}}{1\text{s}} = \mathbf{v}$ (in px/s). The $\cdot \Delta t$ in the position update compensates for fewer frames.

- At 60 fps: $\Delta \mathbf{p}/\text{frame} = 180 \cdot (1/60) = 3$ px/frame $\times$ 60 frames = 180 px/s
- At 30 fps: $\Delta \mathbf{p}/\text{frame} = 180 \cdot (1/30) = 6$ px/frame $\times$ 30 frames = 180 px/s

$$\boxed{\text{Real speed} = v_{\max}^{\text{sec}} = \text{constant regardless of fps}}$$

**Velocity change per second (turning rate):**

$$\Delta \mathbf{v}_{\text{per second}} = \mathbf{F} \cdot \Delta t \cdot \text{fps} = \mathbf{F} \cdot 1 = \mathbf{F}$$

The $\Delta t$ factor cancels with the frame count, giving constant acceleration in real-time units.

### 5.3 Conclusion: Our `*dt` Approach Does NOT Undermine the Model

The `* dt` multiplier converts frame-dependent quantities into frame-independent ones. This is standard forward Euler integration and is the **correct** approach for variable-framerate simulations. All reference implementations that omit `* dt` are implicitly locked to a specific frame rate.

### 5.4 Parameter Conversion Proof

The original master plan specified frame-based values. The conversion to per-second units:

$$v_{\max}^{\text{sec}} = v_{\max}^{\text{frame}} \cdot \text{fps} = 3.0 \times 60 = 180.0 \text{ px/s}$$

For force, the frame-based $F_{\max}^{\text{frame}}$ represents the maximum velocity change per frame. In our model, velocity change per frame is $F_{\max}^{\text{sec}} \cdot \Delta t$. For equivalence:

$$F_{\max}^{\text{sec}} \cdot \Delta t = F_{\max}^{\text{frame}}$$
$$F_{\max}^{\text{sec}} = \frac{F_{\max}^{\text{frame}}}{\Delta t} = F_{\max}^{\text{frame}} \cdot \text{fps}$$

With $F_{\max}^{\text{frame}} = 0.1$ and fps $= 60$:

$$F_{\max}^{\text{sec}} = 0.1 \times 60 = 6.0 \text{ px/s}^2$$

This conversion is **dimensionally correct**. The old value of $6.0$ faithfully reproduced the frame-based behavior of $F_{\max}^{\text{frame}} = 0.1$. The problem was that the original frame-based value itself was too conservative — it resulted in 60-second turn times, far slower than reference implementations.

---

## 6. Turn-Time Comparison

### 6.1 Turn-Time Formula

The time for a boid to reverse direction (180 degrees) under maximum steering force:

$$T_{180} = \frac{2 \cdot v_{\max}}{F_{\text{eff}}}$$

where $F_{\text{eff}}$ is the effective force applied per unit time.

### 6.2 Model A (Adams) — Alignment Turn Time

Maximum alignment force per frame (when going in opposite direction from flock):

$$\Delta v_{\text{frame}} = |\bar{v} - v_i| \cdot w_{\text{match}} = 2 v_{\max} \cdot w_{\text{match}} = 2(6)(0.05) = 0.6 \text{ px/frame}$$

Total velocity change needed: $2 v_{\max} = 12$ px/frame.

$$T_{180} = \frac{12}{0.6} = 20 \text{ frames} = 0.33\text{s at 60fps}$$

### 6.3 Model B (Shiffman) — Alignment Turn Time

Maximum alignment steering force (clamped per-behavior):

$$\Delta v_{\text{frame}} = f_{\max} = 0.03 \text{ px/frame}$$

Total velocity change: $2 v_{\max} = 4$ px/frame.

$$T_{180} = \frac{4}{0.03} = 133 \text{ frames} = 2.2\text{s at 60fps}$$

### 6.4 Our Model — Before Fix

$F_{\max} = 6.0$ px/s$^2$, $v_{\max} = 180$ px/s, $\Delta t = 1/60$ s.

$$\Delta v_{\text{frame}} = F_{\max} \cdot \Delta t = 6.0 \times \frac{1}{60} = 0.1 \text{ px/s per frame}$$

Total velocity change: $2 v_{\max} = 360$ px/s.

$$T_{180} = \frac{360}{0.1 \times 60} = \frac{360}{6.0} = 60\text{s}$$

### 6.5 Our Model — After Fix

$F_{\max} = 180.0$ px/s$^2$, $v_{\max} = 180$ px/s.

$$\Delta v_{\text{frame}} = 180.0 \times \frac{1}{60} = 3.0 \text{ px/s per frame}$$

$$T_{180} = \frac{360}{3.0 \times 60} = \frac{360}{180} = 2.0\text{s}$$

### 6.6 Summary Table

| Model | $\Delta v$ / frame | Frames for 180$^\circ$ | Time at 60fps | Force/Speed Ratio |
|-------|:---:|:---:|:---:|:---:|
| **Model A** (Adams) | 0.6 px/frame | 20 | **0.33s** | No clamp (10%/frame) |
| **Model B** (Shiffman) | 0.03 px/frame | 133 | **2.2s** | $f/v = 1.5\%$ |
| **Ours (before)** | 0.1 px/s | 3600 | **60s** | $F/v = 3.3\%$ but $\times\Delta t$ |
| **Ours (after)** | 3.0 px/s | 120 | **2.0s** | $F/v = 100\%$ but $\times\Delta t$ |

After the fix, our turn time (2.0s) is comparable to Model B (2.2s) — the closest reference model to our Reynolds-hybrid approach.

---

## References

1. Reynolds, C. W. (1987). "Flocks, Herds, and Schools: A Distributed Behavioral Model." *ACM SIGGRAPH Computer Graphics*, 21(4), 25-34. https://www.red3d.com/cwr/boids/
2. Reynolds, C. W. (1999). "Steering Behaviors For Autonomous Characters." *GDC 1999*. https://www.red3d.com/cwr/steer/gdc99/
3. Shiffman, D. "Flocking." *Processing.org Examples*. https://processing.org/examples/flocking.html
4. Adams, V. H. "Boids Algorithm." *Cornell University*. https://vanhunteradams.com/Pico/Animal_Movement/Boids-algorithm.html
5. Land, B. "Boids." *Cornell ECE4760*. https://people.ece.cornell.edu/land/courses/ece4760/labs/s2021/Boids/Boids.html
