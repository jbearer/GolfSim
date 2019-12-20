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

void mat4_ApplyInPlace(const mat4 * restrict m, vec4 * restrict v)
{
    vec4 out;
    mat4_Apply(m, v, &out);
    *v = out;
}

// Elementary row operation #1: swap the rows with index `r1` and `r2`.
static void mat4_SwapRows(mat4 *m, uint8_t r1, uint8_t r2) {
    for (uint8_t col = 0; col < 4; ++col) {
        float e1 = m->M[r1][col];
        float e2 = m->M[r2][col];
        m->M[r2][col] = e1;
        m->M[r1][col] = e2;
    }
}

// Elementary row operation #2: scale the row with index `row` by `scalar`.
static void mat4_ScaleRow(mat4 *m, float scalar, uint8_t row) {
    for (uint8_t col = 0; col < 4; ++col) {
        m->M[row][col] *= scalar;
    }
}

// Elementary row operation #3: Add scalar*m[dst_row] to m[src_row].
static void mat4_ReduceRow(
    mat4 *m, float scalar, uint8_t src_row, uint8_t dst_row)
{
    for (uint8_t col = 0; col < 4; ++col) {
        m->M[dst_row][col] += scalar*m->M[src_row][col];
    }
}

bool mat4_Invert(const mat4 * restrict in, mat4 * restrict out)
{
    // We will use Gauss-Jordan elimination to compute the inverse of `in`. This
    // is not the most efficient algorithm for 4x4 matrices, but it is fast
    // enough (for now, at least) and relatively straightforward.
    //
    // The procedure is fairly simple: we create an augmented matrix
    // [ m | m_inv ] where `m` is initialized to `*in` and `m_inv` is
    // initialized to the identity matrix. We incrementally apply the same
    // sequence of elementary row operations to both matrices as we attempt to
    // reduce `m` to the identity matrix.
    //
    // If we succeed in reducing `m` to the identity matrix, then at the end of
    // the procedure, `m_inv` will contain the inverse of `*in`. If we fail, it
    // is because `*in` is not invertible, and we return `false`.
    mat4 m     = *in;
    mat4 m_inv = I4;

    // We will take the procedure one column at a time, starting from the left
    // and working our way to the right. Each iteration of this loop will
    // "finalize" a new column of the `m` (that is, make that column equal to
    // the corresponding column in the identity matrix) without chaning the
    // previously finalized columns. At the end of the loop (if we don't fail
    // early) `m` will be equal to the identity matrix.
    for (uint8_t col = 0; col < 4; ++col) {
        // Loop invariant: For each 0 <= i < 4, for each 0 <= j < col, if
        // `i == j` then m[i][j] == 1, else m[i][j] == 0.
#ifndef NDEBUG
        // Check invariant:
        for (uint8_t i = 0; i < 4; ++i) {
            for (uint8_t j = 0; j < col; ++j) {
                if (i == j) {
                    ASSERT(m.M[i][j] == 1);
                } else {
                    ASSERT(m.M[i][j] == 0);
                }
            }
        }
#endif

        // Find a row with a non-zero coefficient in this column.
        uint8_t row;
        for (row = col; row < 4; ++row) {
            // We start the search at `col`, because based on the loop invariant
            // all rows less than `col` already have their leading coefficients
            // fixed in earlier columns.
            if (m.M[row][col] != 0) {
                break;
            }
        }
        if (row == 4) {
            // All candidate rows had a zero in this column. This means that it
            // is impossible to reduce this matrix to the identity, and
            // therefore it is not invertible.
            return false;
        }

        if (row > col) {
            // To reestablish the loop invariant, we need the row with a leading
            // coefficient in this column to be in position `col`. If it's not,
            // we swap it with the row that is in `col`.
            //
            // Note that this swap cannot invalidate what we already know about
            // previous columns, because `row > col` and `col >= col`, and so
            // the loop invariant tells us that both rows `row` and `col` have
            // all zero entries up to position `col`.
            mat4_SwapRows(&m,     row, col);
            mat4_SwapRows(&m_inv, row, col);
        }

        // Now we have a leading coefficient at `m[col][col]`, but we need that
        // coefficient to be 1.
        if (m.M[col][col] != 1) {
            // Divide row `col` by the leading coefficient to ensure that
            // coefficient is 1. Once again, this cannot affect earlier columns,
            // because all of the entries in those columns in row `col` are 0.
            float scalar = 1/m.M[col][col];
            mat4_ScaleRow(&m,     scalar, col);
            mat4_ScaleRow(&m_inv, scalar, col);
        }

        // Finally, to reestablish our invariant, we need all the entries in
        // `col` in other rows to be zero.
        for (uint8_t row = 0; row < 4; ++row) {
            if (row != col && m.M[row][col] != 0) {
                // `row` has a nonzero entry in `col`, which is not allowed.
                // Since we know `m[col][col]` is 1, we can add
                // `-m[row][col] * m[col]` to `m[row]`, ensuring that
                // `m[row][col]` will be 0.
                //
                // Once again, we don't have to worry about breaking earlier
                // columns, because `m[row]` has all zeros up to `col`, so we'll
                // just be adding zeros in those positions.
                float scalar = -m.M[row][col];
                mat4_ReduceRow(&m,     scalar, col, row);
                mat4_ReduceRow(&m_inv, scalar, col, row);
            }
        }
    }

    // The loop invariant, together with the fact that `col == 4`, gives us that
    // `m` is now the identity matrix, and therefore `m_inv` is the inverse of
    // the original `m`.
    *out = m_inv;
    return true;
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
