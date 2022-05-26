//==================================================================
/// jsfluid_demo.cpp
///
/// Created by Davide Pasca - 2022/05/26
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <array>
#include <algorithm>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include "ImmGL.h"
#include "FluidSolver.h"

#define c_auto  const auto

using vec2 = std::array<float,2>;

template <typename T, int ROWS, int COLS>
using mtxNM = std::array< std::array<T,COLS>, ROWS>;

static const int N = 64;
static float TIME_DELTA = 0.1f;
static float DIFFUSION_RATE;
static float VISCOSITY = 0.f;
static float FORCE = 5.f;
static float SOURCE_DENSITY = N;

enum DispMode : int {
    DISPMODE_FLAT,
    DISPMODE_SMOOTH,
    DISPMODE_VEL,
    DISPMODE_N
};
static DispMode _dispMode = DISPMODE_SMOOTH;

static std::vector<char>   _tmpBuff;

using Solver = FluidSolver<N,false>;

static const int GRID_NX = 1;
static const int GRID_NY = 1;
static mtxNM<Solver,GRID_NY,GRID_NY> _solvers;

static ImmGL    *_pIGL;

//==================================================================
struct Env
{
    int win_id = 0;
    int win_x = 0;
    int win_y = 0;
    int mouse_down[3] {};
    int omx = 0;
    int omy = 0;
    int mx  = 0;
    int my  = 0;
    int modifiers = 0;
} _env;

//==================================================================
static void drawSolverLines(
        const Solver &solv,
        const vec2 &sca,
        const vec2 &off,
        float vsca )
{
    c_auto col = IColor4 { 1,1,1,1 };

    for (int i=1; i <= N ; ++i)
    {
        c_auto x = off[0] + sca[0] * (i-0.5f);

        for (int j=1; j <= N ; ++j)
        {
            c_auto y = off[1] + sca[1] * (j-0.5f);

            _pIGL->DrawLine(
                    { x,
                      y },
                    { x + vsca * solv.SMPVel<0>( i, j ),
                      y + vsca * solv.SMPVel<1>( i, j ) },
                    col );
        }
    }
}

//==================================================================
static void drawSolverFill(
        const Solver &solv,
        const vec2 &sca,
        const vec2 &off,
        bool doSmooth )
{
    c_auto n = (doSmooth ? N : N+1);

    for (int i=0; i <= n; ++i)
    {
        c_auto x = off[0] + sca[0] * (i-0.0f);

        for (int j=0; j <= n; ++j)
        {
            c_auto y = off[1] + sca[1] * (j-0.0f);

            if ( doSmooth )
            {
                auto mkcol = [&]( c_auto offX, c_auto offY ) -> IColor4
                {
                    c_auto val = solv.SMPDen( i + offX, j + offY );
                    return { val, val, val, 1 };
                };

                // note: colors array order is in zig-zag
                _pIGL->DrawRectFill( {x, y}, sca, { mkcol( 0, 0 ),
                                                    mkcol( 1, 0 ),
                                                    mkcol( 0, 1 ),
                                                    mkcol( 1, 1 ) });
            }
            else
            {
                c_auto d00 = solv.SMPDen( i, j );

                c_auto cr = 0.f;
                c_auto cg = 1.f;
                c_auto cb = 0.f;

                auto ar = 0.f;
                auto ab = 0.f;

                if ( i==0 || j==0 ) { /*cg = 0; cb = 0;*/ ar = 0.4f; } else
                if ( i==n || j==n ) { /*cr = 0; cb = 0;*/ ab = 0.4f; }

                c_auto col = IColor4{ d00 * cr + ar,
                                      d00 * cg,
                                      d00 * cb + ab,
                                      1.f };

                _pIGL->DrawRectFill( {x, y}, sca, col );
            }
        }
    }
}

//==================================================================
static void draw_velocity()
{
    vec2 sca { 1.f / (N+2) / GRID_NX,
               1.f / (N+2) / GRID_NY };

    for (int i=0; i != GRID_NY; ++i)
    {
        for (int j=0; j != GRID_NX; ++j)
        {
            drawSolverLines(
                _solvers[i][j],
                sca,
                {(float)j/GRID_NX,
                 (float)i/GRID_NY},
                1.f );
        }
    }
}

