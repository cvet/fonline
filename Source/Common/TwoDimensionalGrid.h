//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

template<typename TCell, pos_type TPos, size_type TSize>
class TwoDimensionalGrid
{
public:
    explicit TwoDimensionalGrid(TSize size) noexcept
    {
        FO_VERIFY_AND_RETURN(size.width >= 0, "Two-dimensional grid width is negative", size.width, size.height);
        FO_VERIFY_AND_RETURN(size.height >= 0, "Two-dimensional grid height is negative", size.width, size.height);

        _size = size;
    }

    TwoDimensionalGrid(const TwoDimensionalGrid&) = delete;
    TwoDimensionalGrid(TwoDimensionalGrid&&) noexcept = default;
    auto operator=(const TwoDimensionalGrid&) -> TwoDimensionalGrid& = delete;
    auto operator=(TwoDimensionalGrid&&) noexcept -> TwoDimensionalGrid& = default;
    virtual ~TwoDimensionalGrid() = default;

    [[nodiscard]] auto GetSize() const noexcept -> TSize { return _size; }
    [[nodiscard]] virtual auto GetCellForReading(TPos pos) const noexcept -> const TCell& = 0;
    [[nodiscard]] virtual auto GetCellForWriting(TPos pos) -> ptr<TCell> = 0;

    virtual void Resize(TSize size) = 0;

protected:
    TSize _size {};
};

template<typename TCell, pos_type TPos, size_type TSize>
class DynamicTwoDimensionalGrid final : public TwoDimensionalGrid<TCell, TPos, TSize>
{
    using base = TwoDimensionalGrid<TCell, TPos, TSize>;

public:
    explicit DynamicTwoDimensionalGrid(TSize size) noexcept :
        base(size)
    {
    }

    [[nodiscard]] auto GetCellForReading(TPos pos) const noexcept -> const TCell& override
    {
        if (!base::_size.is_valid_pos(pos)) {
            return _emptyCell;
        }

        auto it = _cells.find(pos);

        if (it == _cells.end()) {
            return _emptyCell;
        }
        else {
            return it->second;
        }
    }

    [[nodiscard]] auto GetCellForWriting(TPos pos) -> ptr<TCell> override
    {
        FO_VERIFY_AND_THROW(base::_size.is_valid_pos(pos), "Sparse two-dimensional grid write position is outside the grid bounds", pos, base::_size);

        auto it = _cells.find(pos);

        if (it == _cells.end()) {
            return &(_cells.emplace(pos, TCell {}).first->second);
        }
        else {
            return &it->second;
        }
    }

    void Resize(TSize size) override
    {
        FO_VERIFY_AND_THROW(size.width >= 0, "Size width is negative", size.width);
        FO_VERIFY_AND_THROW(size.height >= 0, "Size height is negative", size.height);

        auto prev_width = base::_size.width;
        auto prev_height = base::_size.height;

        base::_size = size;

        for (int64_t y = 0; y < std::max(prev_height, base::_size.height); y++) {
            for (int64_t x = 0; x < std::max(prev_width, base::_size.width); x++) {
                if ((x >= base::_size.width || y >= base::_size.height) && x < prev_width && y < prev_height) {
                    auto it = _cells.find(TPos {numeric_cast<decltype(std::declval<TPos>().x)>(x), numeric_cast<decltype(std::declval<TPos>().y)>(y)});

                    if (it != _cells.end()) {
                        _cells.erase(it);
                    }
                }
            }
        }
    }

private:
    unordered_map<TPos, TCell> _cells {};
    const TCell _emptyCell {};
};

template<typename TCell, pos_type TPos, size_type TSize, size_t ChunkSide>
class ChunkedTwoDimensionalGrid final : public TwoDimensionalGrid<TCell, TPos, TSize>
{
    static_assert(ChunkSide > 0 && (ChunkSide & (ChunkSide - 1)) == 0, "Chunk side must be a nonzero power of two");

    using base = TwoDimensionalGrid<TCell, TPos, TSize>;
    using Chunk = array<TCell, ChunkSide * ChunkSide>;

public:
    explicit ChunkedTwoDimensionalGrid(TSize size) noexcept :
        base(size),
        _chunksPerRow((static_cast<size_t>(base::_size.width) + ChunkSide - 1) / ChunkSide)
    {
        _chunks.resize(_chunksPerRow * ((static_cast<size_t>(base::_size.height) + ChunkSide - 1) / ChunkSide));
    }

    [[nodiscard]] auto GetCellForReading(TPos pos) const noexcept -> const TCell& override
    {
        if (!base::_size.is_valid_pos(pos)) {
            return _emptyCell;
        }

        const auto& chunk = _chunks[GetChunkIndex(pos)];

        if (!chunk) {
            return _emptyCell;
        }

        return (*chunk)[GetCellIndex(pos)];
    }

