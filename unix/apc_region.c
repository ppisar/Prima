#include "unix/guts.h"
#include "Icon.h"

#define REGION GET_REGION(self)->region
#define HEIGHT GET_REGION(self)->height

Region
prima_region_create( Handle mask)
{
	unsigned long w, h, x, y, size = 256, count = 0;
	Region	  rgn = None;
	Byte	   * idata;
	XRectangle * current, * rdata;
	Bool	  set = 0;

	if ( !mask)
		return None;

	w = PImage( mask)-> w;
	h = PImage( mask)-> h;
	/*
		XUnionRegion is actually SLOWER than the image scan - 
		- uncomment if this is wrong
	if ( X( mask)-> cached_region) {
		rgn = XCreateRegion();
		XUnionRegion( rgn, X( mask)-> cached_region, rgn);
		return rgn;
	}
	*/

	idata  = PImage( mask)-> data + PImage( mask)-> dataSize - PImage( mask)-> lineSize;

	rdata = ( XRectangle*) malloc( size * sizeof( XRectangle));
	if ( !rdata) {
		warn("Not enough memory");
		return None;
	}

	count = 0;
	current = rdata;
	current--;

	for ( y = 0; y < h; y++) {
		for ( x = 0; x < w; x++) {
			if ( idata[ x >> 3] == 0) {
				x += 7;
				continue;
			}
			if ( idata[ x >> 3] & ( 1 << ( 7 - ( x & 7)))) {
				if ( set && current-> y == y && current-> x + current-> width == x)
					current-> width++;
				else {
					set = 1;
					if ( count >= size) {
						void * xrdata = realloc( rdata, ( size *= 3) * sizeof( XRectangle));
						if ( !xrdata) {
							warn("Not enough memory");
							free( rdata); 
							return None;
						}
						rdata = xrdata;
						current = rdata;
						current += count - 1;
					}
					count++;
					current++;
					current-> x   = x;
					current-> y   = y;
					current-> width  = 1;
					current-> height = 1;
				}
			}
		}
		idata -= PImage( mask)-> lineSize;
	}

	if ( set) {
		rgn = XCreateRegion();
		for ( x = 0, current = rdata; x < count; x++, current++) 
			XUnionRectWithRegion( current, rgn, rgn);
		/*
		X( mask)-> cached_region = XCreateRegion();
		XUnionRegion( X( mask)-> cached_region, rgn, X( mask)-> cached_region);
		*/
	}
	free( rdata);

	return rgn;
}

static Bool
rgn_empty(Handle self)
{
	REGION = XCreateRegion();
	return true;
}

static Bool
rgn_rect(Handle self, Box r)
{
	XRectangle xr;
	REGION = XCreateRegion();
	xr. x = r. x;
	xr. y = 0;
	xr. width  = r. width;
	xr. height = r. height;
	XUnionRectWithRegion( &xr, REGION, REGION);
	HEIGHT = r.y + r.height;
	return true;
}

static Bool
rgn_ellipse(Handle self, Box ellipse)
{
	int w  = ellipse. width;
	int h  = ellipse. height;
	int rx = w / 2;
	int ry = h / 2;
	int a  = rx * rx;
	int b  = ry * ry;
	int d  = b - a * ry + a / 4;
	int dx = 0;
	int dy = a * ry * 2;
	int sx = 0;
	int sy = ry;
	int ay = h % 2 - 1;
	int ax = w % 2 - 1;

	REGION = XCreateRegion();
	HEIGHT = ellipse. y + sy + 1;
	while ( dx < dy ) {
		XRectangle xr;
		xr. x      = ellipse. x - sx - ax;
		xr. y      = ry + sy + ay;
		xr. width  = sx + sx;
		xr. height = 1;
		XUnionRectWithRegion( &xr, REGION, REGION);
		xr. y      = ry - sy;
		XUnionRectWithRegion( &xr, REGION, REGION);
		if ( d > 0 ) {
			sy--;
			dy -= a + a;
			d  -= dy;
		}
		sx++;
		dx += b + b;
		d += b + dx;
	}

	d = ( a - b + (a - b)/2 - dx - dy ) / 2;

	while ( sy > 0 ) {
		XRectangle xr;
		xr. x      = ellipse. x - sx - ax;
		xr. y      = ry + sy + ay;
		xr. width  = sx + sx;
		xr. height = 1;
		XUnionRectWithRegion( &xr, REGION, REGION);
		xr. y      = ry - sy;
		XUnionRectWithRegion( &xr, REGION, REGION);
		if ( d < 0 ) {
			sx++;
			dx += b + b;
			d += dx;
		}
		sy--;
		dy -= a + a;
		d += a - dy;
	}

	if ( ay == 0 ) {
		XRectangle xr;
		xr. x      = ellipse. x - rx - ax;
		xr. y      = ry;
		xr. width  = w;
		xr. height = 1;
		XUnionRectWithRegion( &xr, REGION, REGION);
	}

	return true;
}

