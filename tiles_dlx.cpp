#include <stdio.h>
#include <string.h>
#include <vector>
#include "tiles.h"

extern "C" {
#include "dlx.h"
}

extern int verbose;

// ----------------------------------------------------------------
struct PrintInfo {
    PrintInfo() : visu_width(0), visu_height(0), print_desc(false) {}
    struct TilePos {
        TilePos(std::shared_ptr<Shape> orient, Cell::Coord x, Cell::Coord y, char vchar)
            : orient(orient), vchar(vchar), x(x), y(y) {}
        std::shared_ptr<Shape> orient;
        char vchar;
        Cell::Coord x;
        Cell::Coord y;
    };
    std::vector<TilePos> tile_pos_list;
    Cell::Coord visu_width;
    Cell::Coord visu_height;
    bool print_desc;
    bool rotref;
    int sp_name;
    int sp_coord;
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

    if (verbose) printf("dlx_set %d %d # tile num %d (%s at %d,%d)\n", dlx_row, (int)board.size() + tile_num, tile_num, orient->name().c_str(), px, py);
    // Make a list of the dlx columns that should be set.
    // Don't actually set them until we're sure we are going to use this dlx row.
    std::list<int> cols;
    for (auto cell : orient->cells()) {
        if (verbose) printf("dlx_set %d %d # board pos %d,%d\n", dlx_row, board.dlx_column(px+cell.x(), py+cell.y()), px+cell.x(), py+cell.y());
        int col = board.dlx_column(px+cell.x(), py+cell.y());
        if (col < 0) // tile doesn't fit here; skip this px,py
            return false;
        cols.push_back(col);
    }
    if (cols.empty())
        return false;
    dlx_set(dlx, dlx_row, board.size() + tile_num); // tile indicator
    for (int col : cols)
        dlx_set(dlx, dlx_row, col); // one cell covered by this tile
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
static bool is_rotref(int row[], int n)
{
    return false; // FIXME
}

// ----------------------------------------------------------------
static void print_soln(int row[], int n)
{
    if (!PI.rotref && is_rotref(row, n))
        return;
    std::vector<char> visu (PI.visu_width * PI.visu_height);
    memset(visu.data(), '.', visu.size());
    for (int i = 0; i < n; ++i) {
        PrintInfo::TilePos tp = PI.tile_pos_list[row[i]];
        if (PI.print_desc) {
            printf("%-*s(%*d,%*d) ", PI.sp_name, tp.orient->name().c_str(),
                PI.sp_coord, tp.x, PI.sp_coord, tp.y);
        }
        if (PI.visu_width > 0) { // build the visu matrix
            for (auto cell : tp.orient->cells())
                visu[XY(tp.x + cell.x(), tp.y + cell.y(), PI.visu_width)] = tp.vchar;
        }
    }
    printf("\n");
    if (PI.visu_width > 0) { // print the visu matrix
        for (Cell::Coord y = 0; y < PI.visu_height; y++)
            printf("    %.*s\n", PI.visu_width, visu.data()+XY(Cell::Coord(0),y,PI.visu_width));
    }
}

// ----------------------------------------------------------------
void print_solns(Board const& board, Tile::Set const& tiles, bool desc, bool vis, bool print_space, bool no_rev_name, bool rotref)
{
    if (all_tiles_size(tiles) != board.size())
        return;

    // Create the dlx matrix.
    auto dlx = dlx_new();
    int dlx_row = 0;
    int tile_num = 0;
    for (auto tile : tiles) {
        auto orients = tile->all_orientations();
        for (auto orient : orients) {
            // Place tile shape at every possible px,py on board
            // and make a dlx row for each such position.
            if (orient->height() > board.height() || orient->width() > board.width())
                continue;
            for (Cell::Coord py = 0; py <= board.height() - orient->height(); ++py)
            for (Cell::Coord px = 0; px <= board.width() - orient->width(); ++px) {
                if (create_dlx_row(dlx, dlx_row, board, px, py, tile_num, orient)) {
                    char tile_char = no_rev_name ? tile->name()[0] : orient->name()[0];
                    PI.tile_pos_list.push_back(PrintInfo::TilePos(orient, px, py, tile_char));
                    ++dlx_row;
                }
            }
        }
        ++tile_num;
    }
    // Set PrintInfo values for print_soln.
    PI.visu_width = vis ? board.width() : 0;
    PI.visu_height = vis ? board.height() : 0;
    PI.sp_name = print_space ? 3 : 0;
    PI.sp_coord = print_space ? 2 : 0;
    PI.print_desc = desc;
    PI.rotref = rotref;

    dlx_forall_cover(dlx, print_soln);
    dlx_clear(dlx);
    PI.tile_pos_list.clear();
}
