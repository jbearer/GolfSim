#include "errors.h"
#include "matrix.h"
#include "physics.h"
#include "terrain.h"

////////////////////////////////////////////////////////////////////////////////
// Ball flight model
////////////////////////////////////////////////////////////////////////////////
//
// A model of the trajectory of a spinning golf ball in the air.
//
////////////////////////////////////////////////////////////////////////////////
// Motivation
//
// The model implemented here is intended to strike a balance between simplicity
// and realism. For realism, it models physical principles like gravity, spin,
// and drag, rather than relying on a predetermined formula for what the
// trajectory should look like. For simplicity, we use simplified models for the
// forces acting on the ball (for example, air resistance is modelled with
// F = -kv; nothing is simulated at the particle level), we eschew modelling
// contact with the club, the trampoline effect, and the grooves, in favor of
// taking an initial velocity and spin as inputs, and we use numerical
// integration rather than trying to determine an analytical solution for the
// trajectory.
//
// Whenever there is as numeric algorithm, there are questions about stability
// and precision. Forunately, in our case, the algorithm only has to operate in
// a narrow range of velocities, spin rates, and distances -- those reasonable
// for a round of golf. This means it is possible to test the algorithm at the
// extremes of its operating ranges. The algorithm implemented here has been
// tested with a variety of shots (high, low, short, long, low and high spin)
// starting from all corners of the terrain and aiming in various directions. No
// obvious glitches or instabilities have been discovered. Further, the
// algorithm was compared to an analytical model that works in the absence of
// spin, and the two models agreed to within a few tenths of a percent.
//
////////////////////////////////////////////////////////////////////////////////
// Details of the model
//
// The model can be summarized by the fundamental equations for the forces:
//  * Gravity: Fg = -mg, where `m` is the mass of the ball and `g` is the
//    acceleration of the gravitational field, which is effectively a constant
//    9.81m/s^2 on Earth.
//  * Spin: Fs = k1(s×v), where `s` is the axis of spin, `v` is the ball's
//    velocity, and `k1` is a constant which depends on the radius of the ball
//    and the density of the air. This equation is consistent with the standard
//    model of the Magnus effect, which is the model most commonly used to
//    describe the effect of spin on the trajectory of a ball. You will notice
//    that the magnitude of this force depends on both the angular velocity and
//    the linear velocity, which makes sense, and the direction of the cross
//    product matches the intuition about the effect of spin on a golf ball
//    (backspin produces a force backwards and upwards for a rising shot,
//    counter-clockwise sidespin produces a hooking force, etc.).
//  * Drag: Fd = -k2|s|v, where `k2` is a constant which depends on the geometry
//    of the ball and the properties of the air. The drag force is defined to
//    always oppose the direction of the ball. The magnitude of the force is
//    proportional to the magnitude of the velocity, which is intuitive, and
//    also the magnitude of the angular velocity, which may be surprising. This
//    comes once again from the Magnus effect: faster spin creates a wider and
//    more turbulent wake behind the ball, which pulls backwards against the
//    motion of the ball, resulting in increased drag.
//
// Combining these three forces along with Newton's second law gives us a
// differential equation for the motion of the ball:
//
//                  a = v' = F/m = Fg/m + Fs/m + Fd /m
//                         = -g + (k1/m)(s×v) - (k2|s|/m)v
//
// Since `k1` and `k2` are constants which we will calibrate to make the model
// behave well, and since `m` is a constant, we can fold `k1/m` into a single
// constant `K_SPIN`, and we can do the same for `k2/m = K_DRAG`. Thus, we have
//
//               v'(v) = -g + K_SPIN(s(t)×v(t)) - K_DRAG|s(t)|v(t)
//
// We will not attempt to solve this differential equation analytically. I
// believe this equation has an analytical solution, but after attempting to
// solve simpler, related equations, I believe the solution will be
//  1. complicated, and
//  2. numerically unstable.
// Instead, we simulate the effect of this equation using numeric integration:
//  * At each timestep, compute the new position based on the current velocity:
//    `x = x + vΔt`
//  * Compute the current acceleration (`v'`) using the spin and velocity values
//    from the previous timestep.
//  * Compute the change in velocity: `Δv = v'Δt`
//  * Compute the new velocity: `v = v_old + Δv`
//
// The model described above leaves one thing out: how do we compute the change
// in spin at each time step? We know how to compute the new velocity and
// position _given_ the current spin, but how does spin change over time?
//
// The answer we have is not especially rigorous or founded. It is probably the
// weakest part of the model, but empirically it works reasonably well. We
// define `s(t)` by the following differential equation:
//
//                  s'(t) = -K_SPIN_DRAG|v(t)|s(t)
//
// The motivation for this equation is simple: it is analagous to the equation
// for linear air resistance:
//
//                    v'(t) = -K_SPIN_DECAY|s(t)|v(t)
//
// the only differences being that we use a different constant, and the
// direction is parallel to the direction of the spin, rather than the direction
// of the velocity.
//
////////////////////////////////////////////////////////////////////////////////
// Parameters of the model
//
// As described above, our model has three parameters:
#define K_SPIN 0.00525
//      which describes how much the spin rate affects the trajectory,
#define K_SPIN_DECAY 0.0027
//      which describes how quickly the spin rate decays towards zero, and
#define K_DRAG 0.0065
//      which describes how much the air resists the forward motion of the ball.
//
// In theory, these parameters correspond to physical quantities like the mass
// and geometry of the ball, the density of air, and so on. However, we treat
// them like degrees of freedom that we can use to calibrate the model to the
// real world data we have.
//
// The `trackman-*.gs` scripts in the `scripts` directory can be used to
// simulate shots with different clubs using Trackman's data on average PGA Tour
// launch parameters. The parameters above should be tuned so that the resulting
// shots match the trajectories predicted by Trackman as closely as possible.
//
// For now, the parameters are calibrated so that the distances of the shots
// match the ground truth distances pretty closely, but the trajectories are a
// bit too low (calibration A in the table below). To get better results,
// increase K_SPIN to make all the trajectories more vertical. This will cause
// low-spin-rate shots (such as drives) to go farther, as they stay in the air
// longer, and high-spin-rate shots (such as wedges) to go shorter, as the
// backspin opposes the forward motion of the ball. You can then adjust the
// distances by tweaking the other two parameters. The table below summarizes in
// high-level terms the approximate effect of changing each parameter.
//
//                    |                        |       | Known OK calibrations |
//      Parameter     | Increasing it means... | Units | A       |             |
//    --------------------------------------------------------------------------
//       K_SPIN       | All shots will go      | none  | 0.00525 |             |
//                    | higher and descend     |       |         |             |
//                    | steeper. Long shots    |       |         |             |
//                    | get longer and shorter |       |         |             |
//                    | shots shorter.         |       |         |             |
//    --------------------------------------------------------------------------
//       K_SPIN_DECAY | All shots will go very | 1/yd  | 0.0027  |             |
//                    | slightly lower and     |       |         |             |
//                    | land significantly     |       |         |             |
//                    | shallower. Long shots  |       |         |             |
//                    | get shorter and vice   |       |         |             |
//                    | versa.                 |       |         |             |
//    --------------------------------------------------------------------------
//       K_DRAG       | All shots will get     | none  | 0.0065  |             |
//                    | significantly shorter  |       |         |             |
//                    | and slightly lower.    |       |         |             |
//                    | Shots are affected     |       |         |             |
//                    | proportional to their  |       |         |             |
//                    | product of spin and    |       |         |             |
//                    | forward velocity.      |       |         |             |
//
// Known OK calibrations:
//  A: good distance, trajectory too low
//

