/*****************************************************************/
/*    Copyright (c) 2016, Stanford University and the Authors    */
/*    Author: Robert McGibbon <rmcgibbo@gmail.com>               */
/*    Contributors:                                              */
/*                                                               */
/*****************************************************************/
#ifndef MIXTAPE_CPU_LOGSUMEXP_H
#define MIXTAPE_CPU_LOGSUMEXP_H

#if defined(_M_IX86) || defined(_M_X64) || defined(__x86_64__) || defined(__i386) || (defined(_M_ARM64) && defined(USE_SOFT_INTRINSICS))
#include <emmintrin.h>
#else
#include "sse2neon.h"
#endif

#include "float.h"
#include <stdio.h>
#include <math.h>

// Please define USE_SSE2 before using sse_mathfun.h
#define USE_SSE2
#include "sse_mathfun.h"

#if defined(_MSC_VER)
#define _ALIGNED(x) __declspec(align(x))
#else
#if defined(__GNUC__)
#define _ALIGNED(x) __attribute__ ((aligned(x)))
#endif
#endif

template <typename REAL>
static inline REAL realmax(REAL v1, REAL v2) {
    return (((v1) > (v2)) ? (v1) : (v2));
}

static float logsumexp2(float v1, float v2) {
    float max = (((v1) > (v2)) ? (v1) : (v2));
    return log(exp(v1-max) + exp(v2-max)) + max;
}


static double logsumexp(const double* __restrict buf, int N) {
    int i;
    double sum = 0;
    double max = buf[0];

    for (i = 1; i < N; i++)
        if (buf[i] > max)
            max = buf[i];

    for (i = 0; i < N; i++)
        sum += exp(buf[i] - max);

    return log(sum) + max;
}


static float logsumexp(const float* __restrict buf, int N) {
    int nu = (( N >> 2 ) << 2 );
    const float* StX = buf + nu;
    float sum = 0;
    float max = -FLT_MAX;
    const float* X;
    _ALIGNED(16) float max4[4]  = {0};
    __m128 _v;
    __m128 _m;

    if (N == 1)
        return buf[0];
    if (N == 2) {
        max = realmax(buf[0], buf[1]);
        return log(exp(buf[0] - max) + exp(buf[1] - max)) + max;
    } if (N == 3) {
        max = realmax(realmax(buf[0], buf[1]), buf[2]);
        return log(exp(buf[0] - max) + exp(buf[1] - max) + exp(buf[2] - max)) + max;
    }
    
    if (N > 0) {
        X = buf;
        if (nu != 0) {
            _v = _mm_loadu_ps(X);
            X += 4;
            while (X != StX) {
                _v = _mm_max_ps(_v, _mm_loadu_ps(X));
                X += 4;
            }

            _mm_store_ps(max4, _v);
            max = realmax(realmax(realmax(max4[0], max4[1]), max4[2]), max4[3]);
        }

        for(; X < buf + N; X++)
            max = realmax(max, *X);

        X = buf;
        if (nu != 0) {
            _m = _mm_load1_ps(&max);
            _v = exp_ps(_mm_sub_ps(_mm_loadu_ps(X), _m));
            X += 4;
            while (X != StX) {
                _v = _mm_add_ps(_v, exp_ps(_mm_sub_ps(_mm_loadu_ps(X), _m)));
                _mm_store_ps(max4, _v);
                X += 4;
            }

            // horizontal add
            _v = _mm_add_ps(_v, _mm_movehl_ps(_v, _v));
            _v = _mm_add_ss(_v, _mm_shuffle_ps(_v, _v, 1));
            _mm_store_ss(&sum, _v);
        }
        for(; X < buf + N; X++)
            sum += expf(*X - max);
    }

    return log(sum) + max;
}


static float _mm_logsumexp(__m128* buf, int N) {
    int i;
    float sum = 0;
    float mymax = 0;
    _ALIGNED(16) float max4[4]  = {0};

    __m128 _v;
    __m128 _m;

    _v = buf[0];
    for (i = 1; i < N; i++)
        _v = _mm_max_ps(_v, buf[i]);

    _mm_store_ps(max4, _v);
    mymax = realmax(realmax(realmax(max4[0], max4[1]), max4[2]), max4[3]); 

    _m = _mm_load1_ps(&mymax);
    _v = exp_ps(_mm_sub_ps(buf[0], _m));
    for (i = 1; i < N; i++)
        _v = _mm_add_ps(_v, exp_ps(_mm_sub_ps(buf[i], _m)));

    // horizontal add
    _v = _mm_add_ps(_v, _mm_movehl_ps(_v, _v));
    _v = _mm_add_ss(_v, _mm_shuffle_ps(_v, _v, 1));
    _mm_store_ss(&sum, _v);

    return log(sum) + mymax;
}


#endif
