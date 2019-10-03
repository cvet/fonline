#include "Randomizer.h"
#include "Testing.h"
#include "Threading.h"

static uint GenerateSeed()
{
    static THREAD uint seed_counter = 42;
    seed_counter++;
    #ifndef FO_TESTING
    return (uint) time( nullptr ) + seed_counter;
    #else
    return seed_counter;
    #endif
}

class NullRandomizer: public IRandomizer
{
public:
    NullRandomizer() = default;
    virtual int Next( int minimum, int maximum ) override { /* Null */ return 0; }
};

Randomizer Fabric::CreateNullRandomizer()
{
    return std::make_shared< NullRandomizer >();
}

class MersenneTwistRandomizer: public IRandomizer
{
    static const int periodN = 624;
    static const int periodM = 397;
    uint             rndNumbers[ periodN ];
    int              rndIter = 0;

    void GenerateState()
    {
        static uint mag01[ 2 ] = { 0x0, 0x9908B0DF };
        uint        num;
        int         i = 0;

        for( ; i < periodN - periodM; i++ )
        {
            num = ( rndNumbers[ i ] & 0x80000000 ) | ( rndNumbers[ i + 1 ] & 0x7FFFFFFF );
            rndNumbers[ i ] = rndNumbers[ i + periodM ] ^ ( num >> 1 ) ^ mag01[ num & 0x1 ];
        }

        for( ; i < periodN - 1; i++ )
        {
            num = ( rndNumbers[ i ] & 0x80000000 ) | ( rndNumbers[ i + 1 ] & 0x7FFFFFFF );
            rndNumbers[ i ] = rndNumbers[ i + ( periodM - periodN ) ] ^ ( num >> 1 ) ^ mag01[ num & 0x1 ];
        }

        num = ( rndNumbers[ periodN - 1 ] & 0x80000000 ) | ( rndNumbers[ 0 ] & 0x7FFFFFFF );
        rndNumbers[ periodN - 1 ] = rndNumbers[ periodM - 1 ] ^ ( num >> 1 ) ^ mag01[ num & 0x1 ];

        rndIter = 0;
    }

public:
    MersenneTwistRandomizer( uint seed )
    {
        rndNumbers[ 0 ] = seed;

        for( int i = 1; i < periodN; i++ )
            rndNumbers[ i ] = ( 1812433253 * ( rndNumbers[ i - 1 ] ^ ( rndNumbers[ i - 1 ] >> 30 ) ) + i );

        GenerateState();
    }

    virtual int Next( int minimum, int maximum ) override
    {
        if( rndIter < 0 || rndIter >= periodN )
            GenerateState();

        uint num = rndNumbers[ rndIter ];
        rndIter++;
        num ^= ( num >> 11 );
        num ^= ( num << 7 ) & 0x9D2C5680;
        num ^= ( num << 15 ) & 0xEFC60000;
        num ^= ( num >> 18 );

        return minimum + (int) ( (int64) num * (int64) ( (int64) maximum - (int64) minimum + (int64) 1 ) / (int64) 0x100000000 );
    }
};

Randomizer Fabric::CreateMersenneTwistRandomizer( uint seed /* = 0 */ )
{
    return std::make_shared< MersenneTwistRandomizer >( seed == 0 ? GenerateSeed() : seed );
}
