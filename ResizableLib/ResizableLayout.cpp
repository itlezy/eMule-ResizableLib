// ResizableLayout.cpp: implementation of the CResizableLayout class.
//
/////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2000-2002 by Paolo Messina
// (http://www.geocities.com/ppescher - ppescher@yahoo.com)
//
// The contents of this file are subject to the Artistic License (the "License").
// You may not use this file except in compliance with the License. 
// You may obtain a copy of the License at:
// http://www.opensource.org/licenses/artistic-license.html
//
// If you find this code useful, credits would be nice!
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ResizableLayout.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define BS_TYPEMASK SS_TYPEMASK

void CResizableLayout::AddAnchor(HWND hWnd, CSize sizeTypeTL, CSize sizeTypeBR)
{
	CWnd* pParent = GetResizableWnd();

	// child window must be valid
	ASSERT(::IsWindow(hWnd));
	// must be child of parent window
	ASSERT(::IsChild(pParent->GetSafeHwnd(), hWnd));
	// top-left anchor must be valid
	ASSERT(sizeTypeTL != NOANCHOR);

	// get control's window class
	CString sClassName;
	GetClassName(hWnd, sClassName.GetBufferSetLength(MAX_PATH), MAX_PATH);
	sClassName.ReleaseBuffer();

	// add the style 'clipsiblings' to a GroupBox
	// to avoid unnecessary repainting of controls inside
	if (sClassName == "Button")
	{
		DWORD style = GetWindowLong(hWnd, GWL_STYLE);
		if ((style & BS_TYPEMASK) == BS_GROUPBOX)
			SetWindowLong(hWnd, GWL_STYLE, style | WS_CLIPSIBLINGS);
	}

	// get parent window's rect
	CRect rectParent;
	GetTotalClientRect(&rectParent);
	// and child control's rect
	CRect rectChild;
	::GetWindowRect(hWnd, &rectChild);
	::MapWindowPoints(NULL, pParent->m_hWnd, (LPPOINT)&rectChild, 2);

	// go calculate margins
	CSize sizeMarginTL, sizeMarginBR;

	if (sizeTypeBR == NOANCHOR)
		sizeTypeBR = sizeTypeTL;
	
	// calculate margin for the top-left corner

	sizeMarginTL.cx = rectChild.left - rectParent.Width() * sizeTypeTL.cx / 100;
	sizeMarginTL.cy = rectChild.top - rectParent.Height() * sizeTypeTL.cy / 100;
	
	// calculate margin for the bottom-right corner

	sizeMarginBR.cx = rectChild.right - rectParent.Width() * sizeTypeBR.cx / 100;
	sizeMarginBR.cy = rectChild.bottom - rectParent.Height() * sizeTypeBR.cy / 100;

	// add to the list and the map
	LayoutInfo layout(hWnd, sizeTypeTL, sizeMarginTL,
		sizeTypeBR, sizeMarginBR, sClassName);

	POSITION pos = m_listLayout.AddTail(layout);
	m_mapLayout.SetAt(hWnd, pos);
}

void CResizableLayout::AddAnchorCallback(UINT nCallbackID)
{
	// one callback control cannot rely upon another callback control's
	// size and/or position (they're updated all together at the end)
	// it can however use a non-callback control, which is updated before

	// add to the list
	LayoutInfo layout;
	layout.nCallbackID = nCallbackID;
	m_listLayoutCB.AddTail(layout);
}

BOOL CResizableLayout::ArrangeLayoutCallback(CResizableLayout::LayoutInfo& /*layout*/)
{
	ASSERT(FALSE);
	// must be overridden, if callback is used
	
	return FALSE;	// no output data
}

void CResizableLayout::ArrangeLayout()
{
	// common vars
	UINT uFlags;
	LayoutInfo layout;
	CRect rectParent, rectChild;
	GetTotalClientRect(&rectParent); // get parent window's rect
	int count = m_listLayout.GetCount();
	int countCB = m_listLayoutCB.GetCount();

	// reposition child windows
	HDWP hdwp = ::BeginDeferWindowPos(count);
	
	POSITION pos = m_listLayout.GetHeadPosition();
	while (pos != NULL)
	{
		// get layout info
		layout = m_listLayout.GetNext(pos);
		
		// calculate new child's position, size and flags for SetWindowPos
		CalcNewChildPosition(layout, rectParent, rectChild, uFlags);

		// only if size or position changed
		if ((uFlags & (SWP_NOMOVE|SWP_NOSIZE)) != (SWP_NOMOVE|SWP_NOSIZE))
		{
			hdwp = ::DeferWindowPos(hdwp, layout.hWnd, NULL, rectChild.left, rectChild.top,
				rectChild.Width(), rectChild.Height(), uFlags);
		}
	}
	::EndDeferWindowPos(hdwp);

	// force updating of non-callback windows
	// since the others may need it

	// reposition callback windows
	hdwp = ::BeginDeferWindowPos(countCB);

	pos = m_listLayoutCB.GetHeadPosition();
	while (pos != NULL)
	{
		// get layout info
		layout = m_listLayoutCB.GetNext(pos);
		// request data
		if (!ArrangeLayoutCallback(layout))
			continue;

		// calculate new child's position, size and flags for SetWindowPos
		CalcNewChildPosition(layout, rectParent, rectChild, uFlags);

		// only if size or position changed
		if ((uFlags & (SWP_NOMOVE|SWP_NOSIZE)) != (SWP_NOMOVE|SWP_NOSIZE))
		{
			hdwp = ::DeferWindowPos(hdwp, layout.hWnd, NULL, rectChild.left, rectChild.top,
				rectChild.Width(), rectChild.Height(), uFlags);
		}
	}
	::EndDeferWindowPos(hdwp);
}

