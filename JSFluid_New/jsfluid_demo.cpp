//==================================================================
///
///
///
///
//==================================================================

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <GL/glut.h>
#include "FluidSolver.h"

static const int N = 64;
static float dt, diff, visc;
static float force, source;
static int dvel;

//==================================================================
//template <size_t DIMS>
struct SimData
{
    using vec = std::vector<float>;

    vec u;
    vec v;
    vec u_prev;
    vec v_prev;
    vec dens;
    vec dens_prev;

    void Alloc( int n )
    {
        u.resize( n );
        v.resize( n );
        u_prev.resize( n );
        v_prev.resize( n );
        dens.resize( n );
        dens_prev.resize( n );
    }

    void Clear()
    {
        fillZero( u );
        fillZero( v );
        fillZero( u_prev );
        fillZero( v_prev );
        fillZero( dens );
        fillZero( dens_prev );
    }

private:
    void fillZero( std::vector<float> &v ) { for (auto &x : v) x = 0; }
};

static SimData gsSimData;

static int win_id;
static int win_x, win_y;
static int mouse_down[3];
static int omx, omy, mx, my;

//==================================================================
static FluidSolver<N> gsSolver;

//==================================================================
static void pre_display()
{
	glViewport ( 0, 0, win_x, win_y );
	glMatrixMode ( GL_PROJECTION );
	glLoadIdentity ();
	gluOrtho2D ( 0.0, 1.0, 0.0, 1.0 );
	glClearColor ( 0.0f, 0.0f, 0.0f, 1.0f );
	glClear ( GL_COLOR_BUFFER_BIT );
}

//==================================================================
static void draw_velocity()
{
	float h = 1.0f/N;

	glColor3f ( 1.0f, 1.0f, 1.0f );
	glLineWidth ( 1.0f );

	glBegin ( GL_LINES );

		for (int i=1 ; i<=N ; i++ )
        {
			float x = (i-0.5f)*h;

			for (int j=1 ; j<=N ; j++ )
            {
				float y = (j-0.5f)*h;

				glVertex2f( x, y );
				glVertex2f(
                    x + gsSolver.SMP( gsSimData.u, i, j ),
                    y + gsSolver.SMP( gsSimData.v, i, j ) );
			}
		}

	glEnd ();
}

//==================================================================
static void draw_density()
{
	float h = 1.0f/N;

	glBegin ( GL_QUADS );

		for (int i=0 ; i<=N ; i++ )
        {
			float x = (i-0.5f)*h;
			for (int j=0 ; j<=N ; j++ )
            {
				float y = (j-0.5f)*h;

				float d00 = gsSolver.SMP( gsSimData.dens, i  , j   );
				float d01 = gsSolver.SMP( gsSimData.dens, i  , j+1 );
				float d10 = gsSolver.SMP( gsSimData.dens, i+1, j   );
				float d11 = gsSolver.SMP( gsSimData.dens, i+1, j+1 );

				glColor3f( d00, d00, d00 ); glVertex2f( x, y );
				glColor3f( d10, d10, d10 ); glVertex2f( x+h, y );
				glColor3f( d11, d11, d11 ); glVertex2f( x+h, y+h );
				glColor3f( d01, d01, d01 ); glVertex2f( x, y+h );
			}
		}

	glEnd ();
}

//==================================================================
static void get_from_UI ( float * d, float * u, float * v )
{
	int i, j, size = (N+2)*(N+2);

	for ( i=0 ; i<size ; i++ ) {
		u[i] = v[i] = d[i] = 0.0f;
	}

	if ( !mouse_down[0] && !mouse_down[2] ) return;

	i = (int)((       mx /(float)win_x)*N+1);
	j = (int)(((win_y-my)/(float)win_y)*N+1);

	if ( i<1 || i>N || j<1 || j>N ) return;

	if ( mouse_down[0] ) {
		gsSolver.SMP( u, i, j ) = force * (mx-omx);
		gsSolver.SMP( v, i, j ) = force * (omy-my);
	}

	if ( mouse_down[2] ) {
		gsSolver.SMP( d, i, j ) = source;
	}

	omx = mx;
	omy = my;

	return;
}

//==================================================================
static void key_func ( unsigned char key, int x, int y )
{
	switch ( key )
	{
		case 'c':
		case 'C':
			gsSimData.Clear();
			break;

		case 'q':
		case 'Q':
			exit ( 0 );
			break;

		case 'v':
		case 'V':
			dvel = !dvel;
			break;
	}
}

