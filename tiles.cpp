#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include "tiles.h"
#include "linereader.h"

extern "C" {
#include "dlx.h"
}

enum class VisType { NONE, DESC, CHARS, ART };

struct VisParam {
    VisParam(unsigned art_hchars = 0, unsigned art_vrows = 0)
        : art_hchars(art_hchars), art_vrows(art_vrows), desc_spaces(false) {}
    unsigned art_hchars;
    unsigned art_vrows;
    bool desc_spaces;
};

extern const char tiles_pentominos[], tiles_hexominos[];
extern const char help[];

// ----------------------------------------------------------------
static void usage1()
{
    printf("usage: tiles [-vVlsru][-W#,#] [-p][-x][-t TILES] BOARD\n");
    printf("       -v = print ASCII picture for each solution\n");
    printf("       -V = print better ASCII picture for each solution\n");
    printf("       -W = size of -V cells\n");
    printf("       -l = print list of tiles for each solution\n");
    printf("       -s = print extra spaces in -l output for alignment\n");
    printf("       -r = don't suppress rotations and reflections\n");
    printf("       -u = use reversed name for reversed tiles in -v display\n");
    printf("       -p = use pentomino tiles\n");
    printf("       -x = use hexomino tiles\n");
    printf("       -t = use tiles described in TILES file\n");
    printf("       BOARD is either a file containing a board description,\n");
    printf("                or \"NxM\" (integer N,M) for a rectangular board\n");
}

// ----------------------------------------------------------------
static int usage()
{
    usage1();
    printf(" \"tiles help\" for more information\n");
    return 1;
}

// ----------------------------------------------------------------
static int print_help()
{
    usage1();
    printf("%s\n", help);
    return 0;
}

// ----------------------------------------------------------------
static bool setup_board(std::shared_ptr<Board>& board, std::string const& board_file)
{
    if (board_file.empty()) {
        fprintf(stderr, "error: no board description\n");
        return false;
    }
    bool ok = false;
    Cell::Coord board_width, board_height;
    FILE* f = fopen(board_file.c_str(), "r");
    if (f != NULL) {
        board.reset(new ShapeBoard(board_file));
        FileLineReader rd (f);
        ok = board->parse(rd);
        fclose(f);
        if (!ok) fprintf(stderr, "error: cannot parse board file %s\n", board_file.c_str());
    } else if (sscanf(board_file.c_str(), "%ux%u", &board_width, &board_height) == 2 &&
            board_width > 0 && board_height > 0) {
        board.reset(new RectBoard(board_file, board_width, board_height));
        ok = true;
    } else {
        fprintf(stderr, "error: cannot open board file %s\n", board_file.c_str());
    }
    return ok;
}

// ----------------------------------------------------------------
static bool parse_tiles(LineReader& rd, Tile::Set& tiles)
{
    for (;;) {
        Tile tile;
        if (!tile.parse(rd))
            return rd.eof();
        tiles.push_back(std::make_shared<Tile>(tile));
    }
}

// ----------------------------------------------------------------
static bool setup_tiles(Tile::Set& tiles, std::string const& tile_file, std::string const& tile_desc)
{
    bool ok = false;
    if (!tile_desc.empty()) {
        StringLineReader rd (tile_desc);
        ok = parse_tiles(rd, tiles);
        if (!ok) fprintf(stderr, "internal error: cannot parse tile desc\n");
    } else if (!tile_file.empty()) {
        FILE* f = fopen(tile_file.c_str(), "r");
        if (f == NULL) {
            fprintf(stderr, "error: cannot open tile file %s\n", tile_file.c_str());
        } else {
            FileLineReader rd (f);
            ok = parse_tiles(rd, tiles);
            fclose(f);
            if (!ok) fprintf(stderr, "error: cannot parse tile file %s\n", tile_file.c_str());
        }
    } else {
        fprintf(stderr, "error: no tile description\n");
    }
    return ok;
}

