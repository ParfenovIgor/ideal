#include <ncurses.h>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

std::string filename = "program.al";

int lines;
int cursor_line, cursor_pos;
std::vector < std::string > text;

void print(int n) {
    std::string s;
    if (n == 0)
        s.push_back('0');
    while (n) {
        s.push_back((char)(n % 10 + '0'));
        n /= 10;
    }
    std::reverse(s.begin(), s.end());
    s.push_back('\n');
    printw(s.c_str());
}

bool is_char(int c) {
    return (c >= 32 && c <= 126);
}

void init() {
    std::ifstream fin(filename);
    std::string line;
    while (getline(fin, line)) {
        text.push_back(line);
    }
    lines = (int)text.size();
    cursor_line = 0;
    cursor_pos = 0;
}

void save_file() {
    std::ofstream fout(filename);
    for (std::string line : text) {
        fout << line << '\n';
    }
}

void draw() {
    save_file();

    for (int line = 0; line < 100; line++) {
        for (int pos = 0; pos < 100; pos++) {
            mvaddch(line, pos, ' ');
        }
    }

    bool error = false;
    int line_begin, line_end, pos_begin, pos_end;
    std::string error_text;

    std::vector <std::string> states(lines);
    std::string command = "./alias -S " + filename + " > log";
    system(command.c_str());
    std::ifstream fin("log");
    std::string str;
    if (std::getline(fin, str)) {
        if (str == "States") {
            int line;
            std::string state;
            char c;
            while (fin >> line >> c >> state) {
                states[line - 1] = state;
            }
        }
        else {
            error = true;
            char c;
            std::stringstream ss(str);
            ss >> line_begin >> c >> pos_begin >> c >> line_end >> c >> pos_end;
            std::getline(fin, error_text);
        }
    }
    fin.close();
    
    for (int line = 0; line < lines; line++) {
        attron(COLOR_PAIR(2));
        std::string number = std::to_string(line + 1);
        for (int pos = 0; pos < (int)number.size(); pos++) {
            mvaddch(line + 1, pos, number[pos]);
        }

        if (!error) {
            for (int pos = 0; pos < (int)states[line].size(); pos++) {
                mvaddch(line + 1, pos + 50, states[line][pos]);
            }
        }
        attroff(COLOR_PAIR(2));

        for (int pos = 0; pos < (int)text[line].size(); pos++) {
            if (error && 
                ((line + 1 == line_begin && line_begin == line_end && pos + 1 >= pos_begin && pos + 1 <= pos_end) ||
                (line + 1 == line_begin && line_begin != line_end && pos + 1 >= pos_begin) ||
                (line + 1 == line_end && line_begin != line_end && pos + 1 <= pos_end) ||
                (line + 1 > line_begin && line + 1 < line_end)))
                attron(COLOR_PAIR(3));
            if (line == cursor_line && pos == cursor_pos)
                attron(COLOR_PAIR(1));
            mvaddch(line + 1, pos + 5, text[line][pos]);
            attroff(COLOR_PAIR(3));
            attroff(COLOR_PAIR(1));
        }
        if (line == cursor_line && (int)text[line].size() == cursor_pos) {
            attron(COLOR_PAIR(1));
            mvaddch(cursor_line + 1, cursor_pos + 5, ' ');
            attroff(COLOR_PAIR(1));
        }
    }

    if (error) {
        attron(COLOR_PAIR(3));
        for (int pos = 0; pos < (int)error_text.size(); pos++) {
            mvaddch(0, pos, error_text[pos]);
        }
        attroff(COLOR_PAIR(3));
    }
}

void print_char(int key) {
    std::string a = text[cursor_line].substr(0, cursor_pos);
    a.push_back(key);
    a += text[cursor_line].substr(cursor_pos);
    text[cursor_line] = a;
    cursor_pos++;
}

int main(int argc, char *argv[]) {
    if (argc != 1) {
        filename = std::string(argv[1]);
    }

    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_BLUE, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);
    
    init();
    draw();

    while (true) {
        int key = getch();
        
        if (key == KEY_UP) {
            if (cursor_line > 0) {
                cursor_line--;
                cursor_pos = std::min(cursor_pos, (int)text[cursor_line].size());
            }
            else {
                cursor_pos = 0;
            }
        }

        if (key == KEY_DOWN) {
            if (cursor_line + 1< lines) {
                cursor_line++;
                cursor_pos = std::min(cursor_pos, (int)text[cursor_line].size());
            }
            else {
                cursor_pos = (int)text[cursor_line].size();
            }
        }

        if (key == KEY_LEFT) {
            if (cursor_pos > 0) {
                cursor_pos--;
            }
            else if (cursor_line > 0) {
                cursor_line--;
                cursor_pos = (int)text[cursor_line].size();
            }
        }

        if (key == KEY_RIGHT) {
            if (cursor_pos < (int)text[cursor_line].size()) {
                cursor_pos++;
            }
            else if (cursor_line + 1 < lines) {
                cursor_line++;
                cursor_pos = 0;
            }
        }

        if (key == '\n') {
            text.push_back("");
            for (int i = (int)text.size() - 1; i > cursor_line + 1; i--) {
                text[i] = text[i - 1];
            }
            text[cursor_line + 1] = text[cursor_line].substr(cursor_pos);
            text[cursor_line] = text[cursor_line].substr(0, cursor_pos);
            cursor_line++;
            cursor_pos = 0;
            lines++;
        }

        if (key == KEY_BACKSPACE) {
            if (cursor_pos > 0) {
                std::string a = text[cursor_line].substr(0, cursor_pos - 1) + text[cursor_line].substr(cursor_pos);
                text[cursor_line] = a;
                cursor_pos--;
            }
            else if (cursor_line > 0) {
                cursor_pos = (int)text[cursor_line - 1].size();
                text[cursor_line - 1] += text[cursor_line];
                for (int line = cursor_line; line + 1 < lines; line++) {
                    text[line] = text[line + 1];
                }
                text.pop_back();
                lines--;
                cursor_line--;
            }
        }

        if (key == KEY_DC) {
            if (cursor_pos < (int)text[cursor_line].size()) {
                std::string a = text[cursor_line].substr(0, cursor_pos) + text[cursor_line].substr(cursor_pos + 1);
                text[cursor_line] = a;
            }
            else if (cursor_line + 1 < lines) {
                text[cursor_line] += text[cursor_line + 1];
                for (int line = cursor_line + 1; line + 1 < lines; line++) {
                    text[line] = text[line + 1];
                }
                text.pop_back();
                lines--;
            }
        }

        if (key == KEY_F(4)) {
            save_file();
            break;
        }

        if (key == KEY_HOME) {
            cursor_pos = 0;
        }

        if (key == KEY_END) {
            cursor_pos = (int)text[cursor_line].size();
        }

        if (is_char(key)) {
            print_char(key);
        }

        if (key == '\t') {
            if (cursor_pos % 4 == 0) {
                print_char(' ');
            }
            while (cursor_pos % 4 != 0) {
                print_char(' ');
            }
        }

        draw();
    }

    endwin();

    return 0;
}