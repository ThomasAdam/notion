/*
 * ion/ioncore/sizepolicy.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2006. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#include <libtu/minmax.h>

#include "common.h"
#include "region.h"
#include "resize.h"
#include "sizehint.h"
#include "sizepolicy.h"


static void do_gravity(const WRectangle *max_geom, int gravity,
                       WRectangle *geom)
{
    switch(gravity){
    case WestGravity:
    case NorthWestGravity:
    case SouthWestGravity:
        geom->x=max_geom->x;
        break;

    case EastGravity:
    case NorthEastGravity:
    case SouthEastGravity:
        geom->x=max_geom->x+max_geom->w-geom->w;
        break;

    default:
        geom->x=max_geom->x+max_geom->w/2-geom->w/2;
    }

    switch(gravity){
    case NorthGravity:
    case NorthWestGravity:
    case NorthEastGravity:
        geom->y=max_geom->y;
        break;

    case SouthGravity:
    case SouthWestGravity:
    case SouthEastGravity:
        geom->y=max_geom->y+max_geom->h-geom->h;
        break;

    default:
        geom->y=max_geom->y+max_geom->h/2-geom->h/2;
    }
    
    if(geom->h<=1)
        geom->h=1;
    if(geom->w<=1)
        geom->w=1;
}


static void correct_size(WRegion *reg, int *w, int *h)
{
    XSizeHints hints;
    
    if(reg!=NULL){
        region_size_hints(reg, &hints);
        xsizehints_correct(&hints, w, h, FALSE);
    }
}


static void gravity_stretch_policy(int gravity, WRegion *reg,
                                   const WRectangle *rq_geom, WFitParams *fp, 
                                   bool ws, bool hs)
{
    WRectangle max_geom=fp->g;
    int w, h;
    
    fp->g=*rq_geom;
    
    w=(ws ? max_geom.w : minof(fp->g.w, max_geom.w));
    h=(hs ? max_geom.h : minof(fp->g.h, max_geom.h));
    
    if(fp->g.w!=w || fp->g.h!=h){
        /* Only apply size hint correction if we shrunk the 
         * requested geometry.
         */
        correct_size(reg, &w, &h);
    }
    
    fp->g.w=w;
    fp->g.h=h;

    do_gravity(&max_geom, gravity, &(fp->g));
    
    fp->mode=REGION_FIT_EXACT;
}


#define STRETCHPOLICY(G, X, WS, HS)                                \
    if(szplcy==SIZEPOLICY_GRAVITY_##G){                            \
        gravity_stretch_policy(X##Gravity, reg, &tmp, fp, WS, HS); \
        return;                                                    \
    }

#define GRAVPOLICY(G, X) STRETCHPOLICY(G, X, FALSE, FALSE);


void sizepolicy(WSizePolicy szplcy, WRegion *reg,
                const WRectangle *rq_geom, WFitParams *fp)
{
    WRectangle tmp;
    if(rq_geom!=NULL)
        tmp=*rq_geom;
    else if(reg!=NULL)
        tmp=REGION_GEOM(reg);
    else
        tmp=fp->g;

    GRAVPOLICY(NORTHWEST, NorthWest);
    GRAVPOLICY(NORTH, North);
    GRAVPOLICY(NORTHEAST, NorthEast);
    GRAVPOLICY(WEST, West);
    GRAVPOLICY(CENTER, Center);
    GRAVPOLICY(EAST, East);
    GRAVPOLICY(SOUTHWEST, SouthWest);
    GRAVPOLICY(SOUTH, South);
    GRAVPOLICY(SOUTHEAST, SouthEast);

    STRETCHPOLICY(NORTH, North, TRUE, FALSE);
    STRETCHPOLICY(EAST, East, FALSE, TRUE);
    STRETCHPOLICY(WEST, West, FALSE, TRUE);
    STRETCHPOLICY(SOUTH, South, TRUE, FALSE);

    if(szplcy==SIZEPOLICY_FREE){
        /* TODO: size hints */
        rectangle_constrain(&tmp, &(fp->g));
        correct_size(reg, &fp->g.w, &fp->g.h);
        fp->g=tmp;
        fp->mode=REGION_FIT_EXACT;
    }else if(szplcy==SIZEPOLICY_FULL_EXACT){
        gravity_stretch_policy(CenterGravity, reg, &tmp, fp, TRUE, TRUE);
    }else{ /* szplcy==SIZEPOLICY_FULL_BOUNDS */
        fp->mode=REGION_FIT_BOUNDS;
    }
}
