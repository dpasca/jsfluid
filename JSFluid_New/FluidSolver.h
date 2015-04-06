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
template <int N, bool DO_BOUND>
class FluidSolver
{
    static const int DIMS_N = 2;
    static const int RELAX_ITER_COUNT = 20;

    std::vector<float> mCurVel[DIMS_N];
    std::vector<float> mCurDen;

    enum BType
    {
        BTYPE_REPEL0, // repel on x
        BTYPE_REPEL1, // repel on y
        BTYPE_EXPAND  // spill onto the border
    };

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
        for (int i=0; i < DIMS_N; ++i)
            for (auto &x : mCurVel[i])
                x = 0;

        for (auto &x : mCurDen)
            x = 0;
    }

    static size_t GetTempBuffMaxSize()
    {
        return std::max(
                getVelCoordBuffSize() * DIMS_N,
                getDenBuffSize() );
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

    void dens_step( char *pTmpBuff, float diff, float dt );
    void vel_step( char *pTmpBuff, float visc, float dt );

private:
    void setBoundary( BType b, float *x );
    void lin_solve( BType b, float *x, const float *x0, float a, float c );
    void diffuse( BType b, float *x, const float *x0, float diff, float dt );

    void advect(
        float *d,
        const float *d0,
        const float *u,
        const float *v,
        float dt );

    void project( float *u, float *v, float *p, float *div );

    static size_t getVelCoordBuffSize() { return (N+2) * (N+2) * sizeof(float); }
    static size_t getDenBuffSize()      { return (N+2) * (N+2) * sizeof(float); }
};

//==================================================================
template <int N, bool DO_BOUND>
void FluidSolver<N,DO_BOUND>::setBoundary( BType b, float *x )
{
    for (int i=1; i <= N; ++i)
    {
        SMP(x, 0  ,   i) = b==BTYPE_REPEL0 ? -SMP(x, 1, i) : SMP(x, 1, i);
        SMP(x, N+1,   i) = b==BTYPE_REPEL0 ? -SMP(x, N, i) : SMP(x, N, i);
        SMP(x, i  ,   0) = b==BTYPE_REPEL1 ? -SMP(x, i, 1) : SMP(x, i, 1);
        SMP(x, i  , N+1) = b==BTYPE_REPEL1 ? -SMP(x, i, N) : SMP(x, i, N);
    }
    SMP(x, 0  ,0  ) = 0.5f * (SMP(x, 1, 0  ) + SMP(x, 0  , 1));
    SMP(x, 0  ,N+1) = 0.5f * (SMP(x, 1, N+1) + SMP(x, 0  , N));
    SMP(x, N+1,0  ) = 0.5f * (SMP(x, N, 0  ) + SMP(x, N+1, 1));
    SMP(x, N+1,N+1) = 0.5f * (SMP(x, N, N+1) + SMP(x, N+1, N));
}

template <int N, bool DO_BOUND>
void FluidSolver<N,DO_BOUND>::lin_solve( BType b, float *x, const float *x0, float a, float c )
{
    // Gauss-Seidel relaxation:
    //  http://en.wikipedia.org/wiki/Gauss%E2%80%93Seidel_method

    float ooc = 1.f / c;

    for (int k=0 ; k < RELAX_ITER_COUNT; ++k)
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
        if ( DO_BOUND ) setBoundary( b, x );
    }
}

template <int N, bool DO_BOUND>
void FluidSolver<N,DO_BOUND>::diffuse( BType b, float *x, const float *x0, float diff, float dt )
{
    float a = dt * diff * N * N;

    lin_solve( b, x, x0, a, 1+4*a );
}

inline float clamp( float x, float mi, float ma )
{
    auto t = x < mi ? mi : x;
    return t > ma ? ma : t;
}

template <int N, bool DO_BOUND>
void FluidSolver<N,DO_BOUND>::advect(
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

template <int N, bool DO_BOUND>
void FluidSolver<N,DO_BOUND>::project( float *u, float *v, float *p, float *div )
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
    if ( DO_BOUND ) setBoundary( BTYPE_EXPAND, div );
    if ( DO_BOUND ) setBoundary( BTYPE_EXPAND, p );

    lin_solve( BTYPE_EXPAND, p, div, 1, 4 );

    for (int i=1; i <= N; ++i)
    {
        for (int j=1; j <= N; ++j)
        {
            SMP(u,i,j) -= (0.5f * N) * (SMP(p,i+1,j) - SMP(p,i-1,j));
            SMP(v,i,j) -= (0.5f * N) * (SMP(p,i,j+1) - SMP(p,i,j-1));
        }
    }
    if ( DO_BOUND ) setBoundary( BTYPE_REPEL0, u );
    if ( DO_BOUND ) setBoundary( BTYPE_REPEL1, v );
}

template <int N, bool DO_BOUND>
void FluidSolver<N,DO_BOUND>::dens_step( char *pTmpBuff, float diff, float dt )
{
    auto *pCurDen = mCurDen.data();
    auto *pTmpDen = (float *)pTmpBuff;

    diffuse( BTYPE_EXPAND, pTmpDen, pCurDen, diff, dt );

    const auto *pCurVel0 = mCurVel[0].data();
    const auto *pCurVel1 = mCurVel[1].data();

    advect( pCurDen, pTmpDen, pCurVel0, pCurVel1, dt );
    if ( DO_BOUND ) setBoundary( BTYPE_EXPAND, pCurDen );
}

template <int N, bool DO_BOUND>
void FluidSolver<N,DO_BOUND>::vel_step( char *pTmpBuff, float visc, float dt )
{
    auto *pTmpVel0 = (float *)pTmpBuff;
    auto *pTmpVel1 = (float *)(pTmpBuff + getVelCoordBuffSize());

    auto *pCurVel0 = mCurVel[0].data();
    auto *pCurVel1 = mCurVel[1].data();

    diffuse( BTYPE_REPEL0, pTmpVel0, pCurVel0, visc, dt );
    diffuse( BTYPE_REPEL1, pTmpVel1, pCurVel1, visc, dt );

    project( pTmpVel0, pTmpVel1, pCurVel0, pCurVel1 );

    advect( pCurVel0, pTmpVel0, pTmpVel0, pTmpVel1, dt );
    if ( DO_BOUND ) setBoundary( BTYPE_REPEL0, pCurVel0 );

    advect( pCurVel1, pTmpVel1, pTmpVel0, pTmpVel1, dt );
    if ( DO_BOUND ) setBoundary( BTYPE_REPEL1, pCurVel1 );

    project( pCurVel0, pCurVel1, pTmpVel0, pTmpVel1 );
}

#endif