// ----------------------------------------------------------------
class Soln {
public:
    typedef Cell::Coord Coord;
    enum Rotation { r0, r90, r180, r270, r0r, r90r, r180r, r270r };
    Soln(Coord width, Coord height)
        : width_(width), height_(height), board_(width * height) {
        for (unsigned i = 0; i < board_.size(); ++i)
            board_[i] = '.';
    }
    void set_cell(Coord x, Coord y, char ch) { 
        board_[XY(x, y, width_)] = ch;
    }
    char cell(Coord x, Coord y) const {
        if (x >= width_ || y >= height_)
            return '.';
        return board_[XY(x, y, width_)];
    }
    bool is_rotref(Soln const& s) const {
        return is_equal(s,r0) || is_equal(s,r90) || is_equal(s,r180) || is_equal(s,r270) ||
               is_equal(s,r0r) || is_equal(s,r90r) || is_equal(s,r180r) || is_equal(s,r270r);
    }
    void draw_vis_chars() {
        for (Coord y = 0; y < height_; ++y) {
            printf("    ");
            for (Coord x = 0; x < width_; ++x)
                printf("%c", cell(x,y));
            printf("\n");
        }
        printf("\n");
    }
    void draw_vis_art(unsigned hchars, unsigned vrows) {
        for (Coord y = 0; y <= height_; ++y) {
            // Draw row vertically-between squares.
            for (Coord x = 0; x <= width_; ++x) {
                char br = cell(x,y);
                char bl = cell(x-1,y);
                char tr = cell(x,y-1);
                char tl = cell(x-1,y-1);
                bool hh = (tl == tr && bl == br);
                bool vv = (tl == bl && tr == br);
                printc((hh && vv) ? ' ' : hh ? '-' : vv ? '|' : '+'); // corner
                printc(same_cell(x, y, x, y-1) ? ' ' : '-', hchars); // horz line
            }
            printc('\n');
            // Draw row(s) vertically-interior to squares.
            for (int nrow = 0; nrow < vrows; ++nrow) {
                for (Coord x = 0; x <= width_; ++x) {
                    printc(same_cell(x, y, x-1, y) ? ' ' : '|'); // vert line
                    printc(' ', hchars); // empty interior of square
                }
                printc('\n');
            }
        }
    }
protected:
    struct Coords {
        Coords(Coord x, Coord y) : x(x), y(y) {}
        Coord x,y;
    };
    void printc(char ch, unsigned num = 1) {
        while (num-- > 0)
            printf("%c", ch);
    }
    bool same_cell(Coord x, Coord y, Coord x2, Coord y2) {
        return cell(x, y) == cell(x2, y2);
    }
    Coords rotref(Coord x, Coord y, Rotation rot) const {
        auto xm = width_-x-1;
        auto ym = height_-y-1;
        switch (rot) {
        default:
        case r0:    return Coords(x, y);
        case r90:   return Coords(ym, x);
        case r180:  return Coords(xm, ym);
        case r270:  return Coords(y, xm);
        case r0r:   return Coords(x, ym);
        case r90r:  return Coords(y, x);
        case r180r: return Coords(xm, y);
        case r270r: return Coords(ym, xm);
        }
    }
    Coords rotref_dims(Rotation rot) const {
        switch (rot) {
        case r90: case r270: case r90r: case r270r: 
            return Coords(height_, width_);
        default:
            return Coords(width_, height_);
        }
    }
    // Is s rotated by rot equal to *this?
    bool is_equal(Soln const& s, Rotation rot) const {
        auto rsize = rotref_dims(rot);
        if (rsize.x != width_ || rsize.y != height_)
            return false;
        for (Coord x = 0; x < width_; ++x) {
            for (Coord y = 0; y < height_; ++y) {
                auto s_coords = rotref(x, y, rot);
                if (board_[XY(x, y, width_)] != s.board_[XY(s_coords.x, s_coords.y, s.width_)])
                    return false;
            }
        }
        return true;
    }
private:
    Coord width_;
    Coord height_;
    std::vector<char> board_;
};

