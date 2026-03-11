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

#include "BookmarkBar.h"
#include "NppDarkMode.h"
#include "NppXml.h"
#include "ToolBar.h"
#include "menuCmdID.h"
#include <windowsx.h>
#include <shlwapi.h>
#include <cmath>

#ifndef WM_DOOPEN
#define SCINTILLA_USER (WM_USER + 2000)
#define WM_DOOPEN      (SCINTILLA_USER + 8)
#endif

static const wchar_t* BOOKMARK_BAR_CLASS = L"NppBookmarkBar";
static bool s_classRegistered = false;

BookmarkBar::~BookmarkBar()
{
	if (_shimmerTimerId && _hSelf)
		::KillTimer(_hSelf, SHIMMER_TIMER_ID);
	if (_hFont)
		::DeleteObject(_hFont);
	if (_hSelf)
		::DestroyWindow(_hSelf);
}

void BookmarkBar::init(HINSTANCE hInst, HWND hParent)
{
	_hInst = hInst;
	_hParent = hParent;
	_dpiManager.setDpi(hParent);

	if (!s_classRegistered)
	{
		WNDCLASSEX wc{};
		wc.cbSize = sizeof(wc);
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		wc.lpfnWndProc = staticWndProc;
		wc.hInstance = hInst;
		wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = nullptr;
		wc.lpszClassName = BOOKMARK_BAR_CLASS;
		::RegisterClassEx(&wc);
		s_classRegistered = true;
	}

	_hSelf = ::CreateWindowEx(
		0, BOOKMARK_BAR_CLASS, L"",
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		0, 0, 0, _dpiManager.scale(BASE_HEIGHT),
		hParent, nullptr, hInst, this);

	NONCLIENTMETRICS ncm{};
	ncm.cbSize = sizeof(ncm);
	::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
	ncm.lfMenuFont.lfHeight = -_dpiManager.scale(12);
	ncm.lfMenuFont.lfWeight = FW_NORMAL;
	_hFont = ::CreateFontIndirect(&ncm.lfMenuFont);

	// Start shimmer animation timer
	_shimmerTimerId = ::SetTimer(_hSelf, SHIMMER_TIMER_ID, SHIMMER_INTERVAL_MS, nullptr);
}

void BookmarkBar::addToRebar(ReBar* rebar)
{
	REBARBANDINFO rbBand{};
	rbBand.cbSize = sizeof(rbBand);
	rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_ID;
	rbBand.fStyle = RBBS_NOGRIPPER | RBBS_VARIABLEHEIGHT;
	rbBand.hwndChild = _hSelf;
	rbBand.wID = REBAR_BAR_BOOKMARKS;

	int h = _dpiManager.scale(BASE_HEIGHT);
	rbBand.cyMinChild = h;
	rbBand.cyMaxChild = h;
	rbBand.cxMinChild = 0;
	rbBand.cx = 1200;

	rebar->addBand(&rbBand, true);
}