void CResizableLayout::ClipChildWindow(const CResizableLayout::LayoutInfo& layout,
									   CRgn* pRegion)
{
	// obtain window position
	CRect rect;
	::GetWindowRect(layout.hWnd, &rect);
	::MapWindowPoints(NULL, GetResizableWnd()->m_hWnd, (LPPOINT)&rect, 2);

	// use window region if any
	CRgn rgn;
	rgn.CreateRectRgn(0,0,0,0);
	switch (::GetWindowRgn(layout.hWnd, rgn))
	{
	case COMPLEXREGION:
	case SIMPLEREGION:
		rgn.OffsetRgn(rect.TopLeft());
		break;

	default:
		rgn.SetRectRgn(&rect);
	}

	// go clipping?
	if (LikesClipping(layout))
		pRegion->CombineRgn(pRegion, &rgn, RGN_DIFF);
	else
		pRegion->CombineRgn(pRegion, &rgn, RGN_OR);
}

void CResizableLayout::GetClippingRegion(CRgn* pRegion)
{
	CWnd* pWnd = GetResizableWnd();

	// System's default clipping area is screen's size,
	// not enough for max track size:
	// if screen is 1024 x 768 and resizing border is 4 pixels,
	// maximized size is 1024+4*2=1032 x 768+4*2=776,
	// but max track size is 4 pixels bigger 1036 x 780 (don't ask me why!)
	// So, if you resize the window to maximum size, the last 4 pixels
	// are clipped out by the default clipping region, that gets created
	// as soon as you call clipping functions (my guess).

	// reset clipping region to the whole client area
	CRect rect;
	pWnd->GetClientRect(&rect);
	pRegion->CreateRectRgnIndirect(&rect);

	// clip only anchored controls
	LayoutInfo layout;
	POSITION pos = m_listLayout.GetHeadPosition();
	while (pos != NULL)
	{
		// get layout info
		layout = m_listLayout.GetNext(pos);
		
		if (::IsWindowVisible(layout.hWnd))
			ClipChildWindow(layout, pRegion);
	}
	pos = m_listLayoutCB.GetHeadPosition();
	while (pos != NULL)
	{
		// get layout info
		layout = m_listLayoutCB.GetNext(pos);
		// request data
		if (!ArrangeLayoutCallback(layout))
			continue;

		if (::IsWindowVisible(layout.hWnd))
			ClipChildWindow(layout, pRegion);
	}

	// fix for RTL layouts (1 pixel too-much)
	if (pWnd->GetExStyle() & WS_EX_LAYOUTRTL)
		pRegion->OffsetRgn(-1,0);
}

void CResizableLayout::EraseBackground(CDC* pDC)
{
	HWND hWnd = GetResizableWnd()->GetSafeHwnd();

	// retrieve the background brush
	HBRUSH hBrush = NULL;
	// is this a dialog box?
	ATOM atomWndClass = (ATOM)::GetClassLong(hWnd, GCW_ATOM);
	if (atomWndClass == (ATOM)0x8002)
	{
		// send a message to the dialog box
		hBrush = (HBRUSH)::SendMessage(hWnd, WM_CTLCOLORDLG,
			(WPARAM)pDC->GetSafeHdc(), (LPARAM)hWnd);
	}
	else
	{
		// take the background brush from the window's class
		hBrush = (HBRUSH)::GetClassLong(hWnd, GCL_HBRBACKGROUND);
	}

	CRgn rgn;
	GetClippingRegion(&rgn);

	::FillRgn(pDC->GetSafeHdc(), rgn, hBrush);
}

// support legacy code (will disappear in future versions)
void CResizableLayout::ClipChildren(CDC* pDC)
{
	CRgn rgn;
	GetClippingRegion(&rgn);
	// the clipping region is in device units
	rgn.OffsetRgn(-pDC->GetWindowOrg());
	pDC->SelectClipRgn(&rgn);
}

void CResizableLayout::GetTotalClientRect(LPRECT lpRect)
{
	GetResizableWnd()->GetClientRect(lpRect);
}

