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
    const std::list<Cell> cells() const { return cells_; }
    size_t size() const { return cells_.size(); }

    // Create empty shape.
    explicit Shape(std::string const& name) : name_(name), width_(0), height_(0) {}

    // Copy and optionally rename existing shape.
    Shape(Shape const& shape, std::string const& name = "") :
        name_(!name.empty() ? name : shape.name()), cells_(shape.cells_), width_(shape.width_), height_(shape.height_) {}

    // Add a Cell to a Shape.
    void add(Coord x, Coord y) {
        if (width_ <= x) width_ = x+1;
        if (height_ <= y) height_ = y+1;
        cells_.push_back(Cell(x,y));
        cells_.sort(); // keep sorted so operator== is simple
    }

    // Are two shapes the same?
    bool operator==(const Shape& shape) const {
        return cells_ == shape.cells_;
    }

    // Parse one shape descriptor line.
    //   * = occupied cell
    //   . = unoccupied cell
    bool parse_shape_line(std::string const& line, Coord y) {
        Coord x = 0;
        for (char ch : line) {
            switch (ch) {
            case '*': add(x,y); break;
            case '.': break;
            case ' ': case '\t': case '\n': case '\r': continue;
            default: // invalid character
                fprintf(stderr, "error: invalid character '%c' in shape descriptor for %s\n", ch, name_.c_str());
                return false; 
            }
            ++x;
        }
        return true;
    }

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
    explicit Tile() : Shape("") {}
    explicit Tile(Tile const& tile) : Shape(tile), rev_name_(tile.rev_name()) {}
    explicit Tile(std::string const& name) : Shape(name) {}
    explicit Tile(std::string const& name, Shape const& shape) : Shape(shape, name) {}
    std::string rev_name() const { return rev_name_; }

    // Return a list of all orientations of this Tile.
    std::list<std::shared_ptr<Shape> > all_orientations() const {
        std::list<std::shared_ptr<Shape> > list;
        Shape s (*this, name()); // base Shape
        add_unique(list, s);
        s = s.rotate90(name()+"'"); // prime = rotate 90
        add_unique(list, s);
        s = s.rotate90(name()+"\""); // double prime = rotate 180
        add_unique(list, s);
        s = s.rotate90(name()+"!"); // bang = rotate 270
        add_unique(list, s);
        s = s.reverse(rev_name()+"!"); // reversed 270
        add_unique(list, s);
        s = s.rotate90(rev_name()); // reversed base
        add_unique(list, s);
        s = s.rotate90(rev_name()+"'"); // reversed 90
        add_unique(list, s);
        s = s.rotate90(rev_name()+"\""); // reversed 180
        add_unique(list, s);
        return list;
    }

    // Parse one tile descriptor.
    // Descriptor is a line "tile NAME", followed by a sequence
    // of Shape descriptor lines, followed by a line "end".
    // If NAME contains a slash (/), the part before the slash is the
    // normal name and the part after the slash is the reversed name.
    bool parse(LineReader& rd) {
        clear();
        Coord y = 0;
        for (;;) {
            std::string line = rd.getline();
            if (rd.eof()) return false;
            if (line.empty() || line[0] == '#') continue;
            if (line == "end") { // end of Tile descriptor
                return true;
            } else if (line.substr(0,5) == "tile ") { // start of Tile descriptor
                if (!parse_tile_line(line))
                    return false;
            } else if (!parse_shape_line(line, y++)) { // add row of Cells to this Tile
                fprintf(stderr, "error: invalid line in tile file: %s\n", line.c_str());
                return false;
            }
        }
    }

protected:

    // Parse "tile NAME" line.
    bool parse_tile_line(std::string const& line) {
        size_t namex = line.find_first_not_of(" ", 5);
        if (namex == std::string::npos) {
            fprintf(stderr, "error: missing tile name: %s\n", line.c_str());
            return false;
        }
        size_t slash = line.find("/", namex);
        std::string new_name;
        if (slash != std::string::npos && slash < line.size()-1) {
            new_name = line.substr(namex, slash-namex);
            rev_name_ = line.substr(slash+1);
        } else { // no rev name specified
            new_name = line.substr(namex);
            if (rev_type_ == RevType::RevCase) {
                char ch = line[namex];
                ch = isupper(ch) ? tolower(ch) : islower(ch) ? toupper(ch) : ch;
                rev_name_ = std::string(1, ch) + line.substr(namex+1);
            } else if (rev_type_ == RevType::AppendR) {
                rev_name_ = line.substr(namex) + "r";
            } else {
                rev_name_ = new_name;
            }
        }
        if (name().size() > 0)
            fprintf(stderr, "warning: renaming tile %s as %s\n", name().c_str(), new_name.c_str());
        name(new_name);
        return true;
    }

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
};

// ----------------------------------------------------------------
// A Board is a Shape with a "dlx column" (a unique integer) defined for every Cell.
class Board : public Shape {
public:
    explicit Board(std::string const& name) : Shape(name) {}
    virtual ~Board() = default;
    virtual int dlx_column(Coord x, Coord y) const =0;
    bool parse(LineReader& rd) {
        // Board file is just a sequence of Shape lines.
        Coord y = 0;
        for (;;) {
            std::string const line = rd.getline();
            if (rd.eof()) break;
            if (!parse_shape_line(line, y++)) {
                fprintf(stderr, "error: invalid line in board file: %s\n", line.c_str());
                return false;
            }
        }
        return true;
    }
};

// ----------------------------------------------------------------
// A RectBoard is a rectangular Board.
class RectBoard : public Board {
public:
    explicit RectBoard(std::string const& name, Coord width, Coord height) : Board(name) {
        for (Coord x = 0; x < width; ++x)
            for (Coord y = 0; y < height; ++y)
                add(x,y);
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
    explicit ShapeBoard(std::string const& name) : Board(name) {}
    virtual ~ShapeBoard() = default;
    virtual int dlx_column(Coord x, Coord y) const override {
        // Let dlx column be the index of the Cell when we traverse 
        // the list of cells in the standard order.
        int col = 0;
        for (auto cell : cells()) {
            if (cell.x() == x && cell.y() == y)
                return col;
            ++col;
        }
        return -1;
    }
};

#endif // _TILES_H_
