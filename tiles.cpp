#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include "tiles.h"
#include "linereader.h"

extern "C" {
#include "dlx.h"
}

extern int print_solns(Board const& board, Tile::Set const& tiles, bool desc, bool vis, bool print_space, bool no_rev_name, bool rotref);
extern const char pentominos[];
extern const char help[];

int verbose = 0;

// ----------------------------------------------------------------
static int usage(bool more_info = true)
{
    printf("usage: tiles [-v][-l][-s] [-t tiles][-p] board\n");
    printf("       -v = print ASCII picture for each solution\n");
    printf("       -l = print list of tiles for each solution\n");
    printf("       -s = print extra spaces in -l output for alignment\n");
    printf("       -r = suppress rotations and reflections (NOT IMPLEMENTED)\n");
    printf("       -p = use pentomino tiles\n");
    printf("       -u = use reversed name for reversed tiles in -v display\n");
    printf("       tiles is a file containing one or more tile descriptions\n");
    printf("       board is either a file containing a board description,\n");
    printf("                or \"NxM\" (integer N,M) for a rectangular board\n");
    if (more_info) printf(" \"tiles help\" for more information\n");
    return 1;
}

// ----------------------------------------------------------------
static int print_help()
{
    (void) usage(false);
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
        if (!ok) fprintf(stderr, "internal error!\n");
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
    int total;
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
    std::list<int> dlx_cols;
    for (auto cell : orient->cells()) {
        if (verbose) printf("dlx_set %d %d # board pos %d,%d\n", dlx_row, board.dlx_column(px+cell.x(), py+cell.y()), px+cell.x(), py+cell.y());
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
static bool is_rotref(int row[], int n)
{
    return false; // FIXME
}

// ----------------------------------------------------------------
static void print_soln(int row[], int n)
{
    if (!PI.rotref && is_rotref(row, n))
        return;
    PI.total++;
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
int print_solns(Board const& board, Tile::Set const& tiles, bool desc, bool vis, bool print_space, bool no_rev_name, bool rotref)
{
    if (all_tiles_size(tiles) != board.size()) {
        // Area of tiles is different from area of board; they will never fit.
        printf("error: tiles cover %d squares but board is %d squares\n",
            (int) all_tiles_size(tiles), (int) board.size());
        return 0;
    }

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
                    char tile_char = no_rev_name ? tile->name()[0] : orient->name()[0];
                    PI.tile_pos_list.push_back(PrintInfo::TilePos(orient, px, py, tile_char));
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
    // Set PrintInfo values for print_soln.
    PI.total = 0;
    PI.visu_width = vis ? board.width() : 0;
    PI.visu_height = vis ? board.height() : 0;
    PI.sp_name = print_space ? 3 : 0;
    PI.sp_coord = print_space ? 2 : 0;
    PI.print_desc = desc;
    PI.rotref = rotref;

    dlx_forall_cover(dlx, print_soln);
    dlx_clear(dlx);
    PI.tile_pos_list.clear();
    return PI.total;
}
// ----------------------------------------------------------------
int main(int argc, char* argv[])
{
    std::string tile_file;
    std::string tile_desc;
    std::string board_file;
    bool vis = false;
    bool desc = false;
    bool print_space = false;
    bool rotref = true;
    bool no_rev_name = true;
    bool print_count = true;

    if (argc > 1 && (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0))
        return print_help();

    int opt;
    while ((opt = getopt(argc, argv, "chlprst:uvV?")) != -1) {
        switch (opt) {
        case 'l': desc = true; break;
        case 'c': print_count = false; break;
        case 'p': tile_desc = pentominos; break;
        case 'r': rotref = false; break;
        case 's': print_space = true; break;
        case 't': tile_file = optarg; break;
        case 'u': no_rev_name = false; break;
        case 'v': vis = true; break;
        case 'V': ++verbose; break;
        case 'h': case '?': return print_help();
        default: return usage();
        }
    }
    if (!vis && !desc)
        vis = true;
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

    int n = print_solns(*board.get(), tiles, desc, vis, print_space, no_rev_name, rotref);
    if (print_count)
        printf("%d solutions\n", n);
    return 0;
}
