#pragma once

#include <vector>

#include "Cell.h"

class Grid {
public:
    Grid(int w, int h);

    Cell& at(int x, int y);
    const Cell& at(int x, int y) const;

    bool inBounds(int x, int y) const;

    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    int m_width;
    int m_height;
    std::vector<Cell> m_cells;
};