static Bool
rgn_polygon(Handle self, PolygonRegionRec * r)
{
	int i, max;
	XPoint * xp;

	if ( !( xp = malloc( sizeof(XPoint) * r->n_points ))) {
		warn("Not enough memory");
		return false;
	}

	for ( i = 0, max = 0; i < r->n_points; i++) {
		if ( max < r->points[i].y) 
			max = r->points[i].y;
	}
	for ( i = 0; i < r->n_points; i++) {
		xp[i].x = r->points[i].x;
		xp[i].y = max - r->points[i].y;
	}
	
	HEIGHT = max;
	REGION = XPolygonRegion( xp, r->n_points, r-> winding ? WindingRule : EvenOddRule );

	free( xp );
	return true;
}

static Bool
rgn_image(Handle self, Handle image)
{
	REGION = prima_region_create(image);

	if ( !REGION )
		REGION = XCreateRegion();
	else
		HEIGHT = PImage(image)->h;
	return true;
}

Bool
apc_region_create( Handle self, PRegionRec rec)
{
	switch( rec-> type ) {
	case rgnEmpty:
		return rgn_empty(self);
	case rgnRectangle:
		return rgn_rect(self, rec->data.box);
	case rgnEllipse:
		return rgn_ellipse(self, rec->data.box);
	case rgnPolygon:
		return rgn_polygon(self, &rec->data.polygon);
	case rgnImage:
		return rgn_image(self, rec->data.image);
	default:
		return false;
	}
}

Bool
apc_region_destroy( Handle self)
{
	if ( REGION ) {
		XDestroyRegion(REGION);
		REGION = NULL;
	}
	return true;
}

Bool
apc_region_offset( Handle self, int dx, int dy)
{
	XOffsetRegion(REGION, dx, -dy);
	return true;
}

Bool
apc_region_combine( Handle self, Handle other_region, int rgnop)
{
	PRegionSysData r2;
	int d;
	
	r2 = GET_REGION(other_region);
	d = HEIGHT - r2-> height;

	switch (rgnop) {
	case rgnopCopy:
		break;
	case rgnopIntersect:
		break;
	case rgnopUnion:
		if ( d > 0 ) {
			XOffsetRegion( r2-> region, 0, d);
			XUnionRegion( REGION, r2->region, REGION);
			XOffsetRegion( r2-> region, 0, -d);
		} else {
			XOffsetRegion( REGION, 0, -d);
			XUnionRegion( REGION, r2->region, REGION);
			HEIGHT = r2-> height;
		}
		break;
	case rgnopXor:
		break;
	case rgnopDiff:
		break;
	}
	return false;
}

Bool
apc_region_point_inside( Handle self, Point p)
{
	return false;
}

int
apc_region_rect_inside( Handle self, Rect r)
{
	return false;
}

Bool
apc_region_equals( Handle self, Handle other_region)
{
	return XEqualRegion( REGION, GET_REGION(other_region)->region);
}

Box
apc_region_get_box( Handle self)
{
	Box box;
	XRectangle xr;
	XClipBox( REGION, &xr);
	box. x      = xr. x;
	box. y      = HEIGHT - xr. height - xr.y;
	box. width  = xr. width;
	box. height = xr. height;
	return box;
}

#define REVERT(a)	(XX-> size. y - (a) - 1)

