/*
    a simple text editor written in C++, with fltk 1.4.4.
*/
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_string_functions.h>
#include <FL/platform.H>
#include <FL/fl_ask.H>
#include <FL/filename.H>
#include <FL/Fl_Flex.H>
#include <FL/Fl.H>
#include <string>
#include <array>
#include <cerrno>
#include <cstring>
#include <cassert>

static Fl_Text_Display::Style_Table_Entry styletable[] = {     // Style table
  { FL_BLACK, FL_TIMES_BOLD, FL_NORMAL_SIZE }, // A - Plain
  { FL_BLACK, FL_TIMES_BOLD, FL_NORMAL_SIZE, Fl_Text_Display::ATTR_BGCOLOR, FL_YELLOW }, // B - highlight search patterns.
};

struct ShortcutKey {
    std::string key;
    std::string description;

    ShortcutKey(const std::string& _key, const std::string& _description) : key { _key }, description{ _description } {} 
};

static ShortcutKey shortcuts[] = {
    { "Ctrl + F", "查找" },
    { "Ctrl + G", "查找所有" },
    { "Ctrl + P", "查找上一个" },
    { "Ctrl + N", "查找下一个" },
    { "Ctrl + R", "查找并替换" },
    { "Ctrl + C", "复制" },
    { "Ctrl + V", "粘贴" },
    { "Ctrl + Z", "撤销" },
    { "Ctrl + X", "剪切" },
    { "Ctrl + L", "行号显示开启/关闭" },
    { "Ctrl + W", "自动换行开启/关闭" },
    { "Ctrl + O", "打开文件" },
    { "Ctrl + Q", "退出文本编辑器" },
    { "Ctrl + S", "保存" },
    { "Ctrl + Shift + S", "另存为" },
    { "Ctrl + Shift + Z", "回退" },
    { "Ctrl + Shift + N", "创建新文件" },
    { "Esc", "退出文本编辑器" }
};

class ShortcutKeyHelpPage : public Fl_Double_Window {
    Fl_Button* closeButton;

    static void close_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        ShortcutKeyHelpPage* skhp = static_cast<ShortcutKeyHelpPage*>(param);
        skhp->hide();
    }
public:
    ShortcutKeyHelpPage(const char* label) : Fl_Double_Window{ 500, 500, label } {
        closeButton = new Fl_Button(0, 0, 0, 0, "关闭");
        closeButton->callback(close_callback, this);

        color(FL_WHITE);
    }

    void show() override {
        Fl_Double_Window::show();
    }

    void draw() override {
        Fl_Double_Window::draw();
        
        fl_color(FL_BLACK);
        fl_font(FL_TIMES_BOLD, 14);

        const char* titleKey = "快捷键";
        const char* titleDesc = "说明";

        const int keyStartX = w() / 5 - fl_width(titleKey) / 2;
        const int descStartX = 8 * w() / 15 + fl_width(titleDesc) / 2;
        const int titleStartY = 30;
        const int lineStartY = titleStartY + fl_height();
        
        const int paddingY = 20;

        fl_draw(titleKey, keyStartX, titleStartY);
        fl_draw(titleDesc, descStartX, titleStartY);
        
        fl_line(0, lineStartY, w(), lineStartY);

        int shortcutsLen = sizeof(shortcuts) / sizeof(shortcuts[0]);
        for (int i = 0; i < shortcutsLen; i++) {
            int y = lineStartY + (i + 1) * paddingY;

            fl_draw(shortcuts[i].key.c_str(), keyStartX, y);
            fl_draw(shortcuts[i].description.c_str(), descStartX, y);
        }
    }
};

class TextEditor;

class ReplaceDialog : public Fl_Double_Window {
    Fl_Input* findTextInput;
    Fl_Input* replaceTextInput;
    Fl_Button* findNextButton;
    Fl_Button* replaceAndFindButton;
    Fl_Button* closeButton;

    TextEditor* te;
    std::array<char, 512> lastReplaceText;

    static void replace_selection(const char* newText, ReplaceDialog* self);
    static void find_next_callback(Fl_Widget* widget, void* param);
    static void replace_and_find_callback(Fl_Widget* widget, void* param);

    static void close_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        ReplaceDialog* rd = static_cast<ReplaceDialog*>(param);
        rd->hide();
    }