    [[nodiscard]] auto GetCellForWriting(TPos pos) -> ptr<TCell> override
    {
        FO_VERIFY_AND_THROW(base::_size.is_valid_pos(pos), "Chunked two-dimensional grid write position is outside the grid bounds", pos, base::_size);

        auto& chunk = _chunks[GetChunkIndex(pos)];

        if (!chunk) {
            chunk = safe_alloc::make_unique<Chunk>();
        }

        return &(*chunk)[GetCellIndex(pos)];
    }

    void Resize(TSize size) override
    {
        FO_VERIFY_AND_THROW(size.width >= 0, "Size width is negative", size.width);
        FO_VERIFY_AND_THROW(size.height >= 0, "Size height is negative", size.height);

        ChunkedTwoDimensionalGrid replacement {size};

        for (size_t chunk_index = 0; chunk_index < _chunks.size(); chunk_index++) {
            auto& chunk = _chunks[chunk_index];

            if (!chunk) {
                continue;
            }

            for (size_t cell_index = 0; cell_index < chunk->size(); cell_index++) {
                auto& cell = (*chunk)[cell_index];

                size_t x = chunk_index % _chunksPerRow * ChunkSide + cell_index % ChunkSide;
                size_t y = chunk_index / _chunksPerRow * ChunkSide + cell_index / ChunkSide;

                if (x < static_cast<size_t>(size.width) && y < static_cast<size_t>(size.height)) {
                    TPos pos {numeric_cast<decltype(pos.x)>(x), numeric_cast<decltype(pos.y)>(y)};
                    *replacement.GetCellForWriting(pos) = std::move(cell);
                }
            }
        }

        base::_size = size;
        _chunksPerRow = replacement._chunksPerRow;
        _chunks = std::move(replacement._chunks);
    }

private:
    [[nodiscard]] auto GetChunkIndex(TPos pos) const noexcept -> size_t { return static_cast<size_t>(pos.y) / ChunkSide * _chunksPerRow + static_cast<size_t>(pos.x) / ChunkSide; }
    [[nodiscard]] static auto GetCellIndex(TPos pos) noexcept -> size_t { return static_cast<size_t>(pos.y) % ChunkSide * ChunkSide + static_cast<size_t>(pos.x) % ChunkSide; }

    size_t _chunksPerRow {};
    vector<unique_nptr<Chunk>> _chunks {};
    const TCell _emptyCell {};
};

template<typename TCell, pos_type TPos, size_type TSize>
class StaticTwoDimensionalGrid final : public TwoDimensionalGrid<TCell, TPos, TSize>
{
    using base = TwoDimensionalGrid<TCell, TPos, TSize>;

public:
    explicit StaticTwoDimensionalGrid(TSize size) noexcept :
        base(size)
    {
        size_t count = static_cast<size_t>(static_cast<int64_t>(base::_size.width) * base::_size.height);
        _preallocatedCells.resize(count);
    }

    [[nodiscard]] auto GetCellForReading(TPos pos) const noexcept -> const TCell& override
    {
        if (!base::_size.is_valid_pos(pos)) {
            return _emptyCell;
        }

        size_t index = static_cast<size_t>(static_cast<int64_t>(pos.y) * base::_size.width + pos.x);
        auto& cell = _preallocatedCells[index];

        if (!cell) {
            return _emptyCell;
        }

        return *cell;
    }

    [[nodiscard]] auto GetCellForWriting(TPos pos) -> ptr<TCell> override
    {
        FO_VERIFY_AND_THROW(base::_size.is_valid_pos(pos), "Dense two-dimensional grid write position is outside the grid bounds", pos, base::_size);

        auto index = numeric_cast<size_t>(static_cast<int64_t>(pos.y) * base::_size.width + pos.x);
        auto& cell = _preallocatedCells[index];

        if (!cell) {
            cell.emplace();
        }

        return &*cell;
    }

    void Resize(TSize size) override
    {
        FO_VERIFY_AND_THROW(size.width >= 0, "Size width is negative", size.width);
        FO_VERIFY_AND_THROW(size.height >= 0, "Size height is negative", size.height);

        auto prev_width = base::_size.width;
        auto prev_height = base::_size.height;

        base::_size = size;

        vector<optional<TCell>> new_cells;
        auto new_count = numeric_cast<size_t>(numeric_cast<int64_t>(base::_size.width) * base::_size.height);
        new_cells.resize(new_count);

        for (int64_t y = 0; y < std::max(prev_height, base::_size.height); y++) {
            for (int64_t x = 0; x < std::max(prev_width, base::_size.width); x++) {
                if (x < base::_size.width && y < base::_size.height && x < prev_width && y < prev_height) {
                    auto new_index = numeric_cast<size_t>(y * base::_size.width + x);
                    auto prev_index = numeric_cast<size_t>(y * prev_width + x);
                    new_cells[new_index] = std::move(_preallocatedCells[prev_index]);
                }
            }
        }

        _preallocatedCells = std::move(new_cells);
    }

private:
    vector<optional<TCell>> _preallocatedCells {};
    const TCell _emptyCell {};
};

FO_END_NAMESPACE