void BookmarkBar::reSizeTo(const RECT& rc)
{
	::MoveWindow(_hSelf, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
	recalcLayout();
}

int BookmarkBar::getHeight() const
{
	if (!_isVisible || !_hSelf)
		return 0;
	return _dpiManager.scale(BASE_HEIGHT);
}

// --- Bookmark operations ---

void BookmarkBar::addBookmark(const std::wstring& filePath)
{
	if (hasBookmark(filePath))
		return;

	BookmarkItem item;
	item.filePath = filePath;
	const wchar_t* name = ::PathFindFileName(filePath.c_str());
	item.displayName = name ? name : filePath;
	_bookmarks.push_back(std::move(item));
	recalcLayout();
	::InvalidateRect(_hSelf, nullptr, TRUE);
}

void BookmarkBar::removeBookmark(int index)
{
	if (index < 0 || index >= static_cast<int>(_bookmarks.size()))
		return;
	_bookmarks.erase(_bookmarks.begin() + index);
	_hoverIndex = -1;
	recalcLayout();
	::InvalidateRect(_hSelf, nullptr, TRUE);
}

bool BookmarkBar::hasBookmark(const std::wstring& filePath) const
{
	for (const auto& bm : _bookmarks)
	{
		if (!bm.isFolder && _wcsicmp(bm.filePath.c_str(), filePath.c_str()) == 0)
			return true;
	}
	return false;
}

void BookmarkBar::addFolder(const std::wstring& name, const std::vector<std::wstring>& filePaths)
{
	BookmarkItem folder;
	folder.isFolder = true;
	folder.displayName = name;

	for (const auto& fp : filePaths)
	{
		BookmarkItem child;
		child.filePath = fp;
		const wchar_t* fn = ::PathFindFileName(fp.c_str());
		child.displayName = fn ? fn : fp;
		folder.children.push_back(std::move(child));
	}

	_bookmarks.push_back(std::move(folder));
	recalcLayout();
	::InvalidateRect(_hSelf, nullptr, TRUE);
}

bool BookmarkBar::isFolder(int index) const
{
	if (index < 0 || index >= static_cast<int>(_bookmarks.size()))
		return false;
	return _bookmarks[index].isFolder;
}

const std::wstring& BookmarkBar::getBookmarkPath(int index) const
{
	return _bookmarks[index].filePath;
}

const std::vector<BookmarkItem>& BookmarkBar::getFolderChildren(int index) const
{
	return _bookmarks[index].children;
}

// --- XML Persistence ---

void BookmarkBar::loadFromXml(const wchar_t* configPath)
{
	_bookmarks.clear();

	NppXml::Document doc = new NppXml::NewDocument();
	if (!NppXml::loadFile(doc, configPath))
	{
		delete doc;
		return;
	}

	NppXml::Element root = NppXml::firstChildElement(doc, "NotepadPlus");
	if (!root)
	{
		delete doc;
		return;
	}

	NppXml::Element bmRoot = NppXml::firstChildElement(root, "BookmarkedFiles");
	if (!bmRoot)
	{
		delete doc;
		return;
	}

	auto utf8ToWide = [](const char* utf8) -> std::wstring
	{
		if (!utf8 || utf8[0] == '\0')
			return {};
		int len = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		std::wstring wstr(len - 1, L'\0');
		::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr.data(), len);
		return wstr;
	};

	for (NppXml::Node child = NppXml::firstChild(bmRoot); child; child = NppXml::nextSibling(child))
	{
		const char* nodeName = NppXml::name(child);
		if (strcmp(nodeName, "File") == 0)
		{
			const char* path = NppXml::attribute(child, "path");
			if (path && path[0] != '\0')
			{
				BookmarkItem item;
				item.filePath = utf8ToWide(path);
				const wchar_t* fn = ::PathFindFileName(item.filePath.c_str());
				item.displayName = fn ? fn : item.filePath;
				_bookmarks.push_back(std::move(item));
			}
		}
		else if (strcmp(nodeName, "Folder") == 0)
		{
			const char* folderName = NppXml::attribute(child, "name");
			BookmarkItem folder;
			folder.isFolder = true;
			folder.displayName = folderName ? utf8ToWide(folderName) : L"Folder";

			for (NppXml::Element fileNode = NppXml::firstChildElement(child, "File");
				fileNode;
				fileNode = NppXml::nextSiblingElement(fileNode, "File"))
			{
				const char* path = NppXml::attribute(fileNode, "path");
				if (path && path[0] != '\0')
				{
					BookmarkItem childItem;
					childItem.filePath = utf8ToWide(path);
					const wchar_t* fn = ::PathFindFileName(childItem.filePath.c_str());
					childItem.displayName = fn ? fn : childItem.filePath;
					folder.children.push_back(std::move(childItem));
				}
			}
			_bookmarks.push_back(std::move(folder));
		}
	}

	delete doc;
	recalcLayout();
	::InvalidateRect(_hSelf, nullptr, TRUE);
}

void BookmarkBar::saveToXml(const wchar_t* configPath) const
{
	NppXml::Document doc = new NppXml::NewDocument();
	bool loaded = NppXml::loadFile(doc, configPath);

	NppXml::Element root;
	if (loaded)
		root = NppXml::firstChildElement(doc, "NotepadPlus");

	if (!root)
	{
		NppXml::createNewDeclaration(doc);
		root = NppXml::createChildElement(doc, "NotepadPlus");
	}

	NppXml::Element oldBm = NppXml::firstChildElement(root, "BookmarkedFiles");
	if (oldBm)
		[[maybe_unused]] bool removed = NppXml::deleteChild(root, oldBm);

	NppXml::Element bmRoot = NppXml::createChildElement(root, "BookmarkedFiles");

	auto wideToUtf8 = [](const std::wstring& wstr) -> std::string
	{
		if (wstr.empty())
			return {};
		int len = ::WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string utf8(len - 1, '\0');
		::WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, utf8.data(), len, nullptr, nullptr);
		return utf8;
	};

	for (const auto& bm : _bookmarks)
	{
		if (bm.isFolder)
		{
			NppXml::Element folderNode = NppXml::createChildElement(bmRoot, "Folder");
			NppXml::setAttribute(folderNode, "name", wideToUtf8(bm.displayName).c_str());

			for (const auto& child : bm.children)
			{
				NppXml::Element fileNode = NppXml::createChildElement(folderNode, "File");
				NppXml::setAttribute(fileNode, "path", wideToUtf8(child.filePath).c_str());
			}
		}
		else
		{
			NppXml::Element fileNode = NppXml::createChildElement(bmRoot, "File");
			NppXml::setAttribute(fileNode, "path", wideToUtf8(bm.filePath).c_str());
		}
	}

	[[maybe_unused]] bool saved = NppXml::saveFile(doc, configPath);
	delete doc;
}

void BookmarkBar::setVisible(bool visible)
{
	_isVisible = visible;
	::ShowWindow(_hSelf, visible ? SW_SHOW : SW_HIDE);
}

// --- Window procedure ---