// ----------------------------------------------------------------
class PrintInfo {
public:
    typedef Cell::Coord Coord;
    struct TilePos {
        TilePos(std::shared_ptr<Shape> orient, Coord x, Coord y, char vchar)
            : orient(orient), vchar(vchar), x(x), y(y) {}
        std::shared_ptr<Shape> orient;
        char vchar;
        Coord x;
        Coord y;
    };
    PrintInfo() {}
    void init(Coord width, Coord height, VisType vis, VisParam const& vis_param, bool rotref) {
        soln_list_.clear();
        tile_pos_list_.clear();
        total_ = 0;
        width_ = width;
        height_ = height;
        vis_ = vis;
        vis_param_ = vis_param;
        sp_name_ = vis_param.desc_spaces ? 3 : 0;
        sp_coord_ = vis_param.desc_spaces ? 2 : 0;
        rotref_ = rotref;
    }
    void add_tile(std::shared_ptr<Shape> orient, Coord x, Coord y, char tile_char) {
        tile_pos_list_.push_back(PrintInfo::TilePos(orient, x, y, tile_char));
    }
    unsigned total() const { return total_; }
    void print_soln(int row[], int n) {
        Soln soln(width_, height_);
        for (int i = 0; i < n; ++i) {
            PrintInfo::TilePos tp = tile_pos_list_[row[i]];
            FOR_EACH_CELL(cell, tp.orient)
                soln.set_cell(tp.x + cell.x(), tp.y + cell.y(), tp.vchar);
        }
        if (!rotref_) {
            for (auto s2 : soln_list_)
                if (soln.is_rotref(s2))
                    return;
        }
        soln_list_.push_back(soln);
        switch (vis_) {
        case VisType::DESC:
            for (int i = 0; i < n; ++i) {
                PrintInfo::TilePos tp = tile_pos_list_[row[i]];
                printf("%-*s(%*d,%*d) ", sp_name_, tp.orient->name().c_str(),
                    sp_coord_, tp.x, sp_coord_, tp.y);
            }
            printf("\n");
            break;
        case VisType::CHARS:
            soln.draw_vis_chars();
            break;
        case VisType::ART:
            soln.draw_vis_art(vis_param_.art_hchars, vis_param_.art_vrows);
            break;
        default: break;
        }
        total_++;
    }
private:
    std::vector<TilePos> tile_pos_list_;
    std::vector<Soln> soln_list_;
    Coord width_;
    Coord height_;
    VisType vis_;
    VisParam vis_param_;
    bool rotref_;
    int sp_name_;
    int sp_coord_;
    int total_;
};

static PrintInfo PI;

// ----------------------------------------------------------------
static bool create_dlx_row(dlx_t dlx, int dlx_row, Board const& board, Cell::Coord px, Cell::Coord py, int tile_num, std::shared_ptr<Shape> orient)
{
    // Each row of the dlx matrix looks like:
    //   CCCC...CCCC TTTT...TTTT
    // where the first group has one bit for each cell in the board, and
    // the second group has one bit for each tile (eg. 12 bits for pentominos).
    // Thus the entire row contains (board.size() + tiles.size()) bits.
    // The row represents a tile in a specific position and orientation.
    // Bits in the first group indicate which board cells are covered by the tile.
    // Exactly one bit will be set in the second group, to indicate the tile.

    // Make a list of the dlx columns that should be set.
    // Don't actually set them until we're sure we are going to use this dlx row.
    std::list<int> dlx_cols;
    FOR_EACH_CELL(cell, orient) {
        int dlx_col = board.dlx_column(px+cell.x(), py+cell.y());
        if (dlx_col < 0) // tile doesn't fit here; skip this px,py
            return false;
        dlx_cols.push_back(dlx_col);
    }
    if (dlx_cols.empty())
        return false;
    dlx_set(dlx, dlx_row, board.size() + tile_num); // tile indicator
    for (int dlx_col : dlx_cols)
        dlx_set(dlx, dlx_row, dlx_col); // one cell covered by this tile
    return true;
}

