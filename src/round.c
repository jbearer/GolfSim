#include "errors.h"
#include "matrix.h"
#include "physics.h"
#include "round.h"

void Round_Start(Round *round, const Terrain *terrain)
{
    round->terrain = terrain;
    round->shot_sim = NULL;
        // `shot_sim` will be NULL whenever there is no shot in progress.
    memset(&round->shot, 0, sizeof(round->shot));
        // Clear shot statistics until we take a swing.
}

void Round_Swing(Round *round, const vec3 *v, const vec3 *s)
{
    if (round->shot_sim != NULL) {
        warn("ignoring swing command, shot is in progress\n", 0);
        return;
    }

    round->shot.v = *v;
    round->shot.s = *s;
    round->shot_sim = Simulation_New(round->terrain, &round->shot);
}

void Round_Step(Round *round, uint32_t dt)
{
    if (round->shot_sim != NULL) {
        if (!Simulation_Step(round->shot_sim, dt)) {
            // The ball has come to rest.
            Simulation_Delete(round->shot_sim);
            round->shot_sim = NULL;
        }
    }
}

void Round_GetBallPosition(const Round *round, vec3 *ball_position)
{
    *ball_position = round->shot.x;
}

void Round_GetShotStatistics(const Round *round, ShotStatus *stats)
{
    *stats = round->shot;
}

void Round_SetBallPosition(Round *round, const vec2 *ball_position)
{
    // Cancel any shot that might be in progress.
    if (round->shot_sim != NULL) {
        Simulation_Delete(round->shot_sim);
        round->shot_sim = NULL;
    }

    // We are given a horizontal position on the terrain. We need to find the
    // z-coordinate of the terrain at that point to place the ball there.
    float z = Terrain_SampleHeight(
        round->terrain, ball_position->x, ball_position->y);
    round->shot.x = (vec3){ball_position->x, ball_position->y, z};
}
