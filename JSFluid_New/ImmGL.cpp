//==================================================================
/// ImmGL.cpp
///
/// Created by Davide Pasca - 2022/05/26
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <stdexcept>
#include <GL/glew.h>
#include "ImmGL.h"

#undef  c_auto
#undef  NOT
#undef  __DSHORT_FILE__

#define c_auto const auto
#define NOT(X) (!(X))

//==================================================================
inline const char *DEX_SeekFilename( const char *pStr )
{
    return
        (strrchr(pStr,'/') ? strrchr(pStr,'/')+1 : \
            (strrchr(pStr,'\\') ? strrchr(pStr,'\\')+1 : pStr) );
}

//==================================================================
#define __DSHORT_FILE__ DEX_SeekFilename( __FILE__ )

//==================================================================
inline std::string DEX_MakeString( const char *pFmt, ... )
{
    constexpr size_t BUFF_LEN = 2048;

    char buff[ BUFF_LEN ];
    va_list args;
    va_start( args, pFmt );
    vsnprintf( buff, sizeof(buff), pFmt, args );
    va_end(args);

    buff[ BUFF_LEN-1 ] = 0;

    return buff;
}

# define DEX_RUNTIME_ERROR(_FMT_,...) \
    throw std::runtime_error( \
        DEX_MakeString( "[%s:%i] " _FMT_, __DSHORT_FILE__, __LINE__, ##__VA_ARGS__ ) )

//==================================================================
#if defined(DEBUG) || defined(_DEBUG)
# define CHECKGLERR CheckGLErr(__FILE__,__LINE__)
# define FLUSHGLERR FlushGLErr()
#else
# define CHECKGLERR
# define FLUSHGLERR
#endif

//==================================================================
static const char *getErrStr( GLenum err )
{
    switch ( err )
    {
    case GL_NO_ERROR:           return "GL_NO_ERROR";
    case GL_INVALID_ENUM:       return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:      return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:  return "GL_INVALID_OPERATION";
    case GL_OUT_OF_MEMORY:      return "GL_OUT_OF_MEMORY";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    default:
        {
            static char buff[128] {};
            sprintf( buff, "#x%04x", err );
            return buff;
        }
    }
}

//==================================================================
static bool CheckGLErr( const char *pFileName, int line )
{
    bool didErr = false;
	GLenum err = glGetError();
	while (err != GL_NO_ERROR)
	{
        didErr = true;
        const char *pErrStr = getErrStr( err );

        if ( pErrStr )
            printf( "GL error: %s at %s : %i", pErrStr, pFileName, line );
        else
            printf( "Unknown error: %d 0x%x at %s : %i", err, err, pFileName, line );

		err = glGetError();
	}

    return didErr;
}

//==================================================================
static void FlushGLErr()
{
	while ( glGetError() != GL_NO_ERROR )
    {
    }
}

//==================================================================
static void check_shader_compilation( GLuint oid, bool isLink )
{
    GLint n {};
    if ( isLink )
        glGetProgramiv( oid, GL_LINK_STATUS, &n );
    else
        glGetShaderiv( oid, GL_COMPILE_STATUS, &n );

    if NOT( n )
    {
        if ( isLink )
            glGetProgramiv( oid, GL_INFO_LOG_LENGTH, &n );
        else
            glGetShaderiv( oid, GL_INFO_LOG_LENGTH, &n );

        IVec<GLchar>  info_log( n );

        if ( isLink )
            glGetProgramInfoLog( oid, n, &n, info_log.data() );
        else
            glGetShaderInfoLog( oid, n, &n, info_log.data() );

        DEX_RUNTIME_ERROR(
                "%s %s failed: %*s",
                isLink ? "Program" : "Shader",
                isLink ? "linking" : "compilation",
                n,
                info_log.data() );
    }
}

//==================================================================
GShaderProg::GShaderProg( bool useTex )
{
static const IStr vtxSrouce[2] = {
R"RAW(
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec4 a_col;

out vec4 v_col;

void main()
{
   v_col = a_col;

   gl_Position = vec4( a_pos * 2.0 - 1.0, 1.0 );
}
)RAW",
R"RAW(
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec4 a_col;
layout (location = 2) in vec2 a_tc0;

out vec4 v_col;
out vec2 v_tc0;

void main()
{
   v_col = a_col;
   v_tc0 = a_tc0;

   gl_Position = vec4( a_pos * 2.0 - 1.0, 1.0 );
}
)RAW"};

static const IStr frgSource[2] = {
R"RAW(
in vec4 v_col;

out vec4 o_col;

void main()
{
   o_col = v_col;
}
)RAW",
R"RAW(
uniform sampler2D s_tex;

in vec4 v_col;
in vec2 v_tc0;

out vec4 o_col;

void main()
{
   o_col = v_col * texture( s_tex, v_tc0 );
}
)RAW"};

    c_auto srcIdx = useTex ? 1 : 0;

    auto makeShader = []( c_auto type, const IStr &src )
    {
        c_auto obj = glCreateShader( type );

        c_auto fullStr = "#version 330\n" + src;

        const GLchar *ppsrcs[2] = { fullStr.c_str(), 0 };

        glShaderSource( obj, 1, &ppsrcs[0], nullptr ); CHECKGLERR;

        glCompileShader( obj );
        CHECKGLERR;

        check_shader_compilation( obj, false );
        return obj;
    };

    mShaderVertex   = makeShader( GL_VERTEX_SHADER  , vtxSrouce[srcIdx] );
    mShaderFragment = makeShader( GL_FRAGMENT_SHADER, frgSource[srcIdx] );

    mShaderProgram = glCreateProgram();

    glAttachShader( mShaderProgram, mShaderVertex );
    glAttachShader( mShaderProgram, mShaderFragment );
    glLinkProgram( mShaderProgram );

    check_shader_compilation( mShaderProgram, true );

    // Always detach shaders after a successful link.
    glDetachShader( mShaderProgram, mShaderVertex );
    glDetachShader( mShaderProgram, mShaderFragment );

    //
    if ( useTex )
    {
        mTexLoc = glGetUniformLocation( mShaderProgram, "s_tex" );
        glUseProgram( mShaderProgram );
        glUniform1i( mTexLoc, 0 );
    }

    // for consistency
    glUseProgram( 0 );
}

//==================================================================
GShaderProg::~GShaderProg()
{
    if ( mShaderProgram )
        glDeleteProgram( mShaderProgram );

    if ( mShaderVertex )
        glDeleteShader( mShaderVertex );

    if ( mShaderVertex )
        glDeleteShader( mShaderFragment );
}

//==================================================================
//==================================================================
ImmGL::ImmGL()
{
    mUseShaders = true;//(majorVer >= 4 && !isSWRendering);

    if ( mUseShaders )
    {
        FLUSHGLERR;

        moShaProgs.push_back( std::make_unique<GShaderProg>( false ) );
        moShaProgs.push_back( std::make_unique<GShaderProg>( true ) );

        glGenBuffers( 1, &mVBO );
        CHECKGLERR;

        glGenVertexArrays( 1, &mVAO );
        CHECKGLERR;
    }
}

//==================================================================
inline std::array<IFloat3,4> ImmGL::makeRectVtxPos(
                                const IFloat2 &pos,
                                const IFloat2 &siz ) const
{
    return
    {
        pos[0]+siz[0]*0, pos[1]+siz[1]*0,
        pos[0]+siz[0]*1, pos[1]+siz[1]*0,
        pos[0]+siz[0]*0, pos[1]+siz[1]*1,
        pos[0]+siz[0]*1, pos[1]+siz[1]*1
    };
}

//==================================================================
void ImmGL::SetTexture( IUInt texID )
{
    if ( mCurTexID == texID )
        return;

    FlushPrims();
    mCurTexID = texID;
}

//==================================================================
void ImmGL::switchModeFlags( IUInt flags )
{
    if ( mModeFlags == flags )
        return;

    FlushPrims();
    mModeFlags = flags;
}

//==================================================================
void ImmGL::ResetStates()
{
    glDisableClientState( GL_VERTEX_ARRAY );
    glDisableClientState( GL_COLOR_ARRAY );
    glDisableClientState( GL_TEXTURE_COORD_ARRAY );

    mCurBlendMode = BM_NONE;
    glDisable( GL_BLEND );

    mModeFlags = 0;

    glDisable( GL_TEXTURE_2D );
    mCurTexID = (IUInt)0;
    glBindTexture( GL_TEXTURE_2D, mCurTexID );

    //
    glBindBuffer( GL_ARRAY_BUFFER, 0 );

    //
    if ( mUseShaders )
    {
        mCurShaderProgram = 0;
        glUseProgram( 0 );

        //
        glBindVertexArray( 0 );
    }
}

//==================================================================
void ImmGL::FlushPrims()
{
    c_auto n = (GLsizei)((mModeFlags & FLG_TEX) ? mVtxPCT.size() : mVtxPC.size());
    if NOT( n )
        return;

    if ( mUseShaders )
    {
        FLUSHGLERR;
        c_auto newVBOSize  = (mModeFlags & FLG_TEX)
                            ? mVtxPCT.size() * sizeof(mVtxPCT[0])
                            : mVtxPC.size()  * sizeof(mVtxPC[0]);

        glBindBuffer( GL_ARRAY_BUFFER, mVBO );
        CHECKGLERR;

        // expand as necessary
        if ( mLastVBOSize != newVBOSize )
        {
            mLastVBOSize = newVBOSize;
            glBufferData( GL_ARRAY_BUFFER, newVBOSize, 0, GL_DYNAMIC_DRAW );
        }
        CHECKGLERR;

        if ( (mModeFlags & FLG_TEX) )
            glBufferSubData( GL_ARRAY_BUFFER, 0, newVBOSize, mVtxPCT.data() );
        else
            glBufferSubData( GL_ARRAY_BUFFER, 0, newVBOSize, mVtxPC.data() );
        CHECKGLERR;

        //
        glBindVertexArray( mVAO );

        glEnableVertexAttribArray( 0 );
        CHECKGLERR;
        glEnableVertexAttribArray( 1 );
        CHECKGLERR;
        if ( (mModeFlags & FLG_TEX) )
        {
            auto vp = []( c_auto idx, c_auto cnt, c_auto off )
            {
                glVertexAttribPointer( idx, cnt, GL_FLOAT, GL_FALSE, sizeof(VtxPCT), (const void *)off );
                CHECKGLERR;
            };
            vp( 0, 3, offsetof(VtxPCT,pos) );
            vp( 1, 4, offsetof(VtxPCT,col) );

            glEnableVertexAttribArray( 2 );
            CHECKGLERR;
            vp( 2, 2, offsetof(VtxPCT,tc0) );

            //
            glActiveTexture( GL_TEXTURE0 );
            CHECKGLERR;
            glBindTexture( GL_TEXTURE_2D, mCurTexID );
            CHECKGLERR;
        }
        else
        {
            auto vp = []( c_auto idx, c_auto cnt, c_auto off )
            {
                glVertexAttribPointer( idx, cnt, GL_FLOAT, GL_FALSE, sizeof(VtxPC), (const void *)off );
                CHECKGLERR;
            };
            vp( 0, 3, offsetof(VtxPC,pos) );
            vp( 1, 4, offsetof(VtxPC,col) );

            glDisableVertexAttribArray( 2 );
            CHECKGLERR;
        }

        //
        if (c_auto progID = moShaProgs[(mModeFlags & FLG_TEX) ? 1 : 0]->GetProgramID();
                   progID != mCurShaderProgram )
        {
            mCurShaderProgram = progID;
            glUseProgram( progID );
            CHECKGLERR;
        }
    }
    else
    {
        glEnableClientState( GL_VERTEX_ARRAY );
        glEnableClientState( GL_COLOR_ARRAY );

        if ( (mModeFlags & FLG_TEX) )
        {
            glVertexPointer( 3, GL_FLOAT, sizeof(VtxPCT), &mVtxPCT[0].pos );
            glColorPointer( 4, GL_FLOAT, sizeof(VtxPCT), &mVtxPCT[0].col );

            assert( !!mCurTexID );

            glEnableClientState( GL_TEXTURE_COORD_ARRAY );
            glTexCoordPointer( 2, GL_FLOAT, sizeof(VtxPCT), &mVtxPCT[0].tc0 );

            //
            glEnable( GL_TEXTURE_2D );
            glBindTexture( GL_TEXTURE_2D, mCurTexID );
        }
        else
        {
            glVertexPointer( 3, GL_FLOAT, sizeof(VtxPC), &mVtxPC[0].pos );
            glColorPointer( 4, GL_FLOAT, sizeof(VtxPC), &mVtxPC[0].col );

            glDisableClientState( GL_TEXTURE_COORD_ARRAY );

            //
            glDisable( GL_TEXTURE_2D );
        }
    }

    //
    glDrawArrays( (mModeFlags & FLG_LINES) ? GL_LINES : GL_TRIANGLES, 0, n );
    CHECKGLERR;

    if ( mUseShaders )
    {
        //glUseProgram( 0 );
        glBindVertexArray( 0 );

        glBindBuffer( GL_ARRAY_BUFFER, 0 );
        CHECKGLERR;
    }

    mVtxPC.clear();
    mVtxPCT.clear();
}

//
#undef  c_auto
#undef  NOT
#undef  __DSHORT_FILE__

