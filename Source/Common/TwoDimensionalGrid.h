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
    explicit TwoDimensionalGrid(TSize size)
    {
        FO_VERIFY_AND_THROW(size.width >= 0, "Two-dimensional grid width is negative", size.width, size.height);
        FO_VERIFY_AND_THROW(size.height >= 0, "Two-dimensional grid height is negative", size.width, size.height);

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
    // Resets every cell of the area clipped to the grid; storage that only held those cells is released
    virtual void ClearArea(TPos origin, TSize area) = 0;

protected:
    // Clips an area to the grid as inclusive-exclusive int64 bounds {x0, y0, x1, y1}, or nothing when empty
    [[nodiscard]] auto ClipArea(TPos origin, TSize area) const noexcept -> optional<array<int64_t, 4>>
    {
        int64_t x0 {std::max<int64_t>(origin.x, 0)};
        int64_t y0 {std::max<int64_t>(origin.y, 0)};
        int64_t x1 {std::min<int64_t>(safe_numeric_cast<int64_t>(origin.x) + area.width, _size.width)};
        int64_t y1 {std::min<int64_t>(safe_numeric_cast<int64_t>(origin.y) + area.height, _size.height)};

        if (area.width <= 0 || area.height <= 0 || x0 >= x1 || y0 >= y1) {
            return std::nullopt;
        }

        return array<int64_t, 4> {x0, y0, x1, y1};
    }

    TSize _size {};
};

template<typename TCell, pos_type TPos, size_type TSize>
class DynamicTwoDimensionalGrid final : public TwoDimensionalGrid<TCell, TPos, TSize>
{
    using base = TwoDimensionalGrid<TCell, TPos, TSize>;

public:
    explicit DynamicTwoDimensionalGrid(TSize size) :
        base(size)
    {
    }

    [[nodiscard]] auto GetCellForReading(TPos pos) const noexcept -> const TCell& override
    {
        if (!base::_size.is_valid_pos(pos)) {
            return _emptyCell;
        }

        auto it {_cells.find(pos)};

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

        auto it {_cells.find(pos)};

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

        base::_size = size;
        std::erase_if(_cells, [&size](const auto& entry) { return !size.is_valid_pos(entry.first); });
    }

    void ClearArea(TPos origin, TSize area) override
    {
        auto bounds {base::ClipArea(origin, area)};

        if (!bounds) {
            return;
        }

        auto [x0, y0, x1, y1] {*bounds};

        if (checked_mul(x1 - x0, y1 - y0) <= numeric_cast<int64_t>(_cells.size())) {
            for (int64_t y {y0}; y < y1; y++) {
                for (int64_t x {x0}; x < x1; x++) {
                    _cells.erase(TPos {numeric_cast<decltype(TPos::x)>(x), numeric_cast<decltype(TPos::y)>(y)});
                }
            }
        }
        else {
            std::erase_if(_cells, [&](const auto& entry) { return entry.first.x >= x0 && entry.first.x < x1 && entry.first.y >= y0 && entry.first.y < y1; });
        }
    }

private:
    unordered_map<TPos, TCell> _cells {};
    const TCell _emptyCell {};
};

// Directly indexed chunks allocated on first write. A two-level index (super blocks of SUPER_SIDE x SUPER_SIDE chunk
// slots) keeps the index proportional to the touched area, so very large sparse maps do not pay for their whole size
template<typename TCell, pos_type TPos, size_type TSize, size_t ChunkSide>
class ChunkedTwoDimensionalGrid final : public TwoDimensionalGrid<TCell, TPos, TSize>
{
    static_assert(ChunkSide > 0 && (ChunkSide & (ChunkSide - 1)) == 0, "Chunk side must be a nonzero power of two");

    using base = TwoDimensionalGrid<TCell, TPos, TSize>;
    using Chunk = array<TCell, ChunkSide * ChunkSide>;

    static constexpr int64_t CHUNK_SIDE {const_numeric_cast<int64_t>(ChunkSide)};
    static constexpr int64_t SUPER_SIDE {32};

    struct SuperBlock
    {
        array<unique_nptr<Chunk>, SUPER_SIDE * SUPER_SIDE> Chunks {};
        size_t ChunkCount {};
    };

public:
    explicit ChunkedTwoDimensionalGrid(TSize size) :
        base(size)
    {
        int64_t chunks_per_row {(safe_numeric_cast<int64_t>(base::_size.width) + CHUNK_SIDE - 1) / CHUNK_SIDE};
        int64_t chunk_rows {(safe_numeric_cast<int64_t>(base::_size.height) + CHUNK_SIDE - 1) / CHUNK_SIDE};
        _superPerRow = (chunks_per_row + SUPER_SIDE - 1) / SUPER_SIDE;
        int64_t super_rows {(chunk_rows + SUPER_SIDE - 1) / SUPER_SIDE};
        _superBlocks.resize(numeric_cast<size_t>(checked_mul(_superPerRow, super_rows)));
    }