#define NUMERIC_DT 5
    // Time delta for numeric integration, in milliseconds.

// Simulate the ball flying for a duration `t` milliseconds (or less, if the
// ball hits the ground first). The current state of the ball is given in
// `status`, and that object will be updated in place to reflect the new state.
//
// This function does not update the statistics in `status`, only `x`, `v`, and
// `s`. The rest of the statistics are derivable from these.
//
// Return `true` if the simulation is still ongoing, or `false` if the ball hit
// the ground.
static bool FlightSim_Step(const Terrain *terrain, float t, ShotStatus *status)
{
    // If we just do one iteration of the numeric algorithm each time this
    // function is called, the precision of the numeric integration will be tied
    // to the size of the time deltas the caller gives us -- bigger `t` means
    // less precise integration. Ultimately, this probably means the precision
    // of the algorithm will be tied to the frame rate of the application.
    //
    // Instead, we use a static, constant time delta NUMERIC_DT for the
    // precision of the algorithm, and we iterate the algorithm in time steps of
    // NUMERIC_DT until we have simulated the full time delta `t`.
    do {
        float dt = FloatMin(NUMERIC_DT, t);
            // Compute the delta for this iteration of the algorithm. This will
            // either be NUMERIC_DT, or the remainder of this is the last
            // iteration of the loop and `t` is not an exact multiple of
            // NUMERIC_DT.
        t -= dt;
            // Decrement the time left to be simulated by the amount we're going
            // to simulate this iteration. We will stop the simulation after
            // this hits zero.

        ////////////////////////////////////////////////////////////////////////
        // Compute the new position based on the current velocity.
        //
        vec3 dx;
        vec3_Scale(dt, &status->v, &dx);
        vec3_AddInPlace(&dx, &status->x);

        ////////////////////////////////////////////////////////////////////////
        // Compute the current linear (a) and angular (α) accelerations.
        //
        // a = g + K_SPIN(s×v) - K_DRAG|s|v
        vec3 a_spin, a_drag;
        vec3 a = { 0, 0, -1.07e-5 };
            // This magic number is 9.8m/s^2, converted to yds/ms^2.
        vec3_Cross(&status->s, &status->v, &a_spin);
        vec3_ScaleInPlace(K_SPIN, &a_spin);
        vec3_AddInPlace(&a_spin, &a);
            // Acceleration due to the Magnus effect.
        vec3_Scale(-K_DRAG*vec3_Norm(&status->s), &status->v, &a_drag);
        vec3_AddInPlace(&a_drag, &a);
            // Acceleration due to drag.

        // α = -K_SPIN_DECAY|v|s
        vec3 alpha;
        vec3_Scale(-K_SPIN_DECAY, &status->s, &alpha);

        ////////////////////////////////////////////////////////////////////////
        // Compute the new velocity (v) and spin (ω)
        //
        // v = v + aΔt
        vec3 dv;
        vec3_Scale(dt, &a, &dv);
        vec3_AddInPlace(&dv, &status->v);

        // ω = ω + αΔt
        vec3 ds;
        vec3_Scale(dt, &alpha, &ds);
        vec3_AddInPlace(&ds, &status->s);

        ////////////////////////////////////////////////////////////////////////
        // Check termination conditions
        //
        // Return `false` if we should terminate now, or continue to the next
        // iteration of the loop otherwise.
        //
        // There's a design choice to be made whether to check the termination
        // conditions here, after each iteration of the loop, or just once after
        // the loop terminates. The tradeoff is clear: checking every iteration
        // of the loop lets us detect more precisely when the ball hits the
        // ground. Checking after the loop gives better performance, but not
        // only is collision detection less precise, but it also depends on the
        // rate at which `FlightSim_Step` gets called, rather than NUMERIC_DT,
        // which is a constant.
        //
        // Currently performance is not an issue, so we use the more precise,
        // costlier method.
        //

        // Check if we've gone out of bounds.
        if (!(0 <= status->x.x &&
                   status->x.x < Terrain_FaceWidth(terrain)*terrain->xy_resolution)
                // We are out of bounds along the `x` axis.
        ||
            !(0 <= status->x.y &&
                   status->x.y < Terrain_FaceHeight(terrain)*terrain->xy_resolution))
                // We are out of bounds along the `y` axis.
        {
            if (status->x.z > 0) {
                // We are out of bounds, however the ball is still in the air.
                // We could stop the simulation now, but then the ball would
                // effectively hit an invisible wall at the end of the terrain
                // and stick to it. Instead, we will keep simulating until the
                // ball hits elevation 0.
                //
                // We are obligated to either return or explicitly continue the
                // loop on all paths in the out-of-bounds case, though, because
                // the next step is a collision test with the terrain, which
                // makes no sense if we hit it off the terrain.
                continue;
            } else {
                // Out of bounds, and we hit the ground. Stop simulating.
                status->x.z = 0;
                    // Chances are we missed the exact moment of impact by a
                    // short time, so `z` is slightly below zero. Nudge it back
                    // to exactly zero so the ball is at the right elevation
                    // when it's at rest.
                return false;
            }
        }

        // Check if we've hit the ground.
        float z = Terrain_SampleHeight(terrain, status->x.x, status->x.y);
        if (status->x.z <= z) {
            status->x.z = z;
                // Chances are we missed the exact moment of impact by a short
                // time, so the ball is slightly below the terrain. This would
                // lead to the ball not being rendered while it is at rest, so
                // we nudge it back up to exactly the level of the terrain.
            return false;
        }

    } while (t > 0);

    return true;
        // If we didn't hit any of the termination conditions after any
        // iteration of the loop, the ball is still in the air, so the
        // simulation should continue.
}

