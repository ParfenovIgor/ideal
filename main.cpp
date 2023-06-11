#include <ncurses.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fstream>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>

WINDOW *wCodeEditor, *wFiles, *wConsole, *wInfo;
int wCodeEditorHeight = 40;
int wConsoleHeight = 15;

std::string filename;
int lines;
int editor_cursor_line, editor_cursor_pos;
int editor_view_line;
std::vector < std::string > text;
std::string output;
std::vector < std::string > console_text;
int console_cursor_pos;
int console_view_line;

bool draw_active = false;

int active_window = 0;

bool wasType;

int writepipe[2], readpipe[2];

bool is_char(int c) {
    return (c >= 32 && c <= 126);
}

std::vector < std::string > fileList() {
    std::vector < std::string > res;
    DIR *d = opendir(".");
    struct dirent *dir;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            std::string str(dir->d_name);
            res.push_back(str);
        }
        closedir(d);
    }
    return res;
}

void init(std::string _filename) {
    filename = _filename;
    std::string line;
    text.clear();
    std::ifstream fin(filename);
    if (fin) {
        while (getline(fin, line)) {
            text.push_back(line);
        }
    }
    if (text.empty()) {
        text.push_back("");
    }
    lines = (int)text.size();
    editor_cursor_line = 0;
    editor_cursor_pos = 0;
    editor_view_line = 0;
    console_cursor_pos = 2;
    console_view_line = 0;
}

void save_file() {
    std::ofstream fout(filename);
    for (std::string line : text) {
        fout << line << '\n';
    }
}

class ProgramData {
public:
    bool error;
    int line_begin, line_end, pos_begin, pos_end;
    std::string error_filename, error_text;
    std::map < std::string, std::vector <std::string> > states;

    ProgramData() {
    }
};

ProgramData programData;

void compile() {
    save_file();

    int writepipe[2], readpipe[2];
    pipe(writepipe);
    pipe(readpipe);

    int pid = fork();
    
    if (!pid) {
        close(writepipe[1]);
        close(readpipe[0]);
        dup2(writepipe[0], 0);
        close(writepipe[0]);
        dup2(readpipe[1], 1);
        dup2(readpipe[1], 2);
        close(readpipe[1]);
        char *args[6];
        args[0] = strdup("calias");
        args[1] = strdup("-l");
        args[2] = strdup("main.al");
        args[3] = strdup("-o");
        args[4] = strdup("program");
        args[5] = NULL;
        execvp("calias", args);
    }
    close(writepipe[0]);
    close(readpipe[1]);
}

void console_execute() {
    std::string str = console_text.back().substr(2);
    const char *cmd = str.c_str();
    int total_size = strlen(cmd);
    int written_size = 0;
    while (written_size < total_size) {
        int size = write(writepipe[1], cmd + written_size, total_size - written_size);
        written_size += total_size;
    }
    printf("%d\n", total_size);
    console_text.push_back("> ");
    console_cursor_pos = 2;
}

void execute() {
    console_text.back() = "> ./program\n";
    console_execute();
}

