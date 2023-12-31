/******************************************************************************
 * PartDialog.c
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <Windows.h>
#include <commctrl.h>
#include "resource.h"
#include "Language.h"
#include "Ventoy2Disk.h"

static int m_MinWidth;
static int m_MinHeight;
static RECT m_WindowRect;
static HWND g_PartDlgHwnd = NULL;

static BOOL g_enable_reserve_space = FALSE;
static BOOL g_enable_reserve_space_tmp = FALSE;
static BOOL g_align_part_4KB = TRUE;
static BOOL g_align_part_4KB_tmp = TRUE;
static int g_unit_sel = 1;
static int g_unit_sel_tmp = 1;
static int g_cluster_sel = 0;
static int g_cluster_sel_tmp = 0;
static int g_reserve_space = -1;
static int g_fs_type = 0;
static int g_fs_type_tmp = 0;
static int g_fs_radio_id[VTOY_FS_BUTT] =
{
    IDC_RADIO1, IDC_RADIO2, IDC_RADIO3, IDC_RADIO4
};

static char* g_fs_name[VTOY_FS_BUTT] =
{
    "exFAT", "NTFS", "FAT32", "UDF"
};

static CHAR* g_fs_fmt_nameA[VTOY_FS_BUTT] =
{
    "EXFAT", "NTFS", "FAT32", "UDF"
};

static WCHAR* g_fs_fmt_nameW[VTOY_FS_BUTT] =
{
    L"EXFAT", L"NTFS", L"FAT32", L"UDF"
};

static WCHAR g_cluster_size_tip[256];

static int g_cluster_size[] =
{
    0,
    1024 * 2, 1024 * 4, 1024 * 8, 1024 * 16, 1024 * 32, 1024 * 64, 1024 * 128, 1024 * 256, 1024 * 512,    
    SIZE_1MB, SIZE_1MB * 2, SIZE_1MB * 4, SIZE_1MB * 8, SIZE_1MB * 16, SIZE_1MB * 32,
};

int GetClusterSize(void)
{
    return g_cluster_size[g_cluster_sel];
}

void SetClusterSize(int ClusterSize)
{
    int i;

    for (i = 0; i < sizeof(g_cluster_size) / sizeof(g_cluster_size[0]); i++)
    {
        if (g_cluster_size[i] == ClusterSize)
        {
            g_cluster_sel = g_cluster_sel_tmp = i;
        }
    }
}

void FormatClusterSizeTip(int Size, WCHAR* pBuf, size_t len)
{
    WCHAR item[64];

    if (Size == 0)
    {
        swprintf_s(pBuf, len, L"%ls : %ls", _G(STR_PART_CLUSTER), _G(STR_PART_CLUSTER_DEFAULT));
    }
    else
    {
        if (Size == 512)
        {
            swprintf_s(item, 64, L" %d", Size);
        }
        else if (Size < SIZE_1MB)
        {
            swprintf_s(item, 64, L" %d KB", Size / 1024);
        }
        else
        {
            swprintf_s(item, 64, L" %d MB", Size / SIZE_1MB);
        }
        swprintf_s(pBuf, len, L"%ls : %ls", _G(STR_PART_CLUSTER), item);
    }
}

WCHAR* GetClusterSizeTip(void)
{
    int Size = GetClusterSize();

    FormatClusterSizeTip(Size, g_cluster_size_tip, 256);
    return g_cluster_size_tip;
}


int IsPartNeed4KBAlign(void)
{
    return g_align_part_4KB;
}

void SetVentoyFsType(int fs)
{
    g_fs_type = g_fs_type_tmp = fs;
}


int GetVentoyFsType(void)
{
    return g_fs_type;
}

const char* GetVentoyFsNameByType(int fs)
{
    return g_fs_name[fs];
}

CHAR* GetVentoyFsFmtNameByTypeA(int fs)
{
    return g_fs_fmt_nameA[fs];
}

WCHAR* GetVentoyFsFmtNameByTypeW(int fs)
{
    return g_fs_fmt_nameW[fs];
}

const char* GetVentoyFsName(void)
{
    return g_fs_name[g_fs_type];
}

void CLISetReserveSpace(int MB)
{
    g_enable_reserve_space = TRUE;
    g_unit_sel = 0;
    g_reserve_space = MB;
}

int GetReservedSpaceInMB(void)
{
    if (g_enable_reserve_space)
    {
        if (g_unit_sel == 0)
        {
            return g_reserve_space;
        }
        else
        {
            return g_reserve_space * 1024;
        }
    }
    else
    {
        return 0;
    }
}


static VOID UpdateControlStatus(HWND hWnd)
{
	HWND hComobox;
	HWND hCheckbox;
    HWND hCheckbox4KB;
	HWND hEdit;

	hCheckbox = GetDlgItem(hWnd, IDC_CHECK_RESERVE_SPACE);
    hCheckbox4KB = GetDlgItem(hWnd, IDC_CHECK_PART_ALIGN_4KB);
	hComobox = GetDlgItem(hWnd, IDC_COMBO_SPACE_UNIT);
	hEdit = GetDlgItem(hWnd, IDC_EDIT_RESERVE_SPACE_VAL);

	SendMessage(hComobox, CB_SETCURSEL, g_unit_sel, 1);
	SendMessage(hCheckbox, BM_SETCHECK, g_enable_reserve_space_tmp ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(hCheckbox4KB, BM_SETCHECK, g_align_part_4KB_tmp ? BST_CHECKED : BST_UNCHECKED, 0);
	
    EnableWindow(hEdit, g_enable_reserve_space_tmp);
	EnableWindow(hComobox, g_enable_reserve_space_tmp);
}

static int FillClusterCombox(HWND hWnd, BOOL Ext, int def)
{
    int i;
    int Count;
    HWND hComobox;
    char item[64];

    hComobox = GetDlgItem(hWnd, IDC_COMBO_CLUSTER_SIZE);

    //Delete all strings
    Count = (int)SendMessage(hComobox, CB_GETCOUNT, 0, 0);    
    for (i = Count - 1; i >= 0; i--)
    {
        SendMessage(hComobox, CB_DELETESTRING, (WPARAM)i, 0);
    }

    Count = 0;
    for (i = 0; i < sizeof(g_cluster_size) / sizeof(g_cluster_size[0]); i++)
    {
        if (i == 0)
        {
            SendMessage(hComobox, CB_ADDSTRING, 0, (LPARAM)_G(STR_PART_CLUSTER_DEFAULT));
            Count++;
            continue;
        }

        if (g_cluster_size[i] < SIZE_1MB)
        {
            sprintf_s(item, sizeof(item), "%d KB", g_cluster_size[i] / 1024);
        }
        else
        {
            sprintf_s(item, sizeof(item), "%d MB", g_cluster_size[i] / SIZE_1MB);
        }
        
        SendMessageA(hComobox, CB_ADDSTRING, 0, (LPARAM)item);
        Count++;

        if (FALSE == Ext && g_cluster_size[i] == 1024 * 64)
        {
            break;
        }
    }

    if (def >= Count || def < 0)
    {
        Log("Change invalid default index %d to 0", def);
        def = 0;
    }
    SendMessage(hComobox, CB_SETCURSEL, (WPARAM)def, 0);

    return 0;
}

static BOOL PartInitDialog(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    HWND hComobox;
    HWND hCheckbox;
    HWND hCheckbox4KB;
    HWND hEdit;
    WCHAR buf[64];

    g_PartDlgHwnd = hWnd;

    hCheckbox = GetDlgItem(hWnd, IDC_CHECK_RESERVE_SPACE);
    hCheckbox4KB = GetDlgItem(hWnd, IDC_CHECK_PART_ALIGN_4KB);
    hComobox = GetDlgItem(hWnd, IDC_COMBO_SPACE_UNIT);
    hEdit = GetDlgItem(hWnd, IDC_EDIT_RESERVE_SPACE_VAL);

    SetWindowText(hCheckbox, _G(STR_PRESERVE_SPACE));
    SetWindowText(hCheckbox4KB, _G(STR_PART_ALIGN_4KB));
    SetWindowText(GetDlgItem(hWnd, ID_PART_OK), _G(STR_BTN_OK));
    SetWindowText(GetDlgItem(hWnd, ID_PART_CANCEL), _G(STR_BTN_CANCEL));
    SetWindowText(GetDlgItem(hWnd, IDC_VENTOY_PART_FS), _G(STR_PART_VENTOY_FS));
    SetWindowText(GetDlgItem(hWnd, IDC_STATIC_PART_FS_TITLE), _G(STR_PART_FS));
    SetWindowText(GetDlgItem(hWnd, IDC_STATIC_PART_CLUSTER), _G(STR_PART_CLUSTER));

    SetWindowText(hWnd, _G(STR_MENU_PART_CFG));

	SendMessage(hEdit, EM_LIMITTEXT, 9, 0);

    SendMessage(hComobox, CB_ADDSTRING, 0, (LPARAM)TEXT(" MB"));
    SendMessage(hComobox, CB_ADDSTRING, 0, (LPARAM)TEXT(" GB"));
    
    SendMessage(hCheckbox, WM_SETFONT, (WPARAM)g_language_normal_font, TRUE);
    SendMessage(hCheckbox4KB, WM_SETFONT, (WPARAM)g_language_normal_font, TRUE);
    SendMessage(GetDlgItem(hWnd, ID_PART_OK), WM_SETFONT, (WPARAM)g_language_normal_font, TRUE);
    SendMessage(GetDlgItem(hWnd, ID_PART_CANCEL), WM_SETFONT, (WPARAM)g_language_normal_font, TRUE);

	if (g_reserve_space >= 0)
    {
		swprintf_s(buf, 64, L"%lld", (long long)g_reserve_space);
    }
    else
    {
        buf[0] = 0;
    }

    SetWindowText(hEdit, buf);

	g_enable_reserve_space_tmp = g_enable_reserve_space;
    g_align_part_4KB_tmp = g_align_part_4KB;
    g_fs_type_tmp = g_fs_type;
	g_unit_sel_tmp = g_unit_sel;
    g_cluster_sel_tmp = g_cluster_sel;

    CheckRadioButton(hWnd, IDC_RADIO1, IDC_RADIO4, g_fs_radio_id[g_fs_type]);

	UpdateControlStatus(hWnd);

    FillClusterCombox(hWnd, g_fs_type == 0, g_cluster_sel);

    EnableWindow(GetDlgItem(hWnd, IDC_COMBO_CLUSTER_SIZE), g_fs_type_tmp != 3);

    GetWindowRect(hWnd, &m_WindowRect);
    m_MinWidth = m_WindowRect.right - m_WindowRect.left;
    m_MinHeight = m_WindowRect.bottom - m_WindowRect.top;

    return TRUE;
}

static int GetCheckedRadioButton(HWND hWnd)
{
    int i;
    int code;

    for (i = 0; i < sizeof(g_fs_radio_id) / sizeof(g_fs_radio_id[0]); i++)
    {
        code = (INT)SendMessage(GetDlgItem(hWnd, g_fs_radio_id[i]), BM_GETSTATE, 0, 0);
        if (BST_CHECKED & code)
        {
            return i;
        }
    }

    return -1;
}

static VOID OnPartBtnOkClick(HWND hWnd)
{
    int i;
	HWND hEdit;
	ULONG SpaceVal = 0;
	CHAR Value[64] = { 0 };

	hEdit = GetDlgItem(hWnd, IDC_EDIT_RESERVE_SPACE_VAL);

	GetWindowTextA(hEdit, Value, sizeof(Value) - 1);
	
	SpaceVal = strtoul(Value, NULL, 10);

    if (g_enable_reserve_space_tmp)
    {
        if (g_unit_sel_tmp == 0)
        {
            if (SpaceVal == 0 || SpaceVal >= 1024 * 1024 * 2000)
            {
                MessageBox(hWnd, _G(STR_SPACE_VAL_INVALID), _G(STR_ERROR), MB_OK | MB_ICONERROR);
                return;
            }
        }
        else
        {
            if (SpaceVal == 0 || SpaceVal >= 1024 * 2000)
            {
                MessageBox(hWnd, _G(STR_SPACE_VAL_INVALID), _G(STR_ERROR), MB_OK | MB_ICONERROR);
                return;
            }
        }
    }

    i = GetCheckedRadioButton(hWnd);
    if (i >= 0)
    {
        g_fs_type_tmp = i;
    }

	g_reserve_space = (int)SpaceVal;
	g_enable_reserve_space = g_enable_reserve_space_tmp;
    g_align_part_4KB = g_align_part_4KB_tmp;
    g_fs_type = g_fs_type_tmp;
	g_unit_sel = g_unit_sel_tmp;
    g_cluster_sel = g_cluster_sel_tmp;

	SendMessage(hWnd, WM_CLOSE, 0, 0);
}

static VOID OnPartBtnCancelClick(HWND hWnd)
{
	SendMessage(hWnd, WM_CLOSE, 0, 0);
}

static VOID OnClusterSizeSelChange(HWND hwnd)
{
    int index;

    index = (INT)SendMessage(GetDlgItem(hwnd, IDC_COMBO_CLUSTER_SIZE), CB_GETCURSEL, 0, 0);
    if (index != CB_ERR)
    {
        g_cluster_sel_tmp = index;
    }
}

INT_PTR CALLBACK PartDialogProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	INT index;
    WORD NotifyCode;
    WORD CtrlID;

    switch (Message)
    {
        case WM_SIZE:
        {
            //判断窗口是不是最小化了，因为窗口最小化之后 ，窗口的长和宽会变成0，当前一次变化的时就会出现除以0的错误操作
            if (wParam != SIZE_MINIMIZED && m_MinWidth > 0)
            {
                int WidthDelta;
                int halfDelta;
                RECT CurRet;

                GetWindowRect(g_PartDlgHwnd, &CurRet);
                WidthDelta = (CurRet.right - CurRet.left) - (m_WindowRect.right - m_WindowRect.left);
                halfDelta = WidthDelta / 2;

                ExpandDlg(g_PartDlgHwnd, ID_PART_OK, halfDelta);
                ExpandDlg(g_PartDlgHwnd, ID_PART_CANCEL, halfDelta);
                MoveDlg(g_PartDlgHwnd, ID_PART_CANCEL, halfDelta);

                ExpandDlg(g_PartDlgHwnd, IDC_STATIC_XX, WidthDelta);                
                ExpandDlg(g_PartDlgHwnd, IDC_EDIT_RESERVE_SPACE_VAL, WidthDelta / 2);
                ExpandDlg(g_PartDlgHwnd, IDC_STATIC_X4, (WidthDelta % 2) ? halfDelta + 1 : halfDelta);
                MoveDlg(g_PartDlgHwnd, IDC_STATIC_X4, halfDelta);
                MoveDlg(g_PartDlgHwnd, IDC_COMBO_SPACE_UNIT, WidthDelta / 2);

                

                ExpandDlg(g_PartDlgHwnd, IDC_VENTOY_PART_FS, WidthDelta);
                ExpandDlg(g_PartDlgHwnd, IDC_STATIC_PRESERVER_SPACE, WidthDelta);
                ExpandDlg(g_PartDlgHwnd, IDC_STATIC_ALIGN_4KB, WidthDelta);
                ExpandDlg(g_PartDlgHwnd, IDC_CHECK_RESERVE_SPACE, WidthDelta);
                ExpandDlg(g_PartDlgHwnd, IDC_CHECK_PART_ALIGN_4KB, WidthDelta);
                ExpandDlg(g_PartDlgHwnd, IDC_COMBO_CLUSTER_SIZE, WidthDelta);
                

                GetWindowRect(g_PartDlgHwnd, &m_WindowRect);   //最后要更新对话框的大小，当做下一次变化的旧坐标；
                InvalidateRect(g_PartDlgHwnd, &m_WindowRect, TRUE);
            }
            break;
        }
        case WM_COMMAND:
        {
            NotifyCode = HIWORD(wParam);
            CtrlID = LOWORD(wParam);
            
            if (CtrlID == ID_PART_OK && NotifyCode == BN_CLICKED)
            {
				OnPartBtnOkClick(hWnd);
            }
            else if (CtrlID == ID_PART_CANCEL && NotifyCode == BN_CLICKED)
            {
				OnPartBtnCancelClick(hWnd);
            }
			else if (CtrlID == IDC_CHECK_RESERVE_SPACE && NotifyCode == BN_CLICKED)
			{
				g_enable_reserve_space_tmp = !g_enable_reserve_space_tmp;
				UpdateControlStatus(hWnd);
			}
            else if (CtrlID == IDC_CHECK_PART_ALIGN_4KB && NotifyCode == BN_CLICKED)
            {
                g_align_part_4KB_tmp = !g_align_part_4KB_tmp;
                UpdateControlStatus(hWnd);
            }
			else if (CtrlID == IDC_COMBO_SPACE_UNIT && NotifyCode == CBN_SELCHANGE)
			{
				index = (INT)SendMessage(GetDlgItem(hWnd, IDC_COMBO_SPACE_UNIT), CB_GETCURSEL, 0, 0);
				if (index != CB_ERR)
				{
					g_unit_sel_tmp = index;
				}
			}
            else if (CtrlID == IDC_COMBO_CLUSTER_SIZE && NotifyCode == CBN_SELCHANGE)
            {
                OnClusterSizeSelChange(hWnd);
            }
            else if (NotifyCode == BN_CLICKED && (CtrlID >= IDC_RADIO1 && CtrlID <= IDC_RADIO4))
            {
                index = GetCheckedRadioButton(hWnd);
                if (g_fs_type_tmp != index)
                {
                    if (index == 0) // exfat
                    {
                        FillClusterCombox(hWnd, TRUE, 0);
                        OnClusterSizeSelChange(hWnd);
                    }
                    else if (g_fs_type_tmp == 0)
                    {
                        FillClusterCombox(hWnd, FALSE, 0);
                        OnClusterSizeSelChange(hWnd);
                    }

                    g_fs_type_tmp = index;
                }

                if (g_fs_type_tmp == 3)
                {
                    SendMessage(GetDlgItem(hWnd, IDC_COMBO_CLUSTER_SIZE), CB_SETCURSEL, (WPARAM)0, 0);
                    OnClusterSizeSelChange(hWnd);
                }
                EnableWindow(GetDlgItem(hWnd, IDC_COMBO_CLUSTER_SIZE), g_fs_type_tmp != 3);
            }
            else
            {
                return TRUE;
            }

            break;
        }
        case WM_INITDIALOG:
        {
            PartInitDialog(hWnd, wParam, lParam);
            return FALSE;
        }        
        case WM_CLOSE:
        {          
            m_MinWidth = 0;
            m_MinHeight = 0;
            g_PartDlgHwnd = NULL;
            EndDialog(hWnd, 0);
            return TRUE;
        }
    }

    return 0;
}