struct Simulation {
    vec3 x0;
        // Initial position. Used for computing some relative stats in
        // `ShotStatus`, such as `carry`.
    vec3 target;
        // Initial heading (initial velocity, but with a 0 z-component and
        // normalized). Used for computing some relative stats in `ShotStatus`,
        // such as `curve`.
    const Terrain *terrain;
        // Terrain where the shot is taking placed. Used for bounce and roll.
    ShotStatus *status;
        // `ShotStatus` associated with this simulation.
};

Simulation *Simulation_New(const Terrain *terrain, ShotStatus *status)
{
    Simulation *sim = Malloc(sizeof(Simulation));
    sim->terrain = terrain;
    sim->status = status;

    // Clear `status` output fields.
    status->land_angle = 0;
    status->apex = 0;
    status->apex_distance = 0;
    status->curve = 0;
    status->carry = 0;
    status->hang_time = 0;

    // Save initial state that we will need to compute relative stats later.
    sim->x0 = status->x;
    sim->target = (vec3){status->v.x, status->v.y, 0};
    vec3_NormalizeInPlace(&sim->target);

    // Initialize launch-time stats.
    float cos_theta =
        vec3_Dot(&status->v, &sim->target) / vec3_Norm(&status->v);
            // The launch angle θ is the angle between the trajectory (v) and
            // the target, which is a unit vector parallel to `v` in the XY
            // plane, but with no z-component. The dot product of `v` and
            // `target` is `|v||target|cosθ`. Since |target| is 1, Dividing
            // by `|v|` gives us `cos θ`.
    status->launch_angle = acosf(cos_theta);

    return sim;
}

