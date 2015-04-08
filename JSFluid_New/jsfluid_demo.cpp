//==================================================================
///
///
///
///
//==================================================================

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <array>
#include <GL/glut.h>
#include "FluidSolver.h"

using vec2 = std::array<float,2>;

template <typename T, int ROWS, int COLS>
using mtxNM = std::array< std::array<T,COLS>, ROWS>;

static const int N = 32;
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
static DispMode _dispMode = DISPMODE_FLAT;

static std::vector<char>   _tmpBuff;

using Solver = FluidSolver<N,false>;

static const int GRID_NX = 2;
static const int GRID_NY = 2;
static mtxNM<Solver,GRID_NY,GRID_NY> _solvers;

//==================================================================
struct Env
{
    int win_id = 0;
    int win_x = 0;
    int win_y = 0;
    int mouse_down[3];
    int omx = 0;
    int omy = 0;
    int mx  = 0;
    int my  = 0;
    int modifiers = 0;

    Env()
    {
        for (auto &x : mouse_down) x = 0;
    }
} _env;

//==================================================================
static void pre_display()
{
	glViewport( 0, 0, _env.win_x, _env.win_y );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	gluOrtho2D( 0.0, 1.0, 0.0, 1.0 );
	glMatrixMode( GL_MODELVIEW );

	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT );
}

//==================================================================
static void drawSolverLines(
        const Solver &solv,
        const vec2 &sca,
        const vec2 &off,
        float vsca )
{
	glLineWidth( 1.0f );

	glBegin( GL_LINES );

    for (int i=1; i <= N ; ++i)
    {
        float x = off[0] + sca[0] * (i-0.5f);

        for (int j=1; j <= N ; ++j)
        {
            float y = off[1] + sca[1] * (j-0.5f);

            glVertex2f( x, y );

            glVertex2f(
                x + vsca * solv.SMPVel<0>( i, j ),
                y + vsca * solv.SMPVel<1>( i, j ) );
        }
    }

	glEnd();
}

//==================================================================
static void drawSolverFill(
        const Solver &solv,
        const vec2 &sca,
        const vec2 &off,
        bool doSmooth )
{
	glBegin( GL_QUADS );

    int n = (doSmooth ? N : N+1);

    for (int i=0; i <= n; ++i)
    {
        float x = off[0] + sca[0] * (i-0.0f);

        for (int j=0; j <= n; ++j)
        {
            float y = off[1] + sca[1] * (j-0.0f);

            if ( doSmooth )
            {
                float d00 = solv.SMPDen( i  , j   );
                float d01 = solv.SMPDen( i  , j+1 );
                float d10 = solv.SMPDen( i+1, j   );
                float d11 = solv.SMPDen( i+1, j+1 );

                glColor3f( d00, d00, d00 ); glVertex2f( x, y );
                glColor3f( d10, d10, d10 ); glVertex2f( x+sca[0], y );
                glColor3f( d11, d11, d11 ); glVertex2f( x+sca[0], y+sca[1] );
                glColor3f( d01, d01, d01 ); glVertex2f( x, y+sca[1] );
            }
            else
            {
                float d00 = solv.SMPDen( i, j );

                float cr = 0;
                float cg = 1;
                float cb = 0;

                float ar = 0;
                float ab = 0;

                if ( i==0 || j==0 ) { /*cg = 0; cb = 0;*/ ar = 0.4f; } else
                if ( i==n || j==n ) { /*cr = 0; cb = 0;*/ ab = 0.4f; }

                glColor3f( d00 * cr + ar, d00 * cg, d00 * cb + ab );

                glVertex2f( x, y );
                glVertex2f( x+sca[0], y );
                glVertex2f( x+sca[0], y+sca[1] );
                glVertex2f( x, y+sca[1] );
            }
        }
    }

	glEnd();
}

//==================================================================
static void draw_velocity()
{
    vec2 sca { 1.f / (N+2) / GRID_NX,
               1.f / (N+2) / GRID_NY };

	glColor3f( 1.0f, 1.0f, 1.0f );

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

    glEnable( GL_BLEND );
    glBlendFunc( GL_ONE, GL_ONE );

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

    glDisable( GL_BLEND );
}

