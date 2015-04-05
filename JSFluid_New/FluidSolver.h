//==================================================================
///
///
///
///
//==================================================================

#ifndef FLUIDSOLVER_H
#define FLUIDSOLVER_H

#include <algorithm>

//==================================================================
template <int SIZ>
class FluidSolver
{
    void add_source( float * x, float * s, float dt );
    void set_bnd( int b, float * x );
    void lin_solve( int b, float * x, float * x0, float a, float c );
    void diffuse( int b, float * x, float * x0, float diff, float dt );
    void advect( int b, float * d, float * d0, float * u, float * v, float dt );
    void project( float * u, float * v, float * p, float * div );

public:
    int IX(int i, int j) const { return i + (SIZ+2) * j; }

    void dens_step( float * x, float * x0, float * u, float * v, float diff, float dt );
    void vel_step( float * u, float * v, float * u0, float * v0, float visc, float dt );
};

//==================================================================
template <int SIZ>
void FluidSolver<SIZ>::add_source( float * x, float * s, float dt )
{
    int size = (SIZ+2) * (SIZ+2);
    for (int i=0 ; i<size ; i++ ) x[i] += dt*s[i];
}

template <int SIZ>
void FluidSolver<SIZ>::set_bnd( int b, float * x )
{
    int i;

    for ( i=1 ; i<=SIZ ; i++ ) {
        x[IX(0  ,i)] = b==1 ? -x[IX(1,i)] : x[IX(1,i)];
        x[IX(SIZ+1,i)] = b==1 ? -x[IX(SIZ,i)] : x[IX(SIZ,i)];
        x[IX(i,0  )] = b==2 ? -x[IX(i,1)] : x[IX(i,1)];
        x[IX(i,SIZ+1)] = b==2 ? -x[IX(i,SIZ)] : x[IX(i,SIZ)];
    }
    x[IX(0  ,0  )] = 0.5f*(x[IX(1,0  )]+x[IX(0  ,1)]);
    x[IX(0  ,SIZ+1)] = 0.5f*(x[IX(1,SIZ+1)]+x[IX(0  ,SIZ)]);
    x[IX(SIZ+1,0  )] = 0.5f*(x[IX(SIZ,0  )]+x[IX(SIZ+1,1)]);
    x[IX(SIZ+1,SIZ+1)] = 0.5f*(x[IX(SIZ,SIZ+1)]+x[IX(SIZ+1,SIZ)]);
}

template <int SIZ>
void FluidSolver<SIZ>::lin_solve( int b, float * x, float * x0, float a, float c )
{
    for (int k=0 ; k < 20; ++k)
    {
        for (int i=1; i <= SIZ; ++i)
        {
            for (int j=1; j <= SIZ; ++j)
            {
                x[IX(i,j)] = (x0[IX(i,j)] + a*(x[IX(i-1,j)]+x[IX(i+1,j)]+x[IX(i,j-1)]+x[IX(i,j+1)]))/c;
            }
        }
        set_bnd( b, x );
    }
}

template <int SIZ>
void FluidSolver<SIZ>::diffuse( int b, float * x, float * x0, float diff, float dt )
{
    float a=dt*diff*SIZ*SIZ;
    lin_solve( b, x, x0, a, 1+4*a );
}

template <int SIZ>
void FluidSolver<SIZ>::advect( int b, float * d, float * d0, float * u, float * v, float dt )
{
    int i0, j0, i1, j1;
    float x, y, s0, t0, s1, t1, dt0;

    dt0 = dt*SIZ;
    for (int i=1; i <= SIZ; ++i)
    {
        for (int j=1; j <= SIZ; ++j)
        {
            x = i-dt0*u[IX(i,j)]; y = j-dt0*v[IX(i,j)];
            if (x<0.5f) x=0.5f; if (x>SIZ+0.5f) x=SIZ+0.5f; i0=(int)x; i1=i0+1;
            if (y<0.5f) y=0.5f; if (y>SIZ+0.5f) y=SIZ+0.5f; j0=(int)y; j1=j0+1;
            s1 = x-i0; s0 = 1-s1; t1 = y-j0; t0 = 1-t1;
            d[IX(i,j)] = s0*(t0*d0[IX(i0,j0)]+t1*d0[IX(i0,j1)])+
                         s1*(t0*d0[IX(i1,j0)]+t1*d0[IX(i1,j1)]);
        }
    }
    set_bnd( b, d );
}

template <int SIZ>
void FluidSolver<SIZ>::project( float * u, float * v, float * p, float * div )
{
    for (int i=1; i <= SIZ; ++i)
    {
        for (int j=1; j <= SIZ; ++j)
        {
            div[IX(i,j)] = -0.5f*(u[IX(i+1,j)]-u[IX(i-1,j)]+v[IX(i,j+1)]-v[IX(i,j-1)])/SIZ;
            p[IX(i,j)] = 0;
        }
    }
    set_bnd( 0, div ); set_bnd( 0, p );

    lin_solve( 0, p, div, 1, 4 );

    for (int i=1; i <= SIZ; ++i)
    {
        for (int j=1; j <= SIZ; ++j)
        {
            u[IX(i,j)] -= 0.5f*SIZ*(p[IX(i+1,j)]-p[IX(i-1,j)]);
            v[IX(i,j)] -= 0.5f*SIZ*(p[IX(i,j+1)]-p[IX(i,j-1)]);
        }
    }
    set_bnd( 1, u ); set_bnd( 2, v );
}

template <int SIZ>
void FluidSolver<SIZ>::dens_step( float * x, float * x0, float * u, float * v, float diff, float dt )
{
    add_source( x, x0, dt );
    std::swap( x0, x ); diffuse( 0, x, x0, diff, dt );
    std::swap( x0, x ); advect( 0, x, x0, u, v, dt );
}

template <int SIZ>
void FluidSolver<SIZ>::vel_step( float * u, float * v, float * u0, float * v0, float visc, float dt )
{
    add_source( u, u0, dt ); add_source( v, v0, dt );
    std::swap( u0, u ); diffuse( 1, u, u0, visc, dt );
    std::swap( v0, v ); diffuse( 2, v, v0, visc, dt );
    project( u, v, u0, v0 );
    std::swap( u0, u ); std::swap( v0, v );
    advect( 1, u, u0, u0, v0, dt ); advect( 2, v, v0, u0, v0, dt );
    project( u, v, u0, v0 );
}

#endif

