#ifndef _LINEREADER_H_
#define _LINEREADER_H_

#include <string>

class LineReader {
public:
    enum { END = 0 };
    LineReader(bool trim = true) : eof_(false), trim_(trim) {}
    bool eof() { return eof_; }
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

class FileLineReader : public LineReader {
public:
    FileLineReader(FILE* file, bool trim = true) : LineReader(trim), file(file) {}
private:
    virtual int getchar() override {
        int ch = fgetc(file);
        if (ch == EOF) return END;
        return ch;
    }
    FILE* file;
};

class StringLineReader : public LineReader {
public:
    StringLineReader(std::string str, bool trim = true) : LineReader(trim), str(str), pos(0) {}
private:
    virtual int getchar() override {
        if (pos >= str.size()) return END;
        return str[pos++];
    }
    std::string str;
    size_t pos;
};

#endif // _LINEREADER_H_