void draw_wCodeEditor() {
    werase(wCodeEditor);

    for (int line = editor_view_line; line < std::min(lines, editor_view_line + (wCodeEditorHeight - 3)); line++) {
        wattron(wCodeEditor, COLOR_PAIR(2));
        std::string number = std::to_string(line + 1);
        for (int pos = 0; pos < (int)number.size(); pos++) {
            mvwaddch(wCodeEditor, line + 2 - editor_view_line, pos + 1, number[pos]);
        }

        if (!programData.error && line < (int)programData.states[filename].size()) {
            for (int pos = 0; pos < (int)programData.states[filename][line].size(); pos++) {
                mvwaddch(wCodeEditor, line + 2, pos + 75, programData.states[filename][line][pos]);
            }
        }
        wattroff(wCodeEditor, COLOR_PAIR(2));

        for (int pos = 0; pos < (int)text[line].size(); pos++) {
            if (programData.error && programData.error_filename == filename && 
                ((line + 1 == programData.line_begin && programData.line_begin == programData.line_end && pos + 1 >= programData.pos_begin && pos + 1 <= programData.pos_end) ||
                (line + 1 == programData.line_begin && programData.line_begin != programData.line_end && pos + 1 >= programData.pos_begin) ||
                (line + 1 == programData.line_end && programData.line_begin != programData.line_end && pos + 1 <= programData.pos_end) ||
                (line + 1 > programData.line_begin && line + 1 < programData.line_end)))
                wattron(wCodeEditor, COLOR_PAIR(3));
            if (line == editor_cursor_line && pos == editor_cursor_pos)
                wattron(wCodeEditor, COLOR_PAIR(1));
            mvwaddch(wCodeEditor, line + 2 - editor_view_line, pos + 6, text[line][pos]);
            wattroff(wCodeEditor, COLOR_PAIR(3));
            wattroff(wCodeEditor, COLOR_PAIR(1));
        }
        if (line == editor_cursor_line && (int)text[line].size() == editor_cursor_pos) {
            wattron(wCodeEditor, COLOR_PAIR(1));
            mvwaddch(wCodeEditor, editor_cursor_line + 2 - editor_view_line, editor_cursor_pos + 6, ' ');
            wattroff(wCodeEditor, COLOR_PAIR(1));
        }
    }

    if (programData.error) {
        wattron(wCodeEditor, COLOR_PAIR(3));
        for (int pos = 0; pos < (int)programData.error_text.size(); pos++) {
            mvwaddch(wCodeEditor, 1, pos + 1, programData.error_text[pos]);
        }
        wattroff(wCodeEditor, COLOR_PAIR(3));
    }

    box(wCodeEditor, 0, 0);
    mvwaddstr(wCodeEditor, 0, 0, filename.c_str());
    wrefresh(wCodeEditor);
}

void draw_wFiles() {
    werase(wFiles);

    std::vector < std::string > files = fileList();
    for (int line = 0; line < (int)files.size(); line++) {
        if (files[line] == filename) {
            mvwaddch(wFiles, line + 1, 1, '>');
        }
        if (programData.error && files[line] == programData.error_filename)
            wattron(wFiles, COLOR_PAIR(3));
        else
            wattron(wFiles, COLOR_PAIR(2));
        for (int pos = 0; pos < (int)files[line].size(); pos++) {
            mvwaddch(wFiles, line + 1, pos + 3, files[line][pos]);
        }
        wattroff(wFiles, COLOR_PAIR(2));
        wattroff(wFiles, COLOR_PAIR(3));
    }

    box(wFiles, 0, 0);
    mvwaddstr(wFiles, 0, 0, "Files");
    wrefresh(wFiles);
}

void draw_wConsole() {
    werase(wConsole);

    for (int line = 0; line < std::min((int)console_text.size(), console_view_line + (wConsoleHeight - 3)); line++) {
        for (int pos = 0; pos < console_text[line].size(); pos++) {
            if (line == (int)console_text.size() - 1 && pos == console_cursor_pos)
                wattron(wConsole, COLOR_PAIR(1));
            mvwaddch(wConsole, line + 1 - console_view_line, pos + 1, console_text[line][pos]);
            wattroff(wConsole, COLOR_PAIR(1));
        }
        if (line == (int)console_text.size() - 1 && console_text[line].size() == console_cursor_pos) {
            wattron(wConsole, COLOR_PAIR(1));
            mvwaddch(wConsole, line + 1 - console_view_line, console_cursor_pos + 1, ' ');
            wattroff(wConsole, COLOR_PAIR(1));
        }
    }

    box(wConsole, 0, 0);
    mvwaddstr(wConsole, 0, 0, "Console");
    wrefresh(wConsole);
}