LRESULT CALLBACK BookmarkBar::staticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_NCCREATE)
	{
		auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		auto self = static_cast<BookmarkBar*>(cs->lpCreateParams);
		::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		return TRUE;
	}

	auto self = reinterpret_cast<BookmarkBar*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (self)
		return self->runProc(hwnd, msg, wParam, lParam);

	return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT BookmarkBar::runProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_ERASEBKGND:
			return TRUE;

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hDC = ::BeginPaint(hwnd, &ps);
			paint(hDC);
			::EndPaint(hwnd, &ps);
			return 0;
		}

		case WM_MOUSEMOVE:
		{
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);

			// Check if we should start dragging
			if (_dragSourceIndex >= 0 && !_isDragging && (wParam & MK_LBUTTON))
			{
				int dx = x - _dragStartPt.x;
				int dy = y - _dragStartPt.y;
				if (dx * dx + dy * dy > DRAG_THRESHOLD * DRAG_THRESHOLD)
				{
					_isDragging = true;
					::SetCursor(::LoadCursor(nullptr, IDC_HAND));
				}
			}

			if (_isDragging)
			{
				// Find drop target position
				int newTarget = -1;
				for (int i = 0; i < static_cast<int>(_itemRects.size()); ++i)
				{
					int midX = (_itemRects[i].left + _itemRects[i].right) / 2;
					if (x < midX)
					{
						newTarget = i;
						break;
					}
				}
				if (newTarget == -1)
					newTarget = static_cast<int>(_bookmarks.size());

				if (newTarget != _dragTargetIndex)
				{
					_dragTargetIndex = newTarget;
					::InvalidateRect(hwnd, nullptr, TRUE);
				}
				return 0;
			}

			int newHover = hitTest(x, y);
			bool newCloseHover = (newHover >= 0) ? hitTestClose(x, y, newHover) : false;

			if (newHover != _hoverIndex || newCloseHover != _closeHover)
			{
				_hoverIndex = newHover;
				_closeHover = newCloseHover;
				::InvalidateRect(hwnd, nullptr, TRUE);
			}

			TRACKMOUSEEVENT tme{};
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = hwnd;
			::TrackMouseEvent(&tme);
			return 0;
		}

		case WM_MOUSELEAVE:
		{
			if (_hoverIndex != -1 || _closeHover)
			{
				_hoverIndex = -1;
				_closeHover = false;
				::InvalidateRect(hwnd, nullptr, TRUE);
			}
			return 0;
		}

		case WM_LBUTTONDOWN:
		{
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			_pressedIndex = hitTest(x, y);
			_dragStartPt = { x, y };
			_isDragging = false;
			_dragSourceIndex = _pressedIndex;
			_dragTargetIndex = -1;
			if (_pressedIndex >= 0)
				::SetCapture(hwnd);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return 0;
		}

		case WM_LBUTTONUP:
		{
			::ReleaseCapture();
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);

			if (_isDragging && _dragSourceIndex >= 0 && _dragTargetIndex >= 0)
			{
				// Perform reorder
				int src = _dragSourceIndex;
				int dst = _dragTargetIndex;
				if (src != dst && src != dst - 1)
				{
					BookmarkItem item = std::move(_bookmarks[src]);
					_bookmarks.erase(_bookmarks.begin() + src);
					int insertAt = (dst > src) ? dst - 1 : dst;
					_bookmarks.insert(_bookmarks.begin() + insertAt, std::move(item));
					recalcLayout();
					::PostMessage(_hParent, BMN_BOOKMARK_SAVE, 0, 0);
				}
				_isDragging = false;
				_dragSourceIndex = -1;
				_dragTargetIndex = -1;
				_pressedIndex = -1;
				::InvalidateRect(hwnd, nullptr, TRUE);
				return 0;
			}

			_isDragging = false;
			_dragSourceIndex = -1;
			_dragTargetIndex = -1;

			int idx = hitTest(x, y);
			if (idx >= 0 && idx == _pressedIndex)
			{
				if (hitTestClose(x, y, idx))
				{
					removeBookmark(idx);
					::PostMessage(_hParent, BMN_BOOKMARK_SAVE, 0, 0);
				}
				else if (isFolder(idx))
				{
					showFolderDropdown(idx);
				}
				else
				{
					::SendMessage(_hParent, BMN_BOOKMARKCLICKED, static_cast<WPARAM>(idx), 0);
				}
			}
			_pressedIndex = -1;
			::InvalidateRect(hwnd, nullptr, TRUE);
			return 0;
		}

		case WM_RBUTTONUP:
		{
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			int idx = hitTest(x, y);
			POINT pt = { x, y };
			::ClientToScreen(hwnd, &pt);

			if (idx >= 0)
				showItemContextMenu(idx, pt);
			else
				showBarContextMenu(pt);
			return 0;
		}

		case WM_TIMER:
		{
			if (wParam == SHIMMER_TIMER_ID)
			{
				_shimmerPhase += 0.008f;
				if (_shimmerPhase > 1.0f)
					_shimmerPhase -= 1.0f;
				// Only repaint the gradient strip area (top 2-3 pixels)
				RECT gradRc;
				::GetClientRect(hwnd, &gradRc);
				gradRc.bottom = _dpiManager.scale(3);
				::InvalidateRect(hwnd, &gradRc, FALSE);
			}
			return 0;
		}

		case WM_SIZE:
		{
			recalcLayout();
			return 0;
		}

		case WM_DPICHANGED_AFTERPARENT:
		{
			_dpiManager.setDpi(hwnd);

			if (_hFont)
				::DeleteObject(_hFont);

			NONCLIENTMETRICS ncm{};
			ncm.cbSize = sizeof(ncm);
			::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
			ncm.lfMenuFont.lfHeight = -_dpiManager.scale(12);
			ncm.lfMenuFont.lfWeight = FW_NORMAL;
			_hFont = ::CreateFontIndirect(&ncm.lfMenuFont);

			recalcLayout();
			return 0;
		}
	}

	return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

