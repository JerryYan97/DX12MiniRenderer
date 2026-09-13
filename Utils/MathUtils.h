#pragma once

#include <cstdint>
#include <iostream>
#include <algorithm>

#define M_PI 3.1415926535897932384626433832795
#define alignup(value, alignment) ((value + (alignment - 1)) & ~(alignment - 1))

// TODO: Dim can be put into template for optimization.
struct HFVec2
{
    float ele[2];
};

template<typename T1, typename T2>
inline void MatTypeCast(const T1* matSrc, T2* matDst, uint32_t dim)
{
    for (uint32_t i = 0; i < dim * dim; i++)
    {
        matDst[i] = static_cast<T2>(matSrc[i]);
    }
}

template<typename T>
inline void MatrixMul4x4(const T mat1[16], const T mat2[16], T* resMat)
{
    for (uint32_t row = 0; row < 4; row++)
    {
        for (uint32_t col = 0; col < 4; col++)
        {
            uint32_t idx = 4 * row + col;
            resMat[idx] = mat1[4 * row] * mat2[col] +
                mat1[4 * row + 1] * mat2[4 + col] +
                mat1[4 * row + 2] * mat2[8 + col] +
                mat1[4 * row + 3] * mat2[12 + col];
        }
    }
}

template<typename T>
inline void MatMulMat(T* mat1, T* mat2, T* resMat, uint32_t dim)
{
    for (uint32_t row = 0; row < dim; row++)
    {
        for (uint32_t col = 0; col < dim; col++)
        {
            uint32_t idx = dim * row + col;
            resMat[idx] = 0;
            for (uint32_t ele = 0; ele < dim; ele++)
            {
                resMat[idx] += (mat1[dim * row + ele] * mat2[dim * ele + col]);
            }
        }
    }
}

template<typename T>
inline T Norm(T* vec, uint32_t dim)
{
    T res = 0;
    for (uint32_t i = 0; i < dim; i++)
    {
        res += (vec[i] * vec[i]);
    }
    return sqrt(res);
}

template<typename T>
inline bool NormalizeVec(T* vec, uint32_t dim)
{
    T l2Norm = Norm(vec, dim);
    if (l2Norm == 0)
    {
        return false;
    }
    else
    {
        for (uint32_t i = 0; i < dim; i++)
        {
            vec[i] = vec[i] / l2Norm;
        }
        return true;
    }
}

template<typename T>
inline void CrossProductVec3(T* vec1, T* vec2, T* resVec)
{
    resVec[0] = vec1[1] * vec2[2] - vec1[2] * vec2[1];
    resVec[1] = vec1[2] * vec2[0] - vec1[0] * vec2[2];
    resVec[2] = vec1[0] * vec2[1] - vec1[1] * vec2[0];
}

template<typename T>
inline T DotProduct(T* vec1, T* vec2, uint32_t dim)
{
    T res = 0;
    for (uint32_t i = 0; i < dim; i++)
    {
        res += (vec1[i] * vec2[i]);
    }
    return res;
}

template<typename T>
inline void ScalarMul(T scalar, T* vec, uint32_t dim)
{
    for (uint32_t i = 0; i < dim; i++)
    {
        vec[i] *= scalar;
    }
}

template<typename T>
inline void MatMulVec(const T* mat, T* vec, uint32_t dim, T* res)
{
    for (uint32_t row = 0; row < dim; row++)
    {
        T ele = 0;
        for (uint32_t col = 0; col < dim; col++)
        {
            ele += (mat[row * dim + col] * vec[col]);
        }
        res[row] = ele;
    }
}

template<typename T>
inline void VecAdd(const T* vec1, const T* vec2, uint32_t dim, T* res)
{
    for (uint32_t i = 0; i < dim; i++)
    {
        res[i] = vec1[i] + vec2[i];
    }
}

// NOTE: All matrix on the host are row-major but all matrix on GLSL are column-major.
// It means we need to do a matrix transpose before sending a matrix to the device/GLSL.
template<typename T>
inline void MatTranspose(T* mat, uint32_t dim)
{
    for (uint32_t row = 0; row < dim; row++)
    {
        for (uint32_t col = row + 1; col < dim; col++)
        {
            uint32_t rowMajIdx = row * dim + col;
            uint32_t colMajIdx = col * dim + row;

            T rowMajEle = mat[rowMajIdx];
            T colMajEle = mat[colMajIdx];

            mat[colMajIdx] = rowMajEle;
            mat[rowMajIdx] = colMajEle;
        }
    }
}

template<typename T>
inline void SetIdentityMat4x4(T* pResMat)
{
    for (uint32_t i = 0; i < 16; ++i)
    {
        pResMat[i] = static_cast<T>(0);
    }

    pResMat[0] = static_cast<T>(1);
    pResMat[5] = static_cast<T>(1);
    pResMat[10] = static_cast<T>(1);
    pResMat[15] = static_cast<T>(1);
}

template<typename T>
inline void GenTranslationMat4x4(const T translation[3], T* pResMat)
{
    SetIdentityMat4x4(pResMat);
    pResMat[3] = translation[0];
    pResMat[7] = translation[1];
    pResMat[11] = translation[2];
}

template<typename T>
inline void GenScaleMat4x4(const T scale[3], T* pResMat)
{
    SetIdentityMat4x4(pResMat);
    pResMat[0] = scale[0];
    pResMat[5] = scale[1];
    pResMat[10] = scale[2];
}

