// This file is part of Notepad++ project
// Copyright (C)2025 Don HO <don.h@free.fr>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include "dpiManagerV2.h"

class ReBar;

#define REBAR_BAR_BOOKMARKS 2

// Sent to parent when a bookmark is clicked. WPARAM = index, LPARAM = 0
// WPARAM = -1, LPARAM = 1 means save requested (bookmark removed)
// WPARAM = -2, LPARAM = 0 means "bookmark all tabs" requested
#define BMN_BOOKMARKCLICKED   (WM_USER + 8800)
#define BMN_BOOKMARK_SAVE     (WM_USER + 8801)
#define BMN_BOOKMARK_ALL_TABS (WM_USER + 8802)

struct BookmarkItem
{
	std::wstring filePath;
	std::wstring displayName;
	bool isFolder = false;
	std::vector<BookmarkItem> children;
};

class BookmarkBar
{
public:
	BookmarkBar() = default;
	~BookmarkBar();

	void init(HINSTANCE hInst, HWND hParent);
	void addToRebar(ReBar* rebar);
	void reSizeTo(const RECT& rc);

	HWND getHSelf() const { return _hSelf; }
	int getHeight() const;

	// Single file bookmarks
	void addBookmark(const std::wstring& filePath);
	void removeBookmark(int index);
	bool hasBookmark(const std::wstring& filePath) const;
	int getBookmarkCount() const { return static_cast<int>(_bookmarks.size()); }

	// Folder support
	void addFolder(const std::wstring& name, const std::vector<std::wstring>& filePaths);

	// Access for opening
	bool isFolder(int index) const;
	const std::wstring& getBookmarkPath(int index) const;
	const std::vector<BookmarkItem>& getFolderChildren(int index) const;

	// Persistence
	void loadFromXml(const wchar_t* configPath);
	void saveToXml(const wchar_t* configPath) const;

	void setVisible(bool visible);
	bool isVisible() const { return _isVisible; }

private:
	static LRESULT CALLBACK staticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT runProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void paint(HDC hDC);
	int hitTest(int x, int y) const;
	bool hitTestClose(int x, int y, int itemIndex) const;
	RECT getCloseButtonRect(int itemIndex) const;
	void recalcLayout();
	void showItemContextMenu(int index, POINT pt);
	void showBarContextMenu(POINT pt);
	void showFolderDropdown(int index);
	void removeChildFromFolder(int folderIndex, int childIndex);

	HWND _hSelf = nullptr;
	HWND _hParent = nullptr;
	HINSTANCE _hInst = nullptr;
	HFONT _hFont = nullptr;

	std::vector<BookmarkItem> _bookmarks;
	std::vector<RECT> _itemRects;

	int _hoverIndex = -1;
	int _pressedIndex = -1;
	bool _closeHover = false;  // true when mouse is over the X button
	bool _isVisible = true;

	// Drag-and-drop reorder
	bool _isDragging = false;
	int _dragSourceIndex = -1;
	int _dragTargetIndex = -1;
	POINT _dragStartPt = {};
	static constexpr int DRAG_THRESHOLD = 5;

	// Shimmer animation
	float _shimmerPhase = 0.0f;
	UINT_PTR _shimmerTimerId = 0;
	static constexpr int SHIMMER_TIMER_ID = 7777;
	static constexpr int SHIMMER_INTERVAL_MS = 50;

	DPIManagerV2 _dpiManager;

	static constexpr int BASE_HEIGHT = 26;
	static constexpr int ITEM_HPADDING = 10;
	static constexpr int ITEM_VPADDING = 3;
	static constexpr int ITEM_GAP = 4;
	static constexpr int BAR_LPADDING = 8;
	static constexpr int CORNER_RADIUS = 4;
};