// --- Drawing ---

void BookmarkBar::paint(HDC hDC)
{
	RECT clientRc{};
	::GetClientRect(_hSelf, &clientRc);

	HDC hMemDC = ::CreateCompatibleDC(hDC);
	HBITMAP hMemBmp = ::CreateCompatibleBitmap(hDC, clientRc.right, clientRc.bottom);
	HBITMAP hOldBmp = static_cast<HBITMAP>(::SelectObject(hMemDC, hMemBmp));

	bool isDark = NppDarkMode::isEnabled();

	// Background: Mantle (matches rebar/tab strip)
	COLORREF bgColor = isDark ? NppDarkMode::getDlgBackgroundColor() : ::GetSysColor(COLOR_BTNFACE);
	HBRUSH bgBrush = ::CreateSolidBrush(bgColor);
	::FillRect(hMemDC, &clientRc, bgBrush);
	::DeleteObject(bgBrush);

	// Animated gradient accent strip at top (Catppuccin Mocha: Mauve -> Blue -> Teal with shimmer)
	if (isDark)
	{
		int gradH = _dpiManager.scale(2);
		int width = clientRc.right;
		if (width > 0)
		{
			// Four color stops cycling: Mauve -> Blue -> Teal -> Mauve
			struct { float r, g, b; } stops[4] = {
				{ 203.f, 166.f, 247.f },  // Mauve
				{ 137.f, 180.f, 250.f },  // Blue
				{ 148.f, 226.f, 213.f },  // Teal
				{ 203.f, 166.f, 247.f }   // Mauve (wrap)
			};

			for (int x = 0; x < width; ++x)
			{
				// Shift position by shimmer phase for animation
				float rawT = static_cast<float>(x) / static_cast<float>(width) + _shimmerPhase;
				if (rawT > 1.0f) rawT -= 1.0f;

				// Map to 3 segments
				float segment = rawT * 3.0f;
				int seg = static_cast<int>(segment);
				if (seg >= 3) seg = 2;
				float frac = segment - static_cast<float>(seg);

				BYTE r = static_cast<BYTE>(stops[seg].r + frac * (stops[seg + 1].r - stops[seg].r));
				BYTE g = static_cast<BYTE>(stops[seg].g + frac * (stops[seg + 1].g - stops[seg].g));
				BYTE b = static_cast<BYTE>(stops[seg].b + frac * (stops[seg + 1].b - stops[seg].b));

				// Add shimmer highlight: a traveling bright spot
				float shimmerPos = _shimmerPhase * 3.0f;
				if (shimmerPos > 1.0f) shimmerPos -= 1.0f;
				float dist = fabsf(rawT - shimmerPos);
				if (dist > 0.5f) dist = 1.0f - dist;
				float shimmerIntensity = fmaxf(0.0f, 1.0f - dist * 8.0f) * 0.3f;

				r = static_cast<BYTE>(fminf(255.f, r + shimmerIntensity * (255.f - r)));
				g = static_cast<BYTE>(fminf(255.f, g + shimmerIntensity * (255.f - g)));
				b = static_cast<BYTE>(fminf(255.f, b + shimmerIntensity * (255.f - b)));

				HPEN gPen = ::CreatePen(PS_SOLID, 1, RGB(r, g, b));
				HPEN oldGPen = static_cast<HPEN>(::SelectObject(hMemDC, gPen));
				::MoveToEx(hMemDC, x, 0, nullptr);
				::LineTo(hMemDC, x, gradH);
				::SelectObject(hMemDC, oldGPen);
				::DeleteObject(gPen);
			}
		}
	}

	// Thin bottom separator
	COLORREF borderColor = isDark ? NppDarkMode::getEdgeColor() : ::GetSysColor(COLOR_BTNSHADOW);
	HPEN borderPen = ::CreatePen(PS_SOLID, 1, borderColor);
	HPEN oldPen = static_cast<HPEN>(::SelectObject(hMemDC, borderPen));
	::MoveToEx(hMemDC, 0, clientRc.bottom - 1, nullptr);
	::LineTo(hMemDC, clientRc.right, clientRc.bottom - 1);
	::SelectObject(hMemDC, oldPen);
	::DeleteObject(borderPen);

	HFONT oldFont = static_cast<HFONT>(::SelectObject(hMemDC, _hFont));
	::SetBkMode(hMemDC, TRANSPARENT);

	const int cr = _dpiManager.scale(CORNER_RADIUS);

	for (int i = 0; i < static_cast<int>(_bookmarks.size()); ++i)
	{
		if (i >= static_cast<int>(_itemRects.size()))
			break;

		const RECT& rc = _itemRects[i];
		bool isHover = (i == _hoverIndex);
		bool isPressed = (i == _pressedIndex);
		bool folder = _bookmarks[i].isFolder;

		// Button background (Chrome-like: invisible until hovered)
		COLORREF btnColor = bgColor;
		if (isDark)
		{
			if (isPressed)
				btnColor = NppDarkMode::getHotBackgroundColor();
			else if (isHover)
				btnColor = NppDarkMode::getBackgroundColor();
		}
		else
		{
			if (isPressed)
				btnColor = ::GetSysColor(COLOR_BTNSHADOW);
			else if (isHover)
				btnColor = ::GetSysColor(COLOR_BTNHIGHLIGHT);
		}

		if (isHover || isPressed)
		{
			HRGN hRgn = ::CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1, cr * 2, cr * 2);
			HBRUSH btnBrush = ::CreateSolidBrush(btnColor);
			::FillRgn(hMemDC, hRgn, btnBrush);
			::DeleteObject(btnBrush);
			::DeleteObject(hRgn);
		}

		int iconSize = _dpiManager.scale(12);
		int iconX = rc.left + _dpiManager.scale(6);
		int iconY = rc.top + (rc.bottom - rc.top - iconSize) / 2;

		// Determine file-type accent color for non-folder items
		COLORREF fileTypeColor = 0;
		bool hasFileTypeColor = false;
		if (!folder && isDark)
		{
			const wchar_t* ext = wcsrchr(_bookmarks[i].filePath.c_str(), L'.');
			if (ext)
			{
				hasFileTypeColor = true;
				if (_wcsicmp(ext, L".py") == 0 || _wcsicmp(ext, L".pyw") == 0)
					fileTypeColor = RGB(55, 118, 171);
				else if (_wcsicmp(ext, L".js") == 0 || _wcsicmp(ext, L".mjs") == 0)
					fileTypeColor = RGB(241, 224, 90);
				else if (_wcsicmp(ext, L".ts") == 0 || _wcsicmp(ext, L".tsx") == 0)
					fileTypeColor = RGB(49, 120, 198);
				else if (_wcsicmp(ext, L".cpp") == 0 || _wcsicmp(ext, L".cc") == 0 || _wcsicmp(ext, L".c") == 0)
					fileTypeColor = RGB(156, 89, 182);
				else if (_wcsicmp(ext, L".h") == 0 || _wcsicmp(ext, L".hpp") == 0)
					fileTypeColor = RGB(130, 61, 166);
				else if (_wcsicmp(ext, L".html") == 0 || _wcsicmp(ext, L".htm") == 0)
					fileTypeColor = RGB(228, 77, 38);
				else if (_wcsicmp(ext, L".css") == 0)
					fileTypeColor = RGB(86, 61, 124);
				else if (_wcsicmp(ext, L".json") == 0)
					fileTypeColor = RGB(253, 203, 110);
				else if (_wcsicmp(ext, L".xml") == 0)
					fileTypeColor = RGB(233, 102, 59);
				else if (_wcsicmp(ext, L".md") == 0)
					fileTypeColor = RGB(83, 141, 213);
				else if (_wcsicmp(ext, L".ps1") == 0)
					fileTypeColor = RGB(1, 36, 86);
				else if (_wcsicmp(ext, L".sh") == 0 || _wcsicmp(ext, L".bash") == 0)
					fileTypeColor = RGB(77, 170, 87);
				else if (_wcsicmp(ext, L".sql") == 0)
					fileTypeColor = RGB(218, 139, 55);
				else if (_wcsicmp(ext, L".yaml") == 0 || _wcsicmp(ext, L".yml") == 0)
					fileTypeColor = RGB(203, 23, 30);
				else if (_wcsicmp(ext, L".java") == 0)
					fileTypeColor = RGB(176, 114, 25);
				else if (_wcsicmp(ext, L".go") == 0)
					fileTypeColor = RGB(0, 173, 216);
				else if (_wcsicmp(ext, L".rs") == 0)
					fileTypeColor = RGB(222, 165, 132);
				else if (_wcsicmp(ext, L".rb") == 0)
					fileTypeColor = RGB(204, 52, 45);
				else
					hasFileTypeColor = false;
			}
		}

		COLORREF iconColor = hasFileTypeColor ? fileTypeColor :
			(isDark ? NppDarkMode::getDarkerTextColor() : ::GetSysColor(COLOR_GRAYTEXT));

		HPEN iconPen = ::CreatePen(PS_SOLID, 1, iconColor);
		HPEN oldIconPen = static_cast<HPEN>(::SelectObject(hMemDC, iconPen));
		HBRUSH oldBrush2 = static_cast<HBRUSH>(::SelectObject(hMemDC, ::GetStockObject(NULL_BRUSH)));

		if (folder)
		{
			// Filled folder icon with Catppuccin Peach
			COLORREF folderFill = isDark ? RGB(250, 179, 135) : RGB(200, 140, 80);
			HBRUSH folderBrush = ::CreateSolidBrush(folderFill);
			HPEN folderPen = ::CreatePen(PS_SOLID, 1, folderFill);
			HPEN oldFP = static_cast<HPEN>(::SelectObject(hMemDC, folderPen));
			HBRUSH oldFB = static_cast<HBRUSH>(::SelectObject(hMemDC, folderBrush));

			int tabH = _dpiManager.scale(3);
			int tabW = _dpiManager.scale(5);
			POINT folderPts[7] = {
				{ iconX, iconY + tabH },
				{ iconX, iconY },
				{ iconX + tabW, iconY },
				{ iconX + tabW + _dpiManager.scale(2), iconY + tabH },
				{ iconX + iconSize, iconY + tabH },
				{ iconX + iconSize, iconY + iconSize },
				{ iconX, iconY + iconSize }
			};
			::Polygon(hMemDC, folderPts, 7);

			::SelectObject(hMemDC, oldFB);
			::DeleteObject(folderBrush);
			::SelectObject(hMemDC, oldFP);
			::DeleteObject(folderPen);
		}
		else
		{
			// Document icon with file-type colored fill
			int foldSize = _dpiManager.scale(3);
			POINT docPts[6] = {
				{ iconX, iconY },
				{ iconX + iconSize - foldSize, iconY },
				{ iconX + iconSize, iconY + foldSize },
				{ iconX + iconSize, iconY + iconSize },
				{ iconX, iconY + iconSize },
				{ iconX, iconY }
			};

			if (hasFileTypeColor)
			{
				// Filled doc with a dimmed version of the type color
				BYTE r = GetRValue(fileTypeColor), g = GetGValue(fileTypeColor), b = GetBValue(fileTypeColor);
				COLORREF dimColor = RGB(r / 3, g / 3, b / 3);
				HBRUSH docBrush = ::CreateSolidBrush(dimColor);
				HBRUSH oldDB = static_cast<HBRUSH>(::SelectObject(hMemDC, docBrush));
				::Polygon(hMemDC, docPts, 5);
				::SelectObject(hMemDC, oldDB);
				::DeleteObject(docBrush);
			}

			::Polyline(hMemDC, docPts, 6);
			::MoveToEx(hMemDC, iconX + iconSize - foldSize, iconY, nullptr);
			::LineTo(hMemDC, iconX + iconSize - foldSize, iconY + foldSize);
			::LineTo(hMemDC, iconX + iconSize, iconY + foldSize);
		}

		::SelectObject(hMemDC, oldBrush2);
		::SelectObject(hMemDC, oldIconPen);
		::DeleteObject(iconPen);

		// Text
		COLORREF textColor = isDark ? NppDarkMode::getTextColor() : ::GetSysColor(COLOR_BTNTEXT);
		::SetTextColor(hMemDC, textColor);

		int closeSpace = isHover ? _dpiManager.scale(16) : 0;

		RECT textRc = rc;
		textRc.left = iconX + iconSize + _dpiManager.scale(4);
		textRc.right -= _dpiManager.scale(folder ? 14 : 6) + closeSpace;

		::DrawText(hMemDC, _bookmarks[i].displayName.c_str(), -1, &textRc,
			DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

		// Draw X close button on hovered items
		if (isHover)
		{
			RECT closeRc = getCloseButtonRect(i);
			COLORREF xColor;
			if (_closeHover)
				xColor = isDark ? RGB(255, 100, 100) : RGB(200, 50, 50);
			else
				xColor = isDark ? NppDarkMode::getDarkerTextColor() : ::GetSysColor(COLOR_GRAYTEXT);

			// Draw circle background when close is hovered
			if (_closeHover)
			{
				COLORREF closeBg = isDark ? RGB(80, 40, 40) : RGB(230, 200, 200);
				HBRUSH closeBrush = ::CreateSolidBrush(closeBg);
				HRGN closeRgn = ::CreateEllipticRgn(closeRc.left, closeRc.top, closeRc.right + 1, closeRc.bottom + 1);
				::FillRgn(hMemDC, closeRgn, closeBrush);
				::DeleteObject(closeRgn);
				::DeleteObject(closeBrush);
			}

			HPEN xPen = ::CreatePen(PS_SOLID, _dpiManager.scale(1), xColor);
			HPEN oldXPen = static_cast<HPEN>(::SelectObject(hMemDC, xPen));
			int xm = _dpiManager.scale(4); // margin inside the close rect
			::MoveToEx(hMemDC, closeRc.left + xm, closeRc.top + xm, nullptr);
			::LineTo(hMemDC, closeRc.right - xm + 1, closeRc.bottom - xm + 1);
			::MoveToEx(hMemDC, closeRc.right - xm, closeRc.top + xm, nullptr);
			::LineTo(hMemDC, closeRc.left + xm - 1, closeRc.bottom - xm + 1);
			::SelectObject(hMemDC, oldXPen);
			::DeleteObject(xPen);
		}

		// Folder: draw small down-arrow (shifted left when hovered to make room for X)
		if (folder)
		{
			int arrowOffset = isHover ? closeSpace : 0;
			int arrowX = rc.right - _dpiManager.scale(10) - arrowOffset;
			int arrowY = rc.top + (rc.bottom - rc.top) / 2 - 1;
			int arrowW = _dpiManager.scale(4);

			HPEN arrowPen = ::CreatePen(PS_SOLID, 1, isDark ? NppDarkMode::getDarkerTextColor() : ::GetSysColor(COLOR_GRAYTEXT));
			HPEN oldArrowPen = static_cast<HPEN>(::SelectObject(hMemDC, arrowPen));
			// V shape
			::MoveToEx(hMemDC, arrowX, arrowY, nullptr);
			::LineTo(hMemDC, arrowX + arrowW / 2, arrowY + arrowW / 2 + 1);
			::LineTo(hMemDC, arrowX + arrowW + 1, arrowY - 1);
			::SelectObject(hMemDC, oldArrowPen);
			::DeleteObject(arrowPen);
		}
	}

	// Draw drag-and-drop indicator
	if (_isDragging && _dragTargetIndex >= 0)
	{
		int dropX;
		int vpad = _dpiManager.scale(ITEM_VPADDING);
		if (_dragTargetIndex < static_cast<int>(_itemRects.size()))
			dropX = _itemRects[_dragTargetIndex].left - _dpiManager.scale(2);
		else if (!_itemRects.empty())
			dropX = _itemRects.back().right + _dpiManager.scale(2);
		else
			dropX = _dpiManager.scale(BAR_LPADDING);

		// Catppuccin Blue accent for the drop indicator
		COLORREF dropColor = isDark ? RGB(137, 180, 250) : RGB(30, 102, 245);
		HPEN dropPen = ::CreatePen(PS_SOLID, _dpiManager.scale(2), dropColor);
		HPEN oldDropPen = static_cast<HPEN>(::SelectObject(hMemDC, dropPen));
		::MoveToEx(hMemDC, dropX, vpad, nullptr);
		::LineTo(hMemDC, dropX, clientRc.bottom - vpad - 1);

		// Small diamond at top and bottom
		int dSize = _dpiManager.scale(3);
		HBRUSH dropBrush = ::CreateSolidBrush(dropColor);
		HBRUSH oldDropBrush = static_cast<HBRUSH>(::SelectObject(hMemDC, dropBrush));
		::Ellipse(hMemDC, dropX - dSize, vpad - 1, dropX + dSize + 1, vpad + dSize * 2);
		::Ellipse(hMemDC, dropX - dSize, clientRc.bottom - vpad - dSize * 2, dropX + dSize + 1, clientRc.bottom - vpad + 1);
		::SelectObject(hMemDC, oldDropBrush);
		::DeleteObject(dropBrush);
		::SelectObject(hMemDC, oldDropPen);
		::DeleteObject(dropPen);
	}

	// Hint when empty
	if (_bookmarks.empty())
	{
		COLORREF hintColor = isDark ? NppDarkMode::getDarkerTextColor() : ::GetSysColor(COLOR_GRAYTEXT);
		::SetTextColor(hMemDC, hintColor);
		RECT hintRc = clientRc;
		hintRc.left += _dpiManager.scale(BAR_LPADDING);
		::DrawText(hMemDC, L"Right-click a tab to bookmark it", -1, &hintRc,
			DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
	}

	::SelectObject(hMemDC, oldFont);

	::BitBlt(hDC, 0, 0, clientRc.right, clientRc.bottom, hMemDC, 0, 0, SRCCOPY);

	::SelectObject(hMemDC, hOldBmp);
	::DeleteObject(hMemBmp);
	::DeleteDC(hMemDC);
}

int BookmarkBar::hitTest(int x, int y) const
{
	POINT pt = { x, y };
	for (int i = 0; i < static_cast<int>(_itemRects.size()); ++i)
	{
		if (::PtInRect(&_itemRects[i], pt))
			return i;
	}
	return -1;
}

void BookmarkBar::recalcLayout()
{
	_itemRects.clear();
	if (!_hSelf || _bookmarks.empty())
		return;

	HDC hDC = ::GetDC(_hSelf);
	HFONT oldFont = static_cast<HFONT>(::SelectObject(hDC, _hFont));

	RECT clientRc{};
	::GetClientRect(_hSelf, &clientRc);

	int x = _dpiManager.scale(BAR_LPADDING);
	int vpad = _dpiManager.scale(ITEM_VPADDING);
	int hpad = _dpiManager.scale(ITEM_HPADDING);
	int gap = _dpiManager.scale(ITEM_GAP);
	int iconSpace = _dpiManager.scale(12 + 4 + 6);
	int maxWidth = _dpiManager.scale(200);
	int arrowExtra = _dpiManager.scale(14);
	int closeExtra = _dpiManager.scale(16);

	for (const auto& bm : _bookmarks)
	{
		SIZE textSize{};
		::GetTextExtentPoint32(hDC, bm.displayName.c_str(),
			static_cast<int>(bm.displayName.length()), &textSize);

		int itemWidth = iconSpace + textSize.cx + hpad + closeExtra;
		if (bm.isFolder)
			itemWidth += arrowExtra;
		if (itemWidth > maxWidth)
			itemWidth = maxWidth;

		RECT rc;
		rc.left = x;
		rc.top = vpad;
		rc.right = x + itemWidth;
		rc.bottom = clientRc.bottom - vpad - 1;

		_itemRects.push_back(rc);
		x += itemWidth + gap;

		if (x > clientRc.right)
			break;
	}

	::SelectObject(hDC, oldFont);
	::ReleaseDC(_hSelf, hDC);
}

// --- Context menus ---

void BookmarkBar::showItemContextMenu(int index, POINT pt)
{
	HMENU hMenu = ::CreatePopupMenu();
	bool folder = _bookmarks[index].isFolder;

	if (folder)
	{
		::AppendMenu(hMenu, MF_STRING, 1, L"Open All");
		::AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);

		// List each child with a "Remove" option
		const auto& children = _bookmarks[index].children;
		if (!children.empty())
		{
			HMENU hRemoveSub = ::CreatePopupMenu();
			for (int ci = 0; ci < static_cast<int>(children.size()); ++ci)
			{
				::AppendMenu(hRemoveSub, MF_STRING, static_cast<UINT_PTR>(100 + ci), children[ci].displayName.c_str());
			}
			::AppendMenu(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hRemoveSub), L"Remove from Folder");
			::AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
		}

		::AppendMenu(hMenu, MF_STRING, 3, L"Remove Entire Folder");
	}
	else
	{
		::AppendMenu(hMenu, MF_STRING, 1, L"Open");
		::AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
		::AppendMenu(hMenu, MF_STRING, 3, L"Remove Bookmark");
	}

	int cmd = ::TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
		pt.x, pt.y, 0, _hSelf, nullptr);
	::DestroyMenu(hMenu);

	if (cmd == 1)
	{
		if (folder)
		{
			const auto& children = _bookmarks[index].children;
			for (int ci = 0; ci < static_cast<int>(children.size()); ++ci)
			{
				::SendMessage(_hParent, WM_DOOPEN, 0, reinterpret_cast<LPARAM>(children[ci].filePath.c_str()));
			}
		}
		else
		{
			::SendMessage(_hParent, BMN_BOOKMARKCLICKED, static_cast<WPARAM>(index), 0);
		}
	}
	else if (cmd == 3)
	{
		removeBookmark(index);
		::PostMessage(_hParent, BMN_BOOKMARK_SAVE, 0, 0);
	}
	else if (cmd >= 100 && folder)
	{
		int childIdx = cmd - 100;
		removeChildFromFolder(index, childIdx);
		::PostMessage(_hParent, BMN_BOOKMARK_SAVE, 0, 0);
	}
}

