/**
 * \file play.h
 *
 * This module manages the high-level aspects of playing a round of golf. (It
 * delegates the low-level simulation details to the phsyics engine.) Its
 * responsibilities include:
 *  * keeping track of the location of the ball
 *  * providing a user-friendly interface for making shots (in terms of, for
 *    example, club selection and fade/draw) and converting the high-level
 *    inputs into parameters required by the phsyics engine (velocity, spin)
 *  * keeping track of the hole being played, and providing targetting
 *    suggestions accordingly
 *  * keeping score
 */

#ifndef GOLF_PLAY_H
#define GOLF_PLAY_H

#include "matrix.h"
#include "physics.h"

/**
 * \struct Round
 * \brief State used internally by this module to represent a round of golf.
 *
 * This data is meant to be treated opaquely, and should only be inspected and
 * manipulated using the functions defined in this header file.
 */
typedef struct {
    Simulation *shot_sim;
    ShotStatus shot;
    const Terrain *terrain;
} Round;

/**
 * \brief Begin a new round.
 *
 * \param round     A new `Round` object to initialize.
 * \param terrain   The `Terrain` containing the course to play.
 */
void Round_Start(Round *round, const Terrain *terrain);

/**
 * \brief Take the next shot in the current round.
 *
 * \param round     The round being played.
 * \param v         The exit velocity of the shot.
 * \param s         The spin of the ball as it leaves the clubface.
 *
 * The ball in the given round must be in play, at rest, and ready to be struck.
 * If the ball is not ready to be struck (for example, it is still in motion
 * from the previous shot) this is a no-op.
 *
 * The ball will be put in motion from the current spot, with parameters `v` and
 * `s`. From there, the rest of the shot will be simulated using the engine
 * defined in `physics.h`.
 */
void Round_Swing(Round *round, const vec3 *v, const vec3 *s);

/**
 * \brief Advance the simulation of the current round.
 *
 * \param dt    Elapsed time in milliseconds since the last call to `Round_Step`.
 *
 * This function advances animations and real-time processes involved in the
 * simulation of the given round. For example, balls in flight have their flight
 * trajectories updated according to the physics engine.
 */
void Round_Step(Round *round, uint32_t dt);

/**
 * \brief Get the location of the ball associated with this round.
 */
void Round_GetBallPosition(const Round *round, vec3 *ball_position);

/**
 * \brief Set the location of the ball associated with this round.
 *
 * The ball will be placed in its new location on the ground at rest. This
 * function overrides the position of the ball as calculated by the physics
 * engine. As such, it will cause the shot in progress to be cancelled, if there
 * is one.
 *
 * \pre
 * `ball_position` is in the range covered by the terrain associated with this
 * round.
 */
void Round_SetBallPosition(Round *round, const vec2 *ball_position);

/**
 * \brief Get statistics for the most recent (or in-progress) shot.
 */
void Round_GetShotStatistics(const Round *round, ShotStatus *stats);

#endif
