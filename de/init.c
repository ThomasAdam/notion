/*
 * ion/de/init.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2003. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#include <string.h>

#include <ioncore/common.h>
#include <ioncore/global.h>
#include <ioncore/rootwin.h>
#include <ioncore/extl.h>
#include <ioncore/extlconv.h>
#include <ioncore/ioncore.h>
#include <ioncore/readconfig.h>

#include "brush.h"
#include "font.h"
#include "colour.h"


/*{{{ Borders */


#define CF_BORDER_VAL_SANITY_CHECK 16

static void get_border_val(uint *val, ExtlTab tab, const char *what)
{
	int g;
	
	if(extl_table_gets_i(tab, what, &g)){
		if(g>CF_BORDER_VAL_SANITY_CHECK || g<0)
			warn("Border attribute %s sanity check failed.", what);
		else
			*val=g;
	}
}


static void get_border_style(uint *ret, ExtlTab tab)
{
	char *style=NULL;
	
	if(!extl_table_gets_s(tab, "border_style", &style))
		return;
	
	if(strcmp(style, "inlaid")==0)
		*ret=DEBORDER_INLAID;
	else if(strcmp(style, "elevated")==0)
		*ret=DEBORDER_ELEVATED;
	else if(strcmp(style, "groove")==0)
		*ret=DEBORDER_GROOVE;
	else if(strcmp(style, "ridge")==0)
		*ret=DEBORDER_RIDGE;
	else
		warn("Unknown border style \"%s\".", style);
	
	free(style);
}


static void get_border(DEBorder *border, ExtlTab tab)
{
	get_border_val(&(border->sh), tab, "shadow_pixels");
	get_border_val(&(border->hl), tab, "highlight_pixels");
	get_border_val(&(border->pad), tab, "padding_pixels");
	get_border_style(&(border->style), tab);
}


/*}}}*/


/*{{{ Colours */


static bool get_colour(WRootWin *rootwin, DEColour *ret, 
					   ExtlTab tab, const char *what, DEColour substitute)
{
	char *name=NULL;
	bool ok=FALSE;
	
	if(extl_table_gets_s(tab, what, &name)){
		ok=de_alloc_colour(rootwin, ret, name);
	
		if(!ok)
			warn("Unable to allocate colour \"%s\".", name);

		free(name);
	}
	
	if(!ok)
		return de_duplicate_colour(rootwin, substitute, ret);
	
	return ok;
}


static void get_colour_group(WRootWin *rootwin, DEColourGroup *cg, 
							 ExtlTab tab)
{
	get_colour(rootwin, &(cg->hl), tab, "highlight_colour",
			   DE_WHITE(rootwin));
	get_colour(rootwin, &(cg->sh), tab, "shadow_colour",
			   DE_WHITE(rootwin));
	get_colour(rootwin, &(cg->bg), tab, "background_colour",
			   DE_BLACK(rootwin));
	get_colour(rootwin, &(cg->fg), tab, "foreground_colour",
			   DE_WHITE(rootwin));
	get_colour(rootwin, &(cg->pad), tab, "padding_colour", cg->bg);
}


static void get_extra_cgrps(WRootWin *rootwin, DEBrush *brush, ExtlTab tab)
{
	
	uint i=0, nfailed=0, n=extl_table_get_n(tab);
	char *name;
	ExtlTab sub;
	
	if(n==0)
		return;
	
	brush->extra_cgrps=ALLOC_N(DEColourGroup, n);
	
	if(brush->extra_cgrps==NULL){
		warn_err();
		return;
	}

	for(i=0; i<n-nfailed; i++){
		if(!extl_table_geti_t(tab, i+1, &sub))
			goto err;
		if(!extl_table_gets_s(sub, "substyle_pattern", &name)){
			extl_unref_table(sub);
			goto err;
		}
		
		/*de_init_colour_group(rootwin, brush->extra_cgrps+i-nfailed);*/
		brush->extra_cgrps[i-nfailed].spec=name;
		get_colour_group(rootwin, brush->extra_cgrps+i-nfailed, sub);
		
		extl_unref_table(sub);
		continue;
		
	err:
		warn("Corrupt substyle table %d.", i);
		nfailed++;
	}
	
	if(n-nfailed==0){
		free(brush->extra_cgrps);
		brush->extra_cgrps=NULL;
	}
	
	brush->n_extra_cgrps=n-nfailed;
}