    [[nodiscard]] auto GetCellForReading(TPos pos) const noexcept -> const TCell& override
    {
        if (!base::_size.is_valid_pos(pos)) {
            return _emptyCell;
        }

        int64_t cx {safe_numeric_cast<int64_t>(pos.x) / CHUNK_SIDE};
        int64_t cy {safe_numeric_cast<int64_t>(pos.y) / CHUNK_SIDE};
        const auto& super_block {_superBlocks[GetSuperIndex(cx, cy)]};

        if (!super_block) {
            return _emptyCell;
        }

        const auto& chunk {super_block->Chunks[GetSlotIndex(cx, cy)]};

        if (!chunk) {
            return _emptyCell;
        }

        return (*chunk)[GetCellIndex(pos)];
    }

    [[nodiscard]] auto GetCellForWriting(TPos pos) -> ptr<TCell> override
    {
        FO_VERIFY_AND_THROW(base::_size.is_valid_pos(pos), "Chunked two-dimensional grid write position is outside the grid bounds", pos, base::_size);

        int64_t cx {safe_numeric_cast<int64_t>(pos.x) / CHUNK_SIDE};
        int64_t cy {safe_numeric_cast<int64_t>(pos.y) / CHUNK_SIDE};
        auto& super_block {_superBlocks[GetSuperIndex(cx, cy)]};

        if (!super_block) {
            super_block = safe_alloc::make_unique<SuperBlock>();
        }

        auto& chunk {super_block->Chunks[GetSlotIndex(cx, cy)]};

        if (!chunk) {
            chunk = safe_alloc::make_unique<Chunk>();
            super_block->ChunkCount++;
            _chunkCount++;
        }

        return &(*chunk)[GetCellIndex(pos)];
    }

    void Resize(TSize size) override
    {
        FO_VERIFY_AND_THROW(size.width >= 0, "Size width is negative", size.width);
        FO_VERIFY_AND_THROW(size.height >= 0, "Size height is negative", size.height);

        ChunkedTwoDimensionalGrid replacement {size};

        ForEachChunk([&](int64_t cx, int64_t cy, Chunk& chunk) {
            for (size_t cell_index {0}; cell_index < chunk.size(); cell_index++) {
                int64_t cell_slot {numeric_cast<int64_t>(cell_index)};
                int64_t x {cx * CHUNK_SIDE + cell_slot % CHUNK_SIDE};
                int64_t y {cy * CHUNK_SIDE + cell_slot / CHUNK_SIDE};

                if (x < size.width && y < size.height) {
                    TPos pos {numeric_cast<decltype(pos.x)>(x), numeric_cast<decltype(pos.y)>(y)};
                    *replacement.GetCellForWriting(pos) = std::move(chunk[cell_index]);
                }
            }
        });

        base::_size = size;
        _superPerRow = replacement._superPerRow;
        _superBlocks = std::move(replacement._superBlocks);
        _chunkCount = replacement._chunkCount;
    }

    void ClearArea(TPos origin, TSize area) override
    {
        auto bounds {base::ClipArea(origin, area)};

        if (!bounds) {
            return;
        }

        auto [x0, y0, x1, y1] {*bounds};

        for (int64_t cy {y0 / CHUNK_SIDE}; cy <= (y1 - 1) / CHUNK_SIDE; cy++) {
            for (int64_t cx {x0 / CHUNK_SIDE}; cx <= (x1 - 1) / CHUNK_SIDE; cx++) {
                auto& super_block {_superBlocks[GetSuperIndex(cx, cy)]};

                if (!super_block) {
                    cx = (cx / SUPER_SIDE + 1) * SUPER_SIDE - 1; // Skip the remaining slots of this absent super block row
                    continue;
                }

                auto& chunk {super_block->Chunks[GetSlotIndex(cx, cy)]};

                if (!chunk) {
                    continue;
                }

                int64_t chunk_x0 {cx * CHUNK_SIDE};
                int64_t chunk_y0 {cy * CHUNK_SIDE};
                bool covered {x0 <= chunk_x0 && y0 <= chunk_y0 && x1 >= chunk_x0 + CHUNK_SIDE && y1 >= chunk_y0 + CHUNK_SIDE};

                if (covered) {
                    chunk.reset();
                    _chunkCount--;

                    if (--super_block->ChunkCount == 0) {
                        super_block.reset();
                    }
                }
                else {
                    for (int64_t y {std::max(y0, chunk_y0)}; y < std::min(y1, chunk_y0 + CHUNK_SIDE); y++) {
                        for (int64_t x {std::max(x0, chunk_x0)}; x < std::min(x1, chunk_x0 + CHUNK_SIDE); x++) {
                            (*chunk)[numeric_cast<size_t>((y - chunk_y0) * CHUNK_SIDE + (x - chunk_x0))] = TCell {};
                        }
                    }
                }
            }
        }
    }

