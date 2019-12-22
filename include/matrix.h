/**
 * \file matrix.h
 *
 * Lightweight linear algebra.
 */

#ifndef GOLF_MATRIX_H
#define GOLF_MATRIX_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440
#endif

static inline uint64_t UintMin(uint64_t x, uint64_t y)
{
    return x <= y ? x : y;
}

static inline uint64_t UintMax(uint64_t x, uint64_t y)
{
    return x >= y ? x : y;
}

static inline int64_t IntMin(int64_t x, int64_t y)
{
    return x <= y ? x : y;
}

static inline int64_t IntMax(int64_t x, int64_t y)
{
    return x >= y ? x : y;
}

typedef struct {
    float x;
    float y;
} vec2;

static inline float vec2_Norm(const vec2 *v)
{
    return sqrtf(v->x*v->x + v->y*v->y);
}

typedef struct {
    float x;
    float y;
    float z;
} vec3;

static const vec3 x3 = { 1, 0, 0 }; ///< The x axis.
static const vec3 y3 = { 0, 1, 0 }; ///< The y axis.
static const vec3 z3 = { 0, 0, 1 }; ///< The z axis.

/**
 * \brief Multiply a vector by a scalar.
 */
static inline void vec3_Scale(float scalar, const vec3 *in, vec3 *out)
{
    out->x = scalar*in->x;
    out->y = scalar*in->y;
    out->z = scalar*in->z;
}

/**
 * \brief Store the cross-product of `u` and `v` in `out`.
 */
static inline void vec3_Cross(const vec3 *u, const vec3 *v, vec3 *out)
{
    out->x = u->y*v->z - u->z*v->y;
    out->y = u->z*v->x - u->x*v->z;
    out->z = u->x*v->y - u->y*v->x;
}

/**
 * \brief Store the sum of `u` and `v` in `out`.
 */
static inline void vec3_Add(const vec3 *u, const vec3 *v, vec3 *out)
{
    out->x = u->x + v->x;
    out->y = u->y + v->y;
    out->z = u->z + v->z;
}

/**
 * \brief Compute the sum of `u` and `v` and store it in `v`.
 */
static inline void vec3_AddInPlace(const vec3 *u, vec3 *v)
{
    vec3 out;
    vec3_Add(u, v, &out);
    *v = out;
}

/**
 * \brief Compute the difference of `u` and `v` and store it in `v`.
 */
static inline void vec3_Subtract(const vec3 *u, const vec3 *v, vec3 *out)
{
    vec3_Scale(-1, v, out);
    vec3_AddInPlace(u, out);
}

/**
 * \brief Return the norm of a vector.
 */
static inline float vec3_Norm(const vec3 *v)
{
    return sqrtf(v->x*v->x + v->y*v->y + v->z*v->z);
}

/**
 * \brief Store a unit vector with the same direction as `v` in `out`.
 */
static inline void vec3_Normalize(const vec3 *v, vec3 *out)
{
    vec3_Scale(1/vec3_Norm(v), v, out);
}

/**
 * \brief Compute a unit vector with the same direction as `v` and store it in `v`.
 */
static inline void vec3_NormalizeInPlace(vec3 *v)
{
    vec3 out;
    vec3_Normalize(v, &out);
    *v = out;
}

typedef struct {
    float x;
    float y;
    float z;
    float w;
} vec4;

static const vec4 x4 = { 1, 0, 0, 0 };
    ///< The x axis, in homogeneous coordinates.
static const vec4 y4 = { 0, 1, 0, 0 };
    ///< The y axis, in homogeneous coordinates.
static const vec4 z4 = { 0, 0, 1, 0 };
    ///< The z axis, in homogeneous coordinates.

static const vec4 RGBA_CLEAR = { 0, 0, 0, 0 };
    ///< A fully transparent color.
static const vec4 RGBA_BLACK = { 0, 0, 0, 1 };
    ///< An opaque, black color.

/**
 * \brief Multiply a vector by a scalar.
 */
static inline void vec4_Scale(float scalar, const vec4 *in, vec4 *out)
{
    out->x = scalar*in->x;
    out->y = scalar*in->y;
    out->z = scalar*in->z;
    out->w = scalar*in->w;
}

/**
 * \brief Multiply a vector by a scalar, updating the original vector.
 */
static inline void vec4_ScaleInPlace(float scalar, vec4 *v)
{
    vec4 out;
    vec4_Scale(scalar, v, &out);
    *v = out;
}

/**
 * \brief Project a 4-dimensional vector into 3-space.
 */
void vec4_Proj3D(const vec4 *v4, vec3 *v3);

/**
 * \brief Create a quaternion representing a rotation.
 *
 * \param radians Angle in radians to rotate.
 * \param axis    Axis about which to rotate.
 */
