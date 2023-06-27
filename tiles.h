#ifndef _TILES_H_
#define _TILES_H_

#include <list>
#include <string>
#include <memory>
#include "linereader.h"

// A board is a collection of square cells in a Cartesian grid;
// that is, each cell's position is specified as an X,Y pair.
// A board does not need to be rectangular.
// A tile is also a collection of cells in a grid.
// A tile can be placed at a specific position on a board.
// It can also be "reversed" (turned upside down in 3 dimensions),
// and/or rotated to any of the 4 cardinal orientations.

// ----------------------------------------------------------------
// Linear index in a 2-dimensional array.
template <typename ITYPE>
static inline ITYPE XY(ITYPE x, ITYPE y, ITYPE w) {
    return (y * w) + x;
}

// ----------------------------------------------------------------
// Cell represents an x,y position in a grid.
class Cell {
public:
    typedef unsigned Coord;
    explicit Cell(Coord x, Coord y) : x_(x), y_(y) {}
    Coord x() const { return x_; }
    Coord y() const { return y_; }
    bool operator==(Cell const& cell) const { return x_ == cell.x() && y_ == cell.y(); }
    bool operator!=(Cell const& cell) const { return ! (*this == cell); }
    bool operator<(Cell const& cell) const { return y_ < cell.y() || (y_ == cell.y() && x_ < cell.x()); }
private:
    Coord x_, y_;
};

// ----------------------------------------------------------------
// Shape represents a set of cells in a grid.
class Shape {
public:
    typedef Cell::Coord Coord;
    virtual ~Shape() = default;
    std::string name() const { return name_; }
    Coord width() const { return width_; }
    Coord height() const { return height_; }
    size_t size() const { return cells_.size(); }

    typedef std::list<Cell>::iterator iterator;
    typedef std::list<Cell>::const_iterator const_iterator;
    iterator begin() { return cells_.begin(); }
    iterator end() { return cells_.end(); }
    const_iterator cbegin() const { return cells_.cbegin(); }
    const_iterator cend() const { return cells_.cend(); }

    // Create empty shape.
    explicit Shape(std::string const& name) : name_(name), width_(0), height_(0) {}

    // Copy and optionally rename existing shape.
    Shape(Shape const& shape, std::string const& name = "") :
        name_(!name.empty() ? name : shape.name()), cells_(shape.cells_), width_(shape.width_), height_(shape.height_) {}

    // Are two shapes the same?
    bool operator==(const Shape& shape) const {
        return cells_ == shape.cells_;
    }

    // Parse one shape descriptor line.
    //   * = occupied cell
    //   . = unoccupied cell
    bool parse_shape_line(std::string const& line, Coord y);

    // Return a new shape: this shape reversed.
    Shape reverse(std::string const& name) const {
        Shape s (name);
        for (auto const cell : cells_)
            s.add(cell.x(), vflip(cell.y()));
        return s;
    }

    // Return a new shape: this shape rotated 90 degrees clockwise.
    Shape rotate90(std::string const& name) const {
        Shape s (name);
        for (auto const cell : cells_)
            s.add(vflip(cell.y()), cell.x());
        return s;
    }

protected:
    // Add a Cell to a Shape.
    void add(Coord x, Coord y) {
        if (std::find(cells_.begin(), cells_.end(), Cell(x,y)) != cells_.end())
            return;
        if (width_ <= x) width_ = x+1;
        if (height_ <= y) height_ = y+1;
        cells_.push_back(Cell(x,y));
        cells_.sort(); // keep sorted so operator== is simple
    }

    // Remove a Cell from a Shape.
    void remove(Coord x, Coord y) {
        for (iterator cell = begin(); cell != end(); ++cell) {
            if (*cell == Cell(x,y)) {
                cells_.erase(cell);
                return;
            }
        }
    }

    // Add a rectangle of Cells to a Shape.
    void add(Coord x, Coord y, Coord width, Coord height) {
        for (Coord yi = y; yi < y+height; ++yi)
            for (Coord xi = x; xi < x+width; ++xi)
                add(xi,yi);
    }

    // Remove a rectangle of Cells from a Shape.
    void remove(Coord x, Coord y, Coord width, Coord height) {
        for (Coord yi = y; yi < y+height; ++yi)
            for (Coord xi = x; xi < x+width; ++xi)
                remove(xi,yi);
    }

    void clear() { width_ = height_ = 0; cells_.clear(); }
    void name(std::string const& name) { name_ = name; }
    Coord vflip(Coord y) const { return height_ - y - 1; }

private:
    std::string name_;
    std::list<Cell> cells_;
    Coord width_, height_;
};