public:
    ReplaceDialog(const char* label, TextEditor* _te) 
        : Fl_Double_Window{ 430, 110, label }, te{ _te }
    {
        lastReplaceText.fill('\0');

        findTextInput = new Fl_Input(100, 10, 320, 25, "查找: ");
        replaceTextInput = new Fl_Input(100, 40, 320, 25, "替换: ");

        Fl_Flex* buttonField = new Fl_Flex(100, 70, w() - 100, 40);
        buttonField->type(Fl_Flex::HORIZONTAL);
        buttonField->margin(0, 5, 10, 10);
        buttonField->gap(10);

        findNextButton = new Fl_Button(0, 0, 0, 0, "下一个");
        findNextButton->callback(find_next_callback, this);

        replaceAndFindButton = new Fl_Button(0, 0, 0, 0, "替换");
        replaceAndFindButton->callback(replace_and_find_callback, this);

        closeButton = new Fl_Button(0, 0, 0, 0, "关闭");
        closeButton->callback(close_callback, this);

        buttonField->end();
        set_non_modal();
    }
    
    void show() override {
        findTextInput->value("");
        replaceTextInput->value("");

        Fl_Double_Window::show();
    }
};

class TextEditor {
    friend class ReplaceDialog;

    Fl_Double_Window* window;
    Fl_Menu_Bar* menuBar;
    Fl_Text_Editor* editor;
    Fl_Text_Editor* splitEditor;
    Fl_Text_Buffer* textBuffer;
    Fl_Text_Buffer* styleBuffer;
    ShortcutKeyHelpPage* shortcutKeyHelpPage;
    ReplaceDialog* replaceDialog;
    std::array<char, 512> lastFindText;
    std::string fileName;
    bool textChanged;
    bool initEnableLineNumber;
    bool initEnableWordWrap;

    static void menu_file_quit_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;

        if (self->textChanged) {
            int result = fl_choice("当前文件还有修改没有保存,\n是否保存?", "取消", "保存", "不要保存");

            // cancel.
            if (result == 0) {
                return;
            }

            // save.
            if (result == 1) {
                self->menu_file_save_callback(widget, param);
            }
        }
        