void vec4_Quaternion(vec4 *v, float radians, const vec3 *axis);

/**
 * \brief A contiguous array of floats containing the data in vector `v`.
 *
 * The returned buffer is guaranteed to be in a format which OpenGL will
 * understand as a `vec4`. The returned storate lives as long as `v`.
 */
static inline float *vec4_Buffer(vec4 *v)
{
    return (float *)v;
}

/**
 * \brief A read-only array of floats containing the data in vector `v`.
 *
 * The returned buffer is guaranteed to be in a format which OpenGL will
 * understand as a `vec4`. The returned storate lives as long as `v`.
 */
static inline const float *vec4_ConstBuffer(const vec4 *v)
{
    return (const float *)v;
}

typedef struct {
    float M[3][3];
} mat3;

static const mat3 I3 = {{
    { 1, 0, 0 },
    { 0, 1, 0 },
    { 0, 0, 1 },
}};

static inline void mat3_Copy(mat3 * restrict dst, const mat3 * restrict src)
{
    memcpy(dst, src, sizeof(mat3));
}

static inline void mat3_Scale(mat3 *m, const vec2 *v)
{
    *m = (mat3) {{
        { v->x, 0,    0 },
        { 0,    v->y, 0 },
        { 0,    0,    1 },
    }};
}

static inline void mat3_Translation(mat3 *m, const vec2 *v)
{
    *m = (mat3) {{
        { 1, 0, v->x },
        { 0, 1, v->y },
        { 0, 0, 1    },
    }};
}

void mat3_Compose(
    const mat3 * restrict A, const mat3 * restrict B, mat3 * restrict out);
void mat3_ComposeInPlace(const mat3 * restrict src, mat3 * restrict dst);

/**
 * \brief A contiguous array of floats containing the data in matrix `m`.
 *
 * The returned buffer is guaranteed to be in a format which OpenGL will
 * understand as a `mat3`. The returned storate lives as long as `m`.
 */
static inline const float *mat3_ConstBuffer(const mat3 *m)
{
    return (const float *)m;
}

#ifndef NDEBUG
/**
 * \brief Get a string representation of a matrix.
 *
 * The returned pointer may refer to static storage. As such, it is only valid
 * until the next call to `mat3_String`.
 */
const char *mat3_String(mat3 *m);
#endif

typedef struct {
    float M[4][4];
} mat4;

static const mat4 I4 = {{
    { 1, 0, 0, 0 },
    { 0, 1, 0, 0 },
    { 0, 0, 1, 0 },
    { 0, 0, 0, 1 },
}};

/**
 * \brief Copy a matrix.
 *
 * \param dst   Matrix to copy into.
 * \param src   Matrix to copy out of.
 *
 * The data in `src` is copied into `dst`, without modifying `src`. `src`
 * and `dst` must not alias.
 */
static inline void mat4_Copy(mat4 * restrict dst, const mat4 * restrict src)
{
    memcpy(dst, src, sizeof(mat4));
}

/**
 * \brief Create a rotation matrix from a quaternion.
 */
void mat4_FromQuaternion(mat4 *m, const vec4 *q);

/**
 * \brief Create a rotation matrix.
 *
 * The parameters are the same as for `vec4_Quaternion`. The resulting matrix
 * represents an _extrinsic_ rotation; that is, a rotation about `axis` relative
 * to the current, transformed coordinate system. This has implications for how
 * such matrices compose:
 *
 * Suppose Mx represents a rotation 45 degrees about the x-axis
 * `(pi/4, {1, 0, 0})`:
 *
 *       y                    y
 *      ^                    ^
 *      |         --Mx->     |
 *      .--> x               |-->x
 *     z                     Vz
 *
 * and Mz represents a rotation 45 degrees about the z-axis `(pi/4, {0, 0, 1})`:
 *
 *       y
 *      ^                   y     x
 *      |                    \   /
 *      .--> x    --Mz->      `.'
 *     z                      z
 *
 * The the composition `Mx * Mz` (obtained by `mat4_Compose(&Mx, &Mz, &out)`)
 * represents first a rotation 45 degrees about the z-axis and then a rotation
 * 45 degrees about the _new_ x-axis -- that is, an axis normal to the ZY
 * plane before it was rotated:
 *
 *       y
 *      ^                      y     x
 *      |                       \   /
 *      .--> x    --Mx*Mz->      `.'
 *     z                          |
 *                               z
 */
void mat4_Rotation(mat4 *m, float radians, const vec3 *axis);

/**
 * \brief Create a translation matrix.
 */
void mat4_Translation(mat4 *m, const vec3 *direction);