void BookmarkBar::showBarContextMenu(POINT pt)
{
	HMENU hMenu = ::CreatePopupMenu();
	::AppendMenu(hMenu, MF_STRING, 1, L"Bookmark Current Tab");
	::AppendMenu(hMenu, MF_STRING, 2, L"Bookmark All Tabs");
	::AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
	::AppendMenu(hMenu, MF_STRING, 3, L"Hide Bookmark Bar");

	int cmd = ::TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
		pt.x, pt.y, 0, _hSelf, nullptr);
	::DestroyMenu(hMenu);

	switch (cmd)
	{
		case 1:
			::SendMessage(_hParent, WM_COMMAND, IDM_FILE_BOOKMARK_TAB, 0);
			break;
		case 2:
			::SendMessage(_hParent, BMN_BOOKMARK_ALL_TABS, 0, 0);
			break;
		case 3:
			::SendMessage(_hParent, WM_COMMAND, IDM_FILE_TOGGLE_BOOKMARKBAR, 0);
			break;
	}
}

RECT BookmarkBar::getCloseButtonRect(int itemIndex) const
{
	RECT closeRc = { 0, 0, 0, 0 };
	if (itemIndex < 0 || itemIndex >= static_cast<int>(_itemRects.size()))
		return closeRc;

	const RECT& itemRc = _itemRects[itemIndex];
	int btnSize = _dpiManager.scale(14);
	int margin = _dpiManager.scale(2);

	closeRc.right = itemRc.right - margin;
	closeRc.left = closeRc.right - btnSize;
	closeRc.top = itemRc.top + (itemRc.bottom - itemRc.top - btnSize) / 2;
	closeRc.bottom = closeRc.top + btnSize;
	return closeRc;
}