//==================================================================
static void draw_density( bool doSmooth )
{
    vec2 sca { 1.f / (N+2) / GRID_NX,
               1.f / (N+2) / GRID_NY };

    _pIGL->SetBlendAdd();

    for (int i=0; i != GRID_NY; ++i)
    {
        for (int j=0; j != GRID_NX; ++j)
        {
            //if ( i!=1 || j!=1 ) continue;

            drawSolverFill(
                _solvers[i][j],
                sca,
                {(float)j/GRID_NX,
                 (float)i/GRID_NY},
                doSmooth );
        }
    }

    _pIGL->SetBlendNone();
}

//==================================================================
static void get_from_UI()
{
	if ( !_env.mouse_down[GLUT_LEFT_BUTTON] &&
         !_env.mouse_down[GLUT_RIGHT_BUTTON] )
        return;

    const float dt = TIME_DELTA;

	c_auto mouseX_WS = (             _env.mx) / (float)_env.win_x;
	c_auto mouseY_WS = (_env.win_y - _env.my) / (float)_env.win_y;

    c_auto cellW_WS = 1.f / GRID_NX;
    c_auto cellH_WS = 1.f / GRID_NY;

    if ( mouseX_WS < 0 || mouseX_WS > 1.f || mouseY_WS < 0 || mouseY_WS > 1.f )
    {
        _env.omx = _env.mx;
        _env.omy = _env.my;
        return;
    }

    c_auto cell_X = GRID_NX * (mouseX_WS - 0.f);
    c_auto cell_Y = GRID_NY * (mouseY_WS - 0.f);
    c_auto cell_IX = std::min( (int)cell_X, GRID_NX-1 );
    c_auto cell_IY = std::min( (int)cell_Y, GRID_NY-1 );

    auto &solv = _solvers[cell_IY][cell_IX];

    c_auto samp_IX = (int)((mouseX_WS * GRID_NX - cell_IX) * (N+2));
    c_auto samp_IY = (int)((mouseY_WS * GRID_NY - cell_IY) * (N+2));

    // don't apply to the borders
    if ( samp_IX < 1 || samp_IX > N || samp_IY < 1 || samp_IY > N )
    {
        _env.omx = _env.mx;
        _env.omy = _env.my;
        return;
    }

    // draw density if CTRL is pressed or it's right button, otherwise do velocity
    if ( !!(_env.modifiers & GLUT_ACTIVE_CTRL) || _env.mouse_down[GLUT_RIGHT_BUTTON] )
    {
        solv.SMPDen( samp_IX, samp_IY ) += SOURCE_DENSITY * dt;
    }
    else
    {
        vec2 vel { (float)_env.mx  - _env.omx,
                   (float)_env.omy - _env.my };

        solv.SMPVel<0>( samp_IX, samp_IY ) += FORCE * vel[0] * dt;
        solv.SMPVel<1>( samp_IX, samp_IY ) += FORCE * vel[1] * dt;
    }

    _env.omx = _env.mx;
    _env.omy = _env.my;
}

//==================================================================
static void key_func( unsigned char key, int x, int y )
{
	switch ( key )
	{
		case 'c':
		case 'C':
            for (int i=0; i != GRID_NY; ++i)
                for (int j=0; j != GRID_NX; ++j)
                    _solvers[i][j].Clear();
			break;

		case 'q':
		case 'Q':
			exit( 0 );
			break;

		case 'v':
		case 'V':
            _dispMode = (DispMode)((int)_dispMode + 1);
            if ( _dispMode == DISPMODE_N )
                _dispMode = (DispMode)0;
			break;
	}
}

static void mouse_func( int button, int state, int x, int y )
{
	_env.omx = _env.mx = x;
	_env.omy = _env.my = y;

	_env.mouse_down[button] = (state == GLUT_DOWN);

    _env.modifiers = glutGetModifiers();
}

static void motion_func( int x, int y )
{
	_env.mx = x;
	_env.my = y;
}

static void reshape_func( int width, int height )
{
	glutSetWindow( _env.win_id );
	glutReshapeWindow( width, height );

	_env.win_x = width;
	_env.win_y = height;
}

static void idle_func()
{
	get_from_UI();

    for (int i=0; i != GRID_NY; ++i)
    {
        for (int j=0; j != GRID_NX; ++j)
        {
        	_solvers[i][j].vel_step( _tmpBuff.data(), VISCOSITY, TIME_DELTA );
        	_solvers[i][j].dens_step( _tmpBuff.data(), DIFFUSION_RATE, TIME_DELTA );
        }
    }

	glutSetWindow( _env.win_id );
	glutPostRedisplay();
}

