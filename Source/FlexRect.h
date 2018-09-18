#ifndef __FLEX_RECT__
#define __FLEX_RECT__

#include <vector>

template< typename Ty >
struct FlexRect
{
    Ty L, T, R, B;

    FlexRect(): L( 0 ), T( 0 ), R( 0 ), B( 0 ) {}
    template< typename Ty2 > FlexRect( const FlexRect< Ty2 >& fr ): L( ( Ty )fr.L ), T( ( Ty )fr.T ), R( ( Ty )fr.R ), B( ( Ty )fr.B ) {}
    FlexRect( Ty l, Ty t, Ty r, Ty b ): L( l ), T( t ), R( r ), B( b ) {}
    FlexRect( Ty l, Ty t, Ty r, Ty b, Ty ox, Ty oy ): L( l + ox ), T( t + oy ), R( r + ox ), B( b + oy ) {}
    FlexRect( const FlexRect& fr, Ty ox, Ty oy ): L( fr.L + ox ), T( fr.T + oy ), R( fr.R + ox ), B( fr.B + oy ) {}
    template< typename Ty2 >
    FlexRect& operator=( const FlexRect< Ty2 >& fr )
    {
        L = (Ty) fr.L;
        T = (Ty) fr.T;
        R = (Ty) fr.R;
        B = (Ty) fr.B;
        return *this;
    }
    void Clear()
    {
        L = 0;
        T = 0;
        R = 0;
        B = 0;
    }
    bool IsZero() const { return !L && !T && !R && !B; }
    Ty   W()      const { return R - L + 1; }
    Ty   H()      const { return B - T + 1; }
    Ty   CX()     const { return L + W() / 2; }
    Ty   CY()     const { return T + H() / 2; }
    Ty&  operator[]( int index )
    {
        switch( index )
        {
        case 0:
            return L;
        case 1:
            return T;
        case 2:
            return R;
        case 3:
            return B;
        default:
            break;
        }
        return L;
    }
    FlexRect& operator()( Ty l, Ty t, Ty r, Ty b )
    {
        L = l;
        T = t;
        R = r;
        B = b;
        return *this;
    }
    FlexRect& operator()( Ty ox, Ty oy )
    {
        L += ox;
        T += oy;
        R += ox;
        B += oy;
        return *this;
    }
    FlexRect< Ty > Interpolate( const FlexRect< Ty >& to, int procent )
    {
        FlexRect< Ty > result( L, T, R, B );
        result.L += (Ty) ( (int) ( to.L - L ) * procent / 100 );
        result.T += (Ty) ( (int) ( to.T - T ) * procent / 100 );
        result.R += (Ty) ( (int) ( to.R - R ) * procent / 100 );
        result.B += (Ty) ( (int) ( to.B - B ) * procent / 100 );
        return result;
    }
};

typedef FlexRect< int >      Rect;
typedef FlexRect< float >    RectF;
typedef std::vector< Rect >  IntRectVec;
typedef std::vector< RectF > FltRectVec;

template< typename Ty >
struct FlexPoint
{
    Ty X, Y;

    FlexPoint(): X( 0 ), Y( 0 ) {}
    template< typename Ty2 > FlexPoint( const FlexPoint< Ty2 >& r ): X( ( Ty )r.X ), Y( ( Ty )r.Y ) {}
    FlexPoint( Ty x, Ty y ): X( x ), Y( y ) {}
    FlexPoint( const FlexPoint& fp, Ty ox, Ty oy ): X( fp.X + ox ), Y( fp.Y + oy ) {}
    template< typename Ty2 >
    FlexPoint& operator=( const FlexPoint< Ty2 >& fp )
    {
        X = (Ty) fp.X;
        Y = (Ty) fp.Y;
        return *this;
    }
    void Clear()
    {
        X = 0;
        Y = 0;
    }
    bool IsZero() { return !X && !Y; }
    Ty&  operator[]( int index )
    {
        switch( index )
        {
        case 0:
            return X;
        case 1:
            return Y;
        default:
            break;
        }
        return X;
    }
    FlexPoint& operator()( Ty x, Ty y )
    {
        X = x;
        Y = y;
        return *this;
    }
};

typedef FlexPoint< int >   Point;
typedef FlexPoint< float > PointF;

#endif // __FLEX_RECT__
