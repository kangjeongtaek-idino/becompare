; app.inc - application-wide definitions (sessions, messages, ids)

; ---- session object (one per tab)
%define SS_TYPE   0
%define SS_HVIEW  8
%define SS_DATA   16
%define SS_AUTO   24
%define SS_PATHA  32
%define SS_PATHB  1072
%define SS_PATHC  2112
%define SS_PATHO  3152
%define SS_OPTS   4192
%define SS_TITLE  5216
%define SS_EMBED  5376              ; a view inside another view (the preview of the folder compare): no toolbar, not a tab
%define SS_SIZE   5384

%define T_HOME    0
%define T_FOLDER  1
%define T_TEXT    2
%define T_TABLE   3
%define T_HEX     4
%define T_PIC     5
%define T_MERGE   6
%define T_VER     7
%define T_REG     8
%define T_MEDIA   9
%define T_EDIT    10
%define T_FMERGE  11
%define MAX_SESS  64

; ---- private messages
%define BC_COLLECT (WM_APP+10)
%define BC_RUN     (WM_APP+11)
%define BC_CANCEL  (WM_APP+12)
%define BC_SAVE    (WM_APP+13)
%define BC_ISDIRTY (WM_APP+14)
%define BC_STOPPING (WM_APP+15) ; request cancellation; return nonzero until workers exit
%define BC_THEME   (WM_APP+20)
%define BC_NEXTDIFF (WM_APP+30)
%define BC_PREVDIFF (WM_APP+31)
%define BC_BOOKMARK (WM_APP+32)
%define BC_NEXTMARK (WM_APP+33)
%define BC_PREVMARK (WM_APP+34)
%define BC_GOTOLINE (WM_APP+35)      ; Text Compare view: ask for a line number and go there (Ctrl+G)
%define BC_TVQ      (WM_APP+60)       ; Text Compare view: wParam 0 number of differences, 1 pane with the caret, 2 current difference
%define BC_PROGRESS (WM_APP+1)
%define BC_DONE     (WM_APP+2)
%define BC_SYNCPROG (WM_APP+3)
%define BC_SYNCDONE (WM_APP+4)

; ---- frame menu command ids
%define IDM_NEW_FOLDER 1001
%define IDM_NEW_TEXT   1002
%define IDM_NEW_TABLE  1003
%define IDM_NEW_HEX    1004
%define IDM_NEW_PIC    1005
%define IDM_NEW_MERGE  1006
%define IDM_NEW_VER    1007
%define IDM_NEW_REG    1008
%define IDM_NEW_MEDIA  1009
%define IDM_NEW_EDIT   1010
%define IDM_NEW_FMERGE 1011
%define IDM_OPEN_SESS  1210
%define IDM_SAVE_SESS  1211
%define IDM_OPEN_WS    1212
%define IDM_SAVE_WS    1213
%define IDM_CLOSE_TAB  1020
%define IDM_EXIT       1021
%define IDM_LIGHT      1030
%define IDM_DARK       1031
%define IDM_RUN        1040
%define IDM_CANCEL     1041
%define IDM_SAVE       1042
%define IDM_ABOUT      1050
%define IDM_SETTINGS   1060
%define IDM_UPDATE     1070
%define IDM_CLIP       1071
%define IDM_PATCH      1072
%define IDM_SHELL_ON   1061
%define IDM_SHELL_OFF  1062

; ---- list box
%define LB_ADDSTRING   0x0180
%define LB_RESETCONTENT 0x0184
%define LB_GETCURSEL   0x0188
%define LB_GETTEXT     0x0189
%define LB_GETCOUNT    0x018B
%define LBN_DBLCLK     2
%define LBS_NOTIFY     1
%define WM_CTLCOLORLISTBOX 0x0134

; ---- layout helper: move a child control by id (sources must be callee-saved regs / immediates)
%macro MVID 6
    invoke GetDlgItem, %1, %2
    invoke MoveWindow, rax, %3, %4, %5, %6, 1
%endmacro
