#include <cstdlib>

int main() {
    system("g++ text_editor.cpp -I D:\\third-party\\fltk-1.4.4-source\\fltk-1.4.4 " 
                "-I D:\\third-party\\fltk-1.4.4-source\\fltk-1.4.4\\build " 
                "-L D:\\third-party\\fltk-1.4.4-source\\fltk-1.4.4\\build\\lib " 
                "-l fltk " 
                "-l gdi32 " 
                "-l comctl32 " 
                "-l gdiplus " 
                "-l ws2_32 " 
                "-l comdlg32 " 
                "-l ole32 " 
                "-l uuid " 
                "-l winspool "
                "-mwindows "
                "-std=c++20 -O2 -s -o text_editor"
            );
    return 0;
}