    // Number of allocated chunks; storage follows this count rather than the grid size
    [[nodiscard]] auto GetAllocatedChunkCount() const noexcept -> size_t { return _chunkCount; }

private:
    [[nodiscard]] auto GetSuperIndex(int64_t cx, int64_t cy) const noexcept -> size_t { return numeric_cast<size_t>(cy / SUPER_SIDE * _superPerRow + cx / SUPER_SIDE); }
    [[nodiscard]] static auto GetSlotIndex(int64_t cx, int64_t cy) noexcept -> size_t { return numeric_cast<size_t>(cy % SUPER_SIDE * SUPER_SIDE + cx % SUPER_SIDE); }
    [[nodiscard]] static auto GetCellIndex(TPos pos) noexcept -> size_t { return numeric_cast<size_t>(safe_numeric_cast<int64_t>(pos.y) % CHUNK_SIDE * CHUNK_SIDE + safe_numeric_cast<int64_t>(pos.x) % CHUNK_SIDE); }

    template<typename TCallback>
    void ForEachChunk(TCallback&& callback)
    {
        for (size_t super_index {0}; super_index < _superBlocks.size(); super_index++) {
            auto& super_block {_superBlocks[super_index]};

            if (!super_block) {
                continue;
            }

            int64_t sx {numeric_cast<int64_t>(super_index) % _superPerRow};
            int64_t sy {numeric_cast<int64_t>(super_index) / _superPerRow};

            for (size_t slot {0}; slot < super_block->Chunks.size(); slot++) {
                if (auto& chunk {super_block->Chunks[slot]}) {
                    int64_t slot_value {numeric_cast<int64_t>(slot)};
                    callback(sx * SUPER_SIDE + slot_value % SUPER_SIDE, sy * SUPER_SIDE + slot_value / SUPER_SIDE, *chunk);
                }
            }
        }
    }

    int64_t _superPerRow {};
    vector<unique_nptr<SuperBlock>> _superBlocks {};
    size_t _chunkCount {};
    const TCell _emptyCell {};
};

template<typename TCell, pos_type TPos, size_type TSize>
class StaticTwoDimensionalGrid final : public TwoDimensionalGrid<TCell, TPos, TSize>
{
    using base = TwoDimensionalGrid<TCell, TPos, TSize>;

public:
    explicit StaticTwoDimensionalGrid(TSize size) :
        base(size)
    {
        _preallocatedCells.resize(base::_size.template square<size_t>());
    }

    [[nodiscard]] auto GetCellForReading(TPos pos) const noexcept -> const TCell& override
    {
        if (!base::_size.is_valid_pos(pos)) {
            return _emptyCell;
        }

        size_t index {numeric_cast<size_t>(safe_numeric_cast<int64_t>(pos.y) * base::_size.width + pos.x)};
        auto& cell {_preallocatedCells[index]};

        if (!cell) {
            return _emptyCell;
        }

        return *cell;
    }

    [[nodiscard]] auto GetCellForWriting(TPos pos) -> ptr<TCell> override
    {
        FO_VERIFY_AND_THROW(base::_size.is_valid_pos(pos), "Dense two-dimensional grid write position is outside the grid bounds", pos, base::_size);

        auto index {numeric_cast<size_t>(safe_numeric_cast<int64_t>(pos.y) * base::_size.width + pos.x)};
        auto& cell {_preallocatedCells[index]};

        if (!cell) {
            cell.emplace();
        }

        return &*cell;
    }

    void Resize(TSize size) override
    {
        FO_VERIFY_AND_THROW(size.width >= 0, "Size width is negative", size.width);
        FO_VERIFY_AND_THROW(size.height >= 0, "Size height is negative", size.height);

        auto prev_width {base::_size.width};
        auto prev_height {base::_size.height};

        vector<optional<TCell>> new_cells {};
        new_cells.resize(size.template square<size_t>());

        for (int64_t y {0}; y < std::max(prev_height, size.height); y++) {
            for (int64_t x {0}; x < std::max(prev_width, size.width); x++) {
                if (x < size.width && y < size.height && x < prev_width && y < prev_height) {
                    auto new_index {numeric_cast<size_t>(y * size.width + x)};
                    auto prev_index {numeric_cast<size_t>(y * prev_width + x)};
                    new_cells[new_index] = std::move(_preallocatedCells[prev_index]);
                }
            }
        }

        _preallocatedCells = std::move(new_cells);
        base::_size = size;
    }

    void ClearArea(TPos origin, TSize area) override
    {
        auto bounds {base::ClipArea(origin, area)};

        if (!bounds) {
            return;
        }

        auto [x0, y0, x1, y1] {*bounds};

        for (int64_t y {y0}; y < y1; y++) {
            for (int64_t x {x0}; x < x1; x++) {
                _preallocatedCells[numeric_cast<size_t>(y * base::_size.width + x)].reset();
            }
        }
    }

private:
    vector<optional<TCell>> _preallocatedCells {};
    const TCell _emptyCell {};
};

FO_END_NAMESPACE