void draw_wInfo() {
    werase(wInfo);

    box(wInfo, 0, 0);
    mvwaddstr(wInfo, 0, 0, "Info");
    mvwaddstr(wInfo, 1, 1, "F2 - Exit");
    mvwaddstr(wInfo, 2, 1, "F3 - Compile");
    mvwaddstr(wInfo, 3, 1, "F4 - Run");
    mvwaddstr(wInfo, 4, 1, "F5 - Code Editor");
    mvwaddstr(wInfo, 5, 1, "F6 - File Directory");
    mvwaddstr(wInfo, 6, 1, "F7 - Console");
    wrefresh(wInfo);
}

void draw() {
    if (draw_active) return;
    draw_active = true;

    erase();
    refresh();
    
    draw_wCodeEditor();
    draw_wFiles();
    draw_wConsole();
    draw_wInfo();

    draw_active = false;
}

void validate() {
    save_file();

    int writepipe[2], readpipe[2];
    pipe(writepipe);
    pipe(readpipe);

    int pid = fork();
    
    if (!pid) {
        close(writepipe[1]);
        close(readpipe[0]);
        dup2(writepipe[0], 0);
        close(writepipe[0]);
        dup2(readpipe[1], 1);
        dup2(readpipe[1], 2);
        close(readpipe[1]);
        char *args[4];
        args[0] = strdup("calias");
        args[1] = strdup("-s");
        args[2] = strdup("main.al");
        args[3] = NULL;
        execvp("calias", args);
    }
    close(writepipe[0]);
    close(readpipe[1]);

    std::string stream;
    char buffer[1001];
    while (int size = read(readpipe[0], buffer, 1000)) {
        buffer[size] = '\0';
        std::string str(buffer);
        stream += buffer;
    }

    programData = ProgramData();

    std::stringstream input(stream);
    std::string str;
    if (std::getline(input, str)) {
        if (str == "States") {
            programData.error = false;
            std::string filename;
            while (std::getline(input, filename)) {
                int data_cnt;
                input >> data_cnt;
                for (int i = 0; i < data_cnt; i++) {
                    int line;
                    std::string nstates;
                    char c;
                    input >> line >> c >> nstates;
                    while (programData.states[filename].size() <= line - 1)
                        programData.states[filename].push_back("");
                    programData.states[filename][line - 1] = nstates;
                }
                std::getline(input, filename);
            }
        }
        else {
            programData.error = true;
            std::getline(input, programData.error_filename);
            char c;
            std::getline(input, str);
            std::stringstream ss(str);
            ss >> programData.line_begin >> c >> programData.pos_begin >> c >> programData.line_end >> c >> programData.pos_end;
            std::getline(input, programData.error_text);
        }
    }

    int status;
    waitpid(pid, &status, 0);
}