bool BookmarkBar::hitTestClose(int x, int y, int itemIndex) const
{
	if (itemIndex < 0 || itemIndex >= static_cast<int>(_itemRects.size()))
		return false;
	RECT closeRc = getCloseButtonRect(itemIndex);
	POINT pt = { x, y };
	return ::PtInRect(&closeRc, pt) != FALSE;
}

void BookmarkBar::removeChildFromFolder(int folderIndex, int childIndex)
{
	if (folderIndex < 0 || folderIndex >= static_cast<int>(_bookmarks.size()))
		return;
	auto& children = _bookmarks[folderIndex].children;
	if (childIndex < 0 || childIndex >= static_cast<int>(children.size()))
		return;
	children.erase(children.begin() + childIndex);

	// If folder is now empty, remove the folder itself
	if (children.empty())
	{
		_bookmarks.erase(_bookmarks.begin() + folderIndex);
		_hoverIndex = -1;
	}

	recalcLayout();
	::InvalidateRect(_hSelf, nullptr, TRUE);
}

void BookmarkBar::showFolderDropdown(int index)
{
	if (index < 0 || index >= static_cast<int>(_bookmarks.size()) || !_bookmarks[index].isFolder)
		return;

	const auto& children = _bookmarks[index].children;
	if (children.empty())
		return;

	HMENU hMenu = ::CreatePopupMenu();

	for (int i = 0; i < static_cast<int>(children.size()); ++i)
	{
		::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(i + 1), children[i].displayName.c_str());
	}

	::AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
	::AppendMenu(hMenu, MF_STRING, 9000, L"Open All");

	// Position below the folder button
	RECT itemRc = _itemRects[index];
	POINT pt = { itemRc.left, itemRc.bottom };
	::ClientToScreen(_hSelf, &pt);

	int cmd = ::TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
		pt.x, pt.y, 0, _hSelf, nullptr);
	::DestroyMenu(hMenu);

	if (cmd == 9000)
	{
		// Open all
		for (const auto& child : children)
		{
			::SendMessage(_hParent, WM_DOOPEN, 0, reinterpret_cast<LPARAM>(child.filePath.c_str()));
		}
	}
	else if (cmd > 0 && cmd <= static_cast<int>(children.size()))
	{
		const std::wstring& filePath = children[cmd - 1].filePath;
		::SendMessage(_hParent, WM_DOOPEN, 0, reinterpret_cast<LPARAM>(filePath.c_str()));
	}
}