BOOL CResizableLayout::NeedsRefresh(const CResizableLayout::LayoutInfo& layout,
									const CRect& rectOld, const CRect& rectNew)
{
	int nDiffWidth = (rectNew.Width() - rectOld.Width());
	int nDiffHeight = (rectNew.Height() - rectOld.Height());

	// is the same size?
	if (nDiffWidth == 0 && nDiffHeight == 0)
		return FALSE;

	// optimistic, no need to refresh
	BOOL bRefresh = FALSE;

	// window classes that need refresh when resized
	if (layout.sWndClass == "Static")
	{
		DWORD style = ::GetWindowLong(layout.hWnd, GWL_STYLE);

		switch (style & SS_TYPEMASK)
		{
		case SS_LEFT:
		case SS_CENTER:
		case SS_RIGHT:
			// word-wrapped text
			bRefresh = bRefresh || (nDiffWidth != 0);
			// vertically centered text
			if (style & SS_CENTERIMAGE)
				bRefresh = bRefresh || (nDiffHeight != 0);
			break;

		case SS_LEFTNOWORDWRAP:
			// text with ellipsis
			if (style & SS_ELLIPSISMASK)
				bRefresh = bRefresh || (nDiffWidth != 0);
			// vertically centered text
			if (style & SS_CENTERIMAGE)
				bRefresh = bRefresh || (nDiffHeight != 0);
			break;

		case SS_ENHMETAFILE:
		case SS_BITMAP:
		case SS_ICON:
			// images
		case SS_BLACKFRAME:
		case SS_GRAYFRAME:
		case SS_WHITEFRAME:
		case SS_ETCHEDFRAME:
			// and frames
			bRefresh = TRUE;
			break;
		}
	}

	// window classes that don't redraw client area correctly
	// when the hor scroll pos changes due to a resizing
	BOOL bHScroll = FALSE;
	if (layout.sWndClass == "ListBox")
		bHScroll = TRUE;

	// fix for horizontally scrollable windows
	if (bHScroll && (nDiffWidth > 0))
	{
		// get max scroll position
		SCROLLINFO info;
		info.cbSize = sizeof(SCROLLINFO);
		info.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
		if (::GetScrollInfo(layout.hWnd, SB_HORZ, &info))
		{
			// subtract the page size
			info.nMax -= __max(info.nPage-1,0);
		}

		// resizing will cause the text to scroll on the right
		// because the scrollbar is going beyond the right limit
		if ((info.nMax > 0) && (info.nPos + nDiffWidth > info.nMax))
		{
			// needs repainting, due to horiz scrolling
			bRefresh = TRUE;
		}
	}

	return bRefresh;
}

BOOL CResizableLayout::LikesClipping(const CResizableLayout::LayoutInfo& layout)
{
	DWORD style = ::GetWindowLong(layout.hWnd, GWL_STYLE);

	// skip windows that wants background repainted
	if (layout.sWndClass == TOOLBARCLASSNAME && (style & TBSTYLE_TRANSPARENT))
		return FALSE;
	if (layout.sWndClass == "Button" && (style & BS_TYPEMASK) == BS_GROUPBOX)
		return FALSE;
	if (layout.sWndClass == "Static")
	{
		switch (style & SS_TYPEMASK)
		{
		case SS_LEFT:
		case SS_CENTER:
		case SS_RIGHT:
		case SS_SIMPLE:
		case SS_LEFTNOWORDWRAP:
			// text
		case SS_BLACKRECT:
		case SS_GRAYRECT:
		case SS_WHITERECT:
			// filled rects
		case SS_ETCHEDHORZ:
		case SS_ETCHEDVERT:
			// etched lines
		case SS_BITMAP:
			// bitmaps
			return TRUE;
			break;

		case SS_ICON:
		case SS_ENHMETAFILE:
			if (style & SS_CENTERIMAGE)
				return FALSE;
			return TRUE;
			break;

		default:
			return FALSE;
		}
	}

	// assume the others like clipping
	return TRUE;
}

void CResizableLayout::CalcNewChildPosition(const CResizableLayout::LayoutInfo& layout,
								const CRect &rectParent, CRect &rectChild, UINT& uFlags)
{
	CWnd* pParent = GetResizableWnd();

	::GetWindowRect(layout.hWnd, &rectChild);
	::MapWindowPoints(NULL, pParent->m_hWnd, (LPPOINT)&rectChild, 2);
	
	CRect rectNew;

	// calculate new top-left corner
	rectNew.left = layout.sizeMarginTL.cx + rectParent.Width() * layout.sizeTypeTL.cx / 100;
	rectNew.top = layout.sizeMarginTL.cy + rectParent.Height() * layout.sizeTypeTL.cy / 100;
	
	// calculate new bottom-right corner
	rectNew.right = layout.sizeMarginBR.cx + rectParent.Width() * layout.sizeTypeBR.cx / 100;
	rectNew.bottom = layout.sizeMarginBR.cy + rectParent.Height() * layout.sizeTypeBR.cy / 100;

	// window classes that need refresh when resized
	BOOL bRefresh = NeedsRefresh(layout, rectChild, rectNew);

	// set flags 
	uFlags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREPOSITION;
	if (bRefresh)
		uFlags |= SWP_NOCOPYBITS;
	if (rectNew.TopLeft() == rectChild.TopLeft())
		uFlags |= SWP_NOMOVE;
	if (rectNew.Size() == rectChild.Size())
		uFlags |= SWP_NOSIZE;

	// update rect
	rectChild = rectNew;
}
