/*******************************************************

   CoolReader Engine

   lvdocview.cpp:  XML DOM tree rendering tools

   (c) Vadim Lopatin, 2000-2009
   This source code is distributed under the terms of
   GNU General Public License
   See LICENSE file for details

*******************************************************/


#include "../include/fb2def.h"
#include "../include/lvdocview.h"
#include "../include/rtfimp.h"

#include "../include/lvstyles.h"
#include "../include/lvrend.h"
#include "../include/lvstsheet.h"

#include "../include/wolutil.h"
#include "../include/crtxtenc.h"
#include "../include/crtrace.h"

/// to show page bounds rectangles
//#define SHOW_PAGE_RECT

const char * def_stylesheet =
"image { text-align: center; text-indent: 0px } \n"
"empty-line { height: 1em; } \n"
"sub { vertical-align: sub; font-size: 70% }\n"
"sup { vertical-align: super; font-size: 70% }\n"
"body > image, section > image { text-align: center; margin-before: 1em; margin-after: 1em }\n"
"p > image { display: inline }\n"
"a { vertical-align: super; font-size: 80% }\n"
"p { margin-top:0em; margin-bottom: 0em }\n"
"text-author { font-weight: bold; font-style: italic; margin-left: 5%}\n"
"empty-line { height: 1em }\n"
"epigraph { margin-left: 30%; margin-right: 4%; text-align: left; text-indent: 1px; font-style: italic; margin-top: 15px; margin-bottom: 25px; font-family: Times New Roman, serif }\n"
"strong, b { font-weight: bold }\n"
"emphasis, i { font-style: italic }\n"
"title { text-align: center; text-indent: 0px; font-size: 130%; font-weight: bold; margin-top: 10px; margin-bottom: 10px; font-family: Times New Roman, serif }\n"
"subtitle { text-align: center; text-indent: 0px; font-size: 150%; margin-top: 10px; margin-bottom: 10px }\n"
"title { page-break-before: always; page-break-inside: avoid; page-break-after: avoid; }\n"
"body { text-align: justify; text-indent: 2em }\n"
"cite { margin-left: 30%; margin-right: 4%; text-align: justyfy; text-indent: 0px;  margin-top: 20px; margin-bottom: 20px; font-family: Times New Roman, serif }\n"
"td, th { text-indent: 0px; font-size: 80%; margin-left: 2px; margin-right: 2px; margin-top: 2px; margin-bottom: 2px; text-align: left; padding: 5px }\n"
"th { font-weight: bold }\n"
"table > caption { padding: 5px; text-indent: 0px; font-size: 80%; font-weight: bold; text-align: left; background-color: #AAAAAA }\n"
"body[name=\"notes\"] { font-size: 70%; }\n"
"body[name=\"notes\"]  section[id] { text-align: left; }\n"
"body[name=\"notes\"]  section[id] title { display: block; text-align: left; font-size: 110%; font-weight: bold; page-break-before: auto; page-break-inside: auto; page-break-after: auto; }\n"
"body[name=\"notes\"]  section[id] title p { text-align: left; display: inline }\n"
"body[name=\"notes\"]  section[id] empty-line { display: inline }\n"
"code, pre { display: block; white-space: pre; font-family: \"Courier New\", monospace }\n"
;

static const char * DEFAULT_FONT_NAME = "Arial"; //Times New Roman";
static css_font_family_t DEFAULT_FONT_FAMILY = css_ff_sans_serif;
//    css_ff_serif,
//    css_ff_sans_serif,
//    css_ff_cursive,
//    css_ff_fantasy,
//    css_ff_monospace

#ifdef LBOOK
#define INFO_FONT_SIZE      20
#else
#define INFO_FONT_SIZE      18
#endif

#if defined(__SYMBIAN32__)
#include <e32std.h>
#define DEFAULT_PAGE_MARGIN 2
#else
#ifdef LBOOK
#define DEFAULT_PAGE_MARGIN      8
#else
#define DEFAULT_PAGE_MARGIN      12
#endif
#endif

/// minimum EM width of page (prevents show two pages for windows that not enougn wide)
#define MIN_EM_PER_PAGE     20

static int def_font_sizes[] = { 18, 20, 22, 24, 29, 33, 39, 44 };

LVDocView::LVDocView()
: m_dx(400), m_dy(200), m_pos(50), m_battery_state(66)
#if (LBOOK==1)
, m_font_size(32)
#elif defined(__SYMBIAN32__)
, m_font_size(30)
#else
, m_font_size(24)
#endif
, m_status_font_size(INFO_FONT_SIZE)
, m_def_interline_space(130)
, m_font_sizes( def_font_sizes, sizeof(def_font_sizes) / sizeof(int) )
, m_font_sizes_cyclic(false)
, m_is_rendered(false)
, m_view_mode( 1 ? DVM_PAGES : DVM_SCROLL ) // choose 0/1
/*
, m_drawbuf(100, 100
#if COLOR_BACKBUFFER==0
            , GRAY_BACKBUFFER_BITS
#endif
            )
*/
, m_stream(NULL), m_doc(NULL)
, m_stylesheet( def_stylesheet )
, m_pageMargins(DEFAULT_PAGE_MARGIN, DEFAULT_PAGE_MARGIN/2 + INFO_FONT_SIZE + 4, DEFAULT_PAGE_MARGIN, DEFAULT_PAGE_MARGIN / 2)
, m_pagesVisible(2)
, m_pageHeaderInfo (
      PGHDR_PAGE_NUMBER
#ifndef LBOOK
    | PGHDR_CLOCK
#endif
    | PGHDR_BATTERY
    | PGHDR_PAGE_COUNT
    | PGHDR_AUTHOR
    | PGHDR_TITLE)
, m_showCover(true)
, m_rotateAngle(CR_ROTATE_ANGLE_0)
, m_section_bounds_valid(false)
, m_posIsSet(false)
, m_doc_format(doc_format_none)
, m_callback(NULL)
, m_swapDone(false)
{
#if (COLOR_BACKBUFFER==1)
    m_backgroundColor = 0xFFFFE0;
    m_textColor = 0x000060;
#else
#if (GRAY_INVERSE==1)
    m_backgroundColor = 0;
    m_textColor = 3;
#else
    m_backgroundColor = 3;
    m_textColor = 0;
#endif
#endif
    m_defaultFontFace = lString8(DEFAULT_FONT_NAME);
    m_statusFontFace = lString8(DEFAULT_FONT_NAME);
    m_props = LVCreatePropsContainer();
    m_doc_props = LVCreatePropsContainer();
    propsUpdateDefaults(m_props);

    //m_drawbuf.Clear(m_backgroundColor);
    createDefaultDocument( lString16(L"No document"), lString16(L"Welcome to CoolReader! Please select file to open") );

    m_font = fontMan->GetFont( m_font_size, 300, false, DEFAULT_FONT_FAMILY, m_defaultFontFace );
    m_infoFont = fontMan->GetFont( m_status_font_size, 300, false, DEFAULT_FONT_FAMILY, m_statusFontFace );

}

LVDocView::~LVDocView()
{
    Clear();
}

/// get text format options
txt_format_t LVDocView::getTextFormatOptions()
{
    return m_doc->getDocFlag(DOC_FLAG_PREFORMATTED_TEXT) ? txt_format_pre : txt_format_auto;
}

/// set text format options
void LVDocView::setTextFormatOptions( txt_format_t fmt )
{
    txt_format_t m_text_format = getTextFormatOptions();
    CRLog::trace( "setTextFormatOptions( %d ), current state = %d", (int)fmt, (int)m_text_format );
    if ( m_text_format == fmt )
        return; // no change
    m_props->setBool( PROP_TXT_OPTION_PREFORMATTED, ( fmt == txt_format_pre ) );
    m_doc->setDocFlag( DOC_FLAG_PREFORMATTED_TEXT, ( fmt == txt_format_pre ) );
    if ( getDocFormat() == doc_format_txt ) {
        requestReload();
        CRLog::trace( "setTextFormatOptions() -- new value set, reload requested" );
    } else {
        CRLog::trace( "setTextFormatOptions() -- doc format is %d, reload is necessary for %d only", (int)getDocFormat(), (int)doc_format_txt );
    }
}

/// invalidate document data, request reload
void LVDocView::requestReload()
{
    if ( getDocFormat() != doc_format_txt )
        return; // supported for text files only
    if ( m_callback ) {
            m_callback->OnLoadFileStart( m_doc_props->getStringDef( DOC_PROP_FILE_NAME, "" ) );
    }
    if ( m_stream.isNull() && isDocumentOpened() ) {
        savePosition();
        CRFileHist * hist = getHistory();
        if ( !hist || hist->getRecords().length()<=0 )
            return;
        lString16 fn = hist->getRecords()[0]->getFilePathName();
        bool res = LoadDocument( fn.c_str() );
        if ( res ) {
            //swapToCache();
            restorePosition();
        } else {
            createDefaultDocument( lString16(), lString16("Error while opening document ") + fn );
        }
        checkRender();
        return;
    }
    ParseDocument( );
    // TODO: save position
    checkRender();
}

/// returns true if document is opened
bool LVDocView::isDocumentOpened()
{
    return m_doc && m_doc->getRootNode() && !m_doc_props->getStringDef(DOC_PROP_FILE_NAME,"").empty();
}

/// rotate rectangle by current angle, winToDoc==false for doc->window translation, true==ccw
lvRect LVDocView::rotateRect( lvRect & rc, bool winToDoc )
{
    lvRect rc2;
    cr_rotate_angle_t angle = m_rotateAngle;
    if ( winToDoc )
        angle = (cr_rotate_angle_t)((4 - (int)angle) & 3);
    switch ( angle ) {
    case CR_ROTATE_ANGLE_0:
        rc2 = rc;
        break;
    case CR_ROTATE_ANGLE_90:
        /*
          . . . . . .      . . . . . . . .
          . . . . . .      . . . . . 1 . .
          . 1 . . . .      . . . . . . . .
          . . . . . .  ==> . . . . . . . .
          . . . . . .      . 2 . . . . . .
          . . . . . .      . . . . . . . .
          . . . . 2 .
          . . . . . .

        */
        rc2.left = m_dy - rc.bottom - 1;
        rc2.right = m_dy - rc.top - 1;
        rc2.top = rc.left;
        rc2.bottom = rc.right;
        break;
    case CR_ROTATE_ANGLE_180:
        rc2.left = m_dx - rc.left - 1;
        rc2.right = m_dx - rc.right - 1;
        rc2.top = m_dy - rc.top - 1;
        rc2.bottom = m_dy - rc.bottom - 1;
        break;
    case CR_ROTATE_ANGLE_270:
        /*
          . . . . . .      . . . . . . . .
          . . . . . .      . 1 . . . . . .
          . . . . 2 .      . . . . . . . .
          . . . . . .  <== . . . . . . . .
          . . . . . .      . . . . . 2 . .
          . . . . . .      . . . . . . . .
          . 1 . . . .
          . . . . . .

        */
        rc2.left = rc.top;
        rc2.right = rc.bottom;
        rc2.top = m_dx - rc.right - 1;
        rc2.bottom = m_dx - rc.left - 1;
        break;
    }
    return rc2;
}

/// rotate point by current angle, winToDoc==false for doc->window translation, true==ccw
lvPoint LVDocView::rotatePoint( lvPoint & pt, bool winToDoc )
{
    lvPoint pt2;
    cr_rotate_angle_t angle = m_rotateAngle;
    if ( winToDoc )
        angle = (cr_rotate_angle_t)((4 - (int)angle) & 3);
    switch ( angle ) {
    case CR_ROTATE_ANGLE_0:
        pt2 = pt;
        break;
    case CR_ROTATE_ANGLE_90:
        /*
          . . . . . .      . . . . . . . .
          . . . . . .      . . . . . 1 . .
          . 1 . . . .      . . . . . . . .
          . . . . . .  ==> . . . . . . . .
          . . . . . .      . 2 . . . . . .
          . . . . . .      . . . . . . . .
          . . . . 2 .
          . . . . . .

        */
        pt2.y = pt.x;
        pt2.x = m_dx - pt.y - 1;
        break;
    case CR_ROTATE_ANGLE_180:
        pt2.y = m_dy - pt.y - 1;
        pt2.x = m_dx - pt.x - 1;
        break;
    case CR_ROTATE_ANGLE_270:
        /*
          . . . . . .      . . . . . . . .
          . . . . . .      . 1 . . . . . .
          . . . . 2 .      . . . . . . . .
          . . . . . .  <== . . . . . . . .
          . . . . . .      . . . . . 2 . .
          . . . . . .      . . . . . . . .
          . 1 . . . .
          . . . . . .

        */
        pt2.y = m_dy - pt.x - 1;
        pt2.x = pt.y;
        break;
    }
    return pt2;
}

/// sets page margins
void LVDocView::setPageMargins( const lvRect & rc )
{
    if ( m_pageMargins.left + m_pageMargins.right != rc.left + rc.right
            || m_pageMargins.top + m_pageMargins.bottom != rc.top + rc.bottom )
        requestRender();
    else
        clearImageCache();
    m_pageMargins = rc;
}

void LVDocView::setPageHeaderInfo( int hdrFlags )
{
    if ( m_pageHeaderInfo == hdrFlags )
        return;
    LVLock lock(getMutex());
    int oldH = getPageHeaderHeight();
    m_pageHeaderInfo = hdrFlags;
    int h = getPageHeaderHeight();
    if ( h != oldH ) {
        requestRender();
    } else {
        clearImageCache();
    }
}

/// set document stylesheet text
void LVDocView::setStyleSheet( lString8 css_text )
{
    LVLock lock(getMutex());
    requestRender();
    m_stylesheet = css_text;
}

void LVDocView::Clear()
{
    {
        LVLock lock(getMutex());
        if (m_doc)
            delete m_doc;
        m_doc = NULL;
        m_doc_props->clear();
        m_toc.clear();
        if (!m_stream.isNull())
            m_stream.Clear();
        if ( !m_container.isNull() )
            m_container.Clear();
        if (!m_arc.isNull())
            m_arc.Clear();
        _posBookmark = ldomXPointer();
        m_is_rendered = false;
        m_swapDone = false;
        m_pos = 0;
        m_cursorPos.clear();
        m_filename.clear();
        m_section_bounds_valid = false;
    }
    m_imageCache.clear();
    _navigationHistory.clear();
}

/// invalidate image cache, request redraw
void LVDocView::clearImageCache()
{
    m_imageCache.clear();
}

/// invalidate formatted data, request render
void LVDocView::requestRender()
{
    m_is_rendered = false;
    clearImageCache();
    m_doc->clearRendBlockCache();
}

/// render document, if not rendered
void LVDocView::checkRender()
{
    if ( !m_is_rendered ) {
        LVLock lock(getMutex());
        Render();
        m_imageCache.clear();
        m_is_rendered = true;
        m_posIsSet = false;
    }
}

/// ensure current position is set to current bookmark value
void LVDocView::checkPos()
{
    checkRender();
    if ( m_posIsSet )
        return;
    m_posIsSet = true;
    LVLock lock(getMutex());
    if ( _posBookmark.isNull() ) {
        SetPos( 0, false );
    } else {
        //CRLog::trace("checkPos() _posBookmark node=%08X offset=%d", (unsigned)_posBookmark.getNode(), (int)_posBookmark.getOffset());
        lvPoint pt = _posBookmark.toPoint();
        SetPos( pt.y, false );
    }
}

/// returns true if current page image is ready
bool LVDocView::IsDrawed()
{
    return isPageImageReady( 0 );
}

/// returns true if page image is available (0=current, -1=prev, 1=next)
bool LVDocView::isPageImageReady( int delta )
{
    if ( !m_is_rendered || !m_posIsSet )
        return false;
    int offset = m_pos;
    if ( delta<0 )
        offset = getPrevPageOffset();
    else if ( delta>0 )
        offset = getNextPageOffset();
    //CRLog::trace("getPageImage: checking cache for page [%d] (delta=%d)", offset, delta);
    LVDocImageRef ref = m_imageCache.get( offset );
    return ( !ref.isNull() );
}

/// get page image
LVDocImageRef LVDocView::getPageImage( int delta )
{
    checkPos();
    // find existing object in cache
    int offset = m_pos;
    if ( delta<0 )
        offset = getPrevPageOffset();
    else if ( delta>0 )
        offset = getNextPageOffset();
    //CRLog::trace("getPageImage: checking cache for page [%d] (delta=%d)", offset, delta);
    LVDocImageRef ref = m_imageCache.get( offset );
    if ( !ref.isNull() ) {
        //CRLog::trace("getPageImage: + page [%d] found in cache", offset);
        return ref;
    }
    while ( ref.isNull() ) {
        CRLog::trace("getPageImage: - page [%d] not found, force rendering", offset);
        cachePageImage( delta );
        ref = m_imageCache.get( offset );
    }
    //CRLog::trace("getPageImage: page [%d] is ready", offset);
    return ref;
}

class LVDrawThread : public LVThread {
    LVDocView * _view;
    int _offset;
    LVRef<LVDrawBuf> _drawbuf;
public:
    LVDrawThread( LVDocView * view, int offset, LVRef<LVDrawBuf> drawbuf )
    : _view(view), _offset(offset), _drawbuf(drawbuf)
    {
        start();
    }
    virtual void run()
    {
        CRLog::trace("LVDrawThread::run() offset==%d", _offset);
        _view->Draw( *_drawbuf, _offset, true );
        //_drawbuf->Rotate( _view->GetRotateAngle() );
    }
};
/// cache page image (render in background if necessary)
void LVDocView::cachePageImage( int delta )
{
    int offset = m_pos;
    if ( delta<0 )
        offset = getPrevPageOffset();
    else if ( delta>0 )
        offset = getNextPageOffset();
    //CRLog::trace("cachePageImage: request to cache page [%d] (delta=%d)", offset, delta);
    if ( m_imageCache.has(offset) ) {
        //CRLog::trace("cachePageImage: Page [%d] is found in cache", offset);
        return;
    }
    //CRLog::trace("cachePageImage: starting new render task for page [%d]", offset);
#if (COLOR_BACKBUFFER==1)
    LVRef<LVDrawBuf> drawbuf( new LVColorDrawBuf( m_dx, m_dy ) );
#else
    LVRef<LVDrawBuf> drawbuf( new LVGrayDrawBuf( m_dx, m_dy, GRAY_BACKBUFFER_BITS ) );
#endif
    LVRef<LVThread> thread( new LVDrawThread( this, offset, drawbuf ) );
    m_imageCache.set( offset, drawbuf, thread );
    //CRLog::trace("cachePageImage: caching page [%d] is finished", offset);
}

bool LVDocView::exportWolFile( const char * fname, bool flgGray, int levels )
{
    LVStreamRef stream = LVOpenFileStream(fname, LVOM_WRITE);
    if (!stream)
        return false;
	return exportWolFile( stream.get(), flgGray, levels );
}

bool LVDocView::exportWolFile( const wchar_t * fname, bool flgGray, int levels )
{
    LVStreamRef stream = LVOpenFileStream(fname, LVOM_WRITE);
    if (!stream)
        return false;
	return exportWolFile( stream.get(), flgGray, levels );
}

lString16 getSectionHeader( ldomNode * section )
{
    lString16 header;
    if ( !section || section->getChildCount() == 0 )
        return header;
    ldomNode * child = section->getChildNode(0);
    if ( !child->isElement() || child->getNodeName()!=L"title" )
        return header;
    header = child->getText(L' ');
    return header;
}


void dumpSection( ldomNode * elem )
{
    lvRect rc;
    elem->getAbsRect(rc);
    //fprintf( log.f, "rect(%d, %d, %d, %d)  ", rc.left, rc.top, rc.right, rc.bottom );
}

int getSectionPage( ldomNode * section, LVRendPageList & pages )
{
    if ( !section )
        return -1;
#if 1
    int y = ldomXPointer( section, 0 ).toPoint().y;
#else
    lvRect rc;
    section->getAbsRect(rc);
    int y = rc.top;
#endif
    int page = -1;
    if ( y>=0 ) {
        page = pages.FindNearestPage( y, -1 );
        //dumpSection( section );
        //fprintf(log.f, "page %d: %d->%d..%d\n", page+1, y, pages[page].start, pages[page].start+pages[page].height );
    }
    return page;
}