// ----------------------------------------------------------------
// A Tile has between 1 and 8 "orientations", which are Shapes
// that this Tile can assume by rotation and reversing.
// There may be less than 8 orientations if the rotated/reversed
// tile is identical to another orientation.
class Tile : public Shape {
public:
    typedef std::list<std::shared_ptr<Tile> > Set;
    explicit Tile() : Shape(""), parity_(-1) {}
    explicit Tile(Tile const& tile) : Shape(tile), rev_name_(tile.rev_name()), parity_(-1) {}
    explicit Tile(std::string const& name, int parity = -1) : Shape(name), parity_(parity) {}
    explicit Tile(std::string const& name, Shape const& shape, int parity = -1) : Shape(shape, name), parity_(parity) {}
    std::string rev_name() const { return rev_name_; }
    int parity() const { return parity_; }
    enum { num_parity = 2 };

    // Return a list of all orientations of this Tile.
    std::list<std::shared_ptr<Shape> > all_orientations(bool rev = true) const {
        std::list<std::shared_ptr<Shape> > list;
        Shape s (*this, name()); // base Shape
        add_unique(list, s);
        s = s.rotate90(name()+"'"); // prime = rotate 90
        add_unique(list, s);
        s = s.rotate90(name()+"\""); // double prime = rotate 180
        add_unique(list, s);
        s = s.rotate90(name()+"!"); // bang = rotate 270
        add_unique(list, s);
        if (rev) {
            s = s.reverse(rev_name()+"!"); // reversed 270
            add_unique(list, s);
            s = s.rotate90(rev_name()); // reversed base
            add_unique(list, s);
            s = s.rotate90(rev_name()+"'"); // reversed 90
            add_unique(list, s);
            s = s.rotate90(rev_name()+"\""); // reversed 180
            add_unique(list, s);
        }
        return list;
    }

    // Parse one tile descriptor.
    // Descriptor is a line "tile NAME", followed by a sequence
    // of Shape descriptor lines, followed by a line "end".
    // If NAME contains a slash (/), the part before the slash is the
    // normal name and the part after the slash is the reversed name.
    bool parse(LineReader& rd);

protected:

    // Parse "tile NAME" line.
    bool parse_tile_line(std::string const& line);

    // Parse "parity NUM" line.
    int parse_parity_line(std::string const& line);

    // Add a Shape to a list, unless an identical Shape is already in the list.
    static void add_unique(std::list<std::shared_ptr<Shape> >& list, Shape const& shape) {
        for (auto lshape : list)
            if (shape == *lshape)
                return; // already in the list
        list.push_back(std::make_shared<Shape>(shape));
    }

private:
    std::string rev_name_;
    enum class RevType { None, RevCase, AppendR } rev_type_ = RevType::RevCase;
    int parity_;
};

// ----------------------------------------------------------------
// A Board is a Shape with a "dlx column" (a unique integer) defined for every Cell.
class Board : public Shape {
public:
    explicit Board(std::string const& name) : Shape(name) {}
    virtual ~Board() = default;
    virtual bool inited() const { return true; }
    virtual int dlx_column(Coord x, Coord y) const =0;
    bool parse(LineReader& rd);
};

// ----------------------------------------------------------------
// A RectBoard is a rectangular Board.
class RectBoard : public Board {
public:
    explicit RectBoard(std::string const& name, Coord width, Coord height) : Board(name) {
        add(0, 0, width, height);
    }
    virtual ~RectBoard() = default;
    virtual int dlx_column(Coord x, Coord y) const override {
        return XY(x, y, width());
    }
};

// ----------------------------------------------------------------
// A ShapeBoard is an arbitrarily shaped Board (not necessarily rectangular).
class ShapeBoard : public Board {
public:
    explicit ShapeBoard(std::string const& name, std::string const& desc = "")
        : Board(name), inited_(false) {
        if (init(desc)) inited_ = true;
    }
    virtual ~ShapeBoard() = default;
    virtual bool inited() const override { return inited_; }
    bool init(std::string const& desc);
    virtual int dlx_column(Coord x, Coord y) const override {
        // Let dlx column be the index of the Cell when we traverse 
        // the list of cells in the standard order.
        int col = 0;
        for (auto cell = cbegin(); cell != cend(); ++cell) {
            if (cell->x() == x && cell->y() == y)
                return col;
            ++col;
        }
        return -1;
    }
private:
    bool inited_;
};

#endif // _TILES_H_
