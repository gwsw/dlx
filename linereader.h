#ifndef _LINEREADER_H_
#define _LINEREADER_H_

#include <string>

// Abstract class to read text lines.
// Derived class just needs to implement getchar().
class LineReader {
public:
    enum { END = 0 };

    // Construct LineReader.
    // If trim==true, getline trims read lines.
    LineReader(bool trim = true) : eof_(false), trim_(trim) {}

    // Are we at eof?
    bool eof() { return eof_; }

    // Read a line and return it with the trailing newline removed.
    std::string getline() {
        std::string line;
        for (;;) {
            int ch = getchar();
            if (ch == END) { eof_ = true; break; }
            if (ch == '\n' || ch == '\r' || ch == ';') break;
            line += ch;
        }
        if (trim_) line = trim(line);
        return line;
    }

protected:

    // Remove whitespace from start and end of a line.
    static std::string trim(std::string const& line) {
        size_t start = line.find_first_not_of(" ");
        if (start == std::string::npos) start = 0;
        size_t end = line.find_last_not_of(" ");
        if (end == std::string::npos) end = line.size()-1;
        return line.substr(start, end-start+1);
    }

private:
    virtual int getchar() =0;
    bool eof_;
    bool trim_;
};

// Read lines from a file.
class FileLineReader : public LineReader {
public:
    FileLineReader(FILE* file, bool trim = true) : LineReader(trim), file_(file) {}
private:
    virtual int getchar() override {
        int ch = fgetc(file_);
        if (ch == EOF) return END;
        return ch;
    }
    FILE* file_;
};

// Read lines from a string.
class StringLineReader : public LineReader {
public:
    StringLineReader(std::string str, bool trim = true) : LineReader(trim), str_(str), pos_(0) {}
private:
    virtual int getchar() override {
        if (pos_ >= str_.size()) return END;
        return str_[pos_++];
    }
    std::string str_;
    size_t pos_;
};

#endif // _LINEREADER_H_
