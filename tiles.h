#ifndef _TILES_H_24957_
#define _TILES_H_24957_

#include <list>
#include <string>
#include <memory>
#include "linereader.h"

// A board is a collection of cells in a grid. It does not need to be rectangular.
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
    Cell(Coord x, Coord y) : x_(x), y_(y) {}
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
    Shape(Shape const& shape) : name_(shape.name_), cells_(shape.cells_), width_(shape.width_), height_(shape.height_) {}
    Shape(std::string const& name) : name_(name), width_(0), height_(0) {}
    Shape(std::string const& name, Shape const& shape) : name_(name), cells_(shape.cells_), width_(shape.width_), height_(shape.height_) {}
    virtual ~Shape() = default;
    std::string name() const { return name_; }
    Coord width() const { return width_; }
    Coord height() const { return height_; }
    const std::list<Cell> cells() const { return cells_; }
    size_t size() const { return cells_.size(); }

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
    bool parse_line(std::string const& line, Coord y) {
        Coord x = 0;
        for (char ch : line) {
            switch (ch) {
            case '*': add(x,y); break;
            case '.': break;
            case ' ': case '\t': case '\n': case '\r': continue;
            default:  return false; // invalid character
            }
            ++x;
        }
        return true;
    }

    // Return a new shape: this shape reversed.
    Shape reverse(std::string const& name) const {
        Shape s (name);
        for (auto const cell : cells_)
            s.add(cell.x(), height_-1-cell.y());
        return s;
    }

    // Return a new shape: this shape rotated 90 degrees clockwise.
    Shape rotate90(std::string const& name) const {
        Shape s (name);
        for (auto const cell : cells_)
            s.add(height_-1-cell.y(), cell.x());
        return s;
    }

protected:
    void clear() { width_ = height_ = 0; cells_.clear(); }
    void name(std::string const& name) { name_ = name; }

private:
    std::string name_;
    std::list<Cell> cells_;
    Coord width_, height_;
};

// ----------------------------------------------------------------
// A Tile has between 1 and 8 "orientations", which are Shapes
// that this Tile can assume by rotation and reversing.
class Tile : public Shape {
public:
    typedef std::list<std::shared_ptr<Tile>> Set;
    Tile() : Shape("") {}
    Tile(Tile const& tile) : Shape(tile), rev_name_(tile.rev_name()) {}
    explicit Tile(std::string const& name) : Shape(name) {}
    Tile(std::string const& name, Shape const& shape) : Shape(name, shape) {}
    std::string rev_name() const { return rev_name_; }

    std::list<std::shared_ptr<Shape>> all_orientations() const {
        std::list<std::shared_ptr<Shape>> list;
        Shape s (name(), *this); // base Shape
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

    bool parse(LineReader& rd) {
        // Parse one tile descriptor.
        // Descriptor is a line "tile NAME", followed by a sequence
        // of Shape descriptor lines, followed by a line "end".
        // NAME may be "NORMAL/REVERSED".
        clear();
        Coord y = 0;
        for (;;) {
            std::string line = rd.getline();
            if (rd.eof()) return false;
            if (line.empty() || line[0] == '#') continue;
            if (line == "end") {
                return true;
            } else if (line.substr(0,5) == "tile ") {
                size_t namex = line.find_first_not_of(" ", 5);
                if (namex == std::string::npos) {
                    fprintf(stderr, "error: missing tile name: %s\n", line.c_str());
                    return false;
                }
                size_t slash = line.find("/", namex);
                if (slash != std::string::npos && slash < line.size()-1) {
                    name(line.substr(namex, slash-namex));
                    rev_name_ = line.substr(slash+1);
                } else { // no rev name specified
                    name(line.substr(namex));
                    if (rev_type_ == RevType::RevCase) {
                        char ch = line[namex];
                        ch = isupper(ch) ? tolower(ch) : islower(ch) ? toupper(ch) : ch;
                        rev_name_ = std::string(1, ch);
                    } else
                        rev_name_ = line.substr(namex) + "r";
                }
            } else if (line.substr(0,8) == "reverse ") {
                size_t revx = line.find_first_not_of(" ", 8);
                if (revx != std::string::npos) {
                    std::string rev_str = line.substr(revx);
                    if (rev_str == "case") {
                        rev_type_ = RevType::RevCase;
                    } else if (rev_str == "none") {
                        rev_type_ = RevType::None;
                    } else {
                        fprintf(stderr, "error: invalid value for \"reverse\": %s\n", line.c_str());
                        return false;
                    }
                }
            } else if (!parse_line(line, y++)) { // adds Cells to this
                fprintf(stderr, "error: invalid line in tile file: %s\n", line.c_str());
                return false;
            }
        }
    }

protected:
    static void add_unique(std::list<std::shared_ptr<Shape>>& list, Shape const& shape) {
        for (auto lshape : list)
            if (shape == *lshape)
                return; // already in the list
        list.push_back(std::make_shared<Shape>(shape));
    }

private:
    std::string rev_name_;
    enum class RevType { None, RevCase } rev_type_ = RevType::None;
};

// ----------------------------------------------------------------
// A Board is a Shape with a defined "dlx column" for every Cell.
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
            if (!parse_line(line, y++)) {
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
    RectBoard(std::string const& name, Coord width, Coord height) : Board(name) {
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
        int col = 0;
        for (auto cell : cells()) {
            if (cell.x() == x && cell.y() == y)
                return col;
            ++col;
        }
        return -1;
    }
};

#endif // _TILES_H_24957_
