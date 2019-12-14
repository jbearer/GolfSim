#include <math.h>
#include <stdint.h>
#ifndef NDEBUG
#include <stdio.h>
#endif

#include "errors.h"
#include "matrix.h"

void vec4_Proj3D(const vec4 *v4, vec3 *v3)
{
    *v3 = (vec3) { v4->x, v3->y, v4->z };
}

void vec4_Quaternion(vec4 *v, float radians, const vec3 *axis)
{
    v->x = axis->x * sinf(radians/2);
    v->y = axis->y * sinf(radians/2);
    v->z = axis->z * sinf(radians/2);
    v->w = cosf(radians/2);
}

void mat3_Compose(
    const mat3 * restrict A, const mat3 * restrict B, mat3 * restrict out)
{
    for (uint8_t row = 0; row < 3; ++row) {
        for (uint8_t col = 0; col < 3; ++col) {
            out->M[row][col] = 0;
            for (uint8_t i = 0; i < 3; ++i) {
                out->M[row][col] += A->M[row][i]*B->M[i][col];
            }
        }
    }
}

void mat3_ComposeInPlace(const mat3 * restrict src, mat3 * restrict dst)
{
    mat3 tmp;
    mat3_Compose(src, dst, &tmp);
    mat3_Copy(dst, &tmp);
}


#ifndef NDEBUG
const char *mat3_String(mat3 *m)
{
    static char buf[(8*3 + 3)*3 + 1];
        // up to 8 characters per entry. 3 whitespace characters per row. 3
        // rows. 1 NULL terminator.
    snprintf(buf, sizeof(buf),
        "%7.3f %7.3f %7.3f\n"
        "%7.3f %7.3f %7.3f\n"
        "%7.3f %7.3f %7.3f\n",
        m->M[0][0], m->M[0][1], m->M[0][2],
        m->M[1][0], m->M[1][1], m->M[1][2],
        m->M[2][0], m->M[2][1], m->M[2][2]);

    return buf;
}
#endif


void mat4_FromQuaternion(mat4 *m, const vec4 *q)
{
    float x = q->x;
    float y = q->y;
    float z = q->z;
    float w = q->w;

    float x2 = x*x;
    float y2 = y*y;
    float z2 = z*z;
    float w2 = w*w;

    ASSERT(x2 + y2 + z2 + w2 == 1);

    *m = (mat4) {{
        { (1 - 2*y2 - 2*z2), (2*x*y - 2*z*w),   (2*x*z + 2*y*w),   0 },
        { (2*x*y + 2*z*w),   (1 - 2*x2 - 2*z2), (2*y*z - 2*x*w),   0 },
        { (2*x*z - 2*y*w),   (2*y*z + 2*x*w),   (1 - 2*x2 - 2*y2), 0 },
        { 0,                 0,                 0,                 1 },
    }};
}

void mat4_Translation(mat4 *m, const vec3 *direction)
{
    mat4_Copy(m, &I4);
    m->M[0][3] = direction->x;
    m->M[1][3] = direction->y;
    m->M[2][3] = direction->z;
}

void mat4_Rotation(mat4 *m, float radians, const vec3 *axis)
{
    vec4 q;
    vec4_Quaternion(&q, radians, axis);
    mat4_FromQuaternion(m, &q);
}

void mat4_Perspective(mat4 *m, float fov, float aspect, float near, float far)
{
    // This projection matrix is stolen almost directly from the documentation
    // for `glFrustum`, (after we do the extra step of computing `left`,
    // `right`, `top`, and `bottom` using the more user-friendly arguments
    // `near`, `aspect`, and `far`). Our matrix does differ from OpenGL's in one
    // important way, though:
    //
    // We negate the definitions of `C` and `D` provided in the documentation.
    // These are the two matrix entries used to compute the z-coordinate of the
    // output point, so negating them has the effect of swapping the handedness
    // of the coordinate system. Since OpenGL implicitly uses a left-handed
    // coordinate system in clip space (where increasing Z values indicate
    // increasing depth into the screen) this forces a right-handed orientation
    // on the pre-perspective-transform coordinate system, which we want.

    ASSERT(0 < fov && fov < M_PI);
    ASSERT(aspect > 0);
    ASSERT(near > 0);
    ASSERT(far > near);

    float right = near*tanf(fov/2);
    float left = -right;
    float top = right/aspect;
    float bottom = -top;

    ASSERT(top > 0);
    ASSERT(bottom < 0);
    ASSERT(right > 0);
    ASSERT(left < 0);

    float height = top - bottom;
    float width = right - left;
    float depth = far - near;

    float A = (right + left)/width;
    float B = (top + bottom)/height;
    float C = -(far + near)/depth;
    float D = -(2*far*near)/depth;

    *m = (mat4) {{
        { (2*near)/width, 0,               A, 0 },
        { 0,              (2*near)/height, B, 0 },
        { 0,              0,               C, D },
        { 0,              0,              -1, 0 },
    }};
}

void mat4_Compose(
    const mat4 * restrict A, const mat4 * restrict B, mat4 * restrict out)
{
    for (uint8_t row = 0; row < 4; ++row) {
        for (uint8_t col = 0; col < 4; ++col) {
            out->M[row][col] = 0;
            for (uint8_t i = 0; i < 4; ++i) {
                out->M[row][col] += A->M[row][i]*B->M[i][col];
            }
        }
    }
}

void mat4_ComposeInPlace(const mat4 * restrict src, mat4 * restrict dst)
{
    mat4 tmp;
    mat4_Compose(src, dst, &tmp);
    mat4_Copy(dst, &tmp);
}

void mat4_RightComposeInPlace(mat4 * restrict dst, const mat4 * restrict src)
{
    mat4 tmp;
    mat4_Compose(dst, src, &tmp);
    mat4_Copy(dst, &tmp);
}

void mat4_Apply(
    const mat4 * restrict m, const vec4 * restrict v, vec4 * restrict out)
{
    const float *v_buf = vec4_ConstBuffer(v);
    float *component = vec4_Buffer(out);

    for (uint8_t row = 0; row < 4; ++row) {
        *component = 0;
        for (uint8_t col = 0; col < 4; ++col) {
            *component += m->M[row][col]*v_buf[col];
        }
        ++component;
    }
}

#ifndef NDEBUG
const char *mat4_String(mat4 *m)
{
    static char buf[(8*4 + 4)*4 + 1];
        // up to 8 characters per entry. 4 whitespace characters per row. 4
        // rows. 1 NULL terminator.
    snprintf(buf, sizeof(buf),
        "%7.3f %7.3f %7.3f %7.3f\n"
        "%7.3f %7.3f %7.3f %7.3f\n"
        "%7.3f %7.3f %7.3f %7.3f\n"
        "%7.3f %7.3f %7.3f %7.3f\n",
        m->M[0][0], m->M[0][1], m->M[0][2], m->M[0][3],
        m->M[1][0], m->M[1][1], m->M[1][2], m->M[1][3],
        m->M[2][0], m->M[2][1], m->M[2][2], m->M[2][3],
        m->M[3][0], m->M[3][1], m->M[3][2], m->M[3][3]);

    return buf;
}
#endif
