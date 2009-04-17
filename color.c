/*
 * color.c - color functions
 *
 * Copyright © 2008-2009 Julien Danjou <julien@danjou.info>
 * Copyright © 2009 Uli Schlachter <psychon@znc.in>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "color.h"
#include "structs.h"
#include <ctype.h>

#define RGB_8TO16(i)   (65536 * ((i) & 0xff) / 255)

/** Parse an hexadecimal color string to its component.
 * \param colstr The color string.
 * \param len The color string length.
 * \param red A pointer to the red color to fill.
 * \param green A pointer to the green color to fill.
 * \param blue A pointer to the blue color to fill.
 * \param alpha A pointer to the alpha color to fill.
 * \return True if everything alright.
 */
static bool
color_parse(const char *colstr, ssize_t len,
            uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t *alpha)
{
    unsigned long colnum;
    char *p;

    if(len == 7)
    {
        colnum = strtoul(colstr + 1, &p, 16);
        if(p - colstr != 7)
            goto invalid;
        *alpha = 0xff;
    }
    /* we have alpha */
    else if(len == 9)
    {
        colnum = strtoul(colstr + 1, &p, 16);
        if(p - colstr != 9)
            goto invalid;
        *alpha = colnum & 0xff;
        colnum >>= 8;
    }
    else
    {
      invalid:
        warn("awesome: error, invalid color '%s'", colstr);
        return false;
    }

    *red   = (colnum >> 16) & 0xff;
    *green = (colnum >> 8) & 0xff;
    *blue  = colnum & 0xff;

    return true;
}

/** Send a request to initialize a X color.
 * \param color xcolor_t struct to store color into.
 * \param colstr Color specification.
 * \return request informations.
 */
xcolor_init_request_t
xcolor_init_unchecked(xcolor_t *color, const char *colstr, ssize_t len)
{
    xcb_screen_t *s = xutil_screen_get(globalconf.connection, globalconf.default_screen);
    xcolor_init_request_t req;
    uint8_t red, green, blue, alpha;

    p_clear(&req, 1);

    if(!len)
    {
        req.has_error = true;
        return req;
    }

    req.alpha = 0xffff;
    req.color = color;

    /* The color is given in RGB value */
    if(colstr[0] == '#')
    {
        if(!color_parse(colstr, len, &red, &green, &blue, &alpha))
        {
            warn("awesome: error, invalid color '%s'", colstr);
            req.has_error = true;
            return req;
        }

        req.alpha = RGB_8TO16(alpha);

        req.is_hexa = true;
        req.cookie_hexa = xcb_alloc_color_unchecked(globalconf.connection,
                                                    s->default_colormap,
                                                    RGB_8TO16(red),
                                                    RGB_8TO16(green),
                                                    RGB_8TO16(blue));
    }
    else
    {
        req.is_hexa = false;
        req.cookie_named = xcb_alloc_named_color_unchecked(globalconf.connection,
                                                           s->default_colormap, len,
                                                           colstr);
    }

    req.has_error = false;
    req.colstr = colstr;

    return req;
}

/** Initialize a X color.
 * \param req xcolor_init request.
 * \return True if color allocation was successfull.
 */
bool
xcolor_init_reply(xcolor_init_request_t req)
{
    if(req.has_error)
        return false;

    if(req.is_hexa)
    {
        xcb_alloc_color_reply_t *hexa_color;

        if((hexa_color = xcb_alloc_color_reply(globalconf.connection,
                                               req.cookie_hexa, NULL)))
        {
            req.color->pixel = hexa_color->pixel;
            req.color->red   = hexa_color->red;
            req.color->green = hexa_color->green;
            req.color->blue  = hexa_color->blue;
            req.color->alpha = req.alpha;
            req.color->initialized = true;
            p_delete(&hexa_color);
            return true;
        }
    }
    else
    {
        xcb_alloc_named_color_reply_t *named_color;

        if((named_color = xcb_alloc_named_color_reply(globalconf.connection,
                                                      req.cookie_named, NULL)))
        {
            req.color->pixel = named_color->pixel;
            req.color->red   = named_color->visual_red;
            req.color->green = named_color->visual_green;
            req.color->blue  = named_color->visual_blue;
            req.color->alpha = req.alpha;
            req.color->initialized = true;
            p_delete(&named_color);
            return true;
        }
    }

    warn("awesome: error, cannot allocate color '%s'", req.colstr);
    return false;
}

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
