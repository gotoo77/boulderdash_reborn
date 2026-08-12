#include "Grid.h"

#include "../util/Logger.h"

Grid::Grid(int w, int h)
    : m_width(w), m_height(h), m_cells(w * h) {
    LOG_T("Grid ctor %dx%d", w, h);
}

bool Grid::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < m_width && y < m_height;
}

Cell& Grid::at(int x, int y) {
    return m_cells[y * m_width + x];
}

const Cell& Grid::at(int x, int y) const {
    return m_cells[y * m_width + x];
}