Bool
apc_gp_set_region( Handle self, Handle rgn)
{
	DEFXX;
	Region region;
	PImage img;
	PRegionSysData r;

	if ( PObject( self)-> options. optInDrawInfo) return false;
	if ( !XF_IN_PAINT(XX)) return false;

	if (rgn == nilHandle) {
		Rect r;
	EMPTY:
		r. left   = 0;
		r. bottom = 0;
		r. right  = XX-> size. x;
		r. top    = XX-> size. y;
		return apc_gp_set_clip_rect( self, r);
	}

	r = GET_REGION(rgn);

	XClipBox( r-> region, &XX-> clip_rect);
	XX-> clip_rect. y = REVERT( r-> height - XX-> clip_rect. y);
	XX-> clip_mask_extent. x = XX-> clip_rect. width;
	XX-> clip_mask_extent. y = XX-> clip_rect. height;
	if ( XX-> clip_rect. width == 0 || XX-> clip_rect. height == 0) goto EMPTY;

	region = XCreateRegion();
	XUnionRegion( region, r-> region, region);
	/* offset region if drawable is buffered */
	XOffsetRegion( region, XX-> btransform. x, XX-> size.y - r-> height - XX-> btransform. y);
	/* otherwise ( and only otherwise ), and if there's a
		X11 clipping, intersect the region with it. X11 clipping
		must not mix with the buffer clipping */
	if (( !XX-> udrawable || XX-> udrawable == XX-> gdrawable) && 
		XX-> paint_region) 
		XIntersectRegion( region, XX-> paint_region, region);
	XSetRegion( DISP, XX-> gc, region);
	if ( XX-> flags. kill_current_region) 
		XDestroyRegion( XX-> current_region);
	XX-> flags. kill_current_region = 1;
	XX-> current_region = region;
	XX-> flags. xft_clip = 0;
#ifdef USE_XFT
	if ( XX-> xft_drawable) prima_xft_update_region( self);
#endif
#ifdef HAVE_X11_EXTENSIONS_XRENDER_H
	if ( XX-> argb_picture ) XRenderSetPictureClipRegion(DISP, XX->argb_picture, region);
#endif
	return true;
}

Bool
apc_gp_get_region( Handle self, Handle rgn)
{
	DEFXX;
	Bool bitmap, layered;
	GC gc;
	XGCValues gcv;

	if ( !XF_IN_PAINT(XX)) return false;

	if ( !rgn) 
		return XX-> clip_mask_extent. x != 0 && XX-> clip_mask_extent. y != 0;
		
	if ( XX-> clip_mask_extent. x == 0 || XX-> clip_mask_extent. y == 0)
		return false;

	bitmap  = XT_IS_BITMAP(XX) ? true : false;
	layered = XF_LAYERED(XX) ? true : false;

	CIcon( rgn)-> create_empty_icon(
		rgn, XX-> clip_mask_extent. x, XX-> clip_mask_extent. y, 
		layered ? 24 : (bitmap ? imBW : guts. qdepth),
		layered ? 8 : 1
	);
	CIcon( rgn)-> begin_paint( rgn);
	XCHECKPOINT;

	gcv. graphics_exposures = false;
	gcv. fill_style = FillSolid;
	gcv. foreground = (layered || bitmap) ? 0xffffffff : guts.monochromeMap[1];
	gcv. clip_y_origin = XX-> clip_mask_extent. y - XX-> size. y;
	gc = XCreateGC( DISP, XX->gdrawable, GCGraphicsExposures|GCFillStyle|GCForeground|GCClipYOrigin, &gcv);
	XCopyGC( DISP, XX->gc, GCClipMask, gc);
	XFillRectangle( DISP, X(rgn)-> gdrawable, gc, 0, 0, XX-> clip_mask_extent.x, XX-> clip_mask_extent.y);
	XFreeGC( DISP, gc);
	XCHECKPOINT;

	CIcon( rgn)-> end_paint( rgn);
	if ( !bitmap || layered) CIcon( rgn)-> set_type( rgn, imBW);
	XCHECKPOINT;

	return true;
}

Box
apc_gp_get_region_box( Handle self)
{
	DEFXX;
	Box box = {0,0,0,0};

	if ( !XF_IN_PAINT(XX)) return box;
	if ( XX-> clip_mask_extent. x == 0 || XX-> clip_mask_extent. y == 0)
		return box;
	
	box. x      = XX-> clip_rect. x;
	box. y      = REVERT( XX-> clip_mask_extent. y + XX-> clip_rect. y);
//	printf("%d %d %d\n", XX->size.x, XX->clip_mask_extent.x, XX->clip_rect.x);
//	printf("%d %d %d\n", XX->size.y, XX->clip_mask_extent.y, XX->clip_rect.y);
	box. width  = XX-> clip_mask_extent. x;
	box. height = XX-> clip_mask_extent. y;

	return box;
}