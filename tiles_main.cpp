#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "tiles.h"
#include "linereader.h"

extern void print_solns(Board const& board, Tile::Set const& tiles, bool desc, bool vis, bool print_space);
extern const char pentominos[];
extern const char help[];

int verbose = 0;

// ----------------------------------------------------------------
static int usage(bool more_info = true)
{
    printf("usage: tiles [-v][-l][-s] [-t tiles][-p] board\n");
    printf("       -v = print ASCII picture for each solution\n");
    printf("       -l = print list of tiles for each solution\n");
    printf("       -s = print extra spaces in description for alignment\n");
    printf("       -p = use pentomino tiles\n");
    printf("       tiles is a file containing one or more tile descriptions\n");
    printf("       board is either a file containing a board description,\n");
    printf("                or \"NxM\" (integer N,M) for a rectangular board\n");
    if (more_info) printf("       \"tiles help\" for more information\n");
    return 1;
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
    Tile tile;
    for (;;) {
        if (!tile.parse(rd))
            return rd.eof();
        tiles.push_back(std::make_shared<Tile>(tile));
    }
}

// ----------------------------------------------------------------
static bool setup_tiles(Tile::Set& tiles, std::string const& tile_file, bool pent)
{
    bool ok = false;
    if (tile_file.empty()) {
        if (!pent) {
            fprintf(stderr, "error: no tile description\n");
        } else {
            StringLineReader rd (pentominos);
            ok = parse_tiles(rd, tiles);
            if (!ok) fprintf(stderr, "internal error!\n");
        }
    } else {
        FILE* f = fopen(tile_file.c_str(), "r");
        if (f == NULL) {
            fprintf(stderr, "error: cannot open tile file %s\n", tile_file.c_str());
        } else {
            FileLineReader rd (f);
            ok = parse_tiles(rd, tiles);
            fclose(f);
            if (!ok) fprintf(stderr, "error: cannot parse tile file %s\n", tile_file.c_str());
        }
    }
    return ok;
}

// ----------------------------------------------------------------
int main(int argc, char* argv[])
{
    std::string tile_file;
    std::string board_file;
    bool vis = false;
    bool desc = false;
    bool print_space = false;

    char const* slash = strrchr(argv[0], '/');
    char const* pgm = (slash == nullptr) ? argv[0] : slash+1;
    bool pent = (strcmp(pgm, "pent") == 0);

    if (argc > 1 && (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0)) {
        (void) usage(false);
        printf("%s\n", help);
        return 0;
    }

    int opt;
    while ((opt = getopt(argc, argv, "lpst:vV")) != -1) {
        switch (opt) {
        case 'l': desc = true; break;
        case 'p': pent = true; break;
        case 's': print_space = true; break;
        case 't': tile_file = optarg; break;
        case 'v': vis = true; break;
        case 'V': ++verbose; break;
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
    if (!setup_tiles(tiles, tile_file, pent))
        return 1;

    print_solns(*board.get(), tiles, desc, vis, print_space);
    return 0;
}
