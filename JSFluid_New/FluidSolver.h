//==================================================================
///
///
///
///
//==================================================================

#ifndef FLUIDSOLVER_H
#define FLUIDSOLVER_H

#include <algorithm>
#include <vector>

//==================================================================
template <int N>
class FluidSolver
{
    std::vector<float> mCurVel[2];
    std::vector<float> mCurDen;

#if 0
    void add_source( float *x, const float *s, float dt );
    {
        const int size = (N+2) *(N+2);

        for (int i=0 ; i < size; ++i)
            x[i] += dt * s[i];
    }
#endif

    void set_bnd( int b, float *x );
    void lin_solve( int b, float *x, const float *x0, float a, float c );
    void diffuse( int b, float *x, const float *x0, float diff, float dt );

    void advect(
        float *d,
        const float *d0,
        const float *u,
        const float *v,
        float dt );

    void project( float *u, float *v, float *p, float *div );

public:
    FluidSolver()
    {
        const auto size = (N+2) *(N+2);

        mCurVel[0].resize( size );
        mCurVel[1].resize( size );
        mCurDen.resize( size );

        Clear();
    }

    void Clear()
    {
        for (auto &x : mCurVel[0]) x = 0;
        for (auto &x : mCurVel[1]) x = 0;
        for (auto &x : mCurDen) x = 0;
    }

    static       float &SMP(      float *p, int i, int j) { return p[ i + (N+2) *j ]; }
    static const float &SMP(const float *p, int i, int j) { return p[ i + (N+2) *j ]; }
    static const float &SMP(const std::vector<float> &v, int i, int j) { return v[ i + (N+2) *j ]; }

    template <int DIM_IDX>
    const float &SMPVel(int i, int j) const { return mCurVel[DIM_IDX][ i + (N+2) *j ]; }
    template <int DIM_IDX>
          float &SMPVel(int i, int j)       { return mCurVel[DIM_IDX][ i + (N+2) *j ]; }

    const float &SMPDen(int i, int j) const { return mCurDen[ i + (N+2) *j ]; }
          float &SMPDen(int i, int j)       { return mCurDen[ i + (N+2) *j ]; }

    void dens_step( float *pTmpDen, float diff, float dt );
    void vel_step( float *u0, float *v0, float visc, float dt );
};

//==================================================================
template <int N>
void FluidSolver<N>::set_bnd( int b, float *x )
{
    for (int i=1; i <= N; ++i)
    {
        SMP(x, 0  ,   i) = b==1 ? -SMP(x, 1, i) : SMP(x, 1, i);
        SMP(x, N+1,   i) = b==1 ? -SMP(x, N, i) : SMP(x, N, i);
        SMP(x, i  ,   0) = b==2 ? -SMP(x, i, 1) : SMP(x, i, 1);
        SMP(x, i  , N+1) = b==2 ? -SMP(x, i, N) : SMP(x, i, N);
    }
    SMP(x, 0  ,0  ) = 0.5f * (SMP(x, 1, 0  ) + SMP(x, 0  , 1));
    SMP(x, 0  ,N+1) = 0.5f * (SMP(x, 1, N+1) + SMP(x, 0  , N));
    SMP(x, N+1,0  ) = 0.5f * (SMP(x, N, 0  ) + SMP(x, N+1, 1));
    SMP(x, N+1,N+1) = 0.5f * (SMP(x, N, N+1) + SMP(x, N+1, N));
}

template <int N>
void FluidSolver<N>::lin_solve( int b, float *x, const float *x0, float a, float c )
{
    float ooc = 1.f / c;

    for (int k=0 ; k < 20; ++k)
    {
        for (int i=1; i <= N; ++i)
        {
            for (int j=1; j <= N; ++j)
            {
                SMP(x,i,j) = (    SMP(x0, i  , j  ) +
                               a*(SMP(x , i-1, j  ) +
                                  SMP(x , i+1, j  ) +
                                  SMP(x , i  , j-1) +
                                  SMP(x , i  , j+1))) * ooc;
            }
        }
        set_bnd( b, x );
    }
}

