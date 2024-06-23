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

template<typename TCell, typename TIndex>
class BaseTwoDimensionalGrid
{
public:
    BaseTwoDimensionalGrid(TIndex width, TIndex height)
    {
        STACK_TRACE_ENTRY();

        if constexpr (std::is_signed_v<TIndex>) {
            RUNTIME_ASSERT(width >= 0);
            RUNTIME_ASSERT(height >= 0);
        }

        _width = width;
        _height = height;
    }

    BaseTwoDimensionalGrid(const BaseTwoDimensionalGrid&) = default;
    BaseTwoDimensionalGrid(BaseTwoDimensionalGrid&&) noexcept = default;
    auto operator=(const BaseTwoDimensionalGrid&) -> BaseTwoDimensionalGrid& = default;
    auto operator=(BaseTwoDimensionalGrid&&) noexcept -> BaseTwoDimensionalGrid& = default;
    virtual ~BaseTwoDimensionalGrid() = default;

    [[nodiscard]] virtual auto GetCellForReading(TIndex x, TIndex y) const -> const TCell& = 0;
    [[nodiscard]] virtual auto GetCellForWriting(TIndex x, TIndex y) -> TCell& = 0;

protected:
    TIndex _width {};
    TIndex _height {};
};

template<typename TCell, typename TIndex>
class DynamicTwoDimensionalGrid : public BaseTwoDimensionalGrid<TCell, TIndex>
{
    using base = BaseTwoDimensionalGrid<TCell, TIndex>;

public:
    DynamicTwoDimensionalGrid(TIndex width, TIndex height) :
        base(width, height)
    {
        STACK_TRACE_ENTRY();
    }

    [[nodiscard]] auto GetCellForReading(TIndex x, TIndex y) const -> const TCell& override
    {
        NO_STACK_TRACE_ENTRY();

        if constexpr (std::is_signed_v<TIndex>) {
            RUNTIME_ASSERT(x >= 0);
            RUNTIME_ASSERT(y >= 0);
        }

        RUNTIME_ASSERT(x < base::_width);
        RUNTIME_ASSERT(y < base::_height);

        const auto it = _cells.find(tuple {x, y});

        if (it == _cells.end()) {
            return _emptyCell;
        }
        else {
            return it->second;
        }
    }

    [[nodiscard]] auto GetCellForWriting(TIndex x, TIndex y) -> TCell& override
    {
        NO_STACK_TRACE_ENTRY();

        if constexpr (std::is_signed_v<TIndex>) {
            RUNTIME_ASSERT(x >= 0);
            RUNTIME_ASSERT(y >= 0);
        }

        RUNTIME_ASSERT(x < base::_width);
        RUNTIME_ASSERT(y < base::_height);

        const auto it = _cells.find(tuple {x, y});

        if (it == _cells.end()) {
            return _cells.emplace(tuple {x, y}, TCell {}).first->second;
        }
        else {
            return it->second;
        }
    }

private:
    struct IndexHasher
    {
        auto operator()(const tuple<TIndex, TIndex>& index) const noexcept -> size_t;
    };

    unordered_map<tuple<TIndex, TIndex>, TCell, IndexHasher> _cells {};
    const TCell _emptyCell {};
};

template<typename TCell, typename TIndex>
auto DynamicTwoDimensionalGrid<TCell, TIndex>::IndexHasher::operator()(const tuple<TIndex, TIndex>& index) const noexcept -> size_t
{
    NO_STACK_TRACE_ENTRY();

    if constexpr (sizeof(TIndex) <= sizeof(size_t) / 2) {
        return (static_cast<size_t>(std::get<0>(index)) << (sizeof(size_t) / 2 * 8)) | static_cast<size_t>(std::get<1>(index));
    }
    else {
        return static_cast<size_t>(std::get<0>(index)) ^ static_cast<size_t>(std::get<1>(index));
    }
}

template<typename TCell, typename TIndex>
class StaticTwoDimensionalGrid : public BaseTwoDimensionalGrid<TCell, TIndex>
{
    using base = BaseTwoDimensionalGrid<TCell, TIndex>;

public:
    StaticTwoDimensionalGrid(TIndex width, TIndex height) :
        base(width, height)
    {
        STACK_TRACE_ENTRY();

        _preallocatedCells.resize(static_cast<size_t>(base::_width) * base::_height);
    }

    [[nodiscard]] auto GetCellForReading(TIndex x, TIndex y) const -> const TCell& override
    {
        NO_STACK_TRACE_ENTRY();

        if constexpr (std::is_signed_v<TIndex>) {
            RUNTIME_ASSERT(x >= 0);
            RUNTIME_ASSERT(y >= 0);
        }

        RUNTIME_ASSERT(x < base::_width);
        RUNTIME_ASSERT(y < base::_height);

        return _preallocatedCells[static_cast<size_t>(y) * base::_width + x];
    }

    [[nodiscard]] auto GetCellForWriting(TIndex x, TIndex y) -> TCell& override
    {
        NO_STACK_TRACE_ENTRY();

        if constexpr (std::is_signed_v<TIndex>) {
            RUNTIME_ASSERT(x >= 0);
            RUNTIME_ASSERT(y >= 0);
        }

        RUNTIME_ASSERT(x < base::_width);
        RUNTIME_ASSERT(y < base::_height);

        return _preallocatedCells[static_cast<size_t>(y) * base::_width + x];
    }

private:
    vector<TCell> _preallocatedCells {};
    const TCell _emptyCell {};
};