// ----------------------------------------------------------------
static size_t all_tiles_size(Tile::Set const& tiles)
{
    size_t size = 0;
    for (auto tile : tiles)
        size += tile->size();
    return size;
}

// ----------------------------------------------------------------
static void print_soln(int row[], int n)
{
    PI.print_soln(row, n);
}

// ----------------------------------------------------------------
int print_solns(Board const& board, Tile::Set const& tiles, VisType vis, VisParam const& vis_param, bool print_rev_name, bool rotref)
{
    if (all_tiles_size(tiles) != board.size()) {
        // Area of tiles is different from area of board; they will never fit.
        printf("error: tiles cover %d squares but board is %d squares\n",
            (int) all_tiles_size(tiles), (int) board.size());
        return 0;
    }

    // Setup PrintInfo for print_soln.
    PI.init(board.width(), board.height(), vis, vis_param, rotref);

    // Create the dlx matrix.
    auto dlx = dlx_new();
    int dlx_row = 0;
    int tile_num = 0;
    for (auto tile : tiles) {
        bool tile_fits = false;
        auto orients = tile->all_orientations();
        for (auto orient : orients) {
            // Place tile shape at every possible px,py on board
            // and make a dlx row for each such position.
            if (orient->height() > board.height() || orient->width() > board.width())
                continue;
            tile_fits = true;
            for (Cell::Coord py = 0; py <= board.height() - orient->height(); ++py)
            for (Cell::Coord px = 0; px <= board.width() - orient->width(); ++px) {
                if (create_dlx_row(dlx, dlx_row, board, px, py, tile_num, orient)) {
                    char tile_char = print_rev_name ? orient->name()[0] : tile->name()[0];
                    PI.add_tile(orient, px, py, tile_char);
                    ++dlx_row;
                }
            }
        }
        if (!tile_fits) {
            printf("error: board is too narrow to fit some tiles\n");
            return 0;
        }
        ++tile_num;
    }
    // Run the dlx solver.
    dlx_forall_cover(dlx, print_soln);
    dlx_clear(dlx);
    return PI.total();
}

// ----------------------------------------------------------------
int main(int argc, char* argv[])
{
    std::string tile_file;
    std::string tile_desc;
    std::string board_file;
    VisType vis = VisType::NONE;
    VisParam vis_param (3,1);
    bool rotref = false;
    bool print_rev_name = false;
    bool print_count = true;

    if (argc > 1 && (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0))
        return print_help();

    int opt;
    while ((opt = getopt(argc, argv, "chlprst:uvVW:x?")) != -1) {
        switch (opt) {
        case 'c': print_count = false; break;
        case 'l': vis = VisType::DESC; break;
        case 'p': tile_desc = tiles_pentominos; break;
        case 'r': rotref = true; break;
        case 's': vis_param.desc_spaces = true; break;
        case 't': tile_file = optarg; break;
        case 'u': print_rev_name = true; break;
        case 'v': vis = VisType::CHARS; break;
        case 'V': vis = VisType::ART; break;
        case 'W': if (sscanf(optarg, "%u,%u", &vis_param.art_hchars, &vis_param.art_vrows) != 2) return usage(); break;
        case 'x': tile_desc = tiles_hexominos; break;
        case 'h': case '?': return print_help();
        default: return usage();
        }
    }
    if (vis == VisType::NONE)
        vis = VisType::CHARS;
    if (optind < argc)
        board_file = argv[optind++];
    if (optind < argc) {
        fprintf(stderr, "error: extra parameters on command line\n");
        return usage();
    }

    std::shared_ptr<Board> board;
    if (!setup_board(board, board_file))
        return usage();

    Tile::Set tiles;
    if (!setup_tiles(tiles, tile_file, tile_desc))
        return 1;

    int n = print_solns(*board.get(), tiles, vis, vis_param, print_rev_name, rotref);
    if (print_count)
        printf("%d solutions\n", n);
    return 0;
}