/**
 * \brief Create a perspective projection matrix.
 *
 * \param m      The matrix to initialize.
 * \param fov    The field of view, in radians. Must be in `(0, M_PI)`.
 * \param aspect The aspect ratio of the window. Must be positive.
 * \param near   The positive distance to the near clipping plane.
 * \param far    The positive distance to the far clipping plane.
 *
 * This method initializes the matrix `m` with a projection matrix which
 * transforms points in camera space to points in screen space using a
 * perspective transform.
 *
 * The camera setup:
 *
 *           y           near clip far clip
 *      z<--.                |        |
 *          |                |      _.|
 *          V                |   _.-  |
 *          x                |_.-     |
 *                         _.|        |
 *                      _.-  | *P1    |
 *                   _.-\    |        |
 *                CAM._  |<. |        |
 *                     -/_ | |        |
 *                        -|_|        |
 *                        /  |._  P2* |
 *                       /   |  -._   |
 *                     fov   |     -._|
 *                           |        |
 *                 <--------->
 *                    near
 *                 <------------------>
 *                         far
 *
 * This will be transformed to a rectangular prismatic viewport, with points
 * farther from the camera appearing closer to the center of the screen:
 *
 *
 *           y      -------|--------|
 *      z<--.              |        |
 *          |              | *P1    |
 *          V              |        |
 *          x              |    P2* |
 *                         |        |
 *                         |        |
 *                  -------|--------|
 *
 * From there, the graphics card can easily project the points onto the two-
 * dimensional screen:
 *
 *           y
 *          .             |
 *          |             *P1
 *          V             |
 *          x             *P2
 *                        |
 *                        |
 *
 *
 * The `fov` parameter determines the width of the clipping planes. From the
 * first figure above, and with a little geometry, we can determine that the
 * width of the near plane is `w_near = 2*near*tan(fov/2)`, and similarly the
 * width of the far plane is `w_far 2*far*tan(fov/2)`. Equivalently, since the
 * camera normal, the edge of the frustum, and the clipping planes form similar
 * triangles, we can derive that the width of the far plane is `w_far =
 * far*w_near/near`.
 *
 * The heights are of the planes are a function of the corresponding widths and
 * the aspect ratio: `h_near = w_near/aspect` and `h_far = w_far/aspect`. This
 * is illustrated in this view from the camera's perspective:
 *
 *      ^y
 *      |
 *      o-->x              -------------------
 *     z          ^        |\     Near      /|
 *                |        | \             / |
 *                |        |  -------------  |
 *                |      ^ |  |           |  |
 *          h_near| h_far| |  |    Far    |  |
 *                |      | |  |           |  |
 *                |      V |  -------------  |
 *                |        | /             \ |
 *                |        |/               \|
 *                V        -------------------
 *                            <----------->
 *                                w_far
 *
 *                         <----------------->
 *                               w_near
 */
void mat4_Perspective(mat4 *m, float fov, float aspect, float near, float far);

/**
 * \brief Matrix multiplication.
 */
void mat4_Compose(
    const mat4 * restrict m1, const mat4 * restrict m2, mat4 * restrict out);

/**
 * \brief Matrix multiplication, overwriting the second argument.
 */
void mat4_ComposeInPlace(const mat4 * restrict src, mat4 * restrict dst);

/**
 * \brief In-place right-multiplication.
 */
void mat4_RightComposeInPlace(mat4 * restrict dst, const mat4 * restrict src);

/**
 * \brief Apply an affine transformation to a vector.
 */
void mat4_Apply(
    const mat4 * restrict m, const vec4 * restrict v, vec4 * restrict out);

/**
 * \brief Apply an affine transformation to a vector, updating the vector.
 */
void mat4_ApplyInPlace(const mat4 * restrict m, vec4 * restrict v);

/**
 * \brief Store the inverse of `m` in `out`, if it exists.
 *
 * \return
 *  `true`  if `m` is invertible and `*out` is the inverse of `*m`.
 *  `false` if `m` is not invertible, in which case the contents of `*out` are
 *   unspecified.
 */
bool mat4_Invert(const mat4 * restrict m, mat4 * restrict out);

/**
 * \brief A contiguous array of floats containing the data in matrix `m`.
 *
 * The returned buffer is guaranteed to be in a format which OpenGL will
 * understand as a `mat4`. The returned storate lives as long as `m`.
 */
static inline float *mat4_Buffer(mat4 *m)
{
    return (float *)m;
}

#ifndef NDEBUG
/**
 * \brief Get a string representation of a matrix.
 *
 * The returned pointer may refer to static storage. As such, it is only valid
 * until the next call to `mat4_String`.
 */
const char *mat4_String(mat4 *m);
#endif

#endif