template<typename TCell, typename TIndex, bool MemoryOptimized>
class TwoDimensionalGrid
{
public:
    [[nodiscard]] auto GetCellForReading(TIndex x, TIndex y) const -> const TCell&;
    [[nodiscard]] auto GetCellForWriting(TIndex x, TIndex y) -> TCell&;

    void SetSize(TIndex width, TIndex height);

private:
    struct IndexHasher
    {
        auto operator()(const tuple<TIndex, TIndex>& index) const noexcept -> size_t;
    };

    TIndex _width {};
    TIndex _height {};
    unordered_map<tuple<TIndex, TIndex>, TCell, IndexHasher> _cells {};
    vector<TCell> _preallocatedCells {};
    const TCell _emptyCell {};
};

template<typename TCell, typename TIndex, bool MemoryOptimized>
auto TwoDimensionalGrid<TCell, TIndex, MemoryOptimized>::GetCellForReading(TIndex x, TIndex y) const -> const TCell&
{
    NO_STACK_TRACE_ENTRY();

    if constexpr (std::is_signed_v<TIndex>) {
        RUNTIME_ASSERT(x >= 0);
        RUNTIME_ASSERT(y >= 0);
    }

    RUNTIME_ASSERT(x < _width);
    RUNTIME_ASSERT(y < _height);

    if constexpr (MemoryOptimized) {
        const auto it = _cells.find(tuple {x, y});

        if (it == _cells.end()) {
            return _emptyCell;
        }
        else {
            return it->second;
        }
    }
    else {
        return _preallocatedCells[static_cast<size_t>(y) * _width + x];
    }
}

template<typename TCell, typename TIndex, bool MemoryOptimized>
auto TwoDimensionalGrid<TCell, TIndex, MemoryOptimized>::GetCellForWriting(TIndex x, TIndex y) -> TCell&
{
    NO_STACK_TRACE_ENTRY();

    if constexpr (std::is_signed_v<TIndex>) {
        RUNTIME_ASSERT(x >= 0);
        RUNTIME_ASSERT(y >= 0);
    }

    RUNTIME_ASSERT(x < _width);
    RUNTIME_ASSERT(y < _height);

    if constexpr (MemoryOptimized) {
        const auto it = _cells.find(tuple {x, y});

        if (it == _cells.end()) {
            return _cells.emplace(tuple {x, y}, TCell {}).first->second;
        }
        else {
            return it->second;
        }
    }
    else {
        return _preallocatedCells[static_cast<size_t>(y) * _width + x];
    }
}

template<typename TCell, typename TIndex, bool MemoryOptimized>
void TwoDimensionalGrid<TCell, TIndex, MemoryOptimized>::SetSize(TIndex width, TIndex height)
{
    STACK_TRACE_ENTRY();

    if constexpr (std::is_signed_v<TIndex>) {
        RUNTIME_ASSERT(width >= 0);
        RUNTIME_ASSERT(height >= 0);
    }

    const auto prev_width = _width;
    const auto prev_height = _height;

    _width = width;
    _height = height;

    if (prev_width == 0 && prev_height == 0) {
        if constexpr (!MemoryOptimized) {
            _preallocatedCells.resize(static_cast<size_t>(_width) * _height);
        }
    }
    else {
        if constexpr (MemoryOptimized) {
            for (size_t hy = 0; hy < std::max(prev_height, _height); hy++) {
                for (size_t hx = 0; hx < std::max(prev_width, _width); hx++) {
                    if (hx >= _width && hy >= _height && hx < prev_width && hy < prev_height) {
                        const auto it = _cells.find(tuple {static_cast<TIndex>(hx), static_cast<TIndex>(hy)});

                        if (it != _cells.end()) {
                            _cells.erase(it);
                        }
                    }
                }
            }
        }
        else {
            vector<TCell> new_cells;

            new_cells.resize(static_cast<size_t>(_width) * _height);

            for (size_t hy = 0; hy < std::max(prev_height, _height); hy++) {
                for (size_t hx = 0; hx < std::max(prev_width, _width); hx++) {
                    if (hx < _width && hy < _height && hx < prev_width && hy < prev_height) {
                        new_cells[hy * _width + hx] = std::move(_preallocatedCells[hy * prev_width + hx]);
                    }
                }
            }

            _preallocatedCells = std::move(new_cells);
        }
    }
}

template<typename TCell, typename TIndex, bool MemoryOptimized>
auto TwoDimensionalGrid<TCell, TIndex, MemoryOptimized>::IndexHasher::operator()(const tuple<TIndex, TIndex>& index) const noexcept -> size_t
{
    NO_STACK_TRACE_ENTRY();

    if constexpr (sizeof(TIndex) <= sizeof(size_t) / 2) {
        return (static_cast<size_t>(std::get<0>(index)) << (sizeof(size_t) / 2 * 8)) | static_cast<size_t>(std::get<1>(index));
    }
    else {
        return static_cast<size_t>(std::get<0>(index)) ^ static_cast<size_t>(std::get<1>(index));
    }
}
