#ifndef __DATA_MASK__
#define __DATA_MASK__

class OneBitMask
{
public:
    void SetBit( uint x, uint y )
    {
        if( x >= width || y >= height )
            return;
        data[ y * width_dw + x / 32 ] |= 1 << ( x % 32 );
    }

    int GetBit( uint x, uint y )
    {
        if( x >= width || y >= height )
            return 0;
        return ( data[ y * width_dw + x / 32 ] >> ( x % 32 ) ) & 1;
    }

    void Fill( int fill )
    {
        memset( data, fill, width_dw * height * sizeof( uint ) );
    }

    OneBitMask( uint width_bit, uint height_bit, uint* ptr, int fill )
    {
        if( !width_bit )
            width_bit = 1;
        if( !height_bit )
            height_bit = 1;
        width = width_bit;
        height = height_bit;
        width_dw = width / 32;
        if( width % 32 )
            width_dw++;
        if( ptr )
        {
            isAlloc = false;
            data = ptr;
        }
        else
        {
            isAlloc = true;
            data = new uint[ width_dw * height ];
        }
        Fill( fill );
    }

    ~OneBitMask()
    {
        if( isAlloc )
            delete[] data;
        data = NULL;
    }

private:
    bool  isAlloc;
    uint* data;
    uint  width;
    uint  height;
    uint  width_dw;
};

class TwoBitMask
{
public:
    void Set2Bit( uint x, uint y, int val )
    {
        if( x >= width || y >= height )
            return;
        uchar& b = data[ y * width_b + x / 4 ];
        int    bit = ( x % 4 * 2 );
        UNSETFLAG( b, 3 << bit );
        SETFLAG( b, ( val & 3 ) << bit );
    }

    int Get2Bit( uint x, uint y )
    {
        if( x >= width || y >= height )
            return 0;
        return ( data[ y * width_b + x / 4 ] >> ( x % 4 * 2 ) ) & 3;
    }

    void Fill( int fill )
    {
        memset( data, fill, width_b * height );
    }

    void Create( uint width_2bit, uint height_2bit, uchar* ptr )
    {
        if( !width_2bit )
            width_2bit = 1;
        if( !height_2bit )
            height_2bit = 1;
        width = width_2bit;
        height = height_2bit;
        width_b = width / 4;
        if( width % 4 ) width_b++;
        if( ptr )
        {
            isAlloc = false;
            data = ptr;
        }
        else
        {
            isAlloc = true;
            data = new uchar[ width_b * height ];
            Fill( 0 );
        }
    }

    uchar* GetData()
    {
        return data;
    }

    TwoBitMask()
    {
        memset( this, 0, sizeof( TwoBitMask ) );
    }

    TwoBitMask( uint width_2bit, uint height_2bit, uchar* ptr )
    {
        Create( width_2bit, height_2bit, ptr );
    }

    ~TwoBitMask()
    {
        if( isAlloc )
            delete[] data;
        data = NULL;
    }

private:
    bool   isAlloc;
    uchar* data;
    uint   width;
    uint   height;
    uint   width_b;
};

class FourBitMask
{
public:
    void Set4Bit( uint x, uint y, uchar val )
    {
        if( x >= width || y >= height ) return;
        uchar& b = data[ y * width_b + x / 2 ];
        if( x & 1 ) b = ( b & 0xF0 ) | ( val & 0xF );
        else b = ( b & 0xF ) | ( val << 4 );
    }

    uchar Get4Bit( uint x, uint y )
    {
        if( x >= width || y >= height ) return 0;
        uchar& b = data[ y * width_b + x / 2 ];
        if( x & 1 ) return b & 0xF;
        else return b >> 4;
    }

    void Fill( int fill )
    {
        memset( data, fill, width_b * height );
    }

    FourBitMask( uint width_4bit, uint height_4bit, int fill )
    {
        if( !width_4bit ) width_4bit = 1;
        if( !height_4bit ) height_4bit = 1;
        width = width_4bit;
        height = height_4bit;
        width_b = width / 2;
        if( width & 1 ) width_b++;
        data = new uchar[ width_b * height ];
        Fill( fill );
    }

    ~FourBitMask()
    {
        delete[] data;
    }

private:
    uchar* data;
    uint   width;
    uint   height;
    uint   width_b;
};

class ByteMask
{
public:
    void SetByte( uint x, uint y, uchar val )
    {
        if( x >= width || y >= height ) return;
        data[ y * width + x ] = val;
    }

    uchar GetByte( uint x, uint y )
    {
        if( x >= width || y >= height ) return 0;
        return data[ y * width + x ];
    }

    void Fill( int fill )
    {
        memset( data, fill, width * height );
    }

    ByteMask( uint _width, uint _height, int fill )
    {
        if( !_width ) _width = 1;
        if( !_height ) _height = 1;
        width = _width;
        height = _height;
        data = new uchar[ width * height ];
        Fill( fill );
    }

    ~ByteMask()
    {
        delete[] data;
    }

private:
    uchar* data;
    uint   width;
    uint   height;
};

#endif // __DATA_MASK__