/*}}}*/


/*{{{ Misc. */


static void get_text_align(int *alignret, ExtlTab tab)
{
	char *align=NULL;
	
	if(!extl_table_gets_s(tab, "text_align", &align))
		return;
	
	if(strcmp(align, "left")==0)
		*alignret=DEALIGN_LEFT;
	else if(strcmp(align, "right")==0)
		*alignret=DEALIGN_RIGHT;
	else if(strcmp(align, "center")==0)
		*alignret=DEALIGN_CENTER;
	else
		warn("Unknown text alignment \"%s\".", align);
	
	free(align);
}


static void get_transparent_background(uint *mode, ExtlTab tab)
{
	if(extl_table_is_bool_set(tab, "transparent_background"))
		*mode=GR_TRANSPARENCY_YES;
	else
		*mode=GR_TRANSPARENCY_NO;
}


/*}}}*/


/*{{{ de_define_style */


/*EXTL_DOC
 * Define a style for the root window \var{rootwin}. Use
 * \fnref{de_define_style} instead to define for all root windows.
 */
EXTL_EXPORT
bool de_do_define_style(WRootWin *rootwin, const char *name, ExtlTab tab)
{
	DEBrush *brush;
	char *fnt;
	uint n;

	if(name==NULL)
		return FALSE;
	
	brush=de_find_or_create_brush(rootwin, name);

	if(brush==NULL)
		return FALSE;

	get_border(&(brush->border), tab);
	get_border_val(&(brush->spacing), tab, "spacing");

	get_text_align(&(brush->textalign), tab);

	get_transparent_background(&(brush->transparency_mode), tab);
	
	if(extl_table_gets_s(tab, "font", &fnt)){
		brush->font=de_load_font(fnt);
		free(fnt);
	}else{
		brush->font=de_load_font(CF_FALLBACK_FONT_NAME);
	}
#ifndef CF_UTF8
	if(brush->font!=NULL)
		XSetFont(wglobal.dpy, brush->normal_gc, brush->font->fid);
#endif

	brush->cgrp_alloced=TRUE;
	get_colour_group(rootwin, &(brush->cgrp), tab);
	get_extra_cgrps(rootwin, brush, tab);
	
	brush->data_table=extl_ref_table(tab);
	
	return TRUE;
}

	
/*}}}*/


/*{{{ Module initialisation */


#include "../version.h"

char de_module_ion_version[]=ION_VERSION;


extern bool de_module_register_exports();
extern void de_module_unregister_exports();


bool de_module_init()
{
	WRootWin *rootwin;
	DEBrush *brush;
	
#ifndef CF_UTF8
	if(ioncore_is_utf8()){
		warn_obj("DE module", "Ioncore uses UTF8 but the module was not "
				 "compiled with UTF8 support.");
	}
#else
	if(!ioncore_is_utf8()){
		warn("Ioncore does not use UTF8 but module expects UTF8 strings.");
	}
#endif
	
	if(!de_module_register_exports())
		return FALSE;
	
	if(!read_config_for("delib"))
		goto fail;
	
	if(!gr_register_engine("de", (GrGetBrushFn*)&de_get_brush,
						   (GrGetValuesFn*)&de_get_brush_values)){
		warn("DE module", "Failed to register the drawing engine");
		goto fail;
	}
	
	/* Create fallback brushes */
	FOR_ALL_ROOTWINS(rootwin){
		brush=de_find_or_create_brush(rootwin, "*");
		if(brush==NULL){
			warn_obj("DE module", "Could not initialise fallback brush for "
					 "root window %d.\n", rootwin->xscr);
		}
		brush->is_fallback=TRUE;
		brush->font=de_load_font(CF_FALLBACK_FONT_NAME);
	}
	
	return TRUE;
	
fail:
	de_module_unregister_exports();
	return FALSE;
}


void de_module_deinit()
{
	gr_unregister_engine("de");
	de_module_unregister_exports();
	de_deinit_brushes();
}


/*}}}*/