static void addTocItems( ldomNode * basesection, LVTocItem * parent )
{
    if ( !basesection || !parent )
        return;
    lString16 name = getSectionHeader( basesection );
    if ( name.empty() )
        return; // section without header
    ldomXPointer ptr( basesection, 0 );
    LVTocItem * item = parent->addChild( name, ptr );
    for ( int i=0; ;i++ ) {
        ldomNode * section = basesection->findChildElement( LXML_NS_ANY, el_section, i );
        if ( !section )
            break;
        addTocItems( section, item );
    }
}

/// returns Y position
int LVTocItem::getY()
{
    return _position.toPoint().y;
}

/// returns page number
int LVTocItem::getPageNum( LVRendPageList & pages )
{
    return getSectionPage( _position.getNode(), pages );
}

/// update page numbers for items
void LVTocItem::updatePageNumbers( LVDocView * docview )
{
    if ( !_position.isNull() ) {
        lvPoint p = _position.toPoint();
        int y = p.y;
        int h = docview->GetFullHeight();
        int page = docview->getBookmarkPage( _position );
        if ( page>=0 && page < docview->getPageCount() )
            _page = page;
        else
            _page = -1;
        if ( y >=0 && y < h && h > 0 )
            _percent = (int) ( (lInt64)y * 10000 / h ); // % * 100
        else
            _percent = -1;
    } else {
        // unknown position
        _page = -1;
        _percent = -1;
    }
    for ( int i = 0; i<getChildCount(); i++ ) {
        getChild(i)->updatePageNumbers( docview );
    }
}

LVTocItem * LVDocView::getToc()
{
    m_toc.updatePageNumbers( this );
    return &m_toc;
}

void LVDocView::makeToc()
{
    m_toc.clear();
    ldomNode * body = m_doc->getRootNode();
    if ( !body )
        return;
    body = body->findChildElement( LXML_NS_ANY, el_FictionBook, -1 );
	if ( body )
		body = body->findChildElement( LXML_NS_ANY, el_body, 0 );
    if ( !body )
        return;
    for ( int i=0; ;i++ ) {
        ldomNode * section = body->findChildElement( LXML_NS_ANY, el_section, i );
        if ( !section )
            break;
        addTocItems( section, &m_toc );
    }
}

/// returns cover page image source, if any
LVImageSourceRef LVDocView::getCoverPageImage()
{
	lUInt16 path[] = { el_FictionBook, el_description, el_title_info, el_coverpage, el_image, 0 };
    ldomNode * cover_img_el = m_doc->getRootNode()->findChildElement( path );

    if ( cover_img_el )
    {
        LVImageSourceRef imgsrc = cover_img_el->getObjectImageSource();
        return imgsrc;
    }
    return LVImageSourceRef(); // not found: return NULL ref
}


/// draws coverpage to image buffer
void LVDocView::drawCoverTo( LVDrawBuf * drawBuf, lvRect & rc )
{
    if ( rc.width()<130 || rc.height()<130)
        return;
    int base_font_size = 16;
    int w = rc.width();
    if ( w<200 )
        base_font_size = 16;
    else if ( w<300 )
        base_font_size = 18;
    else if ( w<500 )
        base_font_size = 20;
    else if ( w<700 )
        base_font_size = 22;
    else
        base_font_size = 24;
    CRLog::trace("drawCoverTo() - loading fonts...");
    LVFontRef author_fnt( fontMan->GetFont( base_font_size, 600, false, css_ff_serif, lString8("Times New Roman")) );
    LVFontRef title_fnt( fontMan->GetFont( base_font_size+4, 600, false, css_ff_serif, lString8("Times New Roman")) );
    LVFontRef series_fnt( fontMan->GetFont( base_font_size-3, 300, true, css_ff_serif, lString8("Times New Roman")) );
    lString16 authors = getAuthors();
    lString16 title = getTitle();
    lString16 series = getSeries();
    if ( title.empty() )
        title = L"no title";
    LFormattedText txform;
    if ( !authors.empty() )
        txform.AddSourceLine( authors.c_str(), authors.length(), 0xFFFFFFFF, 0xFFFFFFFF, author_fnt.get(), LTEXT_ALIGN_CENTER, 18 );
    txform.AddSourceLine( title.c_str(), title.length(), 0xFFFFFFFF, 0xFFFFFFFF, title_fnt.get(), LTEXT_ALIGN_CENTER, 18 );
    if ( !series.empty() )
        txform.AddSourceLine( series.c_str(), series.length(), 0xFFFFFFFF, 0xFFFFFFFF, series_fnt.get(), LTEXT_ALIGN_CENTER, 18 );
    int title_w = rc.width() - rc.width()/4;
    int h = txform.Format( title_w, rc.height() );

    lvRect imgrc = rc;
    imgrc.bottom -= h + 16;

    CRLog::trace("drawCoverTo() - getting cover image");
    LVImageSourceRef imgsrc = getCoverPageImage();
    LVImageSourceRef defcover = getDefaultCover();
    if ( !imgsrc.isNull() && imgrc.height()>30 )
    {
        //fprintf( stderr, "Writing coverpage image...\n" );
        int src_dx = imgsrc->GetWidth();
        int src_dy = imgsrc->GetHeight();
        int scale_x = imgrc.width() * 0x10000 / src_dx;
        int scale_y = imgrc.height() * 0x10000 / src_dy;
        if ( scale_x < scale_y )
            scale_y = scale_x;
        else
            scale_x = scale_y;
        int dst_dx = (src_dx * scale_x) >> 16;
        int dst_dy = (src_dy * scale_y) >> 16;
        if (dst_dx>rc.width())
            dst_dx = imgrc.width();
        if (dst_dy>rc.height())
            dst_dy = imgrc.height();
            CRLog::trace("drawCoverTo() - drawing image");
            drawBuf->Draw( imgsrc, imgrc.left + (imgrc.width()-dst_dx)/2, imgrc.top + (imgrc.height()-dst_dy)/2, dst_dx, dst_dy );
        //fprintf( stderr, "Done.\n" );
    } else if ( !defcover.isNull() ) {
        // draw default cover with title at center
        imgrc = rc;
        int src_dx = defcover->GetWidth();
        int src_dy = defcover->GetHeight();
        int scale_x = imgrc.width() * 0x10000 / src_dx;
        int scale_y = imgrc.height() * 0x10000 / src_dy;
        if ( scale_x < scale_y )
            scale_y = scale_x;
        else
            scale_x = scale_y;
        int dst_dx = (src_dx * scale_x) >> 16;
        int dst_dy = (src_dy * scale_y) >> 16;
        if (dst_dx>rc.width() - 10)
            dst_dx = imgrc.width();
        if (dst_dy>rc.height() - 10)
            dst_dy = imgrc.height();
        CRLog::trace("drawCoverTo() - drawing image");
        drawBuf->Draw( defcover, imgrc.left + (imgrc.width()-dst_dx)/2, imgrc.top + (imgrc.height()-dst_dy)/2, dst_dx, dst_dy );
        CRLog::trace("drawCoverTo() - drawing text");
        txform.Draw( drawBuf, (rc.right + rc.left - title_w) / 2,
            (rc.bottom + rc.top - h) / 2, NULL );
        CRLog::trace("drawCoverTo() - done");
        return;
    } else {
        imgrc.bottom = imgrc.top;
    }
    rc.top = imgrc.bottom;
    CRLog::trace("drawCoverTo() - drawing text");
    txform.Draw( drawBuf, (rc.right + rc.left - title_w) / 2, (rc.bottom + rc.top - h) / 2, NULL );
    CRLog::trace("drawCoverTo() - done");
}