template <int N>
void FluidSolver<N>::diffuse( int b, float *x, const float *x0, float diff, float dt )
{
    float a = dt * diff * N * N;

    lin_solve( b, x, x0, a, 1+4*a );
}

inline float clamp( float x, float mi, float ma )
{
    auto t = x < mi ? mi : x;
    return t > ma ? ma : t;
}

template <int N>
void FluidSolver<N>::advect(
        float *d,
        const float *d0,
        const float *u,
        const float *v,
        float dt )
{
    float dt0 = dt * N;

    for (int i=1; i <= N; ++i)
    {
        for (int j=1; j <= N; ++j)
        {
            float x = i - dt0 * SMP(u,i,j);
            float y = j - dt0 * SMP(v,i,j);

            x = clamp( x, 0.5f, N + 0.5f );

            int i0 = (int)x;
            int i1 = i0+1;

            y = clamp( y, 0.5f, N + 0.5f );

            int j0 = (int)y;
            int j1 = j0+1;

            float s1 = x - i0;
            float s0 = 1 - s1;
            float t1 = y - j0;
            float t0 = 1 - t1;

            SMP(d,i,j) = s0 * (t0 * SMP(d0,i0,j0) + t1 * SMP(d0,i0,j1)) +
                         s1 * (t0 * SMP(d0,i1,j0) + t1 * SMP(d0,i1,j1));
        }
    }
}

template <int N>
void FluidSolver<N>::project( float *u, float *v, float *p, float *div )
{
    const float sca = -0.5f / N;
    for (int i=1; i <= N; ++i)
    {
        for (int j=1; j <= N; ++j)
        {
            float dx = SMP(u, i+1, j  ) - SMP(u, i-1, j  );
            float dy = SMP(v, i  , j+1) - SMP(v, i  , j-1);

            SMP(div, i, j) = sca * (dx + dy);
            SMP(p  , i, j) = 0;
        }
    }
    set_bnd( 0, div );
    set_bnd( 0, p );

    lin_solve( 0, p, div, 1, 4 );

    for (int i=1; i <= N; ++i)
    {
        for (int j=1; j <= N; ++j)
        {
            SMP(u,i,j) -= (0.5f * N) * (SMP(p,i+1,j) - SMP(p,i-1,j));
            SMP(v,i,j) -= (0.5f * N) * (SMP(p,i,j+1) - SMP(p,i,j-1));
        }
    }
    set_bnd( 1, u );
    set_bnd( 2, v );
}

template <int N>
void FluidSolver<N>::dens_step( float *pTmpDen, float diff, float dt )
{
    auto *pCurDen = mCurDen.data();

    //add_source( pCurDen, pTmpDen, dt );

    diffuse( 0, pTmpDen, pCurDen, diff, dt );

    const auto *pCurVel0 = mCurVel[0].data();
    const auto *pCurVel1 = mCurVel[1].data();

    advect( pCurDen, pTmpDen, pCurVel0, pCurVel1, dt );
    set_bnd( 0, pCurDen );
}

template <int N>
void FluidSolver<N>::vel_step( float *u0, float *v0, float visc, float dt )
{
    auto *pCurVel0 = mCurVel[0].data();
    auto *pCurVel1 = mCurVel[1].data();

    //add_source( pCurVel0, u0, dt );
    //add_source( pCurVel1, v0, dt );

    diffuse( 1, u0, pCurVel0, visc, dt );
    diffuse( 2, v0, pCurVel1, visc, dt );

    project( u0, v0, pCurVel0, pCurVel1 );

    advect( pCurVel0, u0, u0, v0, dt );
    set_bnd( 1, pCurVel0 );

    advect( pCurVel1, v0, u0, v0, dt );
    set_bnd( 2, pCurVel1 );

    project( pCurVel0, pCurVel1, u0, v0 );
}

#endif