void read_wCodeEditor(int key) {
    if (key == KEY_UP) {
        if (editor_cursor_line > 0) {
            editor_cursor_line--;
            editor_cursor_pos = std::min(editor_cursor_pos, (int)text[editor_cursor_line].size());
        }
        else {
            editor_cursor_pos = 0;
        }
    }

    if (key == KEY_DOWN) {
        if (editor_cursor_line + 1< lines) {
            editor_cursor_line++;
            editor_cursor_pos = std::min(editor_cursor_pos, (int)text[editor_cursor_line].size());
        }
        else {
            editor_cursor_pos = (int)text[editor_cursor_line].size();
        }
    }

    if (key == KEY_LEFT) {
        if (editor_cursor_pos > 0) {
            editor_cursor_pos--;
        }
        else if (editor_cursor_line > 0) {
            editor_cursor_line--;
            editor_cursor_pos = (int)text[editor_cursor_line].size();
        }
    }

    if (key == KEY_RIGHT) {
        if (editor_cursor_pos < (int)text[editor_cursor_line].size()) {
            editor_cursor_pos++;
        }
        else if (editor_cursor_line + 1 < lines) {
            editor_cursor_line++;
            editor_cursor_pos = 0;
        }
    }

    if (key == '\n') {
        text.push_back("");
        for (int i = (int)text.size() - 1; i > editor_cursor_line + 1; i--) {
            text[i] = text[i - 1];
        }
        text[editor_cursor_line + 1] = text[editor_cursor_line].substr(editor_cursor_pos);
        text[editor_cursor_line] = text[editor_cursor_line].substr(0, editor_cursor_pos);
        editor_cursor_line++;
        editor_cursor_pos = 0;
        lines++;
    }

    if (key == KEY_BACKSPACE) {
        if (editor_cursor_pos > 0) {
            std::string a = text[editor_cursor_line].substr(0, editor_cursor_pos - 1) + text[editor_cursor_line].substr(editor_cursor_pos);
            text[editor_cursor_line] = a;
            editor_cursor_pos--;
        }
        else if (editor_cursor_line > 0) {
            editor_cursor_pos = (int)text[editor_cursor_line - 1].size();
            text[editor_cursor_line - 1] += text[editor_cursor_line];
            for (int line = editor_cursor_line; line + 1 < lines; line++) {
                text[line] = text[line + 1];
            }
            text.pop_back();
            lines--;
            editor_cursor_line--;
        }
    }

    if (key == KEY_DC) {
        if (editor_cursor_pos < (int)text[editor_cursor_line].size()) {
            std::string a = text[editor_cursor_line].substr(0, editor_cursor_pos) + text[editor_cursor_line].substr(editor_cursor_pos + 1);
            text[editor_cursor_line] = a;
        }
        else if (editor_cursor_line + 1 < lines) {
            text[editor_cursor_line] += text[editor_cursor_line + 1];
            for (int line = editor_cursor_line + 1; line + 1 < lines; line++) {
                text[line] = text[line + 1];
            }
            text.pop_back();
            lines--;
        }
    }

    if (key == KEY_HOME) {
        editor_cursor_pos = 0;
    }

    if (key == KEY_END) {
        editor_cursor_pos = (int)text[editor_cursor_line].size();
    }

    std::function <void(int)> print_char = [&](int key) {
        std::string a = text[editor_cursor_line].substr(0, editor_cursor_pos);
        a.push_back(key);
        a += text[editor_cursor_line].substr(editor_cursor_pos);
        text[editor_cursor_line] = a;
        editor_cursor_pos++;
    };

    if (is_char(key)) {
        print_char(key);
    }

    if (key == '\t') {
        if (editor_cursor_pos % 4 == 0) {
            print_char(' ');
        }
        while (editor_cursor_pos % 4 != 0) {
            print_char(' ');
        }
    }

    if (editor_cursor_line < editor_view_line)
        editor_view_line = editor_cursor_line;

    if (editor_cursor_line > editor_view_line + (wCodeEditorHeight - 4))
        editor_view_line = editor_cursor_line - (wCodeEditorHeight - 4);
}

void read_wFiles(int key) {
    if (key == KEY_UP) {
        save_file();
        std::vector < std::string > files = fileList();
        int k = (int)files.size();
        int pos = -1;
        for (int i = 0; i < k; i++) {
            if (files[i] == filename) {
                pos = i;
            }
        }
        if (pos == -1) {
            pos = 0;
        }
        else {
            pos = (pos - 1 + k) % k;
        }
        init(files[pos]);
    }

    if (key == KEY_DOWN) {
        save_file();
        std::vector < std::string > files = fileList();
        int k = (int)files.size();
        int pos = -1;
        for (int i = 0; i < k; i++) {
            if (files[i] == filename) {
                pos = i;
            }
        }
        if (pos == -1) {
            pos = 0;
        }
        else {
            pos = (pos + 1) % k;
        }
        init(files[pos]);
    }
}