//==================================================================
static void display_func()
{
    // setup viewport and trasnforms
	glViewport( 0, 0, _env.win_x, _env.win_y );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	gluOrtho2D( 0.0, 1.0, 0.0, 1.0 );
	glMatrixMode( GL_MODELVIEW );

	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT );

    // do render
    _pIGL->ResetStates();

    switch ( _dispMode )
    {
    case DISPMODE_FLAT:   draw_density( false ); break;
    case DISPMODE_SMOOTH: draw_density( true ); break;
    case DISPMODE_VEL:    draw_velocity(); break;
    default: break;
    }

    _pIGL->FlushPrims();

    // present the rendering buffer
    glutSwapBuffers();
}

//==================================================================
static void logErr( const char *fmt, ...)
{
    char buffer[1024] {};
    va_list args;
    va_start( args, fmt );
    vsnprintf( buffer, 1024, fmt, args );
    va_end(args );

    printf( "[ERR] " );
    puts( buffer );
}

//==================================================================
static void logMsg( const char *fmt, ...)
{
    char buffer[1024] {};
    va_list args;
    va_start( args, fmt );
    vsnprintf( buffer, 1024, fmt, args );
    va_end( args );

    puts( buffer );
}

//==================================================================
int main( int argc, char ** argv )
{
	if ( argc != 1 && argc != 6 ) {
		logErr( "usage : %s N dt diff visc force source", argv[0] );
		logErr( "where:" );
		logErr( "\t N      : grid resolution" );
		logErr( "\t dt     : time step" );
		logErr( "\t diff   : diffusion rate of the density" );
		logErr( "\t visc   : viscosity of the fluid" );
		logErr( "\t force  : scales the mouse movement that generate a force" );
		logErr( "\t source : amount of density that will be deposited" );
		exit( 1 );
	}

	if ( argc == 1 ) {
		//N = 64;
		TIME_DELTA      = 0.1f;
		DIFFUSION_RATE  = 0.0f;
		VISCOSITY       = 0.0f;
		FORCE           = 5.0f;
		SOURCE_DENSITY  = 100.0f;
		logMsg( "Using defaults : N=%d dt=%g diff=%g visc=%g force = %g source=%g",
			N, TIME_DELTA, DIFFUSION_RATE, VISCOSITY, FORCE, SOURCE_DENSITY );
	} else {
		//N = atoi(argv[1]);
		TIME_DELTA      = (float)atof(argv[2]);
		DIFFUSION_RATE  = (float)atof(argv[3]);
		VISCOSITY       = (float)atof(argv[4]);
		FORCE           = (float)atof(argv[5]);
		SOURCE_DENSITY  = (float)atof(argv[6]);
	}

	logMsg( "\n\nHow to use this demo:" );
	logMsg( "\t Add densities: mouse right-button or left-button + CTRL" );
	logMsg( "\t Add velocities: move the mouse while pressing left-button" );
	logMsg( "\t Toggle density/velocity display with the 'v' key" );
	logMsg( "\t Clear the simulation by pressing the 'c' key" );
	logMsg( "\t Quit by pressing the 'q' key" );

    _tmpBuff.resize( _solvers[0][0].GetTempBuffMaxSize() );

	_env.win_x = 512;
	_env.win_y = 512;

	glutInit( &argc, argv );

    glutInitContextVersion( 3, 2 );
    glutInitContextProfile( GLUT_CORE_PROFILE );

	glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE );
	glutInitWindowPosition( 200, 200 );
	glutInitWindowSize( _env.win_x, _env.win_y );
	_env.win_id = glutCreateWindow( "Fluid Test" );

	glutKeyboardFunc( key_func );
	glutMouseFunc( mouse_func );
	glutMotionFunc( motion_func );
	glutReshapeFunc( reshape_func );
	glutIdleFunc( idle_func );
	glutDisplayFunc( display_func );

    if ( glewInit() )
    {
        logMsg( "Failed to initialize GLEW" );
        exit( 1 );
    }


    ImmGL immGL;
    _pIGL = &immGL;

	glutMainLoop();

	exit( 0 );
}