//==================================================================
static void get_from_UI()
{
	if ( !_env.mouse_down[0] ) return;

    const float dt = TIME_DELTA;

	float mouseX_WS = (             _env.mx) / (float)_env.win_x;
	float mouseY_WS = (_env.win_y - _env.my) / (float)_env.win_y;

    float cellW_WS = 1.f / GRID_NX;
    float cellH_WS = 1.f / GRID_NY;

    if ( mouseX_WS < 0 || mouseX_WS > 1.f || mouseY_WS < 0 || mouseY_WS > 1.f )
    {
        _env.omx = _env.mx;
        _env.omy = _env.my;
        return;
    }

    int cell_X = GRID_NX * (mouseX_WS - 0.f);
    int cell_Y = GRID_NY * (mouseY_WS - 0.f);
    int cell_IX = (int)cell_X;
    int cell_IY = (int)cell_Y;

    auto &solv = _solvers[cell_IY][cell_IX];

    int samp_IX = (int)((mouseX_WS * GRID_NX - cell_IX) * (N+2));
    int samp_IY = (int)((mouseY_WS * GRID_NY - cell_IY) * (N+2));

    // don't apply to the borders
    if ( samp_IX < 1 || samp_IX > N || samp_IY < 1 || samp_IY > N )
    {
        _env.omx = _env.mx;
        _env.omy = _env.my;
        return;
    }

    // draw density if CTRL is pressed, otherwise do velocity
    if ( !!(_env.modifiers & GLUT_ACTIVE_CTRL) )
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
    pre_display();

    switch ( _dispMode )
    {
    case DISPMODE_FLAT:   draw_density( false ); break;
    case DISPMODE_SMOOTH: draw_density( true ); break;
    case DISPMODE_VEL:    draw_velocity(); break;
    default: break;
    }

    glutSwapBuffers();
}

//==================================================================
static void open_glut_window( void )
{
	glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE );

	glutInitWindowPosition( 200, 200 );
	glutInitWindowSize( _env.win_x, _env.win_y );
	_env.win_id = glutCreateWindow( "Fluid Test" );

	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT );
	glutSwapBuffers();
	glClear( GL_COLOR_BUFFER_BIT );
	glutSwapBuffers();

	pre_display();

	glutKeyboardFunc( key_func );
	glutMouseFunc( mouse_func );
	glutMotionFunc( motion_func );
	glutReshapeFunc( reshape_func );
	glutIdleFunc( idle_func );
	glutDisplayFunc( display_func );
}

//==================================================================
int main( int argc, char ** argv )
{
	glutInit( &argc, argv );

	if ( argc != 1 && argc != 6 ) {
		fprintf( stderr, "usage : %s N dt diff visc force source\n", argv[0] );
		fprintf( stderr, "where:\n" );\
		fprintf( stderr, "\t N      : grid resolution\n" );
		fprintf( stderr, "\t dt     : time step\n" );
		fprintf( stderr, "\t diff   : diffusion rate of the density\n" );
		fprintf( stderr, "\t visc   : viscosity of the fluid\n" );
		fprintf( stderr, "\t force  : scales the mouse movement that generate a force\n" );
		fprintf( stderr, "\t source : amount of density that will be deposited\n" );
		exit( 1 );
	}

	if ( argc == 1 ) {
		//N = 64;
		TIME_DELTA = 0.1f;
		DIFFUSION_RATE = 0.0f;
		VISCOSITY = 0.0f;
		SOURCE_DENSITY = 100.0f;
		fprintf( stderr, "Using defaults : N=%d dt=%g diff=%g visc=%g force = %g source=%g\n",
			N, TIME_DELTA, DIFFUSION_RATE, VISCOSITY, FORCE, SOURCE_DENSITY );
	} else {
		//N = atoi(argv[1]);
		TIME_DELTA = atof(argv[2]);
		DIFFUSION_RATE = atof(argv[3]);
		VISCOSITY = atof(argv[4]);
		FORCE = atof(argv[5]);
		SOURCE_DENSITY = atof(argv[6]);
	}

	printf( "\n\nHow to use this demo:\n\n" );
	printf( "\t Add densities with the right mouse button\n" );
	printf( "\t Add velocities with the left mouse button and dragging the mouse\n" );
	printf( "\t Toggle density/velocity display with the 'v' key\n" );
	printf( "\t Clear the simulation by pressing the 'c' key\n" );
	printf( "\t Quit by pressing the 'q' key\n" );

    _tmpBuff.resize( _solvers[0][0].GetTempBuffMaxSize() );
    //for (auto &x : _tmpBuff) x = 0;

	_env.win_x = 512;
	_env.win_y = 512;
	open_glut_window();

	glutMainLoop();

	exit( 0 );
}

