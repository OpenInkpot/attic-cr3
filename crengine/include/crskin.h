/***************************************************************************
 *   Copyright (C) 2008 by Vadim Lopatin                                   *
 *   vadim.lopatin@coolreader.org                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifndef CR_SKIN_INCLUDED
#define CR_SKIN_INCLUDED

#include "lvtypes.h"
#include "lvptrvec.h"
#include "lvdrawbuf.h"
#include "lvdocview.h"


// Vertical alignment flags
#define SKIN_VALIGN_MASK    0x0003
#define SKIN_VALIGN_TOP     0x0001
#define SKIN_VALIGN_CENTER  0x0000
#define SKIN_VALIGN_BOTTOM  0x0002

// Horizontal alignment flags
#define SKIN_HALIGN_MASK    0x0030
#define SKIN_HALIGN_LEFT    0x0000
#define SKIN_HALIGN_CENTER  0x0010
#define SKIN_HALIGN_RIGHT   0x0020


/// base skinned item class
class CRSkinnedItem : public LVRefCounter
{
protected:
    lUInt32 _textcolor;
    lUInt32 _bgcolor;
    LVImageSourceRef _bgimage;
    lvPoint _bgimagesplit;
    lString16 _fontFace;
    int _fontSize;
    bool _fontBold;
    bool _fontItalic;
    LVFontRef _font;
    int _textAlign;
public:
    CRSkinnedItem();
    virtual int getTextAlign() { return _textAlign; }
    virtual int getTextVAlign() { return _textAlign & SKIN_VALIGN_MASK; }
    virtual int getTextHAlign() { return _textAlign & SKIN_HALIGN_MASK; }
    virtual void setTextAlign( int align ) { _textAlign = align; }
    virtual void setTextVAlign( int align ) { _textAlign = (_textAlign & ~SKIN_VALIGN_MASK ) | (align & SKIN_VALIGN_MASK); }
    virtual void setTextHAlign( int align ) { _textAlign = (_textAlign & ~SKIN_HALIGN_MASK ) | (align & SKIN_HALIGN_MASK); }
    virtual lUInt32 getTextColor() { return _textcolor; }
    virtual lUInt32 getBackgroundColor() { return _bgcolor; }
    virtual LVImageSourceRef getBackgroundImage() { return _bgimage; }
    virtual lvPoint getBackgroundImageSplit() { return _bgimagesplit; }
    virtual lString16 getFontFace() { return _fontFace; }
    virtual int getFontSize() { return _fontSize; }
    virtual bool getFontBold() { return _fontBold; }
    virtual bool getFontItalic() { return _fontItalic; }
    virtual void setFontFace( lString16 face );
    virtual void setFontSize( int size );
    virtual void setFontBold( bool bold );
    virtual void setFontItalic( bool italic );
    virtual LVFontRef getFont();
    virtual void setTextColor( lUInt32 color ) { _textcolor = color; }
    virtual void setBackgroundColor( lUInt32 color ) { _bgcolor = color; }
    virtual void setBackgroundImage( LVImageSourceRef img ) { _bgimage = img; }
    virtual void setBackgroundImageSplit( lvPoint pt ) { _bgimagesplit = pt; }
    virtual void setFont( LVFontRef fnt ) { _font = fnt; }
    virtual void draw( LVDrawBuf & buf, const lvRect & rc );
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text, LVFontRef font, lUInt32 textColor, lUInt32 bgColor, int flags );
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text, LVFontRef font )
    {
        drawText(  buf, rc, text, font, getTextColor(), getBackgroundColor(), getTextAlign() );
    }
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text )
    {
        drawText(  buf, rc, text, LVFontRef(), getTextColor(), getBackgroundColor(), getTextAlign() );
    }
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text, lUInt32 color )
    {
        drawText(  buf, rc, text, LVFontRef(), color, getBackgroundColor(), getTextAlign() );
    }
    /// measures text string using current font
    virtual lvPoint measureText( lString16 text );
    virtual ~CRSkinnedItem() { }
};

class CRRectSkin : public CRSkinnedItem
{
protected:
    lvRect _margins;
    lvPoint _minsize;
    lvPoint _maxsize;
public:
    /// same as measureText, but with added margins and minSize applied
    virtual lvPoint measureTextItem( lString16 text );
    CRRectSkin();
    virtual ~CRRectSkin() { }
    virtual lvPoint getMinSize() { return _minsize; }
    virtual lvPoint getMaxSize() { return _maxsize; }
    virtual void setMinSize( lvPoint sz ) { _minsize = sz; }
    virtual void setMaxSize( lvPoint sz ) { _maxsize = sz; }
    virtual void setBorderWidths( const lvRect & rc) { _margins = rc; }
    virtual lvRect getBorderWidths() { return _margins; }
    virtual lvRect getClientRect( const lvRect &windowRect );
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text );
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text, LVFontRef font );
};
typedef LVFastRef<CRRectSkin> CRRectSkinRef;


class CRButtonSkin : public CRRectSkin
{
protected:
    LVImageSourceRef _normalimage;
    LVImageSourceRef _disabledimage;
    LVImageSourceRef _pressedimage;
    LVImageSourceRef _selectedimage;
public:
    enum {
        ENABLED = 1,
        PRESSED = 2,
        SELECTED = 4
    };
    LVImageSourceRef getNormalImage() { return _normalimage; }
    LVImageSourceRef getDisabledImage() { return _disabledimage; }
    LVImageSourceRef getPressedImage() { return _pressedimage; }
    LVImageSourceRef getSelectedImage() { return _selectedimage; }
    void setNormalImage( LVImageSourceRef img ) { _normalimage = img; }
    void setDisabledImage( LVImageSourceRef img ) { _disabledimage = img; }
    void setPressedImage( LVImageSourceRef img ) { _pressedimage = img; }
    void setSelectedImage( LVImageSourceRef img ) { _selectedimage = img; }
    virtual void drawButton( LVDrawBuf & buf, const lvRect & rc, int flags = ENABLED );
    CRButtonSkin();
    virtual ~CRButtonSkin() { }
};
typedef LVFastRef<CRButtonSkin> CRButtonSkinRef;


class CRScrollSkin : public CRRectSkin
{
protected:
    CRButtonSkinRef _upButton;
    CRButtonSkinRef _downButton;
    CRButtonSkinRef _leftButton;
    CRButtonSkinRef _rightButton;
    LVImageSourceRef _hBody;
    LVImageSourceRef _hSlider;
    LVImageSourceRef _vBody;
    LVImageSourceRef _vSlider;
public:
    CRButtonSkinRef getUpButton() { return _upButton; }
    CRButtonSkinRef getDownButton() { return _downButton; }
    CRButtonSkinRef getLeftButton() { return _leftButton; }
    CRButtonSkinRef getRightButton() { return _rightButton; }
    void setUpButton( CRButtonSkinRef btn ) { _upButton = btn; }
    void setDownButton( CRButtonSkinRef btn ) { _downButton = btn; }
    void setLeftButton( CRButtonSkinRef btn ) { _leftButton = btn; }
    void setRightButton( CRButtonSkinRef btn ) { _rightButton = btn; }
    LVImageSourceRef getHBody() { return _hBody; }
    LVImageSourceRef getHSlider() { return _hSlider; }
    LVImageSourceRef getVBody() { return _vBody; }
    LVImageSourceRef getVSlider() { return _vSlider; }
    void setHBody( LVImageSourceRef img ) { _hBody = img; }
    void setHSlider( LVImageSourceRef img ) { _hSlider = img; }
    void setVBody( LVImageSourceRef img ) { _vBody = img; }
    void setVSlider( LVImageSourceRef img ) { _vSlider = img; }
    virtual void drawScroll( LVDrawBuf & buf, const lvRect & rc, bool vertical, int pos, int maxpos, int pagesize );
    CRScrollSkin();
    virtual ~CRScrollSkin() { }
};
typedef LVFastRef<CRScrollSkin> CRScrollSkinRef;


class CRWindowSkin : public CRRectSkin
{
protected:
    lvPoint _titleSize;
    CRRectSkinRef _titleSkin;
    CRRectSkinRef _clientSkin;
    CRScrollSkinRef _scrollSkin;
public:
    CRWindowSkin();
    virtual ~CRWindowSkin() { }
    /// returns necessary window size for specified client size
    CRScrollSkinRef getScrollSkin() { return _scrollSkin; }
    void setScrollSkin( CRScrollSkinRef v ) { _scrollSkin = v; }
    virtual lvPoint getWindowSize( const lvPoint & clientSize );
    virtual lvPoint getTitleSize() { return _titleSize; }
    virtual void setTitleSize( lvPoint sz ) { _titleSize = sz; }
    virtual lvRect getTitleRect( const lvRect &windowRect );
    virtual lvRect getClientRect( const lvRect &windowRect );
    virtual CRRectSkinRef getTitleSkin() { return _titleSkin; }
    virtual void setTitleSkin( CRRectSkinRef skin ) { _titleSkin = skin; }
    virtual CRRectSkinRef getClientSkin() { return _clientSkin; }
    virtual void setClientSkin( CRRectSkinRef skin ) { _clientSkin = skin; }
};
typedef LVFastRef<CRWindowSkin> CRWindowSkinRef;

class CRMenuSkin : public CRWindowSkin
{
protected:
    CRRectSkinRef _itemSkin;
    CRRectSkinRef _itemShortcutSkin;
    CRRectSkinRef _selItemSkin;
    CRRectSkinRef _selItemShortcutSkin;
public:
    CRMenuSkin();
    virtual ~CRMenuSkin() { }
    virtual CRRectSkinRef getItemSkin() { return _itemSkin; }
    virtual void setItemSkin( CRRectSkinRef skin ) { _itemSkin = skin; }
    virtual CRRectSkinRef getItemShortcutSkin() { return _itemShortcutSkin; }
    virtual void setItemShortcutSkin( CRRectSkinRef skin ) { _itemShortcutSkin = skin; }
    virtual CRRectSkinRef getSelItemSkin() { return _selItemSkin; }
    virtual void setSelItemSkin( CRRectSkinRef skin ) { _selItemSkin = skin; }
    virtual CRRectSkinRef getSelItemShortcutSkin() { return _selItemShortcutSkin; }
    virtual void setSelItemShortcutSkin( CRRectSkinRef skin ) { _selItemShortcutSkin = skin; }
};
typedef LVFastRef<CRMenuSkin> CRMenuSkinRef;



/// Base skin class
class CRSkinContainer : public LVRefCounter
{
protected:
    virtual bool readRectSkin(  const lChar16 * path, CRRectSkin * res );
    virtual bool readButtonSkin(  const lChar16 * path, CRButtonSkin * res );
    virtual bool readScrollSkin(  const lChar16 * path, CRScrollSkin * res );
    virtual bool readWindowSkin(  const lChar16 * path, CRWindowSkin * res );
    virtual bool readMenuSkin(  const lChar16 * path, CRMenuSkin * res );
public:
    /// retuns path to base definition, if attribute base="#nodeid" is specified for element of path
    virtual lString16 getBasePath( const lChar16 * path );
    /// find path by id
    virtual lString16 pathById( const lChar16 * id ) = 0;
    /// gets image from container
    virtual LVImageSourceRef getImage( const lChar16 * filename ) = 0;
    /// gets image from container
    virtual LVImageSourceRef getImage( const lString16 & filename ) { return getImage( filename.c_str() ); }
    /// gets doc pointer by path
    virtual ldomXPointer getXPointer( const lString16 & xPointerStr ) = 0;
    /// gets doc pointer by path
    virtual ldomXPointer getXPointer( const lChar16 * xPointerStr ) { return getXPointer( lString16(xPointerStr) ); }
    /// reads int value from attrname attribute of element specified by path, returns defValue if not found
    virtual int readInt( const lChar16 * path, const lChar16 * attrname, int defValue, bool * res=NULL );
    /// reads boolean value from attrname attribute of element specified by path, returns defValue if not found
    virtual bool readBool( const lChar16 * path, const lChar16 * attrname, bool defValue, bool * res=NULL );
    /// reads h align value from attrname attribute of element specified by path, returns defValue if not found
    virtual int readHAlign( const lChar16 * path, const lChar16 * attrname, int defValue, bool * res=NULL );
    /// reads h align value from attrname attribute of element specified by path, returns defValue if not found
    virtual int readVAlign( const lChar16 * path, const lChar16 * attrname, int defValue, bool * res=NULL );
    /// reads string value from attrname attribute of element specified by path, returns empty string if not found
    virtual lString16 readString( const lChar16 * path, const lChar16 * attrname, bool * res=NULL );
    /// reads string value from attrname attribute of element specified by path, returns defValue if not found
    virtual lString16 readString( const lChar16 * path, const lChar16 * attrname, const lString16 & defValue, bool * res=NULL );
    /// reads color value from attrname attribute of element specified by path, returns defValue if not found
    virtual lUInt32 readColor( const lChar16 * path, const lChar16 * attrname, lUInt32 defValue, bool * res=NULL );
    /// reads rect value from attrname attribute of element specified by path, returns defValue if not found
    virtual lvRect readRect( const lChar16 * path, const lChar16 * attrname, lvRect defValue, bool * res=NULL );
    /// reads point(size) value from attrname attribute of element specified by path, returns defValue if not found
    virtual lvPoint readSize( const lChar16 * path, const lChar16 * attrname, lvPoint defValue, bool * res=NULL );
    /// reads rect value from attrname attribute of element specified by path, returns null ref if not found
    virtual LVImageSourceRef readImage( const lChar16 * path, const lChar16 * attrname, bool * res=NULL );


    /// returns rect skin by path or #id
    virtual CRRectSkinRef getRectSkin( const lChar16 * path ) = 0;
    /// returns window skin by path or #id
    virtual CRWindowSkinRef getWindowSkin( const lChar16 * path ) = 0;
    /// returns menu skin by path or #id
    virtual CRMenuSkinRef getMenuSkin( const lChar16 * path ) = 0;

    /// destructor
    virtual ~CRSkinContainer() { }
};

/// skin reference
typedef LVFastRef<CRSkinContainer> CRSkinRef;

/// opens skin from directory or .zip file
CRSkinRef LVOpenSkin( const lString16 & pathname );
/// open simple skin, without image files, from string
CRSkinRef LVOpenSimpleSkin( const lString8 & xml );

#endif// CR_SKIN_INCLUDED
