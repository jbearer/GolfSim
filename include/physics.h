/**
 * \file physics.h
 * \brief A model of ball flight, landing, and roll-out.
 *
 * This model simulates ball flight from the instant after the ball leaves the
 * club to the moment it comes to rest. It accounts for realistic factors like
 * spin, air resistance, and drag, and it handles collision testing and
 * interaction with the physics properties of the materials in the terrain where
 * the ball lands.
 *
 * It does not handle the strike of the ball on the clubface; instead it takes
 * as inputs parameters describing the motion of the ball as it leaves the face.
 */

#ifndef GOLF_PHYSICS_H
#define GOLF_PHYSICS_H

#include "matrix.h"
#include "terrain.h"

/**
 * \struct ShotStatus
 * \brief Information about an in-progress or completed shot.
 *
 * The `ShotStatus` struct contains information about the current state of the
 * ball associated with the shot, such as it's current position, as well as
 * cumulative statistics about the shot, such as the maximum height attained.
 *
 * All fields use the conventional units:
 *  * radians, for angles
 *  * yards, for distances
 *  * milliseconds, for time
 *
 * and ratios thereof.
 */
typedef struct {
    float launch_angle;
        ///< The angle with the vertical of the ball's initial trajectory.
    float land_angle;
        ///< \brief The angle with the vertical of the ball's trajectory when it
        ///< landed (or it's current trajectory, if it is still in the air).
    float apex;
        ///< \brief The maximum height attained by the shot, relative to the
        ///< height of the point from which the ball was struck.
    float apex_distance;
        ///< \brief Horizontal distance from the starting point of the shot
        ///< where the ball attained its maximum height.
    float curve;
        ///< \brief
        ///< Distance that the ball has curved away from its initial trajectory.
        ///<
        ///< For example, consider a shot which was aimed directly in the
        ///< positive y direction, but which hooked to the left:
        ///<
        ///<                      target
        ///<                        *
        ///<         landing    C   |
        ///<                o,------|
        ///<                  `.    |
        ///<                    \   |
        ///<                     |  |
        ///<                      | |
        ///<                       ||
        ///<                        |
        ///<                        |
        ///<                        o start
        ///<
        ///< The `curve` for this shot is the distance marked `C` in the figure.
        ///< The curve will always be measured perpendicular to the original
        ///< target line, as shown.
        ///<
        ///< The sign of `curve` indicates the direction in which the ball
        ///< curved, according to the right-hand rule. So `C` in the figure
        ///< above is positive, since the ball curved counter-clockwise. Had the
        ///< ball sliced (clockwise) instead of hooked, the curve would have
        ///< been `-C`.
    float carry;
        ///< The horizontal distance the ball travelled before landing.
    float hang_time;
        ///< The time the ball spent in the air.
    vec3 x;
        ///< The current position of the ball.
    vec3 v;
        ///< The current velocity of the ball.
    vec3 s;
        ///< The current spin of the ball.
} ShotStatus;

/**
 * \brief Opaque state maintained by an in-progress simulation.
 *
 * A pointer to a new `Simulation` can be obtained by calling `Simulation_New`.
 * Then, over the lifetime of the simulation, the owner may repeatedly call
 * `Simulation_Step` to advance the simulation. When the simulation is finished,
 * it can be destroyed and cleaned up by calling `Simulation_Delete`.
 *
 * The `Simulation` data structure itself is meant to be opaque to users of the
 * physics module. It contains implementation-specific data used to implement
 * the details of the physics model. Public information about the shot being
 * simulated will be available in the `ShotStatus` object passed to
 * `Simulation_New`. This object will be updated after every call to
 * `Simulation_Step`.
 */
typedef struct Simulation Simulation;

/**
 * \brief Create a new shot simulation.
 *
 * \param terrain   The terrain where the shot is being played. This is used to
 *                  simulate the landing, bounce, and roll of the shot.
 * \param status    Structure used to return updates about the shot to the owner
 *                  of the simulation. This is an in-out parameter. The `x`, `v`
 *                  and `s` fields of the structure must be initialized to the
 *                  starting position, velocity, and spin, respectively. The
 *                  remaining fields will be zeroed, and then updated
 *                  accordingly whenever the simulation updates.
 *
 * This creates a simulation of a ball in flight with position, velocity, and
 * spin given by the `status` structure. The simulation can be advanced by
 * calling `Simulation_Step`. The simulation will simulate the flight of the
 * ball until it hits the ground, and then bounce and roll until the ball comes
 * to rest.
 *
 * \warning
 * A given `ShotStatus` object may only be used with one simulation at a time.
 * If a `ShotStatus` object passed to `Simulation_New` is currently in use with
 * a simulation, the existing simulation must first be destroyed by calling
 * `Simulation_Delete`.
 */
Simulation *Simulation_New(const Terrain *terrain, ShotStatus *status);

/**
 * \brief Advance the state of a shot simulation.
 *
 * \param sim   The simulation to run.
 * \param dt    The time elapsed since the last call to `Simulation_Step` (or
 *              `Simulation_New`, if this is the first step).
 *
 * \return `true` if the simulation is still ongoing, or false if the ball has
 *         come to rest.
 *
 * This function advances the simulation by the timestep given as `dt`, and
 * updates the `ShotStatus` structure passed to `Simulation_New` with updated
 * information about the shot.
 *
 * This function is meant to be called repeatedly until the simulation
 * completes. Each time it is called, it will return `true` if it should be
 * called again. Once `Simulation_Step` returns `false`, the ball is guaranteed
 * to be at rest, and the simulation is complete.
 *
 * \warning
 * `Simulation_Step` may not be called on a simulation which has completed. The
 * only valid operation on a completed simulation is `Simulation_Delete`. Note
 * that the `ShotStatus` structure associated with this simulation is considered
 * to be owned by the caller, and will remain valid and may continue to be used
 * after the simulation has completed or has been destroyed.
 *
 */
bool Simulation_Step(Simulation *sim, uint32_t dt);

/**
 * \brief Destroy a simulation.
 *
 * The given simulation will be closed and its resources will be freed. The
 * simulation must have been created with `Simulation_New` and may not be used
 * again after calling `Simulation_Delete`.
 *
 * The simulation need not have completed to be destroyed. This function can be
 * used to cancel and in-progress simulation, or to clean up a completed one.
 */
void Simulation_Delete(Simulation *sim);

#endif