/// export to WOL format
bool LVDocView::exportWolFile( LVStream * stream, bool flgGray, int levels )
{
    checkRender();
    int save_m_dx = m_dx;
    int save_m_dy = m_dy;
    int old_flags = m_pageHeaderInfo;
    int save_pos = m_pos;

    m_pageHeaderInfo &= ~(PGHDR_CLOCK | PGHDR_BATTERY);
    int dx = 600; // - m_pageMargins.left - m_pageMargins.right;
    int dy = 800; // - m_pageMargins.top - m_pageMargins.bottom;
    Resize( dx, dy );

    LVRendPageList &pages = m_pages;

    //Render(dx, dy, &pages);

    const lChar8 * * table = GetCharsetUnicode2ByteTable( L"windows-1251" );

    //ldomXPointer bm = getBookmark();
    {
        WOLWriter wol(stream);
        lString8 authors = UnicodeTo8Bit( getAuthors(), table );
        lString8 name = UnicodeTo8Bit( getTitle(), table );
        wol.addTitle(
                name,
                lString8("-"),
                authors,
                lString8("-"), //adapter
                lString8("-"), //translator
                lString8("-"), //publisher
                lString8("-"), //2006-11-01
                lString8("-"), //This is introduction.
                lString8("")   //ISBN
        );

        LVGrayDrawBuf cover(600, 800);
        lvRect coverRc( 0, 0, 600, 800 );
        cover.Clear(m_backgroundColor);
        drawCoverTo( &cover, coverRc );
        wol.addCoverImage(cover);

        for ( int i=1; i<pages.length(); i+=getVisiblePageCount() )
        {
            LVGrayDrawBuf drawbuf(600, 800, flgGray ? 2 : 1); //flgGray ? 2 : 1);
            //drawbuf.SetBackgroundColor(0xFFFFFF);
            //drawbuf.SetTextColor(0x000000);
            drawbuf.Clear(m_backgroundColor);
            drawPageTo( &drawbuf, *pages[i], NULL, pages.length(), 0 );
            m_pos = pages[i]->start;
            Draw( drawbuf, m_pos, true );
            if (!flgGray)
                drawbuf.ConvertToBitmap(false);
            else
                drawbuf.Invert();
            wol.addImage(drawbuf);
        }

        // add TOC
        ldomNode * body = m_doc->nodeFromXPath( lString16(L"/FictionBook/body[1]") );
        lUInt16 section_id = m_doc->getElementNameIndex( L"section" );

        if ( body ) {
            int l1n = 0;
            for ( int l1=0; l1<1000; l1++ ) {
                ldomNode * l1section = body->findChildElement(LXML_NS_ANY, section_id, l1);
                if ( !l1section )
                    break;
                lString8 title = UnicodeTo8Bit(getSectionHeader( l1section ), table);
                int page = getSectionPage( l1section, pages );
                if ( !title.empty() && page>=0 ) {
                    wol.addTocItem( ++l1n, 0, 0, page, title );
                    int l2n = 0;
                    if ( levels<2 )
                        continue;
                    for ( int l2=0; l2<1000; l2++ ) {
                        ldomNode * l2section = l1section->findChildElement(LXML_NS_ANY, section_id, l2);
                        if ( !l2section )
                            break;
                        lString8 title = UnicodeTo8Bit(getSectionHeader( l2section ), table);
                        int page = getSectionPage( l2section, pages );
                        if ( !title.empty() && page>=0 ) {
                            wol.addTocItem( l1n, ++l2n, 0, page, title );
                            int l3n = 0;
                            if ( levels<3 )
                                continue;
                            for ( int l3=0; l3<1000; l3++ ) {
                                ldomNode * l3section = l2section->findChildElement(LXML_NS_ANY, section_id, l3);
                                if ( !l3section )
                                    break;
                                lString8 title = UnicodeTo8Bit(getSectionHeader( l3section ), table);
                                int page = getSectionPage( l3section, pages );
                                if ( !title.empty() && page>=0 ) {
                                    wol.addTocItem( l1n, l2n, ++l3n, page, title );
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    m_pageHeaderInfo = old_flags;
    m_pos = save_pos;
    int ndx = (GetRotateAngle()&1) ? save_m_dy : save_m_dx;
    int ndy = (GetRotateAngle()&1) ? save_m_dx : save_m_dy;
    Resize( ndx, ndy );
    clearImageCache();

    return true;
}

void LVDocView::SetPos( int pos, bool savePos )
{
    LVLock lock(getMutex());
    m_posIsSet = true;
    checkRender();
    //if ( m_posIsSet && m_pos==pos )
    //    return;
    if (m_view_mode==DVM_SCROLL)
    {
        if (pos > GetFullHeight() - m_dy )
            pos = GetFullHeight() - m_dy;
        if (pos<0)
            pos = 0;
        m_pos = pos;
    }
    else
    {
        int pc = getVisiblePageCount();
        int page = m_pages.FindNearestPage( pos, 0 );
        if ( pc==2 )
            page &= ~1;
        if (page<m_pages.length())
            m_pos = m_pages[page]->start;
        else
            m_pos = 0;
    }
    if ( savePos )
        _posBookmark = getBookmark();
    m_posIsSet = true;
    updateScroll();
    //Draw();
}

int LVDocView::GetFullHeight()
{
    LVLock lock(getMutex());
    checkRender();
    lvdomElementFormatRec * rd = m_doc ? m_doc->getRootNode()->getRenderData() : NULL;
    return ( rd ? rd->getHeight()+rd->getY() : m_dy );
}

#define HEADER_MARGIN 4
/// calculate page header height
int LVDocView::getPageHeaderHeight( )
{
    if ( !getPageHeaderInfo() )
        return 0;
    return getInfoFont()->getHeight() + HEADER_MARGIN + 3;
}

/// calculate page header rectangle
void LVDocView::getPageHeaderRectangle( int pageIndex, lvRect & headerRc )
{
    lvRect pageRc;
    getPageRectangle( pageIndex, pageRc );
    headerRc = pageRc;
    if ( pageIndex==0 ) {
        headerRc.bottom = 0;
    } else {
        int h = getPageHeaderHeight();
        headerRc.bottom = headerRc.top + h;
        headerRc.top += HEADER_MARGIN;
        headerRc.left += HEADER_MARGIN;
        headerRc.right -= HEADER_MARGIN;
    }
}

/// returns current time representation string
lString16 LVDocView::getTimeString()
{
    time_t t = (time_t)time(0);
    tm * bt = localtime(&t);
    char str[12];
    sprintf(str, "%02d:%02d", bt->tm_hour, bt->tm_min);
    return Utf8ToUnicode( lString8( str ) );
}

/// draw battery state to buffer
void LVDocView::drawBatteryState( LVDrawBuf * drawbuf, const lvRect & batteryRc, bool isVertical )
{
    if ( m_batteryIcons.length()>1 ) {
        int iconIndex = ((m_batteryIcons.length() - 1 ) * m_battery_state + (100/m_batteryIcons.length()/2) )/ 100;
        if ( iconIndex<0 )
            iconIndex = 0;
        if ( iconIndex>m_batteryIcons.length()-1 )
            iconIndex = m_batteryIcons.length()-1;
        LVImageSourceRef icon = m_batteryIcons[iconIndex];
        drawbuf->Draw( icon, (batteryRc.left + batteryRc.right - icon->GetWidth() ) / 2,
            (batteryRc.top + batteryRc.bottom - icon->GetHeight())/2,
            icon->GetWidth(),
            icon->GetHeight() );
    } else {
        lvRect rc =  batteryRc;
        if ( m_battery_state<0 )
            return;
        lUInt32 cl = 0xA0A0A0;
        lUInt32 cl2 = 0xD0D0D0;
        if ( drawbuf->GetBitsPerPixel()<=2 ) {
            cl = 1;
            cl2 = 2;
        }
    #if 1

        if ( isVertical ) {
            int h = rc.height();
            h = ( (h - 4) / 4 * 4 ) + 3;
            int dh = (rc.height() - h) / 2;
            rc.bottom -= dh;
            rc.top = rc.bottom - h;
            int w = rc.width();
            int h0 = 4; //h / 6;
            int w0 = w / 3;
            // contour
            drawbuf->FillRect( rc.left, rc.top+h0, rc.left+1, rc.bottom, cl );
            drawbuf->FillRect( rc.right-1, rc.top+h0, rc.right, rc.bottom, cl );

            drawbuf->FillRect( rc.left, rc.top+h0, rc.left+w0, rc.top+h0+1, cl );
            drawbuf->FillRect( rc.right-w0, rc.top+h0, rc.right, rc.top+h0+1, cl );

            drawbuf->FillRect( rc.left+w0-1, rc.top, rc.left+w0, rc.top+h0, cl );
            drawbuf->FillRect( rc.right-w0, rc.top, rc.right-w0+1, rc.top+h0, cl );

            drawbuf->FillRect( rc.left+w0, rc.top, rc.right-w0, rc.top+1, cl );
            drawbuf->FillRect( rc.left, rc.bottom-1, rc.right, rc.bottom, cl );
            // fill
            int miny = rc.bottom - 2 - (h - 4) * m_battery_state / 100;
            for ( int i=0; i<h-4 ; i++ ) {
                if ( (i&3) != 3 ) {
                    int y = rc.bottom - 2 - i;
                    int w = 2;
                    if ( y < rc.top + h0 + 2 )
                        w = w0 + 1;
                    lUInt32 c = cl2;
                    if ( y >= miny )
                        c = cl;
                    drawbuf->FillRect( rc.left+w, y-1, rc.right-w, y, c );
                }
            }
        } else {
            // horizontal
            int h = rc.width();
            h = ( (h - 4) / 4 * 4 ) + 3;
            int dh = (rc.height() - h) / 2;
            rc.right -= dh;
            rc.left = rc.right - h;
            h = rc.height();
            dh = h - (rc.width() * 4/8 + 1);
            if ( dh>0 ) {
                rc.bottom -= dh/2;
                rc.top += (dh/2);
                h = rc.height();
            }
            int w = rc.width();
            int h0 = h / 3; //h / 6;
            int w0 = 4;
            // contour
            drawbuf->FillRect( rc.left+w0, rc.top, rc.right, rc.top+1, cl );
            drawbuf->FillRect( rc.left+w0, rc.bottom-1, rc.right, rc.bottom, cl );

            drawbuf->FillRect( rc.left+w0, rc.top, rc.left+w0+1, rc.top+h0, cl );
            drawbuf->FillRect( rc.left+w0, rc.bottom-h0, rc.left+w0+1, rc.bottom, cl );

            drawbuf->FillRect( rc.left, rc.top+h0-1, rc.left+w0, rc.top+h0, cl );
            drawbuf->FillRect( rc.left, rc.bottom-h0, rc.left+w0, rc.bottom-h0+1, cl );

            drawbuf->FillRect( rc.left, rc.top+h0, rc.left+1, rc.bottom-h0, cl );
            drawbuf->FillRect( rc.right-1, rc.top, rc.right, rc.bottom, cl );
            // fill
            int minx = rc.right - 2 - (w - 4) * m_battery_state / 100;
            for ( int i=0; i<w-4 ; i++ ) {
                if ( (i&3) != 3 ) {
                    int x = rc.right - 2 - i;
                    int h = 2;
                    if ( x < rc.left + w0 + 2 )
                        h = h0 + 1;
                    lUInt32 c = cl2;
                    if ( x >= minx )
                        c = cl;
                    drawbuf->FillRect( x-1, rc.top+h, x, rc.bottom-h, c );
                }
            }
        }
    #else
        //lUInt32 cl = getTextColor();
        int h = rc.height() / 6;
        if ( h<5 )
            h = 5;
        int n = rc.height() / h;
        int dy = rc.height() % h / 2;
        if ( n<1 )
            n = 1;
        int k = m_battery_state * n / 100;
        for ( int i=0; i<n; i++ ) {
            lvRect rrc = rc;
            rrc.bottom -= h * i + dy;
            rrc.top = rrc.bottom - h + 1;
            int dx = (i<n-1) ? 0 : rc.width()/5;
            rrc.left += dx;
            rrc.right -= dx;
            if ( i<k ) {
                // full
                drawbuf->FillRect( rrc.left, rrc.top, rrc.right, rrc.bottom, cl );
            } else {
                // empty
                drawbuf->FillRect( rrc.left, rrc.top, rrc.right, rrc.top+1, cl );
                drawbuf->FillRect( rrc.left, rrc.bottom-1, rrc.right, rrc.bottom, cl );
                drawbuf->FillRect( rrc.left, rrc.top, rrc.left+1, rrc.bottom, cl );
                drawbuf->FillRect( rrc.right-1, rrc.top, rrc.right, rrc.bottom, cl );
            }
        }
    #endif
    }
}

/// returns section bounds, in 1/100 of percent
LVArray<int> & LVDocView::getSectionBounds( )
{
    if ( m_section_bounds_valid )
        return m_section_bounds;
    m_section_bounds.clear();
    m_section_bounds.add(0);
    ldomNode * body = m_doc->nodeFromXPath( lString16(L"/FictionBook/body[1]") );
    lUInt16 section_id = m_doc->getElementNameIndex( L"section" );
    int fh = GetFullHeight();
    if ( body && fh>0 ) {
        for ( int l1=0; l1<1000; l1++) {
            ldomNode * l1section = body->findChildElement(LXML_NS_ANY, section_id, l1);
            if ( !l1section )
                break;
            lvRect rc;
            l1section->getAbsRect( rc );
            int p = (int)(((lInt64)rc.top * 10000) / fh);
            m_section_bounds.add( p );
        }
    }
    m_section_bounds.add(10000);
    m_section_bounds_valid = true;
    return m_section_bounds;
}

int LVDocView::getPosPercent()
{
    LVLock lock(getMutex());
    checkPos();
    if ( getViewMode() == DVM_SCROLL ) {
        int fh = GetFullHeight();
        int p = GetPos();
        if ( fh>0 )
            return (int)(((lInt64)p * 10000) / fh);
        else
            return 0;
    } else {
        int fh = getPageCount();
        int p = getCurPage() + 1;
        if ( fh>0 )
            return (int)(((lInt64)p * 10000) / fh);
        else
            return 0;
    }
}

void LVDocView::getPageRectangle( int pageIndex, lvRect & pageRect )
{
    if ( (pageIndex & 1)==0 || (getVisiblePageCount()<2) )
        pageRect = m_pageRects[0];
    else
        pageRect = m_pageRects[1];
}

void LVDocView::getNavigationBarRectangle( lvRect & navRect )
{
    getNavigationBarRectangle( getVisiblePageCount()==2 ? 1 : 2, navRect );
}

void LVDocView::getNavigationBarRectangle( int pageIndex, lvRect & navRect )
{
    lvRect headerRect;
    getPageHeaderRectangle( pageIndex, headerRect );
    navRect = headerRect;
    if ( headerRect.bottom <= headerRect.top )
        return;
    navRect.top = navRect.bottom - 6;
}

void LVDocView::drawNavigationBar( LVDrawBuf * drawbuf, int pageIndex, int percent )
{
    //LVArray<int> & sbounds = getSectionBounds();
    lvRect navBar;
    getNavigationBarRectangle( pageIndex, navBar );
    //bool leftPage = (getVisiblePageCount()==2 && !(pageIndex&1) );

    //lUInt32 cl1 = 0xA0A0A0;
    //lUInt32 cl2 = getBackgroundColor();
}

/// set list of battery icons to display battery state
void LVDocView::setBatteryIcons( LVRefVec<LVImageSource> icons )
{
    m_batteryIcons = icons;
}

lString16 fitTextWidthWithEllipsis( lString16 text, LVFontRef font, int maxwidth )
{
    int w = font->getTextWidth( text.c_str(), text.length() );
    if ( w <= maxwidth )
        return text;
    int len;
    for ( len = text.length()-1; len>1; len-- ) {
        lString16 s = text.substr(0, len) + L"...";
        w = font->getTextWidth( s.c_str(), s.length() );
        if ( w <= maxwidth )
            return s;
    }
    return lString16();
}

/// draw page header to buffer
void LVDocView::drawPageHeader( LVDrawBuf * drawbuf, const lvRect & headerRc, int pageIndex, int phi, int pageCount )
{
    lvRect oldcr;
    drawbuf->GetClipRect( &oldcr );
    lvRect hrc = headerRc;
    drawbuf->SetClipRect(&hrc);
    bool drawGauge = true;
    lvRect info = headerRc;
    lUInt32 cl1 = getTextColor();
    lUInt32 cl2 = getBackgroundColor();
    lUInt32 cl3 = 0xD0D0D0;
    lUInt32 cl4 = 0xC0C0C0;
    //lUInt32 pal[4];
    int percent = getPosPercent();
    bool leftPage = (getVisiblePageCount()==2 && !(pageIndex&1) );
    if ( leftPage || !drawGauge )
        percent=10000;
    int percent_pos = percent * info.width() / 10000;
//    int gh = 3; //drawGauge ? 3 : 1;
    LVArray<int> & sbounds = getSectionBounds();
    lvRect navBar;
    getNavigationBarRectangle( pageIndex, navBar );
    int gpos = info.bottom;
    cl1 = getTextColor();
    drawbuf->SetTextColor(cl1);
    if ( drawbuf->GetBitsPerPixel() <= 2 ) {
        // gray
        cl3 = 1;
        cl4 = cl1;
        //pal[0] = cl1;
    }
    //drawbuf->FillRect(info.left, gpos-gh, info.left+percent_pos, gpos-gh+1, cl1 );
    drawbuf->FillRect(info.left, gpos-2, info.left+percent_pos, gpos-2+1, cl1 );
    //drawbuf->FillRect(info.left+percent_pos, gpos-gh, info.right, gpos-gh+1, cl1 ); //cl3
    drawbuf->FillRect(info.left+percent_pos, gpos-2, info.right, gpos-2+1, cl1 ); // cl3

    if ( !leftPage ) {
        drawbuf->FillRect(info.left, gpos-3, info.left+percent_pos, gpos-3+1, cl1 );
        drawbuf->FillRect(info.left, gpos-1, info.left+percent_pos, gpos-1+1, cl1 );
    }

    // disable section marks
    if ( !leftPage ) {
        for ( int i=0; i<sbounds.length(); i++) {
            int x = info.left + sbounds[i]*(info.width()-1) / 10000;
            lUInt32 c = x<info.left+percent_pos ? cl2 : cl1;
            drawbuf->FillRect(x, gpos-4, x+1, gpos-0+2, c );
        }
    }


    int iy = info.top; // + (info.height() - m_infoFont->getHeight()) * 2 / 3;
    if ( getVisiblePageCount()==1 || !(pageIndex&1) ) {
        int dwIcons = 0;
        int icony = iy + m_infoFont->getHeight() / 2;
        for ( int ni=0; ni<m_headerIcons.length(); ni++ ) {
            LVImageSourceRef icon = m_headerIcons[ni];
            int h = icon->GetHeight();
            int w = icon->GetWidth();
            drawbuf->Draw( icon, info.left + dwIcons, icony - h / 2, w, h );
            dwIcons += w + 4;
        }
        info.left += dwIcons;
    }

    if ( (phi & PGHDR_BATTERY) && m_battery_state>=0 ) {
        lvRect brc = info;
        brc.right -= 3;
        brc.top += 1;
        brc.bottom -= 2;
        int h = brc.height();
        bool isVertical = (h>30);
        if ( isVertical )
            brc.left = brc.right - brc.height()/2;
        else
            brc.left = brc.right - 30;
        drawBatteryState( drawbuf, brc, isVertical );
        info.right = brc.left - info.height()/2;
    }
    lString16 pageinfo;
    if ( pageCount>0 ) {
        if ( phi & PGHDR_PAGE_NUMBER )
            pageinfo += lString16::itoa( pageIndex+1 );
        if ( phi & PGHDR_PAGE_COUNT )
            pageinfo += L" / " + lString16::itoa( pageCount );
    }
    int piw = 0;
    if ( !pageinfo.empty() ) {
        piw = m_infoFont->getTextWidth( pageinfo.c_str(), pageinfo.length() );
        m_infoFont->DrawTextString( drawbuf, info.right-piw, iy,
            pageinfo.c_str(), pageinfo.length(), L' ', NULL, false);
        info.right -= piw + info.height()/2;
    }
    if ( phi & PGHDR_CLOCK ) {
        lString16 clock = getTimeString();
        m_last_clock = clock;
        int w = m_infoFont->getTextWidth( clock.c_str(), clock.length() ) + 2;
        m_infoFont->DrawTextString( drawbuf, info.right-w, iy,
            clock.c_str(), clock.length(), L' ', NULL, false);
        info.right -= w + info.height()/2;
    }
    int titlew = 0;
    lString16 title;
    if ( phi & PGHDR_TITLE ) {
        title = getTitle();
        if ( !title.empty() )
             titlew = m_infoFont->getTextWidth( title.c_str(), title.length() );
    }
    int authorsw = 0;
    lString16 authors;
    if ( phi & PGHDR_AUTHOR ) {
        authors = getAuthors();
        if ( !authors.empty() ) {
            if ( !title.empty() )
                authors += L'.';
            authorsw = m_infoFont->getTextWidth( authors.c_str(), authors.length() );
        }
    }
    int w = info.width() - 10;
    lString16 text;
    if ( authorsw + titlew + 10 > w ) {
        if ( (pageIndex & 1) )
            text = title;
        else {
            text = authors;
            if ( !text.empty() && text[text.length()-1]=='.')
                text = text.substr(0, text.length() - 1 );
        }
    } else {
        text = authors + L"  " + title;
    }
    lvRect newcr = headerRc;
    newcr.right = info.right - 10;
    drawbuf->SetClipRect(&newcr);
    text = fitTextWidthWithEllipsis( text, m_infoFont, newcr.width() );
    if ( !text.empty() ) {
        m_infoFont->DrawTextString( drawbuf, info.left, iy,
            text.c_str(), text.length(), L' ', NULL, false);
    }
    drawbuf->SetClipRect(&oldcr);
    //--------------
    drawbuf->SetTextColor(getTextColor());
}

void LVDocView::drawPageTo(LVDrawBuf * drawbuf, LVRendPageInfo & page, lvRect * pageRect, int pageCount, int basePage )
{
    int start = page.start;
    int height = page.height;
    int headerHeight = getPageHeaderHeight();
    CRLog::trace("drawPageTo(%d,%d)", start, height);
    lvRect fullRect( 0, 0, drawbuf->GetWidth(), drawbuf->GetHeight() );
    if ( !pageRect )
        pageRect = &fullRect;
    //int offset = (pageRect->height() - m_pageMargins.top - m_pageMargins.bottom - height) / 3;
    //if (offset>16)
    //    offset = 16;
    //if (offset<0)
    //    offset = 0;
    int offset = 0;
    lvRect clip;
    clip.left = pageRect->left + m_pageMargins.left;
    clip.top = pageRect->top + m_pageMargins.top + headerHeight + offset;
    clip.bottom = pageRect->top + m_pageMargins.top + height + headerHeight + offset;
    clip.right = pageRect->left + pageRect->width() - m_pageMargins.right;
    if ( page.type==PAGE_TYPE_COVER )
        clip.top = pageRect->top + m_pageMargins.top;
    if ( m_pageHeaderInfo && page.type!=PAGE_TYPE_COVER) {
        int phi = m_pageHeaderInfo;
        if ( getVisiblePageCount()==2 ) {
            if ( page.index & 1 ) {
                // right
                phi &= ~PGHDR_AUTHOR;
            } else {
                // left
                phi &= ~PGHDR_TITLE;
                phi &= ~PGHDR_PAGE_NUMBER;
                phi &= ~PGHDR_PAGE_COUNT;
                phi &= ~PGHDR_BATTERY;
                phi &= ~PGHDR_CLOCK;
            }
        }
        lvRect info;
        getPageHeaderRectangle( page.index, info );
        drawPageHeader( drawbuf, info, page.index-1+basePage, phi, pageCount-1+basePage );
        //clip.top = info.bottom;
    }
    drawbuf->SetClipRect(&clip);
    if ( m_doc ) {
        if ( page.type == PAGE_TYPE_COVER ) {
            lvRect rc = *pageRect;
            drawbuf->SetClipRect(&rc);
            //if ( m_pageMargins.bottom > m_pageMargins.top )
            //    rc.bottom -= m_pageMargins.bottom - m_pageMargins.top;
            /*
            rc.left += m_pageMargins.left / 2;
            rc.top += m_pageMargins.bottom / 2;
            rc.right -= m_pageMargins.right / 2;
            rc.bottom -= m_pageMargins.bottom / 2;
            */
            CRLog::trace("Entering drawCoverTo()");
            drawCoverTo( drawbuf, rc );
        } else {
            // draw main page text
            CRLog::trace("Entering DrawDocument()");
            DrawDocument( *drawbuf, m_doc->getRootNode(), pageRect->left + m_pageMargins.left, clip.top, pageRect->width() - m_pageMargins.left - m_pageMargins.right, height, 0, -start+offset, m_dy, &m_markRanges );
            CRLog::trace("Done DrawDocument() for main text");
            // draw footnotes
#define FOOTNOTE_MARGIN 8
            int fny = clip.top + page.height + FOOTNOTE_MARGIN;
            int fy = fny;
            bool footnoteDrawed = false;
            for ( int fn=0; fn<page.footnotes.length(); fn++ ) {
                int fstart = page.footnotes[fn].start;
                int fheight = page.footnotes[fn].height;
                clip.top = fy + offset;
                clip.left = pageRect->left + m_pageMargins.left;
                clip.right = pageRect->right - m_pageMargins.right;
                clip.bottom = fy + offset + fheight;
                drawbuf->SetClipRect(&clip);
                DrawDocument( *drawbuf, m_doc->getRootNode(), pageRect->left + m_pageMargins.left, fy + offset, pageRect->width() - m_pageMargins.left - m_pageMargins.right, fheight, 0, -fstart+offset, m_dy, &m_markRanges );
                footnoteDrawed = true;
                fy += fheight;
            }
            if ( footnoteDrawed ) {
                fny -= FOOTNOTE_MARGIN / 2;
                drawbuf->SetClipRect(NULL);
                drawbuf->FillRect( pageRect->left + m_pageMargins.left, fny, pageRect->right - m_pageMargins.right, fny + 1, 0xAAAAAA );
            }
        }
    }
    drawbuf->SetClipRect(NULL);
#ifdef SHOW_PAGE_RECT
    drawbuf->FillRect(pageRect->left, pageRect->top, pageRect->left+1, pageRect->bottom, 0xAAAAAA);
    drawbuf->FillRect(pageRect->left, pageRect->top, pageRect->right, pageRect->top+1, 0xAAAAAA);
    drawbuf->FillRect(pageRect->right-1, pageRect->top, pageRect->right, pageRect->bottom, 0xAAAAAA);
    drawbuf->FillRect(pageRect->left, pageRect->bottom-1, pageRect->right, pageRect->bottom, 0xAAAAAA);
    drawbuf->FillRect(pageRect->left+m_pageMargins.left, pageRect->top+m_pageMargins.top+headerHeight, pageRect->left+1+m_pageMargins.left, pageRect->bottom-m_pageMargins.bottom, 0x555555);
    drawbuf->FillRect(pageRect->left+m_pageMargins.left, pageRect->top+m_pageMargins.top+headerHeight, pageRect->right-m_pageMargins.right, pageRect->top+1+m_pageMargins.top+headerHeight, 0x555555);
    drawbuf->FillRect(pageRect->right-1-m_pageMargins.right, pageRect->top+m_pageMargins.top+headerHeight, pageRect->right-m_pageMargins.right, pageRect->bottom-m_pageMargins.bottom, 0x555555);
    drawbuf->FillRect(pageRect->left+m_pageMargins.left, pageRect->bottom-1-m_pageMargins.bottom, pageRect->right-m_pageMargins.right, pageRect->bottom-m_pageMargins.bottom, 0x555555);
#endif

#if 0
    lString16 pagenum = lString16::itoa( page.index+1 );
    m_font->DrawTextString(drawbuf, 5, 0 , pagenum.c_str(), pagenum.length(), '?', NULL, false); //drawbuf->GetHeight()-m_font->getHeight()
#endif
}

int LVDocView::getCurPage()
{
    LVLock lock(getMutex());
    checkPos();
    return m_pages.FindNearestPage(m_pos, 0);
}

/// returns true if time changed since clock has been last drawed
bool LVDocView::isTimeChanged()
{
    if ( m_pageHeaderInfo & PGHDR_CLOCK ) {
        bool res = (m_last_clock != getTimeString());
        if ( res )
            m_imageCache.clear();
        return res;
    }
    return false;
}

/// draw to specified buffer
void LVDocView::Draw( LVDrawBuf & drawbuf, int position, bool rotate  )
{
    LVLock lock(getMutex());
    checkPos();
    drawbuf.Resize( m_dx, m_dy );
    drawbuf.SetBackgroundColor( m_backgroundColor );
    drawbuf.SetTextColor( m_textColor );
    drawbuf.Clear(m_backgroundColor);
    if ( drawbuf.GetBitsPerPixel()==32 && getVisiblePageCount()==2 ) {
        int x = drawbuf.GetWidth() / 2;
        lUInt32 cl = m_backgroundColor;
        cl = ((cl & 0xFCFCFC) + 0x404040) >> 1;
        drawbuf.FillRect( x, 0, x+1, drawbuf.GetHeight(), cl);
    }

    if ( !m_is_rendered )
        return;
    if ( !m_doc )
        return;
    if (m_font.isNull())
        return;
    if (m_view_mode==DVM_SCROLL)
    {
        drawbuf.SetClipRect(NULL);
        int cover_height = 0;
        if ( m_pages.length()>0 && m_pages[0]->type == PAGE_TYPE_COVER )
            cover_height = m_pages[0]->height;
        if ( position < cover_height ) {
            lvRect rc;
            drawbuf.GetClipRect( &rc );
            rc.top -= position;
            rc.bottom -= position;
            rc.top += m_pageMargins.top;
            rc.bottom -= m_pageMargins.bottom;
            rc.left += m_pageMargins.left;
            rc.right -= m_pageMargins.right;
            drawCoverTo( &drawbuf, rc );
        }
        DrawDocument( drawbuf, m_doc->getRootNode(), m_pageMargins.left, 0, m_dx - m_pageMargins.left - m_pageMargins.right, m_dy, 0, -position, m_dy, &m_markRanges );
    }
    else
    {
        int pc = getVisiblePageCount();
        CRLog::trace("searching for page with offset=%d", position);
        int page = m_pages.FindNearestPage(position, 0);
        CRLog::trace("found page #%d", page);
        if ( page>=0 && page<m_pages.length() )
            drawPageTo( &drawbuf, *m_pages[page], &m_pageRects[0], m_pages.length(), 1 );
        if ( pc==2 && page>=0 && page+1<m_pages.length() )
            drawPageTo( &drawbuf, *m_pages[page + 1], &m_pageRects[1], m_pages.length(), 1 );
    }
    if ( rotate ) {
        CRLog::trace("Rotate drawing buffer. Src size=(%d, %d), angle=%d, buf(%d, %d)", m_dx, m_dy, m_rotateAngle, drawbuf.GetWidth(), drawbuf.GetHeight() );
        drawbuf.Rotate( m_rotateAngle );
        CRLog::trace("Rotate done. buf(%d, %d)", drawbuf.GetWidth(), drawbuf.GetHeight() );
    }
}

//void LVDocView::Draw()
//{
    //Draw( m_drawbuf, m_pos, true );
//}

/// converts point from window to document coordinates, returns true if success
bool LVDocView::windowToDocPoint( lvPoint & pt )
{
    checkRender();
    pt = rotatePoint( pt, true );
    if ( getViewMode() == DVM_SCROLL ) {
        // SCROLL mode
        pt.y += m_pos;
        pt.x -= m_pageMargins.left;
        return true;
    } else {
        // PAGES mode
        int page = m_pages.FindNearestPage(m_pos, 0);
        lvRect * rc = NULL;
        lvRect page1( m_pageRects[0] );
        int headerHeight = getPageHeaderHeight();
        page1.left += m_pageMargins.left;
        page1.top += m_pageMargins.top + headerHeight;
        page1.right -= m_pageMargins.right;
        page1.bottom -= m_pageMargins.bottom;
        if ( page1.isPointInside( pt ) ) {
            rc = &page1;
        } else if ( getVisiblePageCount()==2 ) {
            lvRect page2( m_pageRects[1] );
            page2.left += m_pageMargins.left;
            page2.top += m_pageMargins.top + headerHeight;
            page2.right -= m_pageMargins.right;
            page2.bottom -= m_pageMargins.bottom;
            if ( page2.isPointInside( pt ) ) {
                rc = &page2;
                page++;
            }
        }
        if ( rc && page>=0 && page<m_pages.length() ) {
            int page_y = m_pages[page]->start;
            pt.x -= rc->left;
            pt.y -= rc->top;
            if ( pt.y < m_pages[page]->height ) {
                //CRLog::debug(" point page offset( %d, %d )", pt.x, pt.y );
                pt.y += page_y;
                return true;
            }
        }
    }
    return false;
}

/// converts point from documsnt to window coordinates, returns true if success
bool LVDocView::docToWindowPoint( lvPoint & pt )
{
    LVLock lock(getMutex());
    checkRender();
    // TODO: implement coordinate conversion here
    if ( getViewMode() == DVM_SCROLL ) {
        // SCROLL mode
        pt.y -= m_pos;
        pt.x += m_pageMargins.left;
        return true;
    } else {
        // PAGES mode
#if 0
        int page = m_pages.FindNearestPage(m_pos, 0);
        lvRect * rc = NULL;
        lvRect page1( m_pageRects[0] );
        int headerHeight = getPageHeaderHeight();
        page1.left += m_pageMargins.left;
        page1.top += m_pageMargins.top + headerHeight;
        page1.right -= m_pageMargins.right;
        page1.bottom -= m_pageMargins.bottom;
        if ( page1.isPointInside( pt ) ) {
            rc = &page1;
        } else if ( getVisiblePageCount()==2 ) {
            lvRect page2( m_pageRects[1] );
            page2.left += m_pageMargins.left;
            page2.top += m_pageMargins.top + headerHeight;
            page2.right -= m_pageMargins.right;
            page2.bottom -= m_pageMargins.bottom;
            if ( page2.isPointInside( pt ) ) {
                rc = &page2;
                page++;
            }
        }
        if ( rc && page>=0 && page<m_pages.length() ) {
            int page_y = m_pages[page]->start;
            pt.x -= rc->left;
            pt.y -= rc->top;
            if ( pt.y < m_pages[page]->height ) {
                //CRLog::debug(" point page offset( %d, %d )", pt.x, pt.y );
                pt.y += page_y;
                return true;
            }
        }
#endif
    }
    pt = rotatePoint( pt, false );
    return false;
}

/// returns xpointer for specified window point
ldomXPointer LVDocView::getNodeByPoint( lvPoint pt )
{
    LVLock lock(getMutex());
    checkRender();
    if ( windowToDocPoint( pt ) && m_doc ) {
        ldomXPointer ptr = m_doc->createXPointer( pt );
        //CRLog::debug("  ptr (%d, %d) node=%08X offset=%d", pt.x, pt.y, (lUInt32)ptr.getNode(), ptr.getOffset() );
        return ptr;
    }
    return ldomXPointer();
}

void LVDocView::updateLayout()
{
    lvRect rc( 0, 0, m_dx, m_dy );
    m_pageRects[0] = rc;
    m_pageRects[1] = rc;
    if ( getVisiblePageCount()==2 ) {
        int middle = (rc.left + rc.right) >> 1;
        m_pageRects[0].right = middle - m_pageMargins.right/2;
        m_pageRects[1].left = middle + m_pageMargins.left/2;
    }
}

/// set list of icons to display at left side of header
void LVDocView::setHeaderIcons( LVRefVec<LVImageSource> icons )
{
    m_headerIcons = icons;
}

/// get page document range, -1 for current page
LVRef<ldomXRange> LVDocView::getPageDocumentRange( int pageIndex )
{
    LVLock lock(getMutex());
    checkRender();
    LVRef<ldomXRange> res(NULL);
    if ( getViewMode()==DVM_SCROLL ) {
        // SCROLL mode
        int starty = m_pos;
        int endy = m_pos + m_dy;
        int fh = GetFullHeight();
        if ( endy>=fh )
            endy = fh-1;
        ldomXPointer start = m_doc->createXPointer( lvPoint( 0, starty ) );
        ldomXPointer end = m_doc->createXPointer( lvPoint( 0, endy ) );
        if ( start.isNull() || end.isNull() )
            return res;
        res = LVRef<ldomXRange> ( new ldomXRange(start, end) );
    } else {
        // PAGES mode
        if ( pageIndex<0 || pageIndex>=m_pages.length() )
            pageIndex = getCurPage();
        LVRendPageInfo * page = m_pages[ pageIndex ];
        if ( page->type!=PAGE_TYPE_NORMAL)
            return res;
        ldomXPointer start = m_doc->createXPointer( lvPoint( 0, page->start ) );
        //ldomXPointer end = m_doc->createXPointer( lvPoint( m_dx+m_dy, page->start + page->height - 1 ) );
        ldomXPointer end = m_doc->createXPointer( lvPoint( 0, page->start + page->height  ) );
        if ( start.isNull() || end.isNull() )
            return res;
        res = LVRef<ldomXRange> ( new ldomXRange(start, end) );
    }
    return res;
}

/// get page text, -1 for current page
lString16 LVDocView::getPageText( bool , int pageIndex )
{
    LVLock lock(getMutex());
    checkRender();
    lString16 txt;
    LVRef<ldomXRange> range = getPageDocumentRange( pageIndex );
    txt = range->getRangeText();
    return txt;
}

void LVDocView::Render( int dx, int dy, LVRendPageList * pages )
{
    LVLock lock(getMutex());
    {
        if ( !m_doc || m_doc->getRootNode()==NULL)
            return;
        if ( pages==NULL )
            pages = &m_pages;
        updateLayout();
        lString8 fontName = lString8(DEFAULT_FONT_NAME);
        m_font = fontMan->GetFont( m_font_size, 300, false, DEFAULT_FONT_FAMILY, m_defaultFontFace );
        m_infoFont = fontMan->GetFont( m_status_font_size, 300, false, DEFAULT_FONT_FAMILY, m_statusFontFace );
        if ( !m_font || !m_infoFont )
            return;
        if ( dx==0 )
            dx = m_pageRects[0].width() - m_pageMargins.left - m_pageMargins.right;
        if ( dy==0 )
            dy = m_pageRects[0].height() - m_pageMargins.top - m_pageMargins.bottom - getPageHeaderHeight();

        CRLog::debug("Render(width=%d, height=%d, font=%s(%d))", dx, dy, fontName.c_str(), m_font_size);
        //CRLog::trace("calling render() for document %08X font=%08X", (unsigned int)m_doc, (unsigned int)m_font.get() );
        m_doc->render( pages, dx, dy, m_showCover, m_showCover ? dy + m_pageMargins.bottom*4 : 0, m_font, m_def_interline_space );

    #if 0
        FILE * f = fopen("pagelist.log", "wt");
        if (f) {
            for (int i=0; i<m_pages.length(); i++)
            {
                fprintf(f, "%4d:   %7d .. %-7d [%d]\n", i, m_pages[i].start, m_pages[i].start+m_pages[i].height, m_pages[i].height);
            }
            fclose(f);
        }
    #endif
        fontMan->gc();
        m_is_rendered = true;

        makeToc();
        updateSelections();
        CRLog::debug("Render is finished");
    }
}

/// sets selection for whole element, clears previous selection
void LVDocView::selectElement( ldomNode * elem )
{
    ldomXRangeList & sel = getDocument()->getSelections();
    sel.clear();
    sel.add( new ldomXRange(elem) );
    updateSelections();
}

/// sets selection for list of words, clears previous selection
void LVDocView::selectWords( const LVArray<ldomWord> & words )
{
    ldomXRangeList & sel = getDocument()->getSelections();
    sel.clear();
    sel.addWords( words );
    updateSelections();
}

/// sets selection for range, clears previous selection
void LVDocView::selectRange( const ldomXRange & range )
{
    ldomXRangeList & sel = getDocument()->getSelections();
    if ( sel.length()==1 ) {
        if ( range == *sel[0] )
            return; // the same range is set
    }
    sel.clear();
    sel.add( new ldomXRange(range) );
    updateSelections();
}

/// clears selection
void LVDocView::clearSelection()
{
    ldomXRangeList & sel = getDocument()->getSelections();
    sel.clear();
    updateSelections();
}

/// selects first link on page, if any. returns selected link range, null if no links.
ldomXRange * LVDocView::selectFirstPageLink()
{
    ldomXRangeList list;
    getCurrentPageLinks( list );
    if ( !list.length() )
        return NULL;
    //
    selectRange( *list[0] );
    //
    ldomXRangeList & sel = getDocument()->getSelections();
    updateSelections();
    return sel[0];
}

/// selects link on page, if any (delta==0 - current, 1-next, -1-previous). returns selected link range, null if no links.
ldomXRange * LVDocView::selectPageLink( int delta, bool wrapAround)
{
    ldomXRangeList & sel = getDocument()->getSelections();
    ldomXRangeList list;
    getCurrentPageLinks( list );
    if ( !list.length() )
        return NULL;
    int currentLinkIndex = -1;
    if ( sel.length() > 0 ) {
        ldomNode * currSel = sel[0]->getStart().getNode();
        for ( int i=0; i<list.length(); i++ ) {
            if ( list[i]->getStart().getNode() == currSel ) {
                currentLinkIndex = i;
                break;
            }
        }
    }
    bool error = false;
    if ( delta==1 ) {
        // next
        currentLinkIndex++;
        if ( currentLinkIndex>=list.length() ) {
            if ( wrapAround )
                currentLinkIndex = 0;
            else
                error = true;
        }

    } else if ( delta==-1 ) {
        // previous
        if ( currentLinkIndex==-1 )
            currentLinkIndex = list.length()-1;
        else
            currentLinkIndex--;
        if ( currentLinkIndex<0 ) {
            if ( wrapAround )
                currentLinkIndex = list.length()-1;
            else
                error = true;
        }
    } else {
        // current
        if ( currentLinkIndex < 0 || currentLinkIndex >= list.length() )
            error = true;
    }
    if ( error ) {
        clearSelection();
        return NULL;
    }
    //
    selectRange( *list[currentLinkIndex] );
    //
    updateSelections();
    return sel[0];
}

/// selects next link on page, if any. returns selected link range, null if no links.
ldomXRange * LVDocView::selectNextPageLink( bool wrapAround )
{
    return selectPageLink( +1, wrapAround );
}

/// selects previous link on page, if any. returns selected link range, null if no links.
ldomXRange * LVDocView::selectPrevPageLink( bool wrapAround )
{
    return selectPageLink( -1, wrapAround );
}

/// returns selected link on page, if any. null if no links.
ldomXRange * LVDocView::getCurrentPageSelectedLink()
{
    return selectPageLink( 0, false );
}

/// get document rectangle for specified cursor position, returns false if not visible
bool LVDocView::getCursorDocRect( ldomXPointer ptr, lvRect & rc )
{
    rc.clear();
    if ( ptr.isNull() )
        return false;
    if ( !ptr.getRect( rc ) ) {
        rc.clear();
        return false;
    }
    return true;
}

/// get screen rectangle for specified cursor position, returns false if not visible
bool LVDocView::getCursorRect( ldomXPointer ptr, lvRect & rc, bool scrollToCursor )
{
    if ( !getCursorDocRect( ptr, rc ) )
        return false;
    for (;;) {

        lvPoint topLeft = rc.topLeft();
        lvPoint bottomRight = rc.bottomRight();
        if ( docToWindowPoint( topLeft ) && docToWindowPoint( bottomRight ) ) {
            rc.setTopLeft( topLeft );
            rc.setBottomRight( bottomRight );
            return true;
        }
        // try to scroll and convert doc->window again
        if ( !scrollToCursor )
            break;
        // scroll
        goToBookmark( ptr );
        scrollToCursor = false;
    };
    rc.clear();
    return false;
}

/// follow link, returns true if navigation was successful
bool LVDocView::goLink( lString16 link, bool savePos )
{
    ldomNode * element = NULL;
    if ( link.empty() ) {
        ldomXRange * node = LVDocView::getCurrentPageSelectedLink();
        if ( node ) {
            link = node->getHRef();
            ldomNode * p = node->getStart().getNode();
            if ( p->isText() )
                p = p->getParentNode();
            element = p;
        }
        if ( link.empty() )
            return false;
    }
    if ( link[0]!='#' || link.length()<=1 ) {
        lString16 filename = link;
        lString16 id;
        int p = filename.pos(L"#");
        if ( p>=0 ) {
            // split filename and anchor
            // part1.html#chapter3 =>   part1.html & chapter3
            id = filename.substr( p + 1 );
            filename = filename.substr( 0, p );
        }
        if ( filename.pos(L":")>=0 ) {
            // URL with protocol like http://
            if ( m_callback ) {
                m_callback->OnExternalLink( link, element );
                return true;
            }
        } else {
            // otherwise assume link to another file
            CRLog::debug("Link to another file: %s   anchor=%s", UnicodeToUtf8(filename).c_str(), UnicodeToUtf8(id).c_str() );

            lString16 baseDir = m_doc_props->getStringDef(DOC_PROP_FILE_PATH, ".");
            LVAppendPathDelimiter(baseDir);
            lString16 fn = m_doc_props->getStringDef(DOC_PROP_FILE_NAME, "");
            CRLog::debug("Current path: %s   filename:%s", UnicodeToUtf8(baseDir).c_str(), UnicodeToUtf8(fn).c_str() );
            baseDir = LVExtractPath(baseDir + fn);
            //lString16 newPathName = LVMakeRelativeFilename( baseDir, filename );
            lString16 newPathName = LVCombinePaths( baseDir, filename );
            lString16 dir = LVExtractPath( newPathName );
            lString16 filename = LVExtractFilename( newPathName );
            LVContainerRef container = m_container;
            lString16 arcname = m_doc_props->getStringDef(DOC_PROP_ARC_NAME, "");
            if ( arcname.empty() ) {
                container = LVOpenDirectory( dir.c_str() );
                if ( container.isNull() )
                    return false;
            } else {
                filename = newPathName;
                dir.clear();
            }
            CRLog::debug("Base dir: %s newPathName=%s", UnicodeToUtf8(baseDir).c_str(), UnicodeToUtf8(newPathName).c_str());

            LVStreamRef stream = container->OpenStream( filename.c_str(), LVOM_READ );
            if ( stream.isNull() ) {
                CRLog::error("Go to link: cannot find file %s", UnicodeToUtf8(filename).c_str() );
                return false;
            }
            CRLog::info("Go to link: file %s is found", UnicodeToUtf8(filename).c_str() );
            // return point
            if ( savePos )
                savePosToNavigationHistory();

            // close old document
            savePosition();
            clearSelection();
            _posBookmark = ldomXPointer();
            m_is_rendered = false;
            m_swapDone = false;
            m_pos = 0;
            m_section_bounds_valid = false;
            m_doc_props->setString(DOC_PROP_FILE_PATH, dir);
            m_doc_props->setString(DOC_PROP_FILE_NAME, filename);
            m_doc_props->setString(DOC_PROP_CODE_BASE, LVExtractPath(filename) );
            m_doc_props->setString(DOC_PROP_FILE_SIZE, lString16::itoa((int)stream->GetSize()));
            m_doc_props->setHex(DOC_PROP_FILE_CRC32, stream->crc32());
            // TODO: load document from stream properly
            if ( !LoadDocument(stream) ) {
                createDefaultDocument( lString16(L"Load error"), lString16(L"Cannot open file ") + filename );
                return false;
            }
            //m_filename = newPathName;
            m_stream = stream;
            m_container = container;

            //restorePosition();

            // TODO: setup properties
            // go to anchor
            if ( !id.empty() )
                goLink(lString16(L"#") + id );
            clearImageCache();
            requestRender();
            return true;
        }
        return false; // only internal links supported (started with #)
    }
    link = link.substr( 1, link.length()-1 );
    lUInt16 id = m_doc->getAttrValueIndex(link.c_str());
    ldomNode * dest = m_doc->getNodeById( id );
    if ( !dest )
        return false;
    savePosToNavigationHistory();
    ldomXPointer newPos( dest, 0 );
    goToBookmark( newPos );
    return true;
}

/// follow selected link, returns true if navigation was successful
bool LVDocView::goSelectedLink()
{
    ldomXRange * link = getCurrentPageSelectedLink();
    if ( !link )
        return false;
    lString16 href = link->getHRef();
    if ( href.empty() )
        return false;
    return goLink( href );
}

#define NAVIGATION_FILENAME_SEPARATOR L":"
bool splitNavigationPos( lString16 pos, lString16 & fname, lString16 & path )
{
    int p = pos.pos(lString16(NAVIGATION_FILENAME_SEPARATOR));
    if ( p<=0 ) {
        fname = lString16();
        path = pos;
        return false;
    }
    fname = pos.substr(0, p);
    path = pos.substr(p+1);
    return true;
}

bool LVDocView::savePosToNavigationHistory()
{
    ldomXPointer bookmark = getBookmark();
    if ( !bookmark.isNull() ) {
        lString16 path = bookmark.toString();
        if ( !path.empty() ) {
            lString16 fname = m_doc_props->getStringDef(DOC_PROP_FILE_NAME, "");
            lString16 fpath = m_doc_props->getStringDef(DOC_PROP_FILE_PATH, "");
            LVAppendPathDelimiter(fpath);
            lString16 s = fpath + fname + NAVIGATION_FILENAME_SEPARATOR + path;
            if ( !m_arc.isNull() )
                s = lString16(L"/") + s;
            CRLog::debug("savePosToNavigationHistory(%s)", UnicodeToUtf8(s).c_str() );
            return _navigationHistory.save( s );
        }
    }
    return false;
}

/// go back. returns true if navigation was successful
bool LVDocView::goBack()
{
    if ( _navigationHistory.forwardCount()==0 && savePosToNavigationHistory() )
        _navigationHistory.back();
    lString16 s = _navigationHistory.back();
    if ( s.empty() )
        return false;
    CRLog::debug("goBack(%s)", UnicodeToUtf8(s).c_str() );
    lString16 fname, path;
    if ( splitNavigationPos( s, fname, path ) ) {
        if ( m_filename!=fname )
            if ( !goLink( fname, false ) )
                return false;
    }
    if ( path.empty() )
        return false;
    ldomXPointer bookmark = m_doc->createXPointer( path );
    if ( bookmark.isNull() )
        return false;
    goToBookmark( bookmark );
    return true;
}

/// go forward. returns true if navigation was successful
bool LVDocView::goForward()
{
    lString16 s = _navigationHistory.forward();
    if ( s.empty() )
        return false;
    CRLog::debug("goForward(%s)", UnicodeToUtf8(s).c_str() );
    lString16 fname, path;
    if ( splitNavigationPos( s, fname, path ) ) {
        if ( m_filename!=fname )
            if ( !goLink( fname, false ) )
                return false;
    }
    if ( path.empty() )
        return false;
    ldomXPointer bookmark = m_doc->createXPointer( path );
    if ( bookmark.isNull() )
        return false;
    goToBookmark( bookmark );
    return true;
}


/// update selection ranges
void LVDocView::updateSelections()
{
    checkRender();
    m_imageCache.clear();
    LVLock lock(getMutex());
    ldomXRangeList ranges( m_doc->getSelections(), true );
    ranges.getRanges( m_markRanges );
    if ( m_markRanges.length()>0 ) {
        crtrace trace;
        trace << "LVDocView::updateSelections() - " << "selections: " << m_doc->getSelections().length() << ", ranges: " << ranges.length() << ", marked ranges: " << m_markRanges.length() << " ";
        for ( int i=0; i<m_markRanges.length(); i++ ) {
            ldomMarkedRange * item = m_markRanges[i];
            trace << "(" << item->start.x << "," << item->start.y << "--" << item->end.x << "," << item->end.y << " #" << item->flags << ") ";
        }
    }
}

/// set view mode (pages/scroll)
void LVDocView::setViewMode( LVDocViewMode view_mode, int visiblePageCount )
{
    if ( m_view_mode==view_mode && (visiblePageCount==m_pagesVisible || visiblePageCount<1) )
        return;
    m_imageCache.clear();
    LVLock lock(getMutex());
    m_view_mode = view_mode;
    m_props->setInt( PROP_PAGE_VIEW_MODE, m_view_mode == DVM_PAGES ? 1 : 0 );
    if ( visiblePageCount==1 || visiblePageCount==2 )
        m_pagesVisible = visiblePageCount;
    requestRender();
    goToBookmark(_posBookmark);
}

/// get view mode (pages/scroll)
LVDocViewMode LVDocView::getViewMode()
{
    return m_view_mode;
}

/// toggle pages/scroll view mode
void LVDocView::toggleViewMode()
{
    if ( m_view_mode == DVM_SCROLL )
        setViewMode( DVM_PAGES );
    else
        setViewMode( DVM_SCROLL );

}

int LVDocView::getVisiblePageCount()
{
    return (m_view_mode == DVM_SCROLL || m_dx < m_font_size * MIN_EM_PER_PAGE || m_dx*5 < m_dy*6 )
        ? 1
        : m_pagesVisible;
}

/// set window visible page count (1 or 2)
void LVDocView::setVisiblePageCount( int n )
{
    m_imageCache.clear();
    LVLock lock(getMutex());
    if ( n == 2 )
        m_pagesVisible = 2;
    else
        m_pagesVisible = 1;
    updateLayout();
    requestRender();
}

static int findBestFit( LVArray<int> & v, int n, bool rollCyclic=false )
{
    int bestsz = -1;
    int bestfit = -1;
    if ( rollCyclic ) {
        if ( n<v[0] )
            return v[v.length()-1];
        if ( n>v[v.length()-1] )
            return v[0];
    }
    for ( int i=0; i<v.length(); i++ ) {
        int delta = v[i] - n;
        if ( delta<0 )
            delta = -delta;
        if ( bestfit==-1 || bestfit>delta ) {
            bestfit = delta;
            bestsz = v[i];
        }
    }
    if ( bestsz<0 )
        bestsz = n;
    return bestsz;
}

void LVDocView::setDefaultInterlineSpace( int percent )
{
    LVLock lock(getMutex());
    requestRender();
    m_def_interline_space = percent;
    goToBookmark(_posBookmark);
}

/// sets new status bar font size
void LVDocView::setStatusFontSize( int newSize )
{
    LVLock lock(getMutex());
	int oldSize = m_status_font_size;
    m_status_font_size = newSize;
	if ( oldSize != newSize ) {
		propsGetCurrent()->setInt( PROP_STATUS_FONT_SIZE, m_status_font_size );
		requestRender();
	}
    //goToBookmark(_posBookmark);
}

void LVDocView::setFontSize( int newSize )
{
    LVLock lock(getMutex());
	int oldSize = m_font_size;
    m_font_size = findBestFit( m_font_sizes, newSize );
	if ( oldSize != newSize ) {
		propsGetCurrent()->setInt( PROP_FONT_SIZE, m_font_size );
		requestRender();
	}
    //goToBookmark(_posBookmark);
}

void LVDocView::setDefaultFontFace( const lString8 & newFace )
{
    m_defaultFontFace = newFace;
    requestRender();
}

void LVDocView::setStatusFontFace( const lString8 & newFace )
{
    m_statusFontFace = newFace;
    requestRender();
}

/// sets posible base font sizes (for ZoomFont feature)
void LVDocView::setFontSizes( LVArray<int> & sizes, bool cyclic )
{
    m_font_sizes = sizes;
    m_font_sizes_cyclic = cyclic;
}

void LVDocView::ZoomFont( int delta )
{
    if ( m_font.isNull() )
        return;
#if 1
    int sz = m_font_size;
    for (int i=0; i<15; i++)
    {
        sz += delta;
        int nsz = findBestFit( m_font_sizes, sz, m_font_sizes_cyclic );
        if ( nsz != m_font_size ) {
            setFontSize( nsz );
            return;
        }
        if (sz<12)
            break;
    }
#else
    LVFontRef nfnt;
    int sz = m_font->getHeight();
    for (int i=0; i<15; i++)
    {
        sz += delta;
        nfnt = fontMan->GetFont( sz, 400, false, DEFAULT_FONT_FAMILY, lString8(DEFAULT_FONT_NAME) );
        if ( !nfnt.isNull() && nfnt->getHeight() != m_font->getHeight() )
        {
            // found!
            //ldomXPointer bm = getBookmark();
            m_font_size = nfnt->getHeight();
            Render();
            goToBookmark(_posBookmark);
            return;
        }
        if (sz<12)
            break;
    }
#endif
}

/// sets current bookmark
void LVDocView::setBookmark( ldomXPointer bm )
{
    _posBookmark = bm;
}

/// get view height
int LVDocView::GetHeight()
{
    return (m_rotateAngle & 1) ? m_dx : m_dy;
}

/// get view width
int LVDocView::GetWidth()
{
    return (m_rotateAngle & 1) ? m_dy : m_dx;
}

/// sets rotate angle
void LVDocView::SetRotateAngle( cr_rotate_angle_t angle )
{
    if ( m_rotateAngle==angle )
        return;
    m_props->setInt( PROP_ROTATE_ANGLE, ((int)angle) & 3 );
    m_imageCache.clear();
    LVLock lock(getMutex());
    if ( (m_rotateAngle & 1) == (angle & 1) ) {
        m_rotateAngle = angle;
        return;
    }
    m_rotateAngle = angle;
    int ndx = (angle&1) ? m_dx : m_dy;
    int ndy = (angle&1) ? m_dy : m_dx;
    Resize( ndx, ndy );
}

void LVDocView::Resize( int dx, int dy )
{
    //LVCHECKPOINT("Resize");
    if (dx<80 || dx>3000)
        dx = 80;
    if (dy<80 || dy>3000)
        dy = 80;
    if ( m_rotateAngle==CR_ROTATE_ANGLE_90 || m_rotateAngle==CR_ROTATE_ANGLE_270 ) {
        int tmp = dx;
        dx = dy;
        dy = tmp;
    }
    m_imageCache.clear();
    //m_drawbuf.Resize(dx, dy);
    if (m_doc)
    {
        //ldomXPointer bm = getBookmark();
        if (dx!=m_dx || dy!=m_dy || m_view_mode!=DVM_SCROLL || !m_is_rendered)
        {
            m_dx = dx;
            m_dy = dy;
            updateLayout();
            requestRender();
        }
        goToBookmark(_posBookmark);
    }
    m_dx = dx;
    m_dy = dy;
}

#define XS_IMPLEMENT_SCHEME 1
#include "../include/fb2def.h"

#if 0
void SaveBase64Objects( ldomNode * node )
{
    if ( !node->isElement() || node->getNodeId()!=el_binary )
        return;
    lString16 name = node->getAttributeValue(attr_id);
    if ( name.empty() )
        return;
    fprintf( stderr, "opening base64 stream...\n" );
    LVStreamRef in = node->createBase64Stream();
    if ( in.isNull() )
        return;
    fprintf( stderr, "base64 stream opened: %d bytes\n", (int)in->GetSize() );
    fprintf( stderr, "opening out stream...\n" );
    LVStreamRef outstream = LVOpenFileStream( name.c_str(), LVOM_WRITE );
    if (outstream.isNull())
        return;
    //outstream->Write( "test", 4, NULL );
    fprintf( stderr, "streams opened, copying...\n" );
/*
    lUInt8 dbuf[128000];
    lvsize_t bytesRead = 0;
    if ( in->Read( dbuf, 128000, &bytesRead )==LVERR_OK )
    {
        fprintf(stderr, "Read %d bytes, writing...\n", (int) bytesRead );
        //outstream->Write( "test2", 5, NULL );
        //outstream->Write( "test3", 5, NULL );
        outstream->Write( dbuf, 100, NULL );
        outstream->Write( dbuf, bytesRead, NULL );
        //outstream->Write( "test4", 5, NULL );
    }
*/
    LVPumpStream( outstream, in );
    fprintf(stderr, "...\n");
}
#endif

/// returns pointer to bookmark/last position containter of currently opened file
CRFileHistRecord * LVDocView::getCurrentFileHistRecord()
{
    if ( m_filename.empty() )
        return NULL;
    CRLog::trace("LVDocView::getCurrentFileHistRecord()");
    CRLog::trace("get title, authors, series");
    lString16 title = getTitle();
    lString16 authors = getAuthors();
    lString16 series = getSeries();
    CRLog::trace("get bookmark");
    ldomXPointer bmk = getBookmark();
    CRFileHistRecord * res = m_hist.savePosition( m_filename, m_filesize,
        title, authors, series, bmk );
    CRLog::trace("savePosition() returned");
    return res;
}

/// save last file position
void LVDocView::savePosition()
{
	getCurrentFileHistRecord();
}

/// restore last file position
void LVDocView::restorePosition()
{
    CRLog::trace("LVDocView::restorePosition()");
    if ( m_filename.empty() )
        return;
    LVLock lock(getMutex());
    //checkRender();
    ldomXPointer pos = m_hist.restorePosition( m_doc, m_filename, m_filesize );
    if ( !pos.isNull() ) {
        //goToBookmark( pos );
    	CRLog::info("LVDocView::restorePosition() - last position is found");
        _posBookmark = pos; //getBookmark();
        m_posIsSet = false;
    } else {
    	CRLog::info("LVDocView::restorePosition() - last position not found for file %s, size %d", UnicodeToUtf8(m_filename).c_str(), (int)m_filesize);
    }
}

static void FileToArcProps( CRPropRef props )
{
    lString16 s = props->getStringDef(DOC_PROP_FILE_NAME);
    if ( !s.empty() )
        props->setString(DOC_PROP_ARC_NAME, s );
    s = props->getStringDef(DOC_PROP_FILE_PATH);
    if ( !s.empty() )
        props->setString(DOC_PROP_ARC_PATH, s );
    s = props->getStringDef(DOC_PROP_FILE_SIZE);
    if ( !s.empty() )
        props->setString(DOC_PROP_ARC_SIZE, s );
    props->setString( DOC_PROP_FILE_NAME, lString16() );
    props->setString( DOC_PROP_FILE_PATH, lString16() );
    props->setString( DOC_PROP_FILE_SIZE, lString16() );
    props->setHex(DOC_PROP_FILE_CRC32, 0);
}

/// load document from file
bool LVDocView::LoadDocument( const lChar16 * fname )
{
    if ( !fname || !fname[0] )
        return false;

    Clear();

    // split file path and name
    lString16 filename16( fname );
    lString16 fn = LVExtractFilename( filename16 );
    lString16 dir = LVExtractPath( filename16 );

    CRLog::info( "Loading document %s : fn=%s, dir=%s", LCSTR(filename16), LCSTR(fn), LCSTR(dir) );
#if 0
    int i;
    int last_slash = -1;
    lChar16 slash_char = 0;
    for ( i=0; fname[i]; i++ ) {
        if ( fname[i]=='\\' || fname[i]=='/' ) {
            last_slash = i;
            slash_char = fname[i];
        }
    }
    lString16 dir;
    if ( last_slash==-1 )
        dir = L".";
    else if ( last_slash == 0 )
        dir << slash_char;
    else
        dir = lString16( fname, last_slash );
    lString16 fn( fname + last_slash + 1 );
#endif

    m_doc_props->setString(DOC_PROP_FILE_PATH, dir);
    m_container = LVOpenDirectory(dir.c_str());
    if ( m_container.isNull() )
        return false;
    LVStreamRef stream = m_container->OpenStream(fn.c_str(), LVOM_READ);
    if (!stream)
        return false;
    m_doc_props->setString(DOC_PROP_FILE_NAME, fn);
    m_doc_props->setString(DOC_PROP_FILE_SIZE, lString16::itoa((int)stream->GetSize()));
    m_doc_props->setHex(DOC_PROP_FILE_CRC32, stream->crc32());


    if ( LoadDocument( stream ) ) {
        m_filename = lString16(fname);
        m_stream.Clear();
        return true;
    }
    m_stream.Clear();
    return false;
}

void LVDocView::createDefaultDocument( lString16 title, lString16 message )
{
    Clear();
    m_showCover = false;
    createEmptyDocument();

    ldomDocumentWriter writer(m_doc);

    m_pos = 0;

    // make fb2 document structure
    writer.OnTagOpen( NULL, L"?xml" );
    writer.OnAttribute( NULL, L"version", L"1.0" );
    writer.OnAttribute( NULL, L"encoding", L"utf-8" );
    writer.OnEncoding( L"utf-8", NULL );
    writer.OnTagClose( NULL, L"?xml" );
    writer.OnTagOpen( NULL, L"FictionBook" );
      // DESCRIPTION
      writer.OnTagOpen( NULL, L"description" );
        writer.OnTagOpen( NULL, L"title-info" );
          writer.OnTagOpen( NULL, L"book-title" );
            writer.OnText( title.c_str(), title.length(), 0 );
          writer.OnTagClose( NULL, L"book-title" );
        writer.OnTagOpen( NULL, L"title-info" );
      writer.OnTagClose( NULL, L"description" );
      // BODY
      writer.OnTagOpen( NULL, L"body" );
        //m_callback->OnTagOpen( NULL, L"section" );
          // process text
      if ( title.length() ) {
          writer.OnTagOpen( NULL, L"title" );
          writer.OnTagOpen( NULL, L"p" );
            writer.OnText( title.c_str(), title.length(), 0 );
          writer.OnTagClose( NULL, L"p" );
          writer.OnTagClose( NULL, L"title" );
      }
          writer.OnTagOpen( NULL, L"p" );
            writer.OnText( message.c_str(), message.length(), 0 );
          writer.OnTagClose( NULL, L"p" );
        //m_callback->OnTagClose( NULL, L"section" );
      writer.OnTagClose( NULL, L"body" );
    writer.OnTagClose( NULL, L"FictionBook" );

    // set stylesheet
    m_doc->setStyleSheet( m_stylesheet.c_str(), true );
    //m_doc->getStyleSheet()->clear();
    //m_doc->getStyleSheet()->parse(m_stylesheet.c_str());

    m_doc_props->clear();
    m_doc->setProps( m_doc_props );

    m_doc_props->setString(DOC_PROP_TITLE, title);

    requestRender();
}

class EpubItem {
public:
    lString16 href;
    lString16 mediaType;
    lString16 id;
    EpubItem()
    { }
    EpubItem( const EpubItem & v )
        : href(v.href), mediaType(v.mediaType), id(v.id)
    { }
    EpubItem & operator = ( const EpubItem & v )
    {
        href = v.href;
        mediaType = v.mediaType;
        id = v.id;
        return *this;
    }
};

class EpubItems : public LVPtrVector<EpubItem> {
public:
    EpubItem * findById( const lString16 & id )
    {
        if ( id.empty() )
            return NULL;
        for ( int i=0; i<length(); i++ )
            if ( get(i)->id == id )
                return get(i);
        return NULL;
    }
};

/// load document from stream
bool LVDocView::LoadDocument( LVStreamRef stream )
{
    m_swapDone = false;
    if ( m_callback ) {
            m_callback->OnLoadFileStart( m_doc_props->getStringDef( DOC_PROP_FILE_NAME, "" ) );
    }
    LVLock lock(getMutex());
    {
        clearImageCache();
        m_filesize = stream->GetSize();
        m_stream = stream;

    #if (USE_ZLIB==1)

        m_arc = LVOpenArchieve( m_stream );
        if (!m_arc.isNull())
        {
            m_container = m_arc;
            // read "mimetype" file contents from root of archive
            lString16 mimeType;
            {
                LVStreamRef mtStream = m_arc->OpenStream(L"mimetype", LVOM_READ );
                if ( !mtStream.isNull() ) {
                    int size = mtStream->GetSize();
                    if ( size>4 && size<100 ) {
                        LVArray<char> buf( size+1, '\0' );
                        if ( mtStream->Read( buf.get(), size, NULL )==LVERR_OK ) {
                            for ( int i=0; i<size; i++ )
                                if ( buf[i]<32 || ((unsigned char)buf[i])>127 )
                                    buf[i] = 0;
                            buf[size] = 0;
                            if ( buf[0] )
                                mimeType = Utf8ToUnicode( lString8( buf.get() ) );
                        }
                    }
                }
            }
            // EPUB support
            if ( mimeType == L"application/epub+zip" ) {
                if ( m_doc )
                    delete m_doc;
                createEmptyDocument();
                m_doc->setProps( m_doc_props );

                lString16 rootfilePath;
                lString16 rootfileMediaType;
                // read container.xml
                {
                    LVStreamRef container_stream = m_arc->OpenStream(L"META-INF/container.xml", LVOM_READ);
                    if ( !container_stream.isNull() ) {
                        ldomDocument * doc = LVParseXMLStream( container_stream );
                        if ( doc ) {
                            ldomNode * rootfile = doc->nodeFromXPath( lString16(L"container/rootfiles/rootfile") );
                            if ( rootfile && rootfile->isElement() ) {
                                rootfilePath = rootfile->getAttributeValue(L"full-path");
                                rootfileMediaType = rootfile->getAttributeValue(L"media-type");
                            }
                            delete doc;
                        }
                    }
                }
                // read content.opf
                EpubItems epubItems;
                //EpubItem * epubToc = NULL; //TODO
                LVArray<EpubItem*> spineItems;
                lString16 codeBase;
                lString16 css;
                if ( !rootfilePath.empty() && rootfileMediaType==L"application/oebps-package+xml" ) {
                    //
                    {
                        int lastSlash = -1;
                        for ( int i=0; i<(int)rootfilePath.length(); i++ )
                            if ( rootfilePath[i]=='/' )
                                lastSlash = i;
                        if ( lastSlash>0 )
                            codeBase = lString16( rootfilePath.c_str(), lastSlash + 1);
                    }
                    LVStreamRef content_stream = m_arc->OpenStream(rootfilePath.c_str(), LVOM_READ);
                    if ( !content_stream.isNull() ) {
                        ldomDocument * doc = LVParseXMLStream( content_stream );
                        if ( doc ) {
                            m_doc_props->setString(DOC_PROP_TITLE, doc->textFromXPath( lString16(L"package/metadata/title") ));
                            m_doc_props->setString(DOC_PROP_AUTHORS, doc->textFromXPath( lString16(L"package/metadata/creator") ));
                            // items
                            for ( int i=1; i<50000; i++ ) {
                                ldomNode * item = doc->nodeFromXPath( lString16(L"package/manifest/item[") + lString16::itoa(i) + L"]" );
                                if ( !item )
                                    break;
                                lString16 href = item->getAttributeValue(L"href");
                                lString16 mediaType = item->getAttributeValue(L"media-type");
                                lString16 id = item->getAttributeValue(L"id");
                                if ( !href.empty() && !id.empty() ) {
                                    EpubItem * epubItem = new EpubItem;
                                    epubItem->href = href;
                                    epubItem->id = id;
                                    epubItem->mediaType = mediaType;
                                    epubItems.add( epubItem );
                                }
                                if ( mediaType==L"text/css" ) {
                                    lString16 name = codeBase + href;
                                    LVStreamRef cssStream = m_arc->OpenStream(name.c_str(), LVOM_READ);
                                    if ( !cssStream.isNull() ) {
                                        css << L"\n";
                                        css << LVReadTextFile( cssStream );
                                    }
                                }
                            }
                            // spine == itemrefs
                            if ( epubItems.length()>0 ) {
                                ldomNode * spine = doc->nodeFromXPath( lString16(L"package/spine") );
                                if ( spine ) {


                                    //EpubItem * epubToc = epubItems.findById( spine->getAttributeValue(L"toc") ); //TODO
                                    for ( int i=1; i<50000; i++ ) {
                                        ldomNode * item = doc->nodeFromXPath( lString16(L"package/spine/itemref[") + lString16::itoa(i) + L"]" );
                                        if ( !item )
                                            break;
                                        EpubItem * epubItem = epubItems.findById( item->getAttributeValue(L"idref") );
                                        if ( epubItem ) {
                                            // TODO: add to document
                                            spineItems.add( epubItem );
                                        }
                                    }

                                }
                            }
                            delete doc;
                        }
                    }
                }

                if ( spineItems.length()>0 ) {
                    lUInt32 saveFlags = m_doc ? m_doc->getDocFlags() : DOC_FLAG_DEFAULTS;
                    m_is_rendered = false;
                    m_doc->setDocFlags( saveFlags );
                    m_doc->setContainer( m_container );

                    ldomDocumentWriter writer(m_doc);
                    m_doc->setNodeTypes( fb2_elem_table );
                    m_doc->setAttributeTypes( fb2_attr_table );
                    m_doc->setNameSpaceTypes( fb2_ns_table );
                    m_doc->setCodeBase( codeBase );
                    ldomDocumentFragmentWriter appender(&writer, lString16(L"body"));
                    writer.OnStart(NULL);
                    writer.OnTagOpen(L"", L"body");
                    int fragmentCount = 0;
                    for ( int i=0; i<spineItems.length(); i++ ) {
                        if ( spineItems[i]->mediaType==L"application/xhtml+xml" ) {
                            lString16 name = codeBase + spineItems[i]->href;
                            {
                                LVStreamRef stream = m_arc->OpenStream(name.c_str(), LVOM_READ);
                                if ( !stream.isNull() ) {
                                    LVXMLParser parser(stream, &appender);
                                    if ( parser.CheckFormat() && parser.Parse() ) {
                                        // valid
                                        fragmentCount++;
                                    }
                                }
                            }
                        }
                    }
                    writer.OnTagClose(L"", L"body");
                    writer.OnStop();
                    CRLog::debug("EPUB: %d documents merged", fragmentCount);
                    if ( fragmentCount>0 ) {

                        // set stylesheet
                        //m_doc->getStyleSheet()->clear();
                        m_doc->setStyleSheet( NULL, true );
                        //m_doc->getStyleSheet()->parse(m_stylesheet.c_str());
                        if ( !css.empty() && m_doc->getDocFlag(DOC_FLAG_ENABLE_INTERNAL_STYLES) ) {

                            m_doc->setStyleSheet( "p.p { text-align: justify }\n"
                                "svg { text-align: center }\n"
                                "i { display: inline; font-style: italic }\n"
                                "b { display: inline; font-weight: bold }\n"
                                "abbr { display: inline }\n"
                                "acronym { display: inline }\n"
                                "address { display: inline }\n"
                                "p.title-p { hyphenate: none }\n"
//abbr, acronym, address, blockquote, br, cite, code, dfn, div, em, h1, h2, h3, h4, h5, h6, kbd, p, pre, q, samp, span, strong, var
                            , false);
                            m_doc->setStyleSheet( UnicodeToUtf8(css).c_str(), false );
                            //m_doc->getStyleSheet()->parse(UnicodeToUtf8(css).c_str());
                        } else {
                            //m_doc->getStyleSheet()->parse(m_stylesheet.c_str());
                            m_doc->setStyleSheet( m_stylesheet.c_str(), false );
                        }
#if 0
                        LVStreamRef out = LVOpenFileStream( L"c:\\doc.xml" , LVOM_WRITE );
                        if ( !out.isNull() )
                            m_doc->saveToStream( out, "utf-8" );
#endif

                        // DONE!
                        setDocFormat( doc_format_epub );
                        requestRender();
						if ( m_callback ) {
							m_callback->OnLoadFileEnd( );
						}
                        return true;
                    }
                }
                setDocFormat( doc_format_none );
                createDefaultDocument( lString16(L"ERROR: Error reading EPUB format"), lString16(L"Cannot open document") );
				if ( m_callback ) {
					m_callback->OnLoadFileError( lString16("Error reading EPUB document") );
				}
                return false;
            }
            // archieve
            FileToArcProps( m_doc_props );
            m_container = m_arc;
            m_doc_props->setInt( DOC_PROP_ARC_FILE_COUNT, m_arc->GetObjectCount() );
            bool found = false;
            lString16 htmlExt(L".html");
            lString16 htmExt(L".htm");
            lString16 fb2Ext(L".fb2");
            lString16 rtfExt(L".rtf");
            lString16 txtExt(L".txt");
            lString16 fbdExt(L".fbd");
            int htmCount = 0;
            int fb2Count = 0;
            int rtfCount = 0;
            int txtCount = 0;
            int fbdCount = 0;
            lString16 defHtml;
            lString16 firstGood;
            for (int i=0; i<m_arc->GetObjectCount(); i++)
            {
                const LVContainerItemInfo * item = m_arc->GetObjectInfo(i);
                if (item)
                {
                    if ( !item->IsContainer() )
                    {
                        lString16 name( item->GetName() );
                        CRLog::debug("arc item[%d] : %s", i, LCSTR(name) );
                        lString16 s = name;
                        s.lowercase();
                        bool nameIsOk = true;
                        if ( s.endsWith(htmExt) || s.endsWith(htmlExt) ) {
                            lString16 nm = LVExtractFilenameWithoutExtension( s );
                            if ( nm==L"index" || nm==L"default" )
                                defHtml = name;
                            htmCount++;
                        } else if ( s.endsWith(fb2Ext) ) {
                            fb2Count++;
                        } else if ( s.endsWith(rtfExt) ) {
                            rtfCount++;
                        } else if ( s.endsWith(txtExt) ) {
                            txtCount++;
                        } else if ( s.endsWith(fbdExt) ) {
                            fbdCount++;
                        } else {
                            nameIsOk = false;
                        }
                        if ( nameIsOk ) {
                            if ( firstGood.empty() )
                                firstGood = name;
                        }
                        if ( name.length() >= 5 )
                        {
                            name.lowercase();
                            const lChar16 * pext = name.c_str() + name.length() - 4;
                            if ( pext[0]=='.' && pext[1]=='f' && pext[2]=='b' && pext[3]=='2')
                                nameIsOk = true;
                            else if ( pext[0]=='.' && pext[1]=='t' && pext[2]=='x' && pext[3]=='t')
                                nameIsOk = true;
                            else if ( pext[0]=='.' && pext[1]=='r' && pext[2]=='t' && pext[3]=='f')
                                nameIsOk = true;
                        }
                        if ( !nameIsOk )
                            continue;
                    }
                }
            }
            lString16 fn = !defHtml.empty() ? defHtml : firstGood;
            if ( !fn.empty() ) {
                m_stream = m_arc->OpenStream( fn.c_str(), LVOM_READ );
                if ( !m_stream.isNull() ) {
                    CRLog::debug("Opened archive stream %s", LCSTR(fn) );
                    m_doc_props->setString(DOC_PROP_FILE_NAME, fn);
                    m_doc_props->setString(DOC_PROP_CODE_BASE, LVExtractPath(fn) );
                    m_doc_props->setString(DOC_PROP_FILE_SIZE, lString16::itoa((int)m_stream->GetSize()));
                    m_doc_props->setHex(DOC_PROP_FILE_CRC32, m_stream->crc32());
                    found = true;
                }
            }
            // opened archieve stream
            if ( !found )
            {
                Clear();
				if ( m_callback ) {
					m_callback->OnLoadFileError( lString16("File with supported extension not fouind in archive.") );
				}
                return false;
            }

        }
        else

    #endif //USE_ZLIB

        {
    #if 1
            //m_stream = LVCreateBufferedStream( m_stream, FILE_STREAM_BUFFER_SIZE );
    #else
            LVStreamRef stream = LVCreateBufferedStream( m_stream, FILE_STREAM_BUFFER_SIZE );
            lvsize_t sz = stream->GetSize();
            const lvsize_t bufsz = 0x1000;
            lUInt8 buf[bufsz];
            lUInt8 * fullbuf = new lUInt8 [sz];
            stream->SetPos(0);
            stream->Read(fullbuf, sz, NULL);
            lvpos_t maxpos = sz - bufsz;
            for (int i=0; i<1000; i++)
            {
                lvpos_t pos = (lvpos_t)((((lUInt64)i) * 1873456178) % maxpos);
                stream->SetPos( pos );
                lvsize_t readsz = 0;
                stream->Read( buf, bufsz, &readsz );
                if (readsz != bufsz)
                {
                    //
                    fprintf(stderr, "data read error!\n");
                }
                for (int j=0; j<bufsz; j++)
                {
                    if (fullbuf[pos+j] != buf[j])
                    {
                        fprintf(stderr, "invalid data!\n");
                    }
                }
            }
            delete fullbuf;
    #endif
            LVStreamRef tcrDecoder = LVCreateTCRDecoderStream( m_stream );
            if ( !tcrDecoder.isNull() )
                m_stream = tcrDecoder;
        }

        return ParseDocument();

    }
}

static const char * AC_P[]  = {"p", "p", "hr", NULL};
static const char * AC_COL[] = {"col", NULL};
static const char * AC_LI[] = {"li", "li", "p", NULL};
static const char * AC_UL[] = {"ul", "li", "p", NULL};
static const char * AC_OL[] = {"ol", "li", "p", NULL};
static const char * AC_DD[] = {"dd", "dd", "p", NULL};
static const char * AC_DL[] = {"dl", "dt", "p", NULL};
static const char * AC_DT[] = {"dt", "dt", "dd", "p", NULL};
static const char * AC_BR[] = {"br", NULL};
static const char * AC_HR[] = {"hr", NULL};
static const char * AC_IMG[]= {"img", NULL};
static const char * AC_TD[] = {"td", "td", "th", NULL};
static const char * AC_TH[] = {"th", "th", "td", NULL};
static const char * AC_TR[] = {"tr", "tr", "thead", "tfoot", "tbody", NULL};
static const char * AC_DIV[] = {"div", "p", NULL};
static const char * AC_TABLE[] = {"table", "p", NULL};
static const char * AC_THEAD[] = {"thead", "tr", "thead", "tfoot", "tbody", NULL};
static const char * AC_TFOOT[] = {"tfoot", "tr", "thead", "tfoot", "tbody", NULL};
static const char * AC_TBODY[] = {"tbody", "tr", "thead", "tfoot", "tbody", NULL};
static const char * AC_OPTION[] = {"option", "option", NULL};
static const char * AC_PRE[] = {"pre", "pre", NULL};
static const char * AC_INPUT[] = {"input", NULL};
static const char * *
HTML_AUTOCLOSE_TABLE[] = {
    AC_INPUT,
    AC_OPTION,
    AC_PRE,
    AC_P,
    AC_LI,
    AC_UL,
    AC_OL,
    AC_TD,
    AC_TH,
    AC_DD,
    AC_DL,
    AC_DT,
    AC_TR,
    AC_COL,
    AC_BR,
    AC_HR,
    AC_IMG,
    AC_DIV,
    AC_THEAD,
    AC_TFOOT,
    AC_TBODY,
    AC_TABLE,
    NULL
};

const lChar16 * getDocFormatName( doc_format_t fmt )
{
    switch (fmt) {
    case doc_format_fb2:
        return L"FictionBook (FB2)";
    case doc_format_txt:
        return L"Plain text (TXT)";
    case doc_format_rtf:
        return L"Rich text (RTF)";
    case doc_format_epub:
        return L"EPUB";
    case doc_format_html:
        return L"HTML";
    case doc_format_txt_bookmark:
        return L"CR3 TXT Bookmark";
    default:
        return L"Unknown format";
    }
}

/// sets current document format
void LVDocView::setDocFormat( doc_format_t fmt )
{
    m_doc_format = fmt;
    lString16 desc( getDocFormatName( fmt ) );
    m_doc_props->setString(DOC_PROP_FILE_FORMAT, desc );
    m_doc_props->setInt(DOC_PROP_FILE_FORMAT_ID, (int)fmt );
}

/// create document and set flags
void LVDocView::createEmptyDocument()
{
    m_posIsSet = false;
    m_swapDone = false;
    _posBookmark = ldomXPointer();
    lUInt32 saveFlags = 0;

    //m_doc ? m_doc->getDocFlags() : DOC_FLAG_DEFAULTS;
    m_is_rendered = false;
    if ( m_doc )
        delete m_doc;
    m_doc = new ldomDocument();
    m_cursorPos.clear();
    m_markRanges.clear();
    _posBookmark.clear();
    m_section_bounds.clear();
    m_section_bounds_valid = false;
    m_posIsSet = false;
    m_swapDone = false;

    m_doc->setProps( m_doc_props );
    m_doc->setDocFlags( 0 );
    m_doc->setDocFlag( DOC_FLAG_PREFORMATTED_TEXT, m_props->getBoolDef( PROP_TXT_OPTION_PREFORMATTED, false) );
    m_doc->setDocFlag( DOC_FLAG_ENABLE_FOOTNOTES, m_props->getBoolDef( PROP_FOOTNOTES, true) );
    m_doc->setDocFlag( DOC_FLAG_ENABLE_INTERNAL_STYLES, m_props->getBoolDef( PROP_EMBEDDED_STYLES, true) );

    m_doc->setContainer( m_container );
    m_doc->setNodeTypes( fb2_elem_table );
    m_doc->setAttributeTypes( fb2_attr_table );
    m_doc->setNameSpaceTypes( fb2_ns_table );
}

bool LVDocView::ParseDocument( )
{

    createEmptyDocument();

    if ( m_stream->GetSize() > DOCUMENT_CACHING_SIZE_THRESHOLD ) {
        // try loading from cache

        //lString16 fn( m_stream->GetName() );
        lString16 fn = m_doc_props->getStringDef(DOC_PROP_FILE_NAME, "untitled" );
        fn = LVExtractFilename( fn );
        lUInt32 crc = 0;
        m_stream->crc32( crc );
        CRLog::debug("Check whether document %s crc %08x exists in cache", UnicodeToUtf8(fn).c_str(), crc );

	    // set stylesheet
        m_doc->setStyleSheet( m_stylesheet.c_str(), true );
	    //m_doc->getStyleSheet()->clear();
	    //m_doc->getStyleSheet()->parse(m_stylesheet.c_str());

        if ( m_doc->openFromCache( ) ) {
            CRLog::info("Document is found in cache, will reuse");

            // update document format id
            int fmt = m_doc_props->getIntDef(DOC_PROP_FILE_FORMAT_ID, doc_format_fb2 );
            if ( fmt<doc_format_fb2 || fmt>doc_format_txt_bookmark )
                fmt = doc_format_fb2;
            m_doc_format = (doc_format_t)fmt;
            // notify about format detection, to allow setting format-specific CSS
            if ( m_callback ) {
                m_callback->OnLoadFileFormatDetected( getDocFormat() );
            }

            // TODO: init doc format
            //m_doc_format = fmt;


		    // set stylesheet
            m_doc->setStyleSheet( m_stylesheet.c_str(), true );

            lString16 docstyle = m_doc->createXPointer(L"/FictionBook/stylesheet").getText();
            if ( !docstyle.empty() && m_doc->getDocFlag(DOC_FLAG_ENABLE_INTERNAL_STYLES) ) {
                //m_doc->getStyleSheet()->parse(UnicodeToUtf8(docstyle).c_str());
                m_doc->setStyleSheet( UnicodeToUtf8(docstyle).c_str(), false );
            }

            m_showCover = !getCoverPageImage().isNull();

            return true;
        }
        CRLog::info("Cannot get document from cache, parsing...");
    }

    {
        ldomDocumentWriter writer(m_doc);
        ldomDocumentWriterFilter writerFilter(m_doc, false, HTML_AUTOCLOSE_TABLE);

        if ( m_stream->GetSize()<5 ) {
            createDefaultDocument( lString16(L"ERROR: Wrong document size"), lString16(L"Cannot open document") );
            return false;
        }

        /// FB2 format
        setDocFormat( doc_format_fb2 );
        LVFileFormatParser * parser = new LVXMLParser(m_stream, &writer);
        if ( !parser->CheckFormat() ) {
            delete parser;
            parser = NULL;
        } else {
        }

        /// RTF format
        if ( parser==NULL ) {
            setDocFormat( doc_format_rtf );
            parser = new LVRtfParser(m_stream, &writer);
            if ( !parser->CheckFormat() ) {
                delete parser;
                parser = NULL;
            }
        }

        /// HTML format
        if ( parser==NULL ) {
            setDocFormat( doc_format_html );
            parser = new LVHTMLParser(m_stream, &writerFilter);
            if ( !parser->CheckFormat() ) {
                delete parser;
                parser = NULL;
            } else {
            }
        }
        ///cool reader bookmark in txt format
        if ( parser==NULL ) {

            //m_text_format = txt_format_pre; // DEBUG!!!
            setDocFormat( doc_format_txt_bookmark );
            parser = new LVTextBookmarkParser(m_stream, &writer );
            if ( !parser->CheckFormat() ) {
                delete parser;
                parser = NULL;
            }
        } else {
        }

        /// plain text format
        if ( parser==NULL ) {

            //m_text_format = txt_format_pre; // DEBUG!!!
            setDocFormat( doc_format_txt );
            parser = new LVTextParser(m_stream, &writer, getTextFormatOptions()==txt_format_pre );
            if ( !parser->CheckFormat() ) {
                delete parser;
                parser = NULL;
            }
        } else {
        }

		// unknown format
		if ( !parser ) {
			setDocFormat( doc_format_none );
			createDefaultDocument( lString16(L"ERROR: Unknown document format"), lString16(L"Cannot open document") );
			if ( m_callback ) {
				m_callback->OnLoadFileError( lString16("Unknown document format") );
			}
			return false;
		}

		if ( m_callback ) {
			m_callback->OnLoadFileFormatDetected( getDocFormat() );
		}


		// set stylesheet
		//m_doc->getStyleSheet()->clear();
		//m_doc->getStyleSheet()->parse(m_stylesheet.c_str());
        m_doc->setStyleSheet( m_stylesheet.c_str(), true );

        // parse
        parser->setProgressCallback( m_callback );
        if ( !parser->Parse() ) {
			delete parser;
			if ( m_callback ) {
				m_callback->OnLoadFileError( lString16("Bad document format") );
			}
			createDefaultDocument( lString16(L"ERROR: Bad document format"), lString16(L"Cannot open document") );
			return false;
        }
		delete parser;
		m_pos = 0;

        if ( m_doc_format == doc_format_html ) {
            static lUInt16 path[] = { el_html, el_head, el_title, 0 };
            ldomNode * el = m_doc->getRootNode()->findChildElement( path );
            if ( el!=NULL ) {
                lString16 s = el->getText();
                if ( !s.empty() ) {
                    m_doc_props->setString(DOC_PROP_TITLE, s);
                }
            }
        }

        lString16 docstyle = m_doc->createXPointer(L"/FictionBook/stylesheet").getText();
        if ( !docstyle.empty() && m_doc->getDocFlag(DOC_FLAG_ENABLE_INTERNAL_STYLES) ) {
            //m_doc->getStyleSheet()->parse(UnicodeToUtf8(docstyle).c_str());
            m_doc->setStyleSheet( UnicodeToUtf8(docstyle).c_str(), false );
        }

    #if 0 //def _DEBUG
            LVStreamRef ostream = LVOpenFileStream( "test_save_source.xml", LVOM_WRITE );
            m_doc->saveToStream( ostream, "utf-16" );
    #endif
    #if 0
        {
            LVStreamRef ostream = LVOpenFileStream( "test_save.fb2", LVOM_WRITE );
            m_doc->saveToStream( ostream, "utf-16" );
            m_doc->getRootNode()->recurseElements( SaveBase64Objects );
        }
    #endif


        //m_doc->getProps()->clear();
        if ( m_doc_props->getStringDef(DOC_PROP_TITLE, "").empty() ) {
            m_doc_props->setString(DOC_PROP_AUTHORS, extractDocAuthors( m_doc ));
            m_doc_props->setString(DOC_PROP_TITLE, extractDocTitle( m_doc ));
            m_doc_props->setString(DOC_PROP_SERIES_NAME, extractDocSeries( m_doc ));
        }
    }
    //m_doc->persist();

    m_showCover = !getCoverPageImage().isNull();

    requestRender();
	if ( m_callback ) {
		m_callback->OnLoadFileEnd( );
	}

#if 0 // test serialization
    SerialBuf buf( 1024 );
    m_doc->serializeMaps(buf);
    if ( !buf.error() ) {
        int sz = buf.pos();
        SerialBuf buf2( buf.buf(), buf.pos() );
        ldomDocument * newdoc = new ldomDocument();
        if ( newdoc->deserializeMaps( buf2 ) ) {
            delete newdoc;
        }
    }
#endif
#if 0// test swap to disk
    lString16 cacheFile = lString16("/tmp/cr3swap.bin");
    bool res = m_doc->swapToCacheFile( cacheFile );
    if ( !res ) {
        CRLog::error( "Failed to swap to disk" );
        return false;
    }
#endif
#if 0 // test restore from swap
    delete m_doc;
    m_doc = new ldomDocument();
    res = m_doc->openFromCacheFile( cacheFile );
    m_doc->setDocFlags( saveFlags );
    m_doc->setContainer( m_container );
    if ( !res ) {
        CRLog::error( "Failed loading of swap from disk" );
        return false;
    }
	m_doc->getStyleSheet()->clear();
	m_doc->getStyleSheet()->parse(m_stylesheet.c_str());
#endif


    return true;
}

void LVDocView::swapToCache()
{
    if ( m_swapDone )
        return;
    {
        // try swapping to cache
        //lString16 fn( m_stream->GetName() );
        //fn = LVExtractFilename( fn );
        //lUInt32 crc = 0;
        //m_stream->crc32( crc );
        m_doc->swapToCache( );
        m_swapDone = true;
    }
}

bool LVDocView::LoadDocument( const char * fname )
{
    if ( !fname || !fname[0] )
        return false;
    return LoadDocument( LocalToUnicode(lString8(fname)).c_str() );
}

/// returns XPointer to middle paragraph of current page
ldomXPointer LVDocView::getCurrentPageMiddleParagraph()
{
    LVLock lock(getMutex());
    checkPos();
    ldomXPointer ptr;
    if ( !m_doc )
        return ptr;

    if ( getViewMode()==DVM_SCROLL ) {
        // SCROLL mode
        int starty = m_pos;
        int endy = m_pos + m_dy;
        int fh = GetFullHeight();
        if ( endy>=fh )
            endy = fh-1;
        ptr = m_doc->createXPointer( lvPoint( 0, (starty + endy)/ 2 ) );
    } else {
        // PAGES mode
        int pageIndex = getCurPage();
        if ( pageIndex<0 || pageIndex>=m_pages.length() )
            pageIndex = getCurPage();
        LVRendPageInfo * page = m_pages[ pageIndex ];
        if ( page->type==PAGE_TYPE_NORMAL)
            ptr = m_doc->createXPointer( lvPoint( 0, page->start + page->height/2 ) );
    }
    if ( ptr.isNull() )
        return ptr;
    ldomXPointerEx p( ptr );
    if ( !p.isVisibleFinal() )
        if ( !p.ensureFinal() )
	        if ( !p.prevVisibleFinal() )
		        if ( !p.nextVisibleFinal() )
			        return ptr;
    return ldomXPointer( p );
}

/// returns bookmark
ldomXPointer LVDocView::getBookmark()
{
    LVLock lock(getMutex());
    checkPos();
    ldomXPointer ptr;
    if ( m_doc )
        ptr = m_doc->createXPointer( lvPoint( 0, m_pos ) );
    return ptr;
/*
    lUInt64 pos = m_pos;
    if (m_view_mode==DVM_PAGES)
        m_pos += m_dy/3;
    lUInt64 h = GetFullHeight();
    if (h<1)
        h = 1;
    return pos*1000000/h;
*/
}

/// returns bookmark for specified page
ldomXPointer LVDocView::getPageBookmark( int page )
{
    LVLock lock(getMutex());
    checkRender();
    if ( page<0 || page>=m_pages.length() )
        return ldomXPointer();
    ldomXPointer ptr = m_doc->createXPointer( lvPoint( 0, m_pages[page]->start ) );
    return ptr;
}

void limitStringSize( lString16 & str, int maxSize )
{
    if ( (int)str.length()<maxSize )
        return;
    int lastSpace = -1;
    for ( int i=str.length()-1; i>0; i-- )
        if ( str[i]==' ') {
            while ( i>0 && str[i-1]==' ' )
                i--;
            lastSpace = i;
            break;
        }
    int split = lastSpace>0 ? lastSpace : maxSize;
    str = str.substr( 0, split );
    str += L"...";
}

/// get bookmark position text
bool LVDocView::getBookmarkPosText( ldomXPointer bm, lString16 & titleText, lString16 & posText )
{
    LVLock lock(getMutex());
    checkRender();
    titleText = posText = lString16();
    if ( bm.isNull() )
        return false;
    ldomNode * el = bm.getNode();
    if ( el->isText() ) {
        lString16 txt = bm.getNode()->getText();
        int startPos = bm.getOffset();
        int len = txt.length() - startPos;
        if ( len>0 )
            txt = txt.substr( startPos, len );
        if ( startPos>0 )
            posText = L"...";
        posText = txt;
        el = el->getParentNode();
    } else {
        posText = el->getText();
    }
    bool inTitle = false;
    do {
        while ( el && el->getNodeId()!=el_section && el->getNodeId()!=el_body )
        {
            if ( el->getNodeId()==el_title || el->getNodeId()==el_subtitle )
                inTitle = true;
            el = el->getParentNode();
        }
        if ( el ) {
            if ( inTitle ) {
                posText.clear();
                if ( el->getChildCount()>1 ) {
                    ldomNode * node = el->getChildNode(1);
                    posText = node->getText(' ');
                }
                inTitle = false;
            }
            if ( el->getNodeId()==el_body && !titleText.empty() )
                break;
            lString16 txt = getSectionHeader( el );
            lChar16 lastch = !txt.empty() ? txt[txt.length()-1] : 0;
            if ( !titleText.empty() ) {
                if ( lastch!='.' && lastch!='?' && lastch!='!' )
                    txt += L".";
                txt += L" ";
            }
            titleText = txt + titleText;
            el = el->getParentNode();
        }
        if ( titleText.length()>50 )
            break;
    } while ( el );
    limitStringSize( titleText, 70 );
    limitStringSize( posText, 120 );
    return true;
}


/// moves position to bookmark
void LVDocView::goToBookmark(ldomXPointer bm)
{
    LVLock lock(getMutex());
    checkRender();
    m_posIsSet = false;
    _posBookmark = bm;
}

/// get page number by bookmark
int LVDocView::getBookmarkPage(ldomXPointer bm)
{
    LVLock lock(getMutex());
    checkRender();
    if ( bm.isNull() ) {
        return 0;
    } else {
        lvPoint pt = bm.toPoint();
        if ( pt.y<0 )
            return 0;
        return m_pages.FindNearestPage(pt.y, 0);
    }
}

void LVDocView::updateScroll()
{
    checkPos();
    if (m_view_mode==DVM_SCROLL)
    {
        int npos = m_pos;
        int fh = GetFullHeight();
        int shift = 0;
        int npage = m_dy;
        while (fh > 16384)
        {
            fh >>= 1;
            npos >>= 1;
            npage >>= 1;
            shift++;
        }
        if (npage<1)
            npage = 1;
        m_scrollinfo.pos = npos;
        m_scrollinfo.maxpos = fh - npage;
        m_scrollinfo.pagesize = npage;
        m_scrollinfo.scale = shift;
        char str[32];
        sprintf(str, "%d%%", (int)(fh>0?(100*npos/fh):0));
        m_scrollinfo.posText = lString16( str );
    }
    else
    {
        int page = m_pages.FindNearestPage( m_pos, 0 );
        int vpc = getVisiblePageCount();
        m_scrollinfo.pos = page / vpc;
        m_scrollinfo.maxpos = (m_pages.length() + vpc-1) / vpc - 1;
        m_scrollinfo.pagesize = 1;
        m_scrollinfo.scale = 0;
        char str[32] = {0};
        if ( m_pages.length()>1 ) {
            if ( page<=0 ) {
                sprintf(str, "cover" );
            } else
                sprintf(str, "%d / %d", page, m_pages.length()-1 );
        }
        m_scrollinfo.posText = lString16( str );
    }
}

/// move to position specified by scrollbar
bool LVDocView::goToScrollPos( int pos )
{
    if (m_view_mode==DVM_SCROLL) {
        SetPos( scrollPosToDocPos( pos ) );
        return true;
    } else {
        int vpc = this->getVisiblePageCount();
        int curPage = getCurPage();
        pos = pos * vpc;
        if ( pos >= getPageCount() )
            pos = getPageCount()-1;
        if ( pos < 0 )
            pos = 0;
        if ( curPage == pos )
            return false;
        goToPage( pos );
        return true;
    }
}

/// converts scrollbar pos to doc pos
int LVDocView::scrollPosToDocPos( int scrollpos )
{
    if (m_view_mode==DVM_SCROLL)
    {
        int n = scrollpos<<m_scrollinfo.scale;
        if (n<0)
            n = 0;
        int fh = GetFullHeight();
        if (n>fh)
            n = fh;
        return n;
    }
    else
    {
        int vpc = getVisiblePageCount();
        int n = scrollpos * vpc;
        if (!m_pages.length())
            return 0;
        if (n>=m_pages.length())
            n = m_pages.length()-1;
        if (n<0)
            n = 0;
        return m_pages[n]->start;
    }
}

/// get list of links
void LVDocView::getCurrentPageLinks( ldomXRangeList & list )
{
    list.clear();
    /// get page document range, -1 for current page
    LVRef<ldomXRange> page = getPageDocumentRange();
    if ( !page.isNull() ) {
        // search for links
        class LinkKeeper : public ldomNodeCallback {
            ldomXRangeList &_list;
        public:
            LinkKeeper( ldomXRangeList & list )
                : _list(list) { }
            /// called for each found text fragment in range
            virtual void onText( ldomXRange * ) { }
            /// called for each found node in range
            virtual bool onElement( ldomXPointerEx * ptr )
            {
                //
                ldomNode * elem = ptr->getNode();
                if ( elem->getNodeId()==el_a ) {
                    for ( int i=0; i<_list.length(); i++ ) {
                        if ( _list[i]->getStart().getNode() == elem )
                            return true; // don't add, duplicate found!
                    }
                    _list.add( new ldomXRange(elem) );
                }
                return true;
            }
        };
        LinkKeeper callback( list );
        page->forEach( &callback );
        if ( m_view_mode==DVM_PAGES && getVisiblePageCount()>1 ) {
            // process second page
            int pageNumber = getCurPage();
            page = getPageDocumentRange( pageNumber+1 );
            if ( !page.isNull() )
                page->forEach( &callback );
        }
    }
}


void LVDocView::goToPage( int page )
{
    LVLock lock(getMutex());
    checkRender();
    if (!m_pages.length())
        return;
    if (page >= m_pages.length())
        page = m_pages.length()-1;
    if (page<0)
        page = 0;
    SetPos( m_pages[page]->start );
}

/// returns document offset for next page
int LVDocView::getNextPageOffset()
{
    LVLock lock(getMutex());
    checkPos();
    if (m_view_mode==DVM_SCROLL)
    {
        return GetPos() + m_dy;
    }
    else
    {
        int p = m_pages.FindNearestPage(m_pos, 0)  + getVisiblePageCount();
        if ( p<m_pages.length() )
            return m_pages[p]->start;
        if ( !p || m_pages.length()==0 )
            return 0;
        return m_pages[m_pages.length()-1]->start;
    }
}

/// returns document offset for previous page
int LVDocView::getPrevPageOffset()
{
    LVLock lock(getMutex());
    checkPos();
    if (m_view_mode==DVM_SCROLL)
    {
        return GetPos() - m_dy;
    }
    else
    {
        int p = m_pages.FindNearestPage(m_pos, 0);
        p -= getVisiblePageCount();
        if ( p < 0 )
            p = 0;
        if ( p >= m_pages.length() )
            return 0;
        return m_pages[p]->start;
    }
}

static void addItem( LVPtrVector<LVTocItem, false> & items, LVTocItem * item )
{
    if ( item->getLevel()>0 )
        items.add( item );
    for ( int i=0; i<item->getChildCount(); i++ ) {
        addItem( items, item->getChild( i ) );
    }
}

/// returns pointer to TOC root node
bool LVDocView::getFlatToc( LVPtrVector<LVTocItem, false> & items )
{
    items.clear();
    addItem( items, getToc() );
    return items.length() > 0;
}

/// -1 moveto previous page, 1 to next page
bool LVDocView::moveByPage( int delta )
{
    if (m_view_mode==DVM_SCROLL)
    {
		int p = GetPos();
        SetPos( p + m_dy * delta );
		return GetPos()!=p;
	}
    else
    {
		int cp = getCurPage();
		int p = cp + delta * getVisiblePageCount();
		goToPage( p );
        return getCurPage() != cp;
    }
}

/// -1 moveto previous chapter, 0 to current chaoter first pae, 1 to next chapter
bool LVDocView::moveByChapter( int delta )
{
    /// returns pointer to TOC root node
    LVPtrVector<LVTocItem, false> items;
    if ( !getFlatToc( items ) )
        return false;
    int cp = getCurPage();
    int prevPage = -1;
    int nextPage = -1;
    for ( int i=0; i<items.length(); i++ ) {
        LVTocItem * item = items[i];
        int p = item->getPage();
        if ( p < cp && (prevPage == -1 || prevPage < p) )
            prevPage = p;
        if ( p > cp && (nextPage == -1 || nextPage > p) )
            nextPage = p;
    }
    if ( prevPage<0 )
        prevPage = 0;
    if ( nextPage<0 )
        nextPage = getPageCount()-1;
    int page = delta<0 ? prevPage : nextPage;
    if ( getCurPage() != page ) {
        savePosToNavigationHistory();
        goToPage( page );
    }
    return true;
}

/// saves new bookmark
CRBookmark * LVDocView::saveRangeBookmark( ldomXRange & range, bmk_type type, lString16 comment )
{
	if ( range.isNull() )
        return NULL;
	if ( range.getStart().isNull() )
        return NULL;
	CRFileHistRecord * rec = getCurrentFileHistRecord();
	if ( !rec )
        return NULL;
	CRBookmark * bmk = new CRBookmark();
	bmk->setType( type );
	bmk->setStartPos( range.getStart().toString() );
	if ( !range.getEnd().isNull() )
		bmk->setEndPos( range.getEnd().toString() );
	int p = range.getStart().toPoint().y;
	int fh = m_doc->getFullHeight();
	int percent = fh > 0 ? (int)(p * (lInt64)10000 / fh) : 0;
	if ( percent<0 )
		percent = 0;
	if ( percent>10000 )
		percent = 10000;
	bmk->setPercent( percent );
	lString16 postext = range.getRangeText();
	bmk->setPosText( postext );
	bmk->setCommentText( comment );
	bmk->setTitleText( CRBookmark::getChapterName( range.getStart() ) );
	rec->getBookmarks().add( bmk );
    return bmk;
}

/// removes bookmark from list, and deletes it, false if not found
bool LVDocView::removeBookmark( CRBookmark * bm )
{
    CRFileHistRecord * rec = getCurrentFileHistRecord();
    if ( !rec )
        return false;
    bm = rec->getBookmarks().remove( bm );
    if ( bm ) {
        delete bm;
        return true;
    } else {
        return false;
    }
}

#define MAX_EXPORT_BOOKMARKS_SIZE 200000
/// export bookmarks to text file
bool LVDocView::exportBookmarks( lString16 filename )
{
	if ( m_filename.empty() )
		return true; // no document opened
    lChar16 lastChar = filename.lastChar();
    lString16 dir;
    CRLog::trace("exportBookmarks(%s)", UnicodeToUtf8(filename).c_str() );
    if ( lastChar=='/' || lastChar=='\\' ) {
        dir = filename;
        CRLog::debug("Creating directory, if not exist %s", UnicodeToUtf8(dir).c_str() );
        LVCreateDirectory( dir );
        filename.clear();
    }
	if ( filename.empty() ) {
	    CRPropRef props = getDocProps();
	    lString16 arcname = props->getStringDef(DOC_PROP_ARC_NAME);
		lString16 arcpath = props->getStringDef(DOC_PROP_ARC_PATH);
		int arcFileCount = props->getIntDef(DOC_PROP_ARC_FILE_COUNT, 0);
		if ( !arcpath.empty() )
			LVAppendPathDelimiter( arcpath );
		lString16 fname = props->getStringDef(DOC_PROP_FILE_NAME);
		lString16 fpath = props->getStringDef(DOC_PROP_FILE_PATH);
		if ( !fpath.empty() )
			LVAppendPathDelimiter( fpath );
		if ( !arcname.empty() ) {
            if ( dir.empty() )
                dir = arcpath;
			if ( arcFileCount>1 )
				filename = arcname + L"." + fname + L".bmk.txt";
			else
				filename = arcname + L".bmk.txt";
		} else {
            if ( dir.empty() )
                dir = fpath;
			filename = fname + L".bmk.txt";
		}
        LVAppendPathDelimiter( dir );
        filename = dir + filename;
	}
    CRLog::debug("Exported bookmark filename: %s", UnicodeToUtf8(filename).c_str());
	CRFileHistRecord * rec = getCurrentFileHistRecord();
	if ( !rec )
		return false;
	// check old content
	lString8 oldContent;
	{
		LVStreamRef is = LVOpenFileStream( filename.c_str(), LVOM_READ );
		if ( !is.isNull() ) {
			int sz = (int) is->GetSize();
			if ( sz>0 && sz<MAX_EXPORT_BOOKMARKS_SIZE ) {
				oldContent.append( sz, ' ' );
				lvsize_t bytesRead = 0;
				if ( is->Read( oldContent.modify(), sz, &bytesRead )!=LVERR_OK || (int)bytesRead!=sz )
					oldContent.clear();
			}
		}
	}
	lString8 newContent;
	LVPtrVector<CRBookmark> & bookmarks = rec->getBookmarks();
	for ( int i=0; i<bookmarks.length(); i++ ) {
		CRBookmark * bmk = bookmarks[ i ];
		if ( bmk->getType() != bmkt_comment && bmk->getType() != bmkt_correction )
			continue;
		if ( newContent.empty() ) {
			newContent.append(1, (char)0xef);
			newContent.append(1, (char)0xbb);
			newContent.append(1, (char)0xbf);
			newContent << "# Cool Reader 3 - exported bookmarks\r\n";
			newContent << "# file name: " << UnicodeToUtf8(rec->getFileName()) << "\r\n";
			if ( !rec->getFilePathName().empty() )
				newContent << "# file path: " << UnicodeToUtf8(rec->getFilePath()) << "\r\n";
			newContent << "# book title: " << UnicodeToUtf8(rec->getTitle()) << "\r\n";
			newContent << "# author: " << UnicodeToUtf8(rec->getAuthor()) << "\r\n";
			if ( !rec->getSeries().empty() )
				newContent << "# series: " << UnicodeToUtf8(rec->getSeries()) << "\r\n";
			newContent << "\r\n";
		}
		char pos[16];
		int percent = bmk->getPercent();
		lString16 title = bmk->getTitleText();
		sprintf( pos, "%d.%02d%%", percent/100, percent % 100 );
		newContent << "## " << pos << " - " << (bmk->getType()==bmkt_comment ? "comment" : "correction") << "\r\n";
		if ( !title.empty() )
			newContent << "## " << UnicodeToUtf8( title ) << "\r\n";
		if ( !bmk->getPosText().empty() )
			newContent << "<< " << UnicodeToUtf8(bmk->getPosText()) << "\r\n";
		if ( !bmk->getCommentText().empty() )
			newContent << ">> " << UnicodeToUtf8(bmk->getCommentText()) << "\r\n";
		newContent << "\r\n";
	}
	if ( newContent == oldContent )
		return true;
	{
		LVStreamRef os = LVOpenFileStream( filename.c_str(), LVOM_WRITE );
		if ( os.isNull() )
			return false;
		lvsize_t bytesWritten = 0;
		if ( newContent.length() > 0 )
			if ( os->Write( newContent.c_str(), newContent.length(), &bytesWritten) != LVERR_OK || bytesWritten!=newContent.length() )
				return false;
	}
	return true;
}

/// saves current page bookmark under numbered shortcut
CRBookmark * LVDocView::saveCurrentPageShortcutBookmark( int number )
{
	CRFileHistRecord * rec = getCurrentFileHistRecord();
	if ( !rec )
        return NULL;
    ldomXPointer p = getBookmark();
    if ( p.isNull() )
        return NULL;
	CRBookmark * bm = rec->setShortcutBookmark( number, p );
    lString16 titleText;
    lString16 posText;
    if ( bm && getBookmarkPosText( p, titleText, posText ) ) {
         bm->setTitleText( titleText );
         bm->setPosText( posText );
         return bm;
    }
    return NULL;
}

/// saves current page bookmark under numbered shortcut
CRBookmark * LVDocView::saveCurrentPageBookmark( lString16 comment )
{
    CRFileHistRecord * rec = getCurrentFileHistRecord();
    if ( !rec )
        return NULL;
    ldomXPointer p = getBookmark();
    if ( p.isNull() )
        return NULL;
    CRBookmark * bm = new CRBookmark( p );
    lString16 titleText;
    lString16 posText;
    bm->setType( bmkt_pos );
    if ( getBookmarkPosText( p, titleText, posText ) ) {
         bm->setTitleText( titleText );
         bm->setPosText( posText );
    }
    bm->setStartPos( p.toString() );
    int pos = p.toPoint().y;
    int fh = m_doc->getFullHeight();
    int percent = fh > 0 ? (int)(pos * (lInt64)10000 / fh) : 0;
    if ( percent<0 )
        percent = 0;
    if ( percent>10000 )
        percent = 10000;
    bm->setPercent( percent );
    bm->setCommentText( comment );
    rec->getBookmarks().add( bm );
    return bm;
}


/// restores page using bookmark by numbered shortcut
bool LVDocView::goToPageShortcutBookmark( int number )
{
	CRFileHistRecord * rec = getCurrentFileHistRecord();
	if ( !rec )
		return false;
	CRBookmark * bmk = rec->getShortcutBookmark( number );
	if ( !bmk )
		return false;
	lString16 pos = bmk->getStartPos();
	ldomXPointer p = m_doc->createXPointer( pos );
	if ( p.isNull() )
		return false;
    if ( getCurPage() != getBookmarkPage( p ) )
        savePosToNavigationHistory();
	goToBookmark( p );
	return true;
}

// execute command
void LVDocView::doCommand( LVDocCmd cmd, int param )
{
    switch (cmd)
    {
    case DCMD_TOGGLE_PAGE_SCROLL_VIEW:
        {
            toggleViewMode();
        }
        break;
    case DCMD_GO_SCROLL_POS:
        {
            goToScrollPos( param );
        }
        break;
    case DCMD_BEGIN:
        {
            if ( getCurPage() > 0 ) {
                savePosToNavigationHistory();
                SetPos(0);
            }
        }
        break;
    case DCMD_LINEUP:
        {
            if (m_view_mode==DVM_SCROLL)
            {
                SetPos( GetPos() - param*(m_font_size*3/2));
            }
            else
            {
                int p = m_pages.FindNearestPage(m_pos, 0);
                goToPage( p - getVisiblePageCount() );
                //goToPage( m_pages.FindNearestPage(m_pos, -1));
            }
        }
        break;
    case DCMD_PAGEUP:
        {
            if ( param < 1 )
                param = 1;
			moveByPage( -param );
        }
        break;
    case DCMD_PAGEDOWN:
        {
            if ( param < 1 )
                param = 1;
			moveByPage( param );
        }
        break;
    case DCMD_LINK_NEXT:
        {
            selectNextPageLink(true);
        }
        break;
    case DCMD_LINK_PREV:
        {
            selectPrevPageLink(true);
        }
        break;
    case DCMD_LINK_FIRST:
        {
            selectFirstPageLink();
        }
        break;
    case DCMD_ROTATE_BY: // rotate view, param =  +1 - clockwise, -1 - counter-clockwise
        {
            int a = (int)m_rotateAngle;
            if ( param==0 )
                param = 1;
            a = (a + param) & 3;
            SetRotateAngle( (cr_rotate_angle_t)(a) );
        }
        break;
    case DCMD_ROTATE_SET: // rotate viewm param = 0..3 (0=normal, 1=90`, ...)
        {
            SetRotateAngle( (cr_rotate_angle_t)(param & 3 ) );
        }
        break;
    case DCMD_LINK_GO:
        {
            goSelectedLink();
        }
        break;
    case DCMD_LINK_BACK:
        {
            goBack();
        }
        break;
    case DCMD_LINK_FORWARD:
        {
            goForward();
        }
        break;
    case DCMD_LINEDOWN:
        {
            if (m_view_mode==DVM_SCROLL)
            {
                SetPos( GetPos() + param*(m_font_size*3/2));
            }
            else
            {
                int p = m_pages.FindNearestPage(m_pos, 0);
                goToPage( p + getVisiblePageCount() );
                //goToPage( m_pages.FindNearestPage(m_pos, +1));
            }
        }
        break;
    case DCMD_END:
        {
            if ( getCurPage() < getPageCount() - getVisiblePageCount() ) {
                savePosToNavigationHistory();
                SetPos(GetFullHeight());
            }
        }
        break;
    case DCMD_GO_POS:
        {
            if (m_view_mode==DVM_SCROLL)
            {
                SetPos( param );
            }
            else
            {
                goToPage( m_pages.FindNearestPage(param, 0) );
            }
        }
        break;
    case DCMD_GO_PAGE:
        {
            if ( getCurPage() != param ) {
                savePosToNavigationHistory();
                goToPage( param );
            }
        }
        break;
    case DCMD_ZOOM_IN:
        {
            ZoomFont( +1 );
        }
        break;
    case DCMD_ZOOM_OUT:
        {
            ZoomFont( -1 );
        }
        break;
    case DCMD_TOGGLE_TEXT_FORMAT:
        {
            if ( getTextFormatOptions()==txt_format_auto )
                setTextFormatOptions( txt_format_pre );
            else
                setTextFormatOptions( txt_format_auto );
        }
        break;
	case DCMD_BOOKMARK_SAVE_N:
		{
			// save current page bookmark under spicified number
            saveCurrentPageShortcutBookmark( param );
		}
		break;
	case DCMD_BOOKMARK_GO_N:
		{
			// go to bookmark with specified number
            if ( !goToPageShortcutBookmark( param ) ) {
                // if no bookmark exists with specified shortcut, save new bookmark instead
                saveCurrentPageShortcutBookmark( param );
            }
		}
		break;
	case DCMD_MOVE_BY_CHAPTER:
		{
			moveByChapter( param );
		}
		break;
    default:
        // DO NOTHING
        break;
    }
}

//static int cr_font_sizes[] = { 24, 29, 33, 39, 44 };
static int cr_interline_spaces[] = {100, 90, 110, 120, 140};

/// sets default property values if properties not found, checks ranges
void LVDocView::propsUpdateDefaults( CRPropRef props )
{
    lString16Collection list;
    fontMan->getFaceList( list );
    static int def_aa_props[] = { 2, 1, 0 };
    props->limitValueList( PROP_FONT_ANTIALIASING, def_aa_props, sizeof(def_aa_props)/sizeof(int) );
    props->setHexDef( PROP_FONT_COLOR, 0x000000 );
    props->setHexDef( PROP_BACKGROUND_COLOR, 0xFFFFFF );
    props->setIntDef( PROP_TXT_OPTION_PREFORMATTED, 0 );
    props->setIntDef( PROP_AUTOSAVE_BOOKMARKS, 1 );
    lString8 defFontFace("Arial");
    props->setStringDef( PROP_FONT_FACE, defFontFace.c_str() );
    props->setStringDef( PROP_STATUS_FONT_FACE, defFontFace.c_str() );
    if ( list.length()>0 && !list.contains( props->getStringDef( PROP_FONT_FACE, defFontFace.c_str()) ) )
        props->setString( PROP_FONT_FACE, list[0] );
    props->limitValueList( PROP_FONT_SIZE, m_font_sizes.ptr(), m_font_sizes.length() );
    props->limitValueList( PROP_INTERLINE_SPACE, cr_interline_spaces, sizeof(cr_interline_spaces)/sizeof(int) );
    static int def_rot_angle[] = { 0, 1, 2, 3 };
    props->limitValueList( PROP_ROTATE_ANGLE, def_rot_angle, 4 );
    static int bool_options_def_true[] = { 1, 0 };
    static int bool_options_def_false[] = { 0, 1 };
    static int int_options_1_2[] = { 1, 2 };
    props->limitValueList( PROP_LANDSCAPE_PAGES, int_options_1_2, 2 );
    props->limitValueList( PROP_EMBEDDED_STYLES, bool_options_def_true, 2 );
    props->limitValueList( PROP_PAGE_VIEW_MODE, bool_options_def_true, 2 );
    props->limitValueList( PROP_FOOTNOTES, bool_options_def_true, 2 );
    props->limitValueList( PROP_SHOW_TIME, bool_options_def_false, 2 );
    props->limitValueList( PROP_DISPLAY_INVERSE, bool_options_def_false, 2 );
    props->limitValueList( PROP_BOOKMARK_ICONS, bool_options_def_false, 2 );
    props->limitValueList( PROP_FONT_KERNING_ENABLED, bool_options_def_false, 2 );
    static int def_status_line[] = { 0, 1, 2 };
    props->limitValueList( PROP_STATUS_LINE, def_status_line, 3 );
    props->limitValueList( PROP_TXT_OPTION_PREFORMATTED, bool_options_def_false, 2 );
    static int def_margin[] = { 8, 0, 5, 10, 15, 20, 25, 30 };
    props->limitValueList( PROP_PAGE_MARGIN_TOP, def_margin, 8 );
    props->limitValueList( PROP_PAGE_MARGIN_BOTTOM, def_margin, 8 );
    props->limitValueList( PROP_PAGE_MARGIN_LEFT, def_margin, 8 );
    props->limitValueList( PROP_PAGE_MARGIN_RIGHT, def_margin, 8 );
	int fs = props->getIntDef( PROP_STATUS_FONT_SIZE, INFO_FONT_SIZE );
	if ( fs<14 )
		fs = 14;
	else if ( fs>28 )
		fs = 28;
	props->setIntDef( PROP_STATUS_FONT_SIZE, fs );
	lString16 hyph = props->getStringDef( PROP_HYPHENATION_DICT, DEF_HYPHENATION_DICT );
	HyphDictionaryList * dictlist = HyphMan::getDictList();
	if ( dictlist ) {
		if ( dictlist->find( hyph ) )
			props->setString( PROP_HYPHENATION_DICT, hyph );
		else
			props->setString( PROP_HYPHENATION_DICT, lString16(HYPH_DICT_ID_ALGORITHM) );
	}
    props->setStringDef( PROP_STATUS_LINE, "0" );
    props->setStringDef( PROP_SHOW_TITLE, "1" );
    props->setStringDef( PROP_SHOW_TIME, "1" );
    props->setStringDef( PROP_SHOW_BATTERY, "1" );
}

#define H_MARGIN 8
#define V_MARGIN 8
#define ALLOW_BOTTOM_STATUSBAR 0
void LVDocView::setStatusMode( int newMode, bool showClock, bool showTitle, bool showBattery )
{
    CRLog::debug("LVDocView::setStatusMode(%d, %s %s %s)", newMode, showClock?"clock":"", showTitle?"title":"", showBattery?"battery":"");
#if ALLOW_BOTTOM_STATUSBAR==1
    lvRect margins( H_MARGIN, V_MARGIN, H_MARGIN, V_MARGIN/2 );
    lvRect oldMargins = _docview->getPageMargins( );
    if (newMode==1)
        margins.bottom = STANDARD_STATUSBAR_HEIGHT + V_MARGIN/4;
#endif
    if ( newMode==0 )
        setPageHeaderInfo(
                PGHDR_PAGE_NUMBER
                | (showClock ? PGHDR_CLOCK : 0)
                | (showBattery ? PGHDR_BATTERY : 0)
                | PGHDR_PAGE_COUNT
                | (showTitle ? PGHDR_AUTHOR : 0)
                | (showTitle ? PGHDR_TITLE : 0)
                //| PGHDR_CLOCK
                         );
    else
        setPageHeaderInfo(0);
#if ALLOW_BOTTOM_STATUSBAR==1
    setPageMargins( margins );
#endif
}

/// applies properties, returns list of not recognized properties
CRPropRef LVDocView::propsApply( CRPropRef props )
{
    CRLog::trace( "LVDocView::propsApply( %d items )", props->getCount() );
    CRPropRef unknown = LVCreatePropsContainer();
    for ( int i=0; i<props->getCount(); i++ ) {
        lString8 name( props->getName( i ) );
        lString16 value = props->getValue( i );
        bool isUnknown = false;
        if ( name==PROP_FONT_ANTIALIASING ) {
            int antialiasingMode = props->getIntDef( PROP_FONT_ANTIALIASING, 2 );
            fontMan->SetAntialiasMode( antialiasingMode );
            requestRender();
        } else if ( name==PROP_LANDSCAPE_PAGES ) {
            int pages = props->getIntDef( PROP_LANDSCAPE_PAGES, 0 );
            setVisiblePageCount( pages );
            requestRender();
        } else if ( name==PROP_FONT_KERNING_ENABLED ) {
            bool kerning = props->getBoolDef( PROP_FONT_KERNING_ENABLED, false );
            fontMan->setKerning( kerning );
            requestRender();
        } else if ( name==PROP_TXT_OPTION_PREFORMATTED ) {
            bool preformatted = props->getBoolDef( PROP_TXT_OPTION_PREFORMATTED, false );
            setTextFormatOptions( preformatted ? txt_format_pre : txt_format_auto );
        } else if ( name==PROP_FONT_COLOR || name==PROP_BACKGROUND_COLOR || name==PROP_DISPLAY_INVERSE ) {
            // update current value in properties
            m_props->setString( name.c_str(), value );
            lUInt32 textColor = m_props->getIntDef(PROP_FONT_COLOR, 0x000000 );
            lUInt32 backColor = m_props->getIntDef(PROP_BACKGROUND_COLOR, 0xFFFFFF );
            bool inverse = m_props->getBoolDef(PROP_DISPLAY_INVERSE, false );
            if ( inverse ) {
                CRLog::trace("Setting inverse colors");
                setBackgroundColor( textColor );
                setTextColor( backColor );
                requestRender(); // TODO: only colors to be changed
            } else {
                CRLog::trace("Setting normal colors");
                setBackgroundColor( backColor );
                setTextColor( textColor );
                requestRender(); // TODO: only colors to be changed
            }
        } else if ( name==PROP_PAGE_MARGIN_TOP || name==PROP_PAGE_MARGIN_LEFT
                   || name==PROP_PAGE_MARGIN_RIGHT || name==PROP_PAGE_MARGIN_BOTTOM ) {
            lUInt32 margin = props->getIntDef(name.c_str(), 8 );
            if ( margin>30 )
                margin = 30;
            lvRect rc = getPageMargins();
            if ( name==PROP_PAGE_MARGIN_TOP )
                rc.top = margin;
            else if ( name==PROP_PAGE_MARGIN_BOTTOM )
                rc.bottom = margin;
            else if ( name==PROP_PAGE_MARGIN_LEFT )
                rc.left = margin;
            else if ( name==PROP_PAGE_MARGIN_RIGHT )
                rc.right = margin;
            setPageMargins( rc );
        } else if ( name==PROP_FONT_FACE ) {
            setDefaultFontFace( UnicodeToUtf8(value) );
        } else if ( name==PROP_STATUS_FONT_FACE ) {
            setStatusFontFace( UnicodeToUtf8(value) );
        } else if ( name==PROP_STATUS_LINE || name==PROP_SHOW_TIME  || name==PROP_SHOW_TITLE  || name==PROP_SHOW_BATTERY ) {
            m_props->setString( name.c_str(), value );
            setStatusMode( m_props->getIntDef( PROP_STATUS_LINE, 0 ), m_props->getBoolDef( PROP_SHOW_TIME, false )
                           , m_props->getBoolDef( PROP_SHOW_TITLE, true )
                           , m_props->getBoolDef( PROP_SHOW_BATTERY, true ));
        //} else if ( name==PROP_BOOKMARK_ICONS ) {
        //    enableBookmarkIcons( value==L"1" );
        } else if ( name==PROP_FONT_SIZE ) {
            int fontSize = props->getIntDef( PROP_FONT_SIZE, m_font_sizes[0] );
            setFontSize( fontSize );//cr_font_sizes
            value = lString16::itoa( m_font_size );
        } else if ( name==PROP_STATUS_FONT_SIZE ) {
            int fontSize = props->getIntDef( PROP_STATUS_FONT_SIZE, INFO_FONT_SIZE );
			if ( fontSize<14 )
				fontSize = 14;
			else if ( fontSize > 28 )
				fontSize = 28;
            setStatusFontSize( fontSize );//cr_font_sizes
            value = lString16::itoa( fontSize );
		} else if ( name==PROP_HYPHENATION_DICT ) {
			// hyphenation dictionary
			lString16 id = props->getStringDef( PROP_HYPHENATION_DICT, DEF_HYPHENATION_DICT );
			HyphDictionaryList * list = HyphMan::getDictList();
			HyphDictionary * curr = HyphMan::getSelectedDictionary();
			if ( list ) {
				if ( !curr || curr->getId()!=id ) {
                    //if (
                    CRLog::debug("Changing hyphenation to %s", LCSTR(id));
                    list->activate( id );
                    //)
                    requestRender();
				}
			}
        } else if ( name==PROP_INTERLINE_SPACE ) {
            int interlineSpace = props->getIntDef( PROP_INTERLINE_SPACE,  cr_interline_spaces[0] );
            setDefaultInterlineSpace( interlineSpace );//cr_font_sizes
            value = lString16::itoa( m_def_interline_space );
        } else if ( name==PROP_ROTATE_ANGLE ) {
            cr_rotate_angle_t angle = (cr_rotate_angle_t) (props->getIntDef( PROP_ROTATE_ANGLE, 0 )&3);
            SetRotateAngle( angle );
            value = lString16::itoa( m_rotateAngle );
        } else if ( name==PROP_EMBEDDED_STYLES ) {
            bool value = props->getBoolDef( PROP_EMBEDDED_STYLES, true );
            getDocument()->setDocFlag( DOC_FLAG_ENABLE_INTERNAL_STYLES, value );
            requestRender();
        } else if ( name==PROP_FOOTNOTES ) {
            bool value = props->getBoolDef( PROP_FOOTNOTES, true );
            getDocument()->setDocFlag( DOC_FLAG_ENABLE_FOOTNOTES, value );
            requestRender();
        } else if ( name==PROP_PAGE_VIEW_MODE ) {
            LVDocViewMode m = props->getIntDef( PROP_PAGE_VIEW_MODE, 1 ) ? DVM_PAGES : DVM_SCROLL;
            setViewMode(m);
        } else {
            // unknown property, adding to list of unknown properties
            unknown->setString( name.c_str(), value );
            isUnknown = true;
        }
        if ( !isUnknown ) {
            // update current value in properties
            m_props->setString( name.c_str(), value );
        }
    }
    return unknown;
}

/// returns current values of supported properties
CRPropRef LVDocView::propsGetCurrent()
{
    return m_props;
}