        Fl::hide_all_windows();
    }

    static void menu_file_new_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;

        self->textBuffer->text("");
        self->set_text_changed(false);
    }

    static void menu_file_open_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;

        // check if the current file is saved yet.
        if (self->textChanged) {
            int result = fl_choice("当前文件还有修改没有保存,\n是否保存?", "取消", "保存", "不要保存");

            // cancel.
            if (result == 0) {
                return;
            }

            // save.
            if (result == 1) {
                self->menu_file_save_callback(widget, param);
            }
        }

        // let user choose the file.
        Fl_Native_File_Chooser fileChooser;
        fileChooser.title("打开文件");
        fileChooser.type(Fl_Native_File_Chooser::BROWSE_FILE);

        // load the file content.
        if (fileChooser.show() == 0) {
            self->load_file_content(fileChooser.filename());
        }
    }

    static void menu_file_save_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;

        Fl_Native_File_Chooser fileChooser;
        fileChooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);

        if (self->fileName.empty()) {
            menu_file_save_as_callback(widget, param);
        }
        else {
            self->textBuffer->savefile(self->fileName.c_str());
            self->set_text_changed(false);
        }
    }

    static void menu_file_save_as_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;

        Fl_Native_File_Chooser fileChooser;
        fileChooser.title("另存为");
        fileChooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);

        if (fileChooser.show() == 0) {
            self->textBuffer->savefile(fileChooser.filename());
            self->set_file_name(fileChooser.filename());
            self->set_text_changed(false);
        }
    }

    static void menu_edit_cut(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;
        Fl_Widget* e = Fl::focus();

        if (e && (e == self->editor || e == self->splitEditor)) {
            Fl_Text_Editor::kf_cut(0, (Fl_Text_Editor*)e);
        }
    }

    static void menu_edit_copy(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;
        Fl_Widget* e = Fl::focus();

        if (e && (e == self->editor || e == self->splitEditor)) {
            Fl_Text_Editor::kf_copy(0, (Fl_Text_Editor*)e);
        }
    }

    static void menu_edit_paste(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;
        Fl_Widget* e = Fl::focus();

        if (e && (e == self->editor || e == self->splitEditor)) {
            Fl_Text_Editor::kf_paste(0, (Fl_Text_Editor*)e);
        }
    }

    static void menu_edit_undo(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;
        Fl_Widget* e = Fl::focus();

        if (e && (e == self->editor || e == self->splitEditor)) {
            Fl_Text_Editor::kf_undo(0, (Fl_Text_Editor*)e);
        }
    }

    static void menu_edit_redo(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;
        Fl_Widget* e = Fl::focus();

        if (e && (e == self->editor || e == self->splitEditor)) {
            Fl_Text_Editor::kf_redo(0, (Fl_Text_Editor*)e);
        }
    }

    static void menu_edit_delete(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;
        Fl_Widget* e = Fl::focus();

        if (e && (e == self->editor || e == self->splitEditor)) {
            Fl_Text_Editor::kf_delete(0, (Fl_Text_Editor*)e);
        }
    }

    static void menu_attr_show_line_number_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;

        const Fl_Menu_Item* lineNumberItem = self->menuBar->mvalue();
        if (lineNumberItem->value() > 0) {
            self->editor->linenumber_bgcolor(0xEAEAEAFF);
            self->editor->linenumber_fgcolor(0x4876FFFF);
            self->editor->linenumber_width(40);
            self->editor->linenumber_align(FL_ALIGN_CENTER);
        }
        else {
            self->editor->linenumber_width(0);
        }

        self->editor->redraw();
    }

    static void menu_attr_word_wrap_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;

        const Fl_Menu_Item* wrapModeItem = self->menuBar->mvalue();
        if (wrapModeItem->value() > 0) {
            self->editor->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        }
        else {
            self->editor->wrap_mode(Fl_Text_Display::WRAP_NONE, 0);
        }

        self->editor->redraw();
    }

    static void find_pattern(const char* needle, TextEditor* self, bool findNext) {
        assert(needle != nullptr);
        assert(self != nullptr);

        Fl_Text_Editor* selfEditor = self->editor;

        Fl_Widget* e = Fl::focus();
        if (e && e == self->splitEditor) {
            selfEditor = self->splitEditor;
        }

        int pos = selfEditor->insert_position();
        int found;

        if (findNext) {
            found = self->textBuffer->search_forward(pos, needle, &pos);
        }
        else {
            // be cautions here, we must subtract the strlen(needle) and an extra 1.
            // this is the C-style string manner.
            found = self->textBuffer->search_backward(pos - strlen(needle) - 1, needle, &pos);
        }
        
        if (found) {
            self->textBuffer->select(pos, pos + (int)strlen(needle));
            selfEditor->insert_position(pos + (int)strlen(needle));
            selfEditor->show_insert_position();
        }
        else {
            // search again.
            if (findNext) {
                pos = 0;
                found = self->textBuffer->search_forward(pos, needle, &pos);
            }
            else {
                pos = selfEditor->buffer()->length();
                found = self->textBuffer->search_backward(pos, needle, &pos);
            }

            if (found) {
                self->textBuffer->select(pos, pos + (int)strlen(needle));
                selfEditor->insert_position(pos + (int)strlen(needle));
                selfEditor->show_insert_position();
            }
            else {
                fl_alert("当前文本中未找到 %s", needle);
            }
        }
    }

    static void find_all_pattern(const char* needle, TextEditor* self) {
        assert(needle != nullptr);
        assert(self != nullptr);

        Fl_Text_Editor* selfEditor = self->editor;

        Fl_Widget* e = Fl::focus();
        if (e && e == self->splitEditor) {
            selfEditor = self->splitEditor;
        }

        int pos = 0;
        int found = 0;
        int totalNumber = 0;

        self->styleBuffer->text(std::string(self->textBuffer->length(), 'A').c_str());
        found = self->textBuffer->search_forward(pos, needle, &pos);
        int needleLen = strlen(needle);

        while (found) {
            ++totalNumber;
            
            // highlight.
            for (int i = pos; i < pos + needleLen; ++i) {
                self->styleBuffer->replace(i, i + 1, "B");
            }

            pos += needleLen;
            found = self->textBuffer->search_forward(pos, needle, &pos);
        }

        self->editor->redraw();
        if (totalNumber == 0) {
            fl_alert("当前文本中未找到 %s", needle);
        }
        else {
            fl_message("当前文本中找到 %s 共计有 %d 处", needle, totalNumber);
            self->styleBuffer->text(std::string(self->textBuffer->length(), 'A').c_str());
            self->editor->redraw();
        }
    }

    static void menu_find_find_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;
        
        const char* pattern = fl_input(self->lastFindText.size() - 1, "查找: ", "");
        if (pattern) {
            fl_strlcpy(self->lastFindText.data(), pattern, self->lastFindText.size());
            find_pattern(pattern, self, true);
        }
    }

    static void menu_find_all_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;

        const char* pattern = fl_input(self->lastFindText.size() - 1, "查找所有: ", "");
        if (pattern) {
            fl_strlcpy(self->lastFindText.data(), pattern, self->lastFindText.size());
            find_all_pattern(pattern, self);
        }
    }

    static void menu_find_next_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;

        if (self->lastFindText[0] != '\0') {
            find_pattern(self->lastFindText.data(), self, true);
        }
        else {
            menu_find_find_callback(nullptr, self);
        }
    }

    static void menu_find_prev_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;

        if (self->lastFindText[0] != '\0') {
            find_pattern(self->lastFindText.data(), self, false);
        }
        else {
            menu_find_find_callback(nullptr, self);
        }
    }

    static void menu_find_and_replace_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;

        if (self->replaceDialog == nullptr) {
            self->window->begin();
            self->replaceDialog = new ReplaceDialog("查找并替换", self);
            self->window->end();
        }

        self->replaceDialog->show();
    }

    static void menu_help_shortcut_key_callback(Fl_Widget* widget, void* param) {
        assert(param != nullptr);
        TextEditor* self = (TextEditor*)param;

        if (self->shortcutKeyHelpPage == nullptr) {
            self->window->begin();
            self->shortcutKeyHelpPage = new ShortcutKeyHelpPage("快捷键说明");
            self->window->end();
        }

        self->shortcutKeyHelpPage->show();
    }

    static void text_changed_callback(int, int n_inserted, int n_deleted, int, const char*, void* param) {
        if (n_inserted || n_deleted) {
            if (param != nullptr) {
                TextEditor* self = (TextEditor*)param;
                self->set_text_changed(true);
            }
        }
    }

    void build_menu_bar() {
        window->begin();

        menuBar = new Fl_Menu_Bar(0, 0, window->w(), 25);
        
        menuBar->add("文件/新建",   FL_COMMAND + 'N', menu_file_new_callback, this);
        menuBar->add("文件/打开",   FL_COMMAND + 'o', menu_file_open_callback, this);
        menuBar->add("文件/保存",   FL_COMMAND + 's', menu_file_save_callback, this);
        menuBar->add("文件/另存为", FL_COMMAND + 'S', menu_file_save_as_callback, this, FL_MENU_DIVIDER);
        menuBar->add("文件/退出",   FL_COMMAND + 'q', menu_file_quit_callback, this);

        menuBar->add("编辑/撤销", FL_COMMAND + 'z', menu_edit_undo, this);
        menuBar->add("编辑/回退", FL_COMMAND + 'Z', menu_edit_redo, this, FL_MENU_DIVIDER);
        menuBar->add("编辑/剪切", FL_COMMAND + 'x', menu_edit_cut, this);
        menuBar->add("编辑/复制", FL_COMMAND + 'c', menu_edit_copy, this);
        menuBar->add("编辑/粘贴", FL_COMMAND + 'v', menu_edit_paste, this, FL_MENU_DIVIDER);
        menuBar->add("编辑/删除",                0, menu_edit_delete, this);

        int flag = (initEnableLineNumber ? FL_MENU_TOGGLE | FL_MENU_VALUE : FL_MENU_TOGGLE);
        menuBar->add("属性/显式行号", FL_COMMAND + 'l', menu_attr_show_line_number_callback, this, flag);

        flag = (initEnableWordWrap ? FL_MENU_TOGGLE | FL_MENU_VALUE : FL_MENU_TOGGLE);
        menuBar->add("属性/自动换行", FL_COMMAND + 'w', menu_attr_word_wrap_callback, this, flag);
        
        menuBar->add("查找/查找",       FL_COMMAND + 'f', menu_find_find_callback, this);
        menuBar->add("查找/查找所有",   FL_COMMAND + 'g', menu_find_all_callback, this);
        menuBar->add("查找/下一个",     FL_COMMAND + 'n', menu_find_next_callback, this);
        menuBar->add("查找/上一个",     FL_COMMAND + 'p', menu_find_prev_callback, this, FL_MENU_DIVIDER);
        menuBar->add("查找/查找并替换", FL_COMMAND + 'r', menu_find_and_replace_callback, this);

        menuBar->add("帮助/快捷键", 0, menu_help_shortcut_key_callback, this);

        window->callback(menu_file_quit_callback, this);
        window->end();
    }

    void update_title() {
        if (fileName.empty()) {
            window->label(textChanged ? "临时文件 *" : "临时文件");
            return;
        }

        if (textChanged) {
            std::string temp = fileName + " *";
            window->label(temp.c_str());
        }
        else {
            window->label(fileName.c_str());
        }
    }

    void set_text_changed(bool changed) {
        textChanged = changed;
        update_title();
    }

    void set_file_name(const std::string& name) {
        fileName = name;
        update_title();
    }

    void load_file_content(const std::string& path) {
        if (textBuffer->loadfile(path.c_str()) == 0) {
            set_file_name(path);
            set_text_changed(false);
        }
        else {
            int err = errno;
            fl_alert("无法打开文件:\n%s\n%s", path.c_str(), strerror(errno));
        }
    }

    void build_main_editor() {
        window->begin();

        textBuffer = new Fl_Text_Buffer();
        textBuffer->add_modify_callback(text_changed_callback, this);

        editor = new Fl_Text_Editor(0, menuBar->h(), window->w(), window->h() - menuBar->h());
        editor->buffer(textBuffer);
        editor->textfont(FL_COURIER);

        styleBuffer = new Fl_Text_Buffer();
        editor->highlight_data(styleBuffer, styletable, sizeof(styletable) / sizeof(styletable[0]), 'A', 0, 0);

        window->resizable(editor);
        window->end();
    }

    void set_default_components() {
        if (initEnableLineNumber) {
            editor->linenumber_bgcolor(0xEAEAEAFF);
            editor->linenumber_fgcolor(0x4876FFFF);
            editor->linenumber_width(40);
            editor->linenumber_align(FL_ALIGN_CENTER);
        }
        
        if (initEnableWordWrap) {
            editor->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        }
    }
