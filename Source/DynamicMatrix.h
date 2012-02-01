#include "Common.h"
#include <vector>
#include <deque>

using namespace std;

template< int ChunkSize >
class DynamicMatrix
{
public:
    DynamicMatrix()
    {
        init();
    }

    ~DynamicMatrix()
    {
        delete_chunks();
    }

    ushort AddIndex()
    {
        // If out of index space, allocate more chunks to get more indices
        if( free_idx.empty() ) add_chunks();

        // Return the first free index
        ushort r = free_idx.front();
        free_idx.pop_front();
        size++;
        return r;
    }

    void RemoveIndex( ushort index )
    {
        // Just free the index
        free_idx.push_back( index );
        size--;
    }

    ushort* At( ushort x, ushort y )
    {
        return &( chunks[ chunk_num( x, y ) ]->Data[ x % ChunkSize ][ y % ChunkSize ] );
    }

    bool ToCompactify()
    {
        // Never go below a single chunk
        if( chunks.size() == 1 ) return false;

        return Size() < capacity / 2;
    }

    // The user must delete[] the return value
    ushort* Compactify()
    {
        // Prepare valid indices
        vector< bool > indices( capacity, true );
        for( auto it = free_idx.begin(), end = free_idx.end(); it != end; ++it ) indices[ *it ] = false;
        ushort*        ret = new ushort[ Size() ];
        for( int i = 0, j = 0; i < capacity; i++ )
            if( indices[ i ] )
            {
                ret[ j ] = i;
                j++;
            }

        // Create a new set of chunks
        // Do not initialize the chunks that don't need it
        // Update the capacity
        ushort           nchunks = Size() / ChunkSize;
        vector< Chunk* > new_chunks;
        if( Size() % ChunkSize || !size )
        {
            new_chunks.reserve( ( nchunks + 1 ) * ( nchunks + 1 ) );
            for( int i = 0; i < nchunks * nchunks; i++ ) new_chunks.push_back( new Chunk );
            for( int i = 0; i < 2 * nchunks + 1; i++ ) new_chunks.push_back( new Chunk( 0 ) );
            capacity = ( nchunks + 1 ) * ChunkSize;
        }
        else
        {
            new_chunks.reserve( nchunks * nchunks );
            for( int i = 0; i < nchunks * nchunks; i++ ) new_chunks.push_back( new Chunk );
            capacity = nchunks * ChunkSize;
        }

        // Update free indices
        free_idx.clear();
        for( int i = size; i < capacity; i++ ) free_idx.push_back( i );

        // Copy data to new chunks
        for( int i = 0; i < Size(); i++ )
            for( int j = 0; j < Size(); j++ )
            {
                // printf("copy to (%d,%d) from (%d,%d) : %d\n",i,j,ret[i],ret[j],*At(ret[i],ret[j]));
                new_chunks[ chunk_num( i, j ) ]->Data[ i % ChunkSize ][ j % ChunkSize ] = *At( ret[ i ], ret[ j ] );
            }

        // Delete the old chunks and set new
        delete_chunks();
        chunks = new_chunks;

        // Old ret[i] index now has index i
        return ret;
    }

    ushort Size() { return size; }

    void Clear()
    {
        free_idx.clear();
        delete_chunks();
        chunks.resize( 0 );
        init();
    }

private:
    struct Chunk
    {
        Chunk() {}
        Chunk( ushort def_value )
        {
            memset( Data, def_value, ChunkSize * ChunkSize * sizeof( ushort ) );
        }

        ushort Data[ ChunkSize ][ ChunkSize ];
    };

    ushort chunk_num( ushort x, ushort y )
    {
        // Compute the number of chunk where the data for (x,y) is at
        x /= ChunkSize;
        y /= ChunkSize;
        if( y < x ) return x * x + y;
        return y * y + x + y;
    }

    void add_chunks()
    {
        ushort nchunks = size / ChunkSize;
        nchunks <<= 1;
        nchunks++;

        // Add initialized chunks
        for( ushort i = 0; i < nchunks; i++ ) chunks.push_back( new Chunk( 0 ) );
        for( ushort i = size; i < size + ChunkSize; i++ ) free_idx.push_back( i );
        capacity += ChunkSize;
    }

    void delete_chunks()
    {
        // Delete all allocated chunks
        for( auto it = chunks.begin(), end = chunks.end(); it != end; ++it ) delete *it;
    }

    void init()
    {
        size = 0;
        capacity = ChunkSize;

        // Allocate a chunk for ChunkSize indices and set the indices as free
        chunks.push_back( new Chunk( 0 ) );
        for( int i = 0; i < ChunkSize; i++ ) free_idx.push_back( i );
    }

    ushort           size;
    ushort           capacity;
    vector< Chunk* > chunks;
    deque< ushort >  free_idx;
};
