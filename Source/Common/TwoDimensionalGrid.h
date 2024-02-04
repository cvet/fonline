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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

template<typename TCell, typename TPos, typename TSize, bool MemoryOptimized>
class TwoDimensionalGrid
{
public:
    [[nodiscard]] auto GetCellForReading(TPos pos) const -> const TCell&;
    [[nodiscard]] auto GetCellForWriting(TPos pos) -> TCell&;

    void SetSize(TSize size);

private:
    TSize _size {};
    unordered_map<TPos, TCell> _cells {};
    vector<TCell> _preallocatedCells {};
    const TCell _emptyCell {};
};

template<typename TCell, typename TPos, typename TSize, bool MemoryOptimized>
auto TwoDimensionalGrid<TCell, TPos, TSize, MemoryOptimized>::GetCellForReading(TPos pos) const -> const TCell&
{
    NO_STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_size.IsValidPos(pos));

    if constexpr (MemoryOptimized) {
        const auto it = _cells.find(pos);

        if (it == _cells.end()) {
            return _emptyCell;
        }
        else {
            return it->second;
        }
    }
    else {
        return _preallocatedCells[static_cast<size_t>(pos.y) * _size.width + pos.x];
    }
}

template<typename TCell, typename TPos, typename TSize, bool MemoryOptimized>
auto TwoDimensionalGrid<TCell, TPos, TSize, MemoryOptimized>::GetCellForWriting(TPos pos) -> TCell&
{
    NO_STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_size.IsValidPos(pos));

    if constexpr (MemoryOptimized) {
        const auto it = _cells.find(pos);

        if (it == _cells.end()) {
            return _cells.emplace(pos, TCell {}).first->second;
        }
        else {
            return it->second;
        }
    }
    else {
        return _preallocatedCells[static_cast<size_t>(pos.y) * _size.width + pos.x];
    }
}

template<typename TCell, typename TPos, typename TSize, bool MemoryOptimized>
void TwoDimensionalGrid<TCell, TPos, TSize, MemoryOptimized>::SetSize(TSize size)
{
    STACK_TRACE_ENTRY();

    const auto prev_size = _size;

    _size = size;

    if (prev_size == TSize {}) {
        if constexpr (!MemoryOptimized) {
            _preallocatedCells.resize(_size.GetSquare());
        }
    }
    else {
        if constexpr (MemoryOptimized) {
            for (size_t y = 0; y < std::max(prev_size.height, _size.height); y++) {
                for (size_t x = 0; x < std::max(prev_size.width, _size.width); x++) {
                    if (x >= _size.width && y >= _size.height && x < prev_size.width && y < prev_size.height) {
                        const auto it = _cells.find(prev_size.FromRawPos(ipos {static_cast<int>(x), static_cast<int>(y)}));

                        if (it != _cells.end()) {
                            _cells.erase(it);
                        }
                    }
                }
            }
        }
        else {
            vector<TCell> new_cells;

            new_cells.resize(_size.GetSquare());

            for (size_t y = 0; y < std::max(prev_size.height, _size.height); y++) {
                for (size_t x = 0; x < std::max(prev_size.width, _size.width); x++) {
                    if (x < _size.width && y < _size.height && x < prev_size.width && y < prev_size.height) {
                        new_cells[y * _size.width + x] = std::move(_preallocatedCells[y * prev_size.width + x]);
                    }
                }
            }

            _preallocatedCells = std::move(new_cells);
        }
    }
}