public:
    TextEditor() 
        : window{ new Fl_Double_Window(960, 480, "文本编辑器") },
            menuBar{ nullptr },
            editor{ nullptr },
            splitEditor{ nullptr },
            textBuffer{ nullptr },
            styleBuffer{ nullptr },
            shortcutKeyHelpPage{ nullptr },
            replaceDialog{ nullptr },
            fileName{ "" },
            textChanged{ false },
            initEnableLineNumber{ true },
            initEnableWordWrap{ true }
    {
        lastFindText.fill('\0');

        // be careful here, we must build menu bar first, then build the editor, 
        // then setting the on/off state of the components.
        build_menu_bar();
        build_main_editor();
        set_default_components();
    }

    void show(int argc, char* argv[]) {
        window->show(argc, argv);
    }
};

void ReplaceDialog::replace_selection(const char* newText, ReplaceDialog* self) {
    assert(newText != nullptr);
    assert(self != nullptr);

    Fl_Text_Editor* editor = self->te->editor;
    Fl_Widget* e = Fl::focus();

    if (e && e == self->te->splitEditor) {
        editor = self->te->splitEditor;
    }

    int start;
    int end;

    if (self->te->textBuffer->selection_position(&start, &end)) {
        self->te->textBuffer->remove_selection();
        self->te->textBuffer->insert(start, newText);
        self->te->textBuffer->select(start, start + (int)strlen(newText));

        editor->insert_position(start + (int)strlen(newText));
        editor->show_insert_position();
    }
}

void ReplaceDialog::find_next_callback(Fl_Widget* widget, void* param) {
    assert(param != nullptr);
    ReplaceDialog* rd = static_cast<ReplaceDialog*>(param);

    fl_strlcpy(rd->te->lastFindText.data(), rd->findTextInput->value(), rd->te->lastFindText.size());
    fl_strlcpy(rd->lastReplaceText.data(), rd->replaceTextInput->value(), rd->lastReplaceText.size());
    
    if (rd->te->lastFindText[0] != '\0') {
        rd->te->find_pattern(rd->te->lastFindText.data(), rd->te, true);
    }
}

void ReplaceDialog::replace_and_find_callback(Fl_Widget* widget, void* param) {
    assert(param != nullptr);
    ReplaceDialog* rd = static_cast<ReplaceDialog*>(param);

    // replace then select the next pattern.
    replace_selection(rd->replaceTextInput->value(), rd);
    rd->te->find_pattern(rd->te->lastFindText.data(), rd->te, true);
}

int main(int argc, char* argv[]) {
    TextEditor editor;
    editor.show(argc, argv);
    return Fl::run();
}
