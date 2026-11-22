//---------------------------------------------------------------------------------------
// src/fontgenerator.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2022, Steffen Schümann <s.schuemann@pobox.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//---------------------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

// The limits of gui::FontCharInfo (rlguipp/rlguipp_font.hpp): one byte per column, so 8 rows,
// data[7], so at most 7 columns, and a 16 bit codepoint
static constexpr int GLYPH_ROWS = 8;
static constexpr int MAX_GLYPH_WIDTH = 7;
static constexpr int MIN_GLYPH_WIDTH = 5;
static constexpr unsigned long MAX_CODEPOINT = 0xFFFF;

static std::string codepointText(unsigned long codepoint)
{
    char text[16];
    std::snprintf(text, sizeof(text), "U+%04lX", codepoint);
    return text;
}

// Reads font.txt ("char: <codepoint>" followed by 8 rows of '-' and '#') and writes the
// entries of fontRom to stdout. Every error is reported, then nothing is written.
int main(int argc, char* argv[])
{
    if(argc < 2) {
        std::cerr << "usage: " << argv[0] << " <font.txt>" << std::endl;
        return 1;
    }

    std::ifstream is(argv[1]);
    if(!is) {
        std::cerr << argv[1] << ": error: can't open the file" << std::endl;
        return 1;
    }

    int errors = 0;
    auto error = [&](int lineNumber, const std::string& message) {
        std::cerr << argv[1] << ":" << lineNumber << ": error: " << message << std::endl;
        ++errors;
    };

    std::ostringstream out;
    std::map<unsigned long, int> definedIn;  // codepoint to the line of its "char:"
    bool inGlyph = false;
    bool validHeader = false;  // no error in the "char:" line, else its row count is not checked
    unsigned long codepoint = 0;
    int glyphLine = 0;
    uint8_t buffer[MAX_GLYPH_WIDTH]{};
    int rows = 0;
    int width = MIN_GLYPH_WIDTH;

    auto finishGlyph = [&]() {
        if(!inGlyph)
            return;
        if(validHeader && rows != GLYPH_ROWS)
            error(glyphLine, codepointText(codepoint) + " has " + std::to_string(rows) + " rows, a glyph needs " + std::to_string(GLYPH_ROWS));
        out << "    {" << codepoint << "," << width;
        for(int i = 0; i < width; ++i)
            out << "," << (int)buffer[i];
        out << "}," << std::endl;
    };

    std::string line;
    int lineNumber = 0;
    while(std::getline(is, line)) {
        ++lineNumber;
        if(!line.empty() && line.back() == '\r')
            line.pop_back();
        if(line.rfind("char: ", 0) == 0) {
            finishGlyph();
            const char* start = line.c_str() + 6;
            char* end = nullptr;
            codepoint = std::strtoul(start, &end, 0);
            inGlyph = true;
            validHeader = false;
            glyphLine = lineNumber;
            std::memset(buffer, 0, sizeof(buffer));
            rows = 0;
            width = MIN_GLYPH_WIDTH;
            if(end == start) {
                error(lineNumber, "missing codepoint after \"char: \"");
            }
            else if(codepoint > MAX_CODEPOINT) {
                error(lineNumber, codepointText(codepoint) + " is above " + codepointText(MAX_CODEPOINT) + ", fontRom only has 16 bit codepoints");
            }
            else if(auto [it, inserted] = definedIn.emplace(codepoint, lineNumber); !inserted) {
                error(lineNumber, codepointText(codepoint) + " is already defined in line " + std::to_string(it->second));
            }
            else {
                validHeader = true;
            }
        }
        else if(!line.empty() && (line[0] == '-' || line[0] == '#')) {
            if(!inGlyph) {
                error(lineNumber, "glyph row before the first \"char:\" line");
                continue;
            }
            if(rows == GLYPH_ROWS) {
                error(lineNumber, codepointText(codepoint) + " has more than " + std::to_string(GLYPH_ROWS) + " rows");
                continue;
            }
            const uint8_t bit = 1u << rows;
            ++rows;
            if((int)line.size() > MAX_GLYPH_WIDTH) {
                error(lineNumber, "row is " + std::to_string(line.size()) + " pixels wide, a glyph can have at most " + std::to_string(MAX_GLYPH_WIDTH));
                continue;
            }
            for(int i = 0; i < (int)line.size(); ++i) {
                if(line[i] == '#') {
                    buffer[i] |= bit;
                    if(width <= i)
                        width = i + 1;
                }
                else if(line[i] != '-') {
                    error(lineNumber, std::string("unexpected character '") + line[i] + "' in column " + std::to_string(i + 1) + ", rows may only contain '-' and '#'");
                }
            }
        }
    }
    finishGlyph();

    if(errors) {
        std::cerr << errors << (errors == 1 ? " error" : " errors") << ", nothing written" << std::endl;
        return 1;
    }
    std::cout << out.str();
    return 0;
}
