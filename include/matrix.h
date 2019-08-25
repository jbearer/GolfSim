/**
 * \file matrix.h
 *
 * Lightweight linear algebra.
 */

#ifndef GOLF_MATRIX_H
#define GOLF_MATRIX_H

typedef struct {
    float x;
    float y;
    float z;
} vec3;

typedef float mat4[4][4];

static const mat4 I4 = {
    { 1, 0, 0, 0 },
    { 0, 1, 0, 0 },
    { 0, 0, 1, 0 },
    { 0, 0, 0, 1 },
};

#endif