template<typename T>
inline void GenQuaternionRotationMat4x4(const T quatXYWZ[4], T* pResMat)
{
    const T x = quatXYWZ[0];
    const T y = quatXYWZ[1];
    const T z = quatXYWZ[2];
    const T w = quatXYWZ[3];

    const T xx = x * x;
    const T yy = y * y;
    const T zz = z * z;
    const T xy = x * y;
    const T xz = x * z;
    const T yz = y * z;
    const T wx = w * x;
    const T wy = w * y;
    const T wz = w * z;

    SetIdentityMat4x4(pResMat);

    pResMat[0] = static_cast<T>(1) - static_cast<T>(2) * (yy + zz);
    pResMat[1] = static_cast<T>(2) * (xy - wz);
    pResMat[2] = static_cast<T>(2) * (xz + wy);

    pResMat[4] = static_cast<T>(2) * (xy + wz);
    pResMat[5] = static_cast<T>(1) - static_cast<T>(2) * (xx + zz);
    pResMat[6] = static_cast<T>(2) * (yz - wx);

    pResMat[8] = static_cast<T>(2) * (xz - wy);
    pResMat[9] = static_cast<T>(2) * (yz + wx);
    pResMat[10] = static_cast<T>(1) - static_cast<T>(2) * (xx + yy);
}

// Generate 4x4 matrices
// Realtime rendering -- P67
void GenViewMat(float* const pView, float* const pPos, float* const pWorldUp, float* pResMat);

// Realtime rendering -- P99. Far are near are posive, which correspond to f' and n'. And far > near.
void GenPerspectiveProjMat(float near, float far, float fov, float aspect, float* pResMat);

// Realtime rendering -- P70, P65. E = R (roll -- z) * R (pitch -- x) * R (head -- y)
void GenModelMat(float* pPos, float roll, float pitch, float head, float* pScale, float* pResMat);

void GenRotationMat(float roll, float pitch, float head, float* pResMat);

// Realtime rendering -- P75 -- Eqn(4.30)
void GenRotationMatArb(float* axis, float radien, float* pResMat);

// Realtime rendering -- P61 -- Eqn(4.5, 4.6, 4.7)
// The positive axis points to your face. Counterclock-wise is positive radians.
// Right hand coordinate system.
void GenRotationMatX(float radien, float* pResmat);
void GenRotationMatY(float radien, float* pResMat);
void GenRotationMatZ(float radien, float* pResMat);

void Mat3x3ToMat4x4(float* mat3x3, float* mat4x4);

template<typename T>
inline void GenTRSModelMat(const T translation[3], const T rotationQuat[4], const T scale[3], T* pResMat)
{
    T tMat[16];
    T rMat[16];
    T sMat[16];
    T trMat[16];

    GenTranslationMat4x4(translation, tMat);
    GenQuaternionRotationMat4x4(rotationQuat, rMat);
    GenScaleMat4x4(scale, sMat);

    MatrixMul4x4(tMat, rMat, trMat);
    MatrixMul4x4(trMat, sMat, pResMat);
}

template<typename T>
inline bool ExtractEulerZXYFromTRSMatrix(const T mat[16], T outRotation[3], T outScale[3], T outTranslation[3])
{
    outTranslation[0] = mat[3];
    outTranslation[1] = mat[7];
    outTranslation[2] = mat[11];

    outScale[0] = static_cast<T>(sqrt(mat[0] * mat[0] + mat[4] * mat[4] + mat[8] * mat[8]));
    outScale[1] = static_cast<T>(sqrt(mat[1] * mat[1] + mat[5] * mat[5] + mat[9] * mat[9]));
    outScale[2] = static_cast<T>(sqrt(mat[2] * mat[2] + mat[6] * mat[6] + mat[10] * mat[10]));

    if (outScale[0] == static_cast<T>(0) ||
        outScale[1] == static_cast<T>(0) ||
        outScale[2] == static_cast<T>(0))
    {
        outRotation[0] = static_cast<T>(0);
        outRotation[1] = static_cast<T>(0);
        outRotation[2] = static_cast<T>(0);
        return false;
    }

    const T r00 = mat[0] / outScale[0];
    const T r01 = mat[1] / outScale[1];
    const T r02 = mat[2] / outScale[2];

    const T r10 = mat[4] / outScale[0];
    const T r11 = mat[5] / outScale[1];
    const T r12 = mat[6] / outScale[2];

    const T r20 = mat[8] / outScale[0];
    const T r21 = mat[9] / outScale[1];
    const T r22 = mat[10] / outScale[2];

    const T pitch = static_cast<T>(asin(std::clamp(r21, static_cast<T>(-1), static_cast<T>(1))));
    const T cosPitch = static_cast<T>(cos(pitch));

    T roll = static_cast<T>(0);
    T head = static_cast<T>(0);

    if (static_cast<T>(fabs(cosPitch)) > static_cast<T>(1e-6))
    {
        roll = static_cast<T>(atan2(-r01, r11));
        head = static_cast<T>(atan2(-r20, r22));
    }
    else
    {
        roll = static_cast<T>(atan2(r10, r00));
        head = static_cast<T>(0);
    }

    outRotation[0] = pitch;
    outRotation[1] = head;
    outRotation[2] = roll;
    return true;
}