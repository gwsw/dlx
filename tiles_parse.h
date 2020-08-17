#ifndef _TILES_PARSE_H_
#define _TILES_PARSE_H_

// ----------------------------------------------------------------
bool Shape::parse_shape_line(std::string const& line, Coord y) {
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

// ----------------------------------------------------------------
bool Tile::parse(LineReader& rd) {
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

// ----------------------------------------------------------------
bool Tile::parse_tile_line(std::string const& line) {
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

// ----------------------------------------------------------------
bool Board::parse(LineReader& rd) {
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

// ----------------------------------------------------------------
class BoardDescParser {
public:
    BoardDescParser(std::string const& str) : str_(str), ix_(0) {}
    unsigned get_int() {
        if (at_end()) throw std::runtime_error("missing decimal integer");
        char* p;
        auto num = strtoul(str_.c_str()+ix_, &p, 0);
        auto oix = ix_;
        ix_ = p - str_.c_str();
        if (ix_ == oix) throw std::runtime_error("invalid decimal integer");
        return num;
    }
    bool skip_char(char ch, bool return_error = false) {
        if (getc() == ch) return true;
        ungetc();
        if (return_error) return false;
        throw std::runtime_error("missing "+std::string(1,ch));
    }
    bool at_end() const {
        return ix_ >= str_.size();
    }
    char getc() {
        if (at_end()) throw std::runtime_error("unexpected end of string");
        return str_[ix_++];
    }
    void ungetc() {
        --ix_;
    }
private:
    std::string str_;
    size_t ix_;
};

// ----------------------------------------------------------------
bool ShapeBoard::init(std::string const& desc) {
    if (desc.empty()) return true;
    BoardDescParser parser ((desc[0] == '+') ? desc : std::string("+0,0:") + desc);
    while (!parser.at_end()) {
        try {
            auto action = parser.getc();
            unsigned x = parser.get_int();
            parser.skip_char(',');
            unsigned y = parser.get_int();
            unsigned width = 1;
            unsigned height = 1;
            if (parser.skip_char(':', true)) {
                width = parser.get_int();
                parser.skip_char('x');
                height = parser.get_int();
            }
            switch (action) {
            case '+': add(x, y, width, height); break;
            case '-': remove(x, y, width, height); break;
            default: throw std::runtime_error("invalid char "+std::string(1,action));
            }
        } catch (std::runtime_error& e) {
            fprintf(stderr, "error: %s\n", e.what());
            return false;
        }
    }
    return true;
}

#endif // _TILES_PARSE_H_