void Simulation_Delete(Simulation *sim)
{
    free(sim);
}

bool Simulation_Step(Simulation *sim, uint32_t dt)
{
    ShotStatus *status = sim->status;

    ////////////////////////////////////////////////////////////////////////////
    // Run the simulation.
    //
    bool ret = FlightSim_Step(sim->terrain, dt, status);


    ////////////////////////////////////////////////////////////////////////////
    // Update statistics in `status` based on the new `x`, `v`, and `s`.
    //
    status->hang_time += dt;

    vec3 carry = { status->x.x - sim->x0.x, status->x.y - sim->x0.y, 0 };
        // Displacement from the start of the shot to the current position,
        // projected onto the XY plane.
    vec3 heading;
    vec3_Normalize(&carry, &heading);
        // The directional component of `carry`.
    status->carry = vec3_Norm(&carry);

    float height = status->x.z - sim->x0.z;
    if (height > status->apex) {
        // If we've reached a new apex, update both `apex` and the distance to
        // the apex.
        status->apex = height;
        status->apex_distance = status->carry;
    }

    // Compute the curve of the shot.
    vec3 curve;
    vec3_Cross(&sim->target, &heading, &curve);
        // The curve of the shot is defined as the distance C here:
        //
        //                      target
        //                        *
        //         heading    C   |
        //                o,------|
        //                  `.    |
        //                    \   |
        //                     |  |
        //                      | |
        //                       ||
        //                        |
        //                        |
        //                        o start
        //
        // Let θ be the angle between `heading` and `target`.
        // Then `C = carry * sin θ`.
        //
        // We can compute `sin θ` by observing that
        //              |target×heading| = |target||heading|sinθ
        // `target` and `heading` are both unit vectors, so this quantity is
        // exactly `sinθ`.
        //
    ASSERT(curve.x == 0);
    ASSERT(curve.y == 0);
    float sin_theta = curve.z;
        // Since `heading` and `target` both lie in the XY plane,
        // `curve = heading×target` is orthogonal to the plane, so it's
        // magnitude `sinθ` is equal to its z-component.
    status->curve = status->carry*sin_theta;

    // Compute the landing angle.
    vec3 vxy = { status->v.x, status->v.y, 0 };
        // Velocity projected onto the XY plane.
    float cos_phi =
        vec3_Dot(&status->v, &vxy) / (vec3_Norm(&status->v)*vec3_Norm(&vxy));
            // The landing angle ϕ is defined as the angle between the velocity
            // `v` and the horizontal velocity `vxy`. We can compute this angle
            // by using `v⋅vxy = |v||vxy|cosϕ`, so `cosϕ = v⋅vxy/(|v||vxy|).
    status->land_angle = acosf(cos_phi);

    return ret;
}