static void mouse_func ( int button, int state, int x, int y )
{
	omx = mx = x;
	omx = my = y;

	mouse_down[button] = state == GLUT_DOWN;
}

static void motion_func ( int x, int y )
{
	mx = x;
	my = y;
}

static void reshape_func ( int width, int height )
{
	glutSetWindow ( win_id );
	glutReshapeWindow ( width, height );

	win_x = width;
	win_y = height;
}

static void idle_func()
{
	get_from_UI(
            gsSimData.dens_prev.data(),
            gsSimData.u_prev.data(),
            gsSimData.v_prev.data() );

	gsSolver.vel_step(
            gsSimData.u.data(),
            gsSimData.v.data(),
            gsSimData.u_prev.data(),
            gsSimData.v_prev.data(),
            visc,
            dt );

	gsSolver.dens_step(
            gsSimData.dens.data(),
            gsSimData.dens_prev.data(),
            gsSimData.u.data(),
            gsSimData.v.data(),
            diff,
            dt );

	glutSetWindow ( win_id );
	glutPostRedisplay ();
}

//==================================================================
static void display_func()
{
	pre_display ();

		if ( dvel ) draw_velocity ();
		else		draw_density ();

	glutSwapBuffers();
}


//==================================================================
static void open_glut_window ( void )
{
	glutInitDisplayMode ( GLUT_RGBA | GLUT_DOUBLE );

	glutInitWindowPosition ( 200, 200 );
	glutInitWindowSize ( win_x, win_y );
	win_id = glutCreateWindow ( "Fluid Test" );

	glClearColor ( 0.0f, 0.0f, 0.0f, 1.0f );
	glClear ( GL_COLOR_BUFFER_BIT );
	glutSwapBuffers ();
	glClear ( GL_COLOR_BUFFER_BIT );
	glutSwapBuffers ();

	pre_display ();

	glutKeyboardFunc ( key_func );
	glutMouseFunc ( mouse_func );
	glutMotionFunc ( motion_func );
	glutReshapeFunc ( reshape_func );
	glutIdleFunc ( idle_func );
	glutDisplayFunc ( display_func );
}

//==================================================================
int main ( int argc, char ** argv )
{
	glutInit ( &argc, argv );

	if ( argc != 1 && argc != 6 ) {
		fprintf ( stderr, "usage : %s N dt diff visc force source\n", argv[0] );
		fprintf ( stderr, "where:\n" );\
		fprintf ( stderr, "\t N      : grid resolution\n" );
		fprintf ( stderr, "\t dt     : time step\n" );
		fprintf ( stderr, "\t diff   : diffusion rate of the density\n" );
		fprintf ( stderr, "\t visc   : viscosity of the fluid\n" );
		fprintf ( stderr, "\t force  : scales the mouse movement that generate a force\n" );
		fprintf ( stderr, "\t source : amount of density that will be deposited\n" );
		exit ( 1 );
	}

	if ( argc == 1 ) {
		//N = 64;
		dt = 0.1f;
		diff = 0.0f;
		visc = 0.0f;
		force = 5.0f;
		source = 100.0f;
		fprintf ( stderr, "Using defaults : N=%d dt=%g diff=%g visc=%g force = %g source=%g\n",
			N, dt, diff, visc, force, source );
	} else {
		//N = atoi(argv[1]);
		dt = atof(argv[2]);
		diff = atof(argv[3]);
		visc = atof(argv[4]);
		force = atof(argv[5]);
		source = atof(argv[6]);
	}

	printf ( "\n\nHow to use this demo:\n\n" );
	printf ( "\t Add densities with the right mouse button\n" );
	printf ( "\t Add velocities with the left mouse button and dragging the mouse\n" );
	printf ( "\t Toggle density/velocity display with the 'v' key\n" );
	printf ( "\t Clear the simulation by pressing the 'c' key\n" );
	printf ( "\t Quit by pressing the 'q' key\n" );

	dvel = 0;

    gsSimData.Alloc( (N+2)*(N+2) );
    gsSimData.Clear();

	win_x = 512;
	win_y = 512;
	open_glut_window ();

	glutMainLoop ();

	exit ( 0 );
}