void read_wConsole(int key) {
    if (key == KEY_UP) {
        console_view_line--;
        return;
    }

    if (key == KEY_DOWN) {
        console_view_line++;
        return;
    }

    if (key == KEY_LEFT) {
        if (console_cursor_pos > 2) {
            console_cursor_pos--;
        }
    }

    if (key == KEY_RIGHT) {
        if (console_cursor_pos < (int)console_text.back().size()) {
            console_cursor_pos++;
        }
    }

    if (key == '\n') {
        console_text.back().push_back('\n');
        console_execute();
    }
    
    if (key == KEY_BACKSPACE) {
        if (console_cursor_pos > 2) {
            console_text.back() = console_text.back().substr(0, console_cursor_pos - 1) + console_text.back().substr(console_cursor_pos);
            console_cursor_pos--;
        }
    }
    
    if (key == KEY_DC) {
        if (editor_cursor_pos < (int)console_text.back().size()) {
            console_text.back() = console_text.back().substr(0, console_cursor_pos) + console_text.back().substr(console_cursor_pos + 1);
        }
    }

    if (key == KEY_HOME) {
        console_cursor_pos = 2;
    }

    if (key == KEY_END) {
        console_cursor_pos = console_text.back().size();
    }

    std::function <void(int)> print_char = [&](int key) {
        std::string a = console_text.back().substr(0, console_cursor_pos);
        a.push_back(key);
        a += console_text.back().substr(console_cursor_pos);
        console_text.back() = a;
        console_cursor_pos++;
    };

    if (is_char(key)) {
        print_char(key);
    }

    if ((int)console_text.size() - 1 < console_view_line)
        console_view_line = (int)console_text.size() - 1;

    if ((int)console_text.size() - 1 > console_view_line + (wConsoleHeight - 4))
        console_view_line = ((int)console_text.size() - 1) - (wConsoleHeight - 4);
}

void *thread_update(void*) {
    bool need_draw = false;
    while (true) {
        usleep(1000000);
        if (wasType) {
            validate();
            need_draw = true;
        }
        wasType = false;
        if (need_draw && !draw_active) {
            draw();
            need_draw = false;
        }
    }
    return NULL;
}

void *thread_sh(void*) {
    bool need_draw = false;
    while (true) {
        usleep(1000000);
        char buffer[1000];
        int size = read(readpipe[0], buffer, 999);
        buffer[size] = '\0';

        std::string stream(buffer);
        std::stringstream input(stream);
        std::string line;
        while (std::getline(input, line)) {
            std::string last = console_text.back();
            console_text.pop_back();
            console_text.push_back(line);
            console_text.push_back(last);
        }
        if (need_draw && !draw_active) {
            draw();
            need_draw = false;
        }
    }
    return NULL;
}

void init_sh() {
    pipe(writepipe);
    pipe(readpipe);

    int pid = fork();
    
    if (pid) {
        close(writepipe[0]);
        close(readpipe[1]);
        pthread_t id;
        pthread_create(&id, NULL, thread_sh, NULL);
    }
    else {
        close(writepipe[1]);
        close(readpipe[0]);
        dup2(writepipe[0], 0);
        close(writepipe[0]);
        dup2(readpipe[1], 1);
        dup2(readpipe[1], 2);
        close(readpipe[1]);
        execvp("sh", NULL);
    }
}

int main(int argc, char *argv[]) {
    init_sh();
    
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
    
    init("main.al");
    wCodeEditor =   newwin(40, 80, 0, 0);
    wFiles =        newwin(40, 30, 0, 80);
    wConsole =      newwin(15, 80, 40, 0);
    wInfo =         newwin(15, 30, 40, 80);
    console_text.push_back("> ");
    validate();
    draw();

    pthread_t id;
    pthread_create(&id, NULL, thread_update, NULL);

    while (true) {
        int key = getch();

        if (active_window == 0) read_wCodeEditor(key);
        if (active_window == 1) read_wFiles(key);
        if (active_window == 2) read_wConsole(key);
        
        if (key == KEY_F(2)) {
            save_file();
            break;
        }

        if (key == KEY_F(3)) {
            compile();
        }

        if (key == KEY_F(4)) {
            execute();
        }

        if (key == KEY_F(5)) {
            active_window = 0;
        }

        if (key == KEY_F(6)) {
            active_window = 1;
        }

        if (key == KEY_F(7)) {
            active_window = 2;
        }

        wasType = true;

        draw();
    }

    endwin();

    return 0;
}