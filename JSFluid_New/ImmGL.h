//==================================================================
/// ImmGL.h
///
/// Created by Davide Pasca - 2022/05/26
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef IMMGL_H
#define IMMGL_H

#include <array>
#include <vector>
#include <string>
#include <memory>

template <typename T> using IVec = std::vector<T>;

using IFloat2 = std::array<float,2>;
using IFloat3 = std::array<float,3>;
using IFloat4 = std::array<float,4>;
using IColor4 = std::array<float,4>;

using IUInt = unsigned int;
using IStr  = std::string;

//==================================================================
class GShaderProg
{
public:
    IUInt   mShaderVertex   {};
    IUInt   mShaderFragment {};
    IUInt   mShaderProgram  {};

    IUInt   mTexLoc {};

    GShaderProg( bool useTex );
    ~GShaderProg();

    const auto GetProgramID() const { return mShaderProgram; }
};

//==================================================================
class ImmGL
{
    struct VtxPC
    {
        IFloat3 pos;
        IColor4 col;
    };
    struct VtxPCT
    {
        IFloat3 pos;
        IColor4 col;
        IFloat2 tc0;
    };
    IVec<VtxPC>     mVtxPC;
    IVec<VtxPCT>    mVtxPCT;

    enum : int
    {
        BM_NONE,
        BM_ADD,
        BM_ALPHA,
    };
    int             mCurBlendMode = BM_NONE;

    enum : IUInt
    {
        FLG_LINES = 1 << 0,
        FLG_TEX   = 1 << 1,
    };
    IUInt           mModeFlags = 0;
    IUInt           mCurTexID = 0;

    bool            mUseShaders {};

    IVec<std::unique_ptr<GShaderProg>>   moShaProgs;
    IUInt           mCurShaderProgram {};

    IUInt           mVAO = 0;
    IUInt           mVBO = 0;
    size_t          mLastVBOSize {};

public:
    ImmGL();

    void ResetStates();
    void FlushPrims();

    void SetBlendNone();
    void SetBlendAdd();
    void SetBlendAlpha();

    void SetTexture( IUInt texID );
    void SetNoTexture() { SetTexture( 0 ); }

    void DrawLine( const IFloat2 &p1, const IFloat2 &p2, const IColor4 &col );
    void DrawLine( const IFloat2 &p1, const IFloat2 &p2, const IColor4 &col1, const IColor4 &col2 );

    void DrawRectFill( const IFloat2 &pos, const IFloat2 &siz, const std::array<IColor4,4> &cols );
    void DrawRectFill( const IFloat2 &pos, const IFloat2 &siz, const IColor4 &col );
    void DrawRectFill( float x, float y, float w, float h, const IColor4 &col )
    {
        DrawRectFill( {x,y}, {w,h}, col );
    }
    void DrawRectFill( const IFloat4 &rc, const IColor4 &col )
    {
        DrawRectFill( {rc[0],rc[1]}, {rc[2],rc[3]}, col );
    }

private:
    //==================================================================
    template <typename T> static inline void resize_loose( IVec<T> &vec, size_t newSize )
    {
        if ( newSize > vec.capacity() )
        {
            auto locmax = []( auto a, auto b ) { return a > b ? a : b; };
            vec.reserve( locmax( newSize, vec.capacity()/2*3 ) );
        }
        vec.resize( newSize );
    }
    //
    template <typename T> static inline T *growVec( IVec<T> &vec, size_t growN )
    {
        size_t n = vec.size();
        resize_loose<T>( vec, n + growN );
        return vec.data() + n;
    }

    std::array<IFloat3,4> makeRectVtxPos( const IFloat2 &pos, const IFloat2 &siz ) const
    {
        return
        {
            pos[0]+siz[0]*0, pos[1]+siz[1]*0, 0,
            pos[0]+siz[0]*1, pos[1]+siz[1]*0, 0,
            pos[0]+siz[0]*0, pos[1]+siz[1]*1, 0,
            pos[0]+siz[0]*1, pos[1]+siz[1]*1, 0
        };
    }

    void switchModeFlags( IUInt flags );

    //==================================================================
    template <typename D, typename S>
    static void setQuadStripAsTrigsP(
            D out_des[6], const S &v0, const S &v1, const S &v2, const S &v3 )
    {
        out_des[0].pos = v0;
        out_des[1].pos = v1;
        out_des[2].pos = v2;
        out_des[3].pos = v3;
        out_des[4].pos = v2;
        out_des[5].pos = v1;
    }
    template <typename D, typename S>
    static void setQuadStripAsTrigsC(
            D out_des[6], const S &v0, const S &v1, const S &v2, const S &v3 )
    {
        out_des[0].col = v0;
        out_des[1].col = v1;
        out_des[2].col = v2;
        out_des[3].col = v3;
        out_des[4].col = v2;
        out_des[5].col = v1;
    }
    template <typename D, typename S>
    static void setQuadStripAsTrigsT(
            D out_des[6], const S &v0, const S &v1, const S &v2, const S &v3 )
    {
        out_des[0].tc0 = v0;
        out_des[1].tc0 = v1;
        out_des[2].tc0 = v2;
        out_des[3].tc0 = v3;
        out_des[4].tc0 = v2;
        out_des[5].tc0 = v1;
    }
};

//==================================================================
inline void ImmGL::DrawLine(
        const IFloat2 &p1,
        const IFloat2 &p2,
        const IColor4 &col )
{
    switchModeFlags( FLG_LINES );

    auto *pVtx = growVec( mVtxPC, 2 );
    pVtx[0].pos = { p1[0], p1[1], 0 };
    pVtx[1].pos = { p2[0], p2[1], 0 };

    pVtx[0].col =
    pVtx[1].col = col;
}


//==================================================================
inline void ImmGL::DrawLine(
        const IFloat2 &p1,
        const IFloat2 &p2,
        const IColor4 &col1,
        const IColor4 &col2 )
{
    switchModeFlags( FLG_LINES );

    auto *pVtx = growVec( mVtxPC, 2 );
    pVtx[0].pos = { p1[0], p1[1], 0 };
    pVtx[1].pos = { p2[0], p2[1], 0 };

    pVtx[0].col = col1;
    pVtx[1].col = col2;
}

//==================================================================
inline void ImmGL::DrawRectFill(
            const IFloat2 &pos,
            const IFloat2 &siz,
            const std::array<IColor4,4> &cols )
{
    switchModeFlags( 0 );

    auto *pVtx = growVec( mVtxPC, 6 );
    const auto vps = makeRectVtxPos( pos, siz );
    setQuadStripAsTrigsP( pVtx, vps[0], vps[1], vps[2], vps[3] );
    setQuadStripAsTrigsC( pVtx, cols[0], cols[1], cols[2], cols[3] );
}

//==================================================================
inline void ImmGL::DrawRectFill(
            const IFloat2 &pos,
            const IFloat2 &siz,
            const IColor4 &col )
{
    switchModeFlags( 0 );

    auto *pVtx = growVec( mVtxPC, 6 );
    const auto vps = makeRectVtxPos( pos, siz );
    setQuadStripAsTrigsP( pVtx, vps[0], vps[1], vps[2], vps[3] );
    setQuadStripAsTrigsC( pVtx, col, col, col, col );
}

#endif

