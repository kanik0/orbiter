// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// Stub implementation of DlgCtrl for non-Windows platforms.
// The Win32 custom controls (gauge, switch, property list) are not
// available outside Windows. Functions compile to no-ops so that
// code linking against DlgCtrl still links cleanly.

#include "DlgCtrl.h"

void oapiRegisterCustomControls(HINSTANCE) {}
void oapiUnregisterCustomControls(HINSTANCE) {}

void oapiSetGaugeParams(HWND, GAUGEPARAM*, bool) {}
void oapiSetGaugeRange(HWND, int, int, bool) {}
int  oapiSetGaugePos(HWND, int, bool) { return 0; }
int  oapiIncGaugePos(HWND, int, bool) { return 0; }
int  oapiGetGaugePos(HWND) { return 0; }

// PropertyItem
PropertyItem::PropertyItem(PropertyGroup* grp) {
	label = nullptr; value = nullptr;
	labelw = -1; valuew = -1;
	label_dirty = false; value_dirty = false;
	group = grp;
}
PropertyItem::~PropertyItem() { delete[] label; delete[] value; }
void PropertyItem::SetLabel(const char*) {}
void PropertyItem::SetValue(const char*) {}

// PropertyGroup
PropertyGroup::PropertyGroup(PropertyList* list, bool expand) {
	plist = list; item = nullptr; nitem = 0;
	title = nullptr; expanded = expand;
}
PropertyGroup::~PropertyGroup() {
	for (int i = 0; i < nitem; i++) delete item[i];
	delete[] item;
	delete[] title;
}
PropertyItem* PropertyGroup::AppendItem() { return nullptr; }
PropertyItem* PropertyGroup::GetItem(int) { return nullptr; }
void PropertyGroup::SetTitle(const char*) {}
void PropertyGroup::Expand(bool expand) { expanded = expand; }

// PropertyList
int PropertyList::titleh = 20;
int PropertyList::itemh = 18;
int PropertyList::gaph = 6;
HBITMAP PropertyList::hBmpArrows = nullptr;

PropertyList::PropertyList() : npg(0), listh(0), yofs(0), valx0(0), hFontTitle(nullptr), hFontItem(nullptr), hPenLine(nullptr), hBrushTitle(nullptr) {}
PropertyList::~PropertyList() {
	for (int i = 0; i < npg; i++) delete pg[i];
	delete[] pg;
}
void PropertyList::OnInitDialog(HWND, int) {}
void PropertyList::OnPaint(HWND) {}
void PropertyList::OnSize(int, int) {}
void PropertyList::OnVScroll(unsigned int, int) {}
void PropertyList::OnLButtonDown(int, int) {}
PropertyGroup* PropertyList::AppendGroup(bool) { return nullptr; }
bool PropertyList::DeleteGroup(PropertyGroup*) { return false; }
PropertyGroup* PropertyList::GetGroup(int) { return nullptr; }
bool PropertyList::ExpandGroup(PropertyGroup*, bool) { return false; }
void PropertyList::ExpandAll(bool) {}
void PropertyList::ClearGroups() {}
PropertyItem* PropertyList::AppendItem(PropertyGroup*) { return nullptr; }
void PropertyList::SetListHeight(int, bool) {}
void PropertyList::VScrollTo(int) {}
void PropertyList::Move(int, int, int, int) {}
void PropertyList::Redraw() {}
void PropertyList::Update() {}
void PropertyList::SetColWidth(int, int) {}

// RegisterPropertyList / UnregisterPropertyList stubs
void RegisterPropertyList(HINSTANCE) {}
void UnregisterPropertyList(HINSTANCE) {}
