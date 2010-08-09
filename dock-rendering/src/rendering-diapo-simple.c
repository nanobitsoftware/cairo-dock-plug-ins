/**
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <cairo.h>

#include "rendering-diapo-simple.h"

extern gint     my_diapo_simple_iconGapX;
extern gint     my_diapo_simple_iconGapY;
extern gdouble  my_diapo_simple_fScaleMax;
extern gint     my_diapo_simple_sinW;
extern gboolean my_diapo_simple_lineaire;
extern gboolean  my_diapo_simple_wide_grid;

extern gdouble  my_diapo_simple_color_frame_start[4];
extern gdouble  my_diapo_simple_color_frame_stop[4];
extern gboolean my_diapo_simple_fade2bottom;
extern gboolean my_diapo_simple_fade2right;
extern gint    my_diapo_simple_arrowWidth;
extern gint    my_diapo_simple_arrowHeight;
extern gdouble  my_diapo_simple_arrowShift;
extern gint    my_diapo_simple_lineWidth;
extern gint    my_diapo_simple_radius;
extern gdouble  my_diapo_simple_color_border_line[4];
extern gboolean my_diapo_simple_draw_background;
extern gboolean my_diapo_simple_display_all_labels;

extern gdouble  my_diapo_simple_color_scrollbar_line[4];
extern gdouble  my_diapo_simple_color_scrollbar_inside[4];
extern gdouble  my_diapo_simple_color_grip[4];

const gint X_BORDER_SPACE = 40;  // espace laisse de chaque cote pour eviter de sortir trop facilement (et pour laisser de la place pour les etiquettes).
const gint ARROW_TIP = 5;  // pour gerer la pointe de la fleche.
const double fArrowHeight = 14, fScrollbarWidth = 10, fScrollbarArrowGap = 4;
const double fScrollbarIconGap = 10;
/// On considere qu'on a my_diapo_simple_iconGapX entre chaque icone horizontalement, et my_diapo_simple_iconGapX/2 entre les icones et les bords (pour aerer un peu plus le dessin). Idem verticalement. X_BORDER_SPACE est la pour empecher que les icones debordent de la fenetre au zoom.

typedef struct {
	gint nRowsX;
	gint nRowsY;
	gint iDeltaHeight;  // hauteur scrollable, en pixels
	gint iScrollOffset;  // hauteur scrollee, en pixels, positive.
	gboolean bDraggingScrollbar;  // si le clic est couramment enfonce sur la scrollbar.
	guint iSidPressEvent;  // sid du clic
	guint iSidReleaseEvent;  // sid du relachement du clic
	gint iClickY;  // hauteur ou on a clique, en coordonnees fenetre
	gint iClickOffset;  // hauteur scrollee au moment du clic
	} CDSlideData;

static void cd_rendering_render_diapo_simple (cairo_t *pCairoContext, CairoDock *pDock);

// Fonctions utiles pour transformer l'index de la liste en couple (x,y) sur la grille et inversement.
static inline void _get_gridXY_from_index (guint nRowsX, guint index, guint* gridX, guint* gridY)
{
	*gridX = index % nRowsX;
	*gridY = index / nRowsX;
}
static inline guint _get_index_from_gridXY (guint nRowsX, guint gridX, guint gridY)
{
	return gridX + gridY * nRowsX;
}

static guint _cd_rendering_diapo_simple_guess_grid (GList *pIconList, guint *nRowX, guint *nRowY)
{
	// Calcul du nombre de lignes (nY) / colonnes (nX) :
	guint count = 0;  // g_list_length (pIconList)
	GList *ic;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
			count ++;
	}
	
	if (count == 0)
	{
		*nRowX = 0;
		*nRowY = 0;
	}
	else if (my_diapo_simple_wide_grid)
	{
		*nRowX = ceil(sqrt(count));
		*nRowY = ceil(((double) count) / *nRowX);
	}
	else
	{
		*nRowY = ceil(sqrt(count));
		*nRowX = ceil(((double) count) / *nRowY);
	}
	return count;
}

const double sr = .5;  // screen ratio

static void _set_scroll (CairoDock *pDock, int iOffsetY)
{
	//g_print ("%s (%d)\n", __func__, iOffsetY);
	CDSlideData *pData = pDock->pRendererData;
	g_return_if_fail (pData != NULL);
	
	pData->iScrollOffset = MAX (0, MIN (iOffsetY, pData->iDeltaHeight));
	cairo_dock_calculate_dock_icons (pDock);
	gtk_widget_queue_draw (pDock->container.pWidget);
}
static gboolean _add_scroll (CairoDock *pDock, int iDeltaOffsetY)
{
	//g_print ("%s (%d)\n", __func__, iDeltaOffsetY);
	CDSlideData *pData = pDock->pRendererData;
	g_return_val_if_fail (pData != NULL, FALSE);
	
	if (iDeltaOffsetY < 0)
	{
		if (pData->iScrollOffset <= 0)
			return FALSE;
	}
	else
	{
		if (pData->iScrollOffset >= pData->iDeltaHeight)
			return FALSE;
	}
	_set_scroll (pDock, pData->iScrollOffset + iDeltaOffsetY);
	return TRUE;
}

static gboolean _cd_slide_on_scroll (gpointer data, Icon *pClickedIcon, CairoDock *pDock, int iDirection)
{
	CDSlideData *pData = pDock->pRendererData;
	g_return_val_if_fail (pData != NULL, CAIRO_DOCK_LET_PASS_NOTIFICATION);
	if (pData->iDeltaHeight == 0)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	gboolean bScrolled = _add_scroll (pDock, iDirection == 1 ? pDock->iMaxIconHeight : - pDock->iMaxIconHeight);
	return (bScrolled ? CAIRO_DOCK_INTERCEPT_NOTIFICATION : CAIRO_DOCK_LET_PASS_NOTIFICATION);
}
static gboolean _cd_slide_on_click (gpointer data, Icon *pClickedIcon, CairoDock *pDock, guint iButtonState)
{
	CDSlideData *pData = pDock->pRendererData;
	g_return_val_if_fail (pData != NULL, CAIRO_DOCK_LET_PASS_NOTIFICATION);
	if (pData->iDeltaHeight == 0)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	if (pData->bDraggingScrollbar)
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	else
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}
gboolean _cd_slide_on_press_button (GtkWidget* pWidget, GdkEventButton* pButton, CairoDock *pDock)
{
	CDSlideData *pData = pDock->pRendererData;
	g_return_val_if_fail (pData != NULL, FALSE);
	if (pData->iDeltaHeight == 0)
		return FALSE;
	
	if (pButton->type == GDK_BUTTON_PRESS && pButton->button == 1)
	{
		double x_arrow = pDock->iMaxDockWidth - X_BORDER_SPACE - fScrollbarWidth;
		if (pButton->x > x_arrow)  // on a clique dans la zone de scroll.
		{
			//g_print ("click (y=%d, scroll=%d)\n", (int) pButton->y, pData->iScrollOffset);
			
			// on regarde sur quoi on clic.
			double y_arrow_top, y_arrow_bottom;
			if (pDock->container.bDirectionUp)
			{
				y_arrow_bottom = pDock->iMaxDockHeight - (my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth);
				y_arrow_top = my_diapo_simple_lineWidth;
			}
			else
			{
				y_arrow_bottom = pDock->iMaxDockHeight - my_diapo_simple_lineWidth;
				y_arrow_top = my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth;
			}
			if (pButton->y > y_arrow_top - fScrollbarArrowGap/2 && pButton->y < y_arrow_top + fArrowHeight + fScrollbarArrowGap/2)  // bouton haut
			{
				_set_scroll (pDock, 0);
			}
			else if (pButton->y < y_arrow_bottom + fScrollbarArrowGap/2 && pButton->y > y_arrow_bottom - fArrowHeight - fScrollbarArrowGap/2)  // bouton bas
			{
				_set_scroll (pDock, pData->iDeltaHeight);
			}
			else  // scrollbar
			{
				pData->bDraggingScrollbar = TRUE;
				pData->iClickY = pButton->y;
				pData->iClickOffset = pData->iScrollOffset;
			}
		}
	}
	else if (GDK_BUTTON_RELEASE)
	{
		//g_print ("release\n");
		pData->bDraggingScrollbar = FALSE;
	}
	return FALSE;
}
static gboolean _cd_slide_on_mouse_moved (gpointer data, CairoDock *pDock, gboolean *bStartAnimation)
{
	CDSlideData *pData = pDock->pRendererData;
	g_return_val_if_fail (pData != NULL, CAIRO_DOCK_LET_PASS_NOTIFICATION);
	if (pData->iDeltaHeight == 0)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	if (pData->bDraggingScrollbar)
	{
		//g_print ("scroll on motion (y=%d)\n", pDock->container.iMouseY);
		
		double y_arrow_top, y_arrow_bottom;
		if (pDock->container.bDirectionUp)
		{
			y_arrow_bottom = my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth;
			y_arrow_top = pDock->iMaxDockHeight - my_diapo_simple_lineWidth;
		}
		else
		{
			y_arrow_top = pDock->iMaxDockHeight - (my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth);
			y_arrow_bottom = my_diapo_simple_lineWidth;
		}
		double fFrameHeight = pDock->iMaxDockHeight- (my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth);  // hauteur du cadre avec les rayons et sans la pointe.
		double fGripHeight = fFrameHeight / (fFrameHeight + pData->iDeltaHeight) * (y_arrow_top - y_arrow_bottom - 2*(fArrowHeight+fScrollbarArrowGap));
		double ygrip = (double) pData->iScrollOffset / pData->iDeltaHeight * (y_arrow_top - y_arrow_bottom - 2*(fArrowHeight+fScrollbarArrowGap) - fGripHeight);
		
		int delta = pDock->container.iMouseY - pData->iClickY;
		_set_scroll (pDock, (pData->iClickOffset + (double)delta / (y_arrow_top - y_arrow_bottom - 2*(fArrowHeight+fScrollbarArrowGap) - fGripHeight) * pData->iDeltaHeight));
	}
	return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
}
gboolean cd_slide_on_leave (gpointer data, CairoDock *pDock, gboolean *bStartAnimation)
{
	CDSlideData *pData = pDock->pRendererData;
	//g_return_val_if_fail (pData != NULL, CAIRO_DOCK_LET_PASS_NOTIFICATION);
	if (pData == NULL || ! pDock->pRenderer || pDock->pRenderer->render != cd_rendering_render_diapo_simple)  // pas nous
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	//g_print (" LEAVE (%d)\n", pData->bDraggingScrollbar);
	
	return (pData->bDraggingScrollbar ? CAIRO_DOCK_INTERCEPT_NOTIFICATION : CAIRO_DOCK_LET_PASS_NOTIFICATION);
}
static void cd_rendering_calculate_max_dock_size_diapo_simple (CairoDock *pDock)
{
	// On calcule la configuration de la grille sans contrainte.
	guint nRowsX = 0;  // nb colonnes.
	guint nRowsY = 0;  // nb lignes.
	guint nIcones = 0;  // nb icones.
	int iDeltaHeight = 0;  // hauteur ne pouvant rentrer dans le dock.
	int iMaxIconWidth = 0;
	nIcones = _cd_rendering_diapo_simple_guess_grid(pDock->icons, &nRowsX, &nRowsY);
	
	// On calcule la taille de l'affichage avec contrainte taille ecran.
	if(nIcones != 0)
	{
		// on calcule la largeur avec contrainte, ce qui donne aussi le nombre de lignes.
		iMaxIconWidth = ((Icon*)pDock->icons->data)->fWidth;  // approximation un peu bof.
		pDock->iMaxDockWidth = nRowsX * (iMaxIconWidth + my_diapo_simple_iconGapX) + 2*X_BORDER_SPACE;
		int iMaxWidth = sr * g_desktopGeometry.iXScreenWidth[pDock->container.bIsHorizontal];
		if (pDock->iMaxDockWidth > iMaxWidth)
		{
			nRowsX = (iMaxWidth - 2*X_BORDER_SPACE) / (iMaxIconWidth + my_diapo_simple_iconGapX);
			nRowsY = ceil((double) nIcones / nRowsX);
			pDock->iMaxDockWidth = nRowsX * (iMaxIconWidth + my_diapo_simple_iconGapX) + 2*X_BORDER_SPACE;
			//g_print ("%d -> %d\n", iMaxWidth, pDock->iMaxDockWidth);
		}
		
		// on calcule la hauteur avec contrainte, ce qui donne aussi la marge de defilement.
		pDock->iMaxDockHeight = (nRowsY - 1) * (pDock->iMaxIconHeight * pDock->container.fRatio + my_diapo_simple_iconGapY) +  // les icones
			pDock->iMaxIconHeight * pDock->container.fRatio * my_diapo_simple_fScaleMax +  // les icones des bords zooment
			myLabels.iLabelSize +  // le texte des icones de la 1ere ligne
			my_diapo_simple_lineWidth + // les demi-lignes du haut et du bas
			my_diapo_simple_arrowHeight + ARROW_TIP;  // la fleche etendue
		int iMaxHeight = MIN (pDock->iMaxDockWidth, sr * g_desktopGeometry.iXScreenHeight[pDock->container.bIsHorizontal]);
		if (pDock->iMaxDockHeight > iMaxHeight)
		{
			nRowsY = (iMaxHeight - (my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth + myLabels.iLabelSize + pDock->iMaxIconHeight * pDock->container.fRatio * my_diapo_simple_fScaleMax)) / (pDock->iMaxIconHeight * pDock->container.fRatio + my_diapo_simple_iconGapY);
			int iMaxDockHeight0 = pDock->iMaxDockHeight;
			pDock->iMaxDockHeight = (nRowsY - 1) * (pDock->iMaxIconHeight * pDock->container.fRatio + my_diapo_simple_iconGapY) +
				pDock->iMaxIconHeight * pDock->container.fRatio * my_diapo_simple_fScaleMax +
				myLabels.iLabelSize +
				my_diapo_simple_lineWidth +
				my_diapo_simple_arrowHeight + ARROW_TIP;
			iDeltaHeight = iMaxDockHeight0 - pDock->iMaxDockHeight;
			//g_print ("%d -> %d\n", iMaxHeight, pDock->iMaxDockHeight);
		}
		
		pDock->iMinDockWidth = pDock->iMaxDockWidth - 2*X_BORDER_SPACE;
		pDock->iMinDockHeight = pDock->iMaxDockHeight;
	}
	else
	{
		pDock->iMaxDockWidth = pDock->iMinDockWidth = X_BORDER_SPACE * 2 + 1;
		pDock->iMaxDockHeight = pDock->iMinDockHeight = my_diapo_simple_lineWidth + my_diapo_simple_arrowHeight + ARROW_TIP + 1;
	}
	
	// pas de decorations.
	pDock->iDecorationsHeight = 0;
	pDock->iDecorationsWidth  = 0;
	
	// On affecte ca aussi au cas ou.
	pDock->fFlatDockWidth = pDock->iMaxDockWidth;
	pDock->fMagnitudeMax = my_diapo_simple_fScaleMax / (1+g_fAmplitude);
	
	CDSlideData *pData = pDock->pRendererData;
	if (pData == NULL)
	{
		pData = g_new0 (CDSlideData, 1);
		pDock->pRendererData = pData;
		cairo_dock_register_notification_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_SCROLL_ICON, (CairoDockNotificationFunc) _cd_slide_on_scroll, CAIRO_DOCK_RUN_AFTER, NULL);
		cairo_dock_register_notification_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_CLICK_ICON, (CairoDockNotificationFunc) _cd_slide_on_click, CAIRO_DOCK_RUN_FIRST, NULL);
		cairo_dock_register_notification_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_MOUSE_MOVED, (CairoDockNotificationFunc) _cd_slide_on_mouse_moved, CAIRO_DOCK_RUN_AFTER, NULL);
		pData->iSidPressEvent = g_signal_connect (G_OBJECT (pDock->container.pWidget),
			"button-press-event",
			G_CALLBACK (_cd_slide_on_press_button),
			pDock);  // car les notification de clic en provenance du dock sont emises lors du relachement du bouton.
		pData->iSidReleaseEvent = g_signal_connect (G_OBJECT (pDock->container.pWidget),
			"button-release-event",
			G_CALLBACK (_cd_slide_on_press_button),
			pDock);
	}
	pData->nRowsX = nRowsX;
	pData->nRowsY = nRowsY;
	pData->iDeltaHeight = iDeltaHeight;
	if (iDeltaHeight != 0)
	{
		int iScrollMargin = iMaxIconWidth * (my_diapo_simple_fScaleMax - 1) / 2
			+ fScrollbarIconGap
			+ fScrollbarWidth;  // donc a droite on a : derniere icone en taille max + demi-gapx + + gab + scrollbar + X_BORDER_SPACE
		pDock->iMaxDockWidth += iScrollMargin;
	}
}



//////////////////////////////////////////////////////////////////////////////////////// Methodes de dessin :

static void cairo_dock_draw_frame_horizontal_for_diapo_simple (cairo_t *pCairoContext, CairoDock *pDock)
{
	const gdouble arrow_dec = 2;
	gdouble fFrameWidth  = pDock->iMaxDockWidth - 2 * X_BORDER_SPACE - 0 * my_diapo_simple_radius;
	gdouble fFrameHeight = pDock->iMaxDockHeight - (my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth);
	gdouble fDockOffsetX = X_BORDER_SPACE + 0*my_diapo_simple_radius;
	gdouble fDockOffsetY = (pDock->container.bDirectionUp ? .5*my_diapo_simple_lineWidth : my_diapo_simple_arrowHeight + ARROW_TIP);
	
	cairo_move_to (pCairoContext, fDockOffsetX, fDockOffsetY);

	//HautGauche -> HautDroit
	if(pDock->container.bDirectionUp)
	{
		cairo_rel_line_to (pCairoContext, fFrameWidth, 0);
	}
	else
	{
		//On fait la fleche
		cairo_rel_line_to (pCairoContext,  (fFrameWidth/2 - my_diapo_simple_arrowWidth/2 - my_diapo_simple_arrowShift * fFrameWidth), 0);                //     _
		cairo_rel_line_to (pCairoContext, + my_diapo_simple_arrowWidth/2 + my_diapo_simple_arrowShift * fFrameWidth - my_diapo_simple_arrowShift * fFrameWidth / arrow_dec,  -my_diapo_simple_arrowHeight);       //  \.
		cairo_rel_line_to (pCairoContext, + my_diapo_simple_arrowWidth/2 - my_diapo_simple_arrowShift * fFrameWidth + my_diapo_simple_arrowShift * fFrameWidth / arrow_dec, +my_diapo_simple_arrowHeight);        //    /
		cairo_rel_line_to (pCairoContext, (fFrameWidth/2 - my_diapo_simple_arrowWidth/2 + my_diapo_simple_arrowShift * fFrameWidth) , 0);               // _
	}
	//\_________________ Coin haut droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		my_diapo_simple_radius, 0,
		my_diapo_simple_radius, my_diapo_simple_radius );
	
	//HautDroit -> BasDroit
	cairo_rel_line_to (pCairoContext, 0, fFrameHeight + my_diapo_simple_lineWidth - my_diapo_simple_radius *  2 );
	//\_________________ Coin bas droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		0 , my_diapo_simple_radius,
		-my_diapo_simple_radius , my_diapo_simple_radius);
	
	//BasDroit -> BasGauche
	if(!pDock->container.bDirectionUp)
	{
		cairo_rel_line_to (pCairoContext, - fFrameWidth , 0);
	}
	else
	{
		//On fait la fleche
		cairo_rel_line_to (pCairoContext, - (fFrameWidth/2 - my_diapo_simple_arrowWidth/2 - my_diapo_simple_arrowShift * fFrameWidth), 0);                //     _
		cairo_rel_line_to (pCairoContext, - my_diapo_simple_arrowWidth/2 - my_diapo_simple_arrowShift * fFrameWidth + my_diapo_simple_arrowShift * fFrameWidth / arrow_dec, my_diapo_simple_arrowHeight);        //    /     
		cairo_rel_line_to (pCairoContext, - my_diapo_simple_arrowWidth/2 + my_diapo_simple_arrowShift * fFrameWidth - my_diapo_simple_arrowShift * fFrameWidth / arrow_dec, -my_diapo_simple_arrowHeight);       //  \. 
		cairo_rel_line_to (pCairoContext, - (fFrameWidth/2 - my_diapo_simple_arrowWidth/2 + my_diapo_simple_arrowShift * fFrameWidth) , 0);               // _      
	}
	//\_________________ Coin bas gauche.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		-my_diapo_simple_radius, 0,
		-my_diapo_simple_radius, -my_diapo_simple_radius);
	
	//BasGauche -> HautGauche
	cairo_rel_line_to (pCairoContext, 0, - fFrameHeight - my_diapo_simple_lineWidth + my_diapo_simple_radius * 2);
	//\_________________ Coin haut gauche.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		0 , -my_diapo_simple_radius ,
		my_diapo_simple_radius, -my_diapo_simple_radius);

}
static void cairo_dock_draw_frame_vertical_for_diapo_simple (cairo_t *pCairoContext, CairoDock *pDock)
{
	const gdouble arrow_dec = 2;
	gdouble fFrameWidth  = pDock->iMaxDockWidth - 2 * X_BORDER_SPACE - 2 * my_diapo_simple_radius;
	gdouble fFrameHeight = pDock->iMaxDockHeight - (my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth);
	gdouble fDockOffsetX = X_BORDER_SPACE + my_diapo_simple_radius;
	gdouble fDockOffsetY = (pDock->container.bDirectionUp ? .5*my_diapo_simple_lineWidth : my_diapo_simple_arrowHeight + ARROW_TIP);
	
	cairo_move_to (pCairoContext, fDockOffsetY, fDockOffsetX);

	if(pDock->container.bDirectionUp)
	{
		cairo_rel_line_to (pCairoContext, 0, fFrameWidth);
	}
	else
	{
		cairo_rel_line_to (pCairoContext,0,(fFrameWidth/2 - my_diapo_simple_arrowWidth/2 - my_diapo_simple_arrowShift * fFrameWidth));                //     _
		cairo_rel_line_to (pCairoContext, -my_diapo_simple_arrowHeight, my_diapo_simple_arrowWidth/2 + my_diapo_simple_arrowShift * fFrameWidth - my_diapo_simple_arrowShift * fFrameWidth / arrow_dec);       //  \. 
		cairo_rel_line_to (pCairoContext, my_diapo_simple_arrowHeight, + my_diapo_simple_arrowWidth/2 - my_diapo_simple_arrowShift * fFrameWidth + my_diapo_simple_arrowShift * fFrameWidth / arrow_dec);        //    /     
		cairo_rel_line_to (pCairoContext,0,(fFrameWidth/2 - my_diapo_simple_arrowWidth/2 + my_diapo_simple_arrowShift * fFrameWidth));               // _     
	}
	//\_________________ Coin haut droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		0, my_diapo_simple_radius,
		my_diapo_simple_radius, my_diapo_simple_radius);
	cairo_rel_line_to (pCairoContext, fFrameHeight + my_diapo_simple_lineWidth - my_diapo_simple_radius * 2, 0);
	//\_________________ Coin bas droit.
	cairo_rel_curve_to (pCairoContext,
			0, 0,
			my_diapo_simple_radius, 0,
			my_diapo_simple_radius, -my_diapo_simple_radius);
	if(!pDock->container.bDirectionUp)
	{
		cairo_rel_line_to (pCairoContext, 0, - fFrameWidth);
	}
	else
	{
		//On fait la fleche
		cairo_rel_line_to (pCairoContext, 0, - (fFrameWidth/2 - my_diapo_simple_arrowWidth/2 - my_diapo_simple_arrowShift * fFrameWidth));                 //     _
		cairo_rel_line_to (pCairoContext,  my_diapo_simple_arrowHeight, - my_diapo_simple_arrowWidth/2 - my_diapo_simple_arrowShift * fFrameWidth + my_diapo_simple_arrowShift * fFrameWidth / arrow_dec);        //    /     
		cairo_rel_line_to (pCairoContext, -my_diapo_simple_arrowHeight, - my_diapo_simple_arrowWidth/2 + my_diapo_simple_arrowShift * fFrameWidth - my_diapo_simple_arrowShift * fFrameWidth / arrow_dec );       //  \. 
		cairo_rel_line_to (pCairoContext, 0, - (fFrameWidth/2 - my_diapo_simple_arrowWidth/2 + my_diapo_simple_arrowShift * fFrameWidth));                 // _      
	}
	
	//\_________________ Coin bas gauche.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		0, -my_diapo_simple_radius,
		-my_diapo_simple_radius, -my_diapo_simple_radius);
	cairo_rel_line_to (pCairoContext, - fFrameHeight - my_diapo_simple_lineWidth + my_diapo_simple_radius * 2, 0);
	//\_________________ Coin haut gauche.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		-my_diapo_simple_radius, 0,
		-my_diapo_simple_radius, my_diapo_simple_radius);
}
static void cairo_dock_draw_frame_for_diapo_simple (cairo_t *pCairoContext, CairoDock *pDock)
{
	if (pDock->container.bIsHorizontal)
		cairo_dock_draw_frame_horizontal_for_diapo_simple (pCairoContext, pDock);
	else
		cairo_dock_draw_frame_vertical_for_diapo_simple (pCairoContext, pDock);
}

static void cairo_dock_render_decorations_in_frame_for_diapo_simple (cairo_t *pCairoContext, CairoDock *pDock, double fAlpha)
{
	// On se fait un beau pattern degrade :
	cairo_pattern_t *mon_super_pattern;
	mon_super_pattern = cairo_pattern_create_linear (0.0,
		0.0,
		my_diapo_simple_fade2right  ? pDock->iMaxDockWidth  : 0.0, // Y'aurait surement des calculs complexes à faire mais 
		my_diapo_simple_fade2bottom ? pDock->iMaxDockHeight : 0.0);     //  a quelques pixels près pour un dégradé : OSEF !
			
	cairo_pattern_add_color_stop_rgba (mon_super_pattern, 0, 
		my_diapo_simple_color_frame_start[0],
		my_diapo_simple_color_frame_start[1],
		my_diapo_simple_color_frame_start[2],
		my_diapo_simple_color_frame_start[3] * fAlpha);  // transparent -> opaque au depliage.
		
	cairo_pattern_add_color_stop_rgba (mon_super_pattern, 1, 
		my_diapo_simple_color_frame_stop[0],
		my_diapo_simple_color_frame_stop[1],
		my_diapo_simple_color_frame_stop[2],
		my_diapo_simple_color_frame_stop[3] * fAlpha);
	cairo_set_source (pCairoContext, mon_super_pattern);
	
	//On remplit le contexte en le préservant -> pourquoi ?  ----> parce qu'on va tracer le contour plus tard ;-)
	cairo_fill_preserve (pCairoContext);
	cairo_pattern_destroy (mon_super_pattern);
}

static void cd_rendering_render_diapo_simple (cairo_t *pCairoContext, CairoDock *pDock)
{
	double fAlpha = (pDock->fFoldingFactor < .3 ? (.3 - pDock->fFoldingFactor) / .3 : 0.);  // apparition du cadre de 0.3 a 0
	if (my_diapo_simple_draw_background)
	{
		//\____________________ On trace le cadre.
		cairo_save (pCairoContext);
		cairo_dock_draw_frame_for_diapo_simple (pCairoContext, pDock);
		
		//\____________________ On dessine les decorations dedans.
		cairo_dock_render_decorations_in_frame_for_diapo_simple (pCairoContext, pDock, fAlpha);

		//\____________________ On dessine le cadre.
		if (my_diapo_simple_lineWidth != 0 && my_diapo_simple_color_border_line[3] != 0 && fAlpha != 0)
		{
			cairo_set_line_width (pCairoContext,  my_diapo_simple_lineWidth);
			cairo_set_source_rgba (pCairoContext,
				my_diapo_simple_color_border_line[0],
				my_diapo_simple_color_border_line[1],
				my_diapo_simple_color_border_line[2],
				my_diapo_simple_color_border_line[3] * fAlpha);
			cairo_stroke (pCairoContext);
		}
		else
			cairo_new_path (pCairoContext);
		cairo_restore (pCairoContext);
	}
	
	if (pDock->icons == NULL)
		return;
	
	//\____________________ On dessine la ficelle qui les joint.
	//TODO Rendre joli !
	if (myIcons.iStringLineWidth > 0)
		cairo_dock_draw_string (pCairoContext, pDock, myIcons.iStringLineWidth, TRUE, TRUE);
	
	//\____________________ On dessine les barres de defilement.
	CDSlideData *pData = pDock->pRendererData;
	if (pData != NULL && pData->iDeltaHeight != 0)
	{
		cairo_save (pCairoContext);
		cairo_set_line_width (pCairoContext, 2.);
		
		double x_arrow = pDock->iMaxDockWidth - X_BORDER_SPACE - fScrollbarWidth/2;  // pointe de la fleche.
		double y_arrow_top, y_arrow_bottom;
		if (pDock->container.bDirectionUp)
		{
			y_arrow_bottom = pDock->iMaxDockHeight - (my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth);
			y_arrow_top = my_diapo_simple_lineWidth;
		}
		else
		{
			y_arrow_top = my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth;
			y_arrow_bottom = pDock->iMaxDockHeight - my_diapo_simple_lineWidth;
		}
		if (pData->iScrollOffset != 0)  // fleche vers le haut.
		{
			cairo_move_to (pCairoContext, x_arrow, y_arrow_top);
			cairo_rel_line_to (pCairoContext, fScrollbarWidth/2, fArrowHeight);
			cairo_rel_line_to (pCairoContext, -fScrollbarWidth, 0.);
			cairo_close_path (pCairoContext);
			
			cairo_set_source_rgba (pCairoContext, my_diapo_simple_color_scrollbar_inside[0], my_diapo_simple_color_scrollbar_inside[1], my_diapo_simple_color_scrollbar_inside[2], my_diapo_simple_color_scrollbar_inside[3] * fAlpha);
			cairo_fill_preserve (pCairoContext);
			
			cairo_set_source_rgba (pCairoContext, my_diapo_simple_color_scrollbar_line[0], my_diapo_simple_color_scrollbar_line[1], my_diapo_simple_color_scrollbar_line[2], my_diapo_simple_color_scrollbar_line[3] * fAlpha);
			cairo_stroke (pCairoContext);
		}
		if (pData->iScrollOffset != pData->iDeltaHeight)  // fleche vers le bas.
		{
			cairo_move_to (pCairoContext, x_arrow, y_arrow_bottom);
			cairo_rel_line_to (pCairoContext, fScrollbarWidth/2, - fArrowHeight);
			cairo_rel_line_to (pCairoContext, -fScrollbarWidth, 0.);
			cairo_close_path (pCairoContext);
			
			cairo_set_source_rgba (pCairoContext, my_diapo_simple_color_scrollbar_inside[0], my_diapo_simple_color_scrollbar_inside[1], my_diapo_simple_color_scrollbar_inside[2], my_diapo_simple_color_scrollbar_inside[3] * fAlpha);
			cairo_fill_preserve (pCairoContext);
			
			cairo_set_source_rgba (pCairoContext, my_diapo_simple_color_scrollbar_line[0], my_diapo_simple_color_scrollbar_line[1], my_diapo_simple_color_scrollbar_line[2], my_diapo_simple_color_scrollbar_line[3] * fAlpha);
			cairo_stroke (pCairoContext);
		}
		// scrollbar outline
		cairo_move_to (pCairoContext, x_arrow - fScrollbarWidth/2, y_arrow_top + fArrowHeight + fScrollbarArrowGap);
		cairo_rel_line_to (pCairoContext, fScrollbarWidth, 0.);
		cairo_rel_line_to (pCairoContext, 0., y_arrow_bottom - y_arrow_top - 2*(fArrowHeight+fScrollbarArrowGap));
		cairo_rel_line_to (pCairoContext, -fScrollbarWidth, 0.);
		cairo_close_path (pCairoContext);
		
		cairo_set_source_rgba (pCairoContext, my_diapo_simple_color_scrollbar_inside[0], my_diapo_simple_color_scrollbar_inside[1], my_diapo_simple_color_scrollbar_inside[2], my_diapo_simple_color_scrollbar_inside[3] * fAlpha);
		cairo_fill_preserve (pCairoContext);
		
		cairo_set_source_rgba (pCairoContext, my_diapo_simple_color_scrollbar_line[0], my_diapo_simple_color_scrollbar_line[1], my_diapo_simple_color_scrollbar_line[2], my_diapo_simple_color_scrollbar_line[3] * fAlpha);
		cairo_stroke (pCairoContext);
		// grip
		double fFrameHeight = pDock->iMaxDockHeight - (my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth);  // hauteur du cadre avec les rayons et sans la pointe.
		double fGripHeight = fFrameHeight / (fFrameHeight + pData->iDeltaHeight) * (y_arrow_bottom - y_arrow_top - 2*(fArrowHeight+fScrollbarArrowGap));
		double ygrip = (double) pData->iScrollOffset / pData->iDeltaHeight * (y_arrow_bottom - y_arrow_top - 2*(fArrowHeight+fScrollbarArrowGap) - fGripHeight);
		cairo_set_source_rgba (pCairoContext, my_diapo_simple_color_grip[0], my_diapo_simple_color_grip[1], my_diapo_simple_color_grip[2], my_diapo_simple_color_grip[3] * fAlpha);
		cairo_move_to (pCairoContext, x_arrow - fScrollbarWidth/2 + 1, y_arrow_top + fArrowHeight + fScrollbarArrowGap + ygrip);
		cairo_rel_line_to (pCairoContext, fScrollbarWidth - 2, 0.);
		cairo_rel_line_to (pCairoContext, 0., fGripHeight);
		cairo_rel_line_to (pCairoContext, - (fScrollbarWidth - 2), 0.);
		cairo_fill (pCairoContext);
		
		cairo_restore (pCairoContext);
	}
	
	//\____________________ On dessine les icones avec leurs etiquettes.
	// on determine la 1ere icone a tracer : l'icone suivant l'icone pointee.
	GList *pFirstDrawnElement = cairo_dock_get_first_drawn_element_linear (pDock->icons);
	if (pFirstDrawnElement == NULL)
		return;
	
	//clip pour le scroll
	if (pData != NULL && pData->iDeltaHeight != 0) // on fait un clip pour les icones qui debordent.
	{
		int h = my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth;
		if (pDock->container.bIsHorizontal)
		{
			cairo_rectangle (pCairoContext,
				0.,
				(pDock->container.bDirectionUp ? my_diapo_simple_lineWidth : h),  // top left corner.
				pDock->container.iWidth,
				pDock->container.iHeight - h - my_diapo_simple_lineWidth);
			cairo_clip (pCairoContext);
		}  // sinon clip inutile.
	}
	
	// on dessine les icones, l'icone pointee en dernier.
	Icon *icon;
	GList *ic = pFirstDrawnElement;
	do
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
		{
			ic = cairo_dock_get_next_element (ic, pDock->icons);
			continue;
		}
		
		cairo_save (pCairoContext);
		/// faire un clip si scroll != 0 ...
		cairo_dock_render_one_icon (icon, pDock, pCairoContext, 1., FALSE);
		cairo_restore (pCairoContext);
		
//////////////////////////////////////////////////////////////////////////////////////// On affiche le texte !
		gdouble zoom;
		if(icon->pTextBuffer != NULL && (my_diapo_simple_display_all_labels || icon->bPointed))
		{
			double fAlpha = (pDock->fFoldingFactor > .5 ? (1 - pDock->fFoldingFactor) / .5 : 1.);
			cairo_save (pCairoContext);
			
			double fOffsetX = -icon->iTextWidth/2 + icon->fWidthFactor * icon->fWidth * icon->fScale / 2;
			if (fOffsetX < 0)
				fOffsetX = 0;
			else if (0 + fOffsetX + icon->iTextWidth > pDock->container.iWidth)
				fOffsetX = pDock->container.iWidth - icon->iTextWidth - 0;
			
			if (icon->iTextWidth > icon->fWidth * icon->fScale + my_diapo_simple_iconGapX && ! icon->bPointed)
			{
				if (pDock->container.bIsHorizontal)
				{
					cairo_translate (pCairoContext,
						icon->fDrawX - my_diapo_simple_iconGapX/2,
						icon->fDrawY - icon->iTextHeight);
				}
				else
				{
					cairo_translate (pCairoContext,
						icon->fDrawY - my_diapo_simple_iconGapX/2,
						icon->fDrawX - icon->iTextHeight);
				}
				cairo_set_source_surface (pCairoContext,
					icon->pTextBuffer,
					fOffsetX,
					0.);
				
				cairo_pattern_t *pGradationPattern = cairo_pattern_create_linear (0.,
					0.,
					icon->fWidth * icon->fScale + my_diapo_simple_iconGapX,
					0.);
				cairo_pattern_set_extend (pGradationPattern, icon->bPointed ? CAIRO_EXTEND_PAD : CAIRO_EXTEND_NONE);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					0.,
					0.,
					0.,
					0.,
					fAlpha);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					0.75,
					0.,
					0.,
					0.,
					fAlpha);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					1.,
					0.,
					0.,
					0.,
					MIN (0.2, fAlpha/2));
				cairo_mask (pCairoContext, pGradationPattern);
				cairo_pattern_destroy (pGradationPattern);
			}
			else  // le texte tient dans l'icone.
			{
				if (pDock->container.bIsHorizontal)
				{
					fOffsetX = icon->fDrawX + (icon->fWidth * icon->fScale - icon->iTextWidth) / 2;
					if (fOffsetX < 0)
						fOffsetX = 0;
					else if (fOffsetX + icon->iTextWidth > pDock->container.iWidth)
						fOffsetX = pDock->container.iWidth - icon->iTextWidth;
					cairo_translate (pCairoContext,
						fOffsetX,
						icon->fDrawY - icon->iTextHeight);
				}
				else
				{
					fOffsetX = icon->fDrawY + (icon->fWidth * icon->fScale - icon->iTextWidth) / 2;
					if (fOffsetX < 0)
						fOffsetX = 0;
					else if (fOffsetX + icon->iTextWidth > pDock->container.iHeight)
						fOffsetX = pDock->container.iHeight - icon->iTextWidth;
					cairo_translate (pCairoContext,
						fOffsetX,
						icon->fDrawX - icon->iTextHeight);
				}
				cairo_set_source_surface (pCairoContext,
					icon->pTextBuffer,
					0.,
					0.);
				cairo_paint_with_alpha (pCairoContext, fAlpha);
			}
			cairo_restore (pCairoContext);
		}
		
		ic = cairo_dock_get_next_element (ic, pDock->icons);
	}
	while (ic != pFirstDrawnElement);
}



static Icon* _cd_rendering_calculate_icons_for_diapo_simple (CairoDock *pDock, gint nRowsX, gint nRowsY, gint Mx, gint My)
{
	double fScrollOffset = 0.;
	CDSlideData *pData = pDock->pRendererData;
	if (pData != NULL)
		fScrollOffset = (pDock->container.bDirectionUp ? - pData->iScrollOffset : pData->iScrollOffset);
	
	// On calcule la position de base pour toutes les icones
	int iOffsetY = .5 * pDock->iMaxIconHeight * pDock->container.fRatio * (my_diapo_simple_fScaleMax - 1) +  // les icones de la 1ere ligne zooment
		myLabels.iLabelSize +  // le texte des icones de la 1ere ligne
		.5 * my_diapo_simple_lineWidth +  // demi-ligne du haut;
		fScrollOffset;
	
	double fFoldingX = (pDock->fFoldingFactor > .2 ? (pDock->fFoldingFactor - .2) / .8 : 0.);  // placement de 1 a 0.2
	double fFoldingY = (pDock->fFoldingFactor > .5 ? (pDock->fFoldingFactor - .5) / .5 : 0.);  // placement de 1 a 0.5
	Icon* icon;
	GList* ic, *pointed_ic=NULL;
	int i, x, y;
	for (ic = pDock->icons, i = 0; ic != NULL; ic = ic->next, i++)
	{
		icon = ic->data;
		
		// position sur la grille.
		_get_gridXY_from_index(nRowsX, i, &x, &y);
		
		// on en deduit la position au repos.
		icon->fX = X_BORDER_SPACE + .5*my_diapo_simple_iconGapX + (icon->fWidth + my_diapo_simple_iconGapX) * x;
		if (pDock->container.bDirectionUp)
			icon->fY = iOffsetY + (icon->fHeight + my_diapo_simple_iconGapY) * y;
		else
			icon->fY = pDock->container.iHeight - iOffsetY - icon->fHeight - (nRowsY - 1 - y) * (icon->fHeight + my_diapo_simple_iconGapY) + (my_diapo_simple_arrowHeight + ARROW_TIP);  // la ligne du haut quand le dock est en bas, reste en haut quand le dock est en haut.
		
		// on en deduit le zoom par rapport a la position de la souris.
		gdouble distanceE = sqrt ((icon->fX + icon->fWidth/2 - Mx) * (icon->fX + icon->fWidth/2 - Mx) + (icon->fY + icon->fHeight/2 - My) * (icon->fY + icon->fHeight/2 - My));
		if (my_diapo_simple_lineaire)
		{
			gdouble eloignementMax = my_diapo_simple_sinW;  // 3. * (icon->fWidth + icon->fHeight)  / 2
			icon->fScale = MAX (1., my_diapo_simple_fScaleMax + (1. - my_diapo_simple_fScaleMax) * distanceE / eloignementMax);
			icon->fPhase = 0.;
		}
		else
		{
			icon->fPhase = distanceE * G_PI / my_diapo_simple_sinW + G_PI / 2.;
			if (icon->fPhase < 0)
				icon->fPhase = 0;
			else if (icon->fPhase > G_PI)
				icon->fPhase = G_PI;
			icon->fScale = 1. + (my_diapo_simple_fScaleMax - 1.) * sin (icon->fPhase);
		}
		
		// on tient compte du zoom (zoom centre).
		icon->fXMin = icon->fXMax = icon->fXAtRest =  // Ca on s'en sert pas encore
		icon->fDrawX = icon->fX + icon->fWidth  * (1. - icon->fScale) / 2;
		icon->fDrawY = icon->fY + icon->fHeight * (1. - icon->fScale) / 2;
		
		// on tient compte du depliage.
		icon->fDrawX -= (icon->fDrawX - pDock->container.iWidth/2) * fFoldingX;
		icon->fDrawY = icon->fDrawY + (pDock->container.bDirectionUp ?
			pDock->container.iHeight - (my_diapo_simple_arrowHeight + ARROW_TIP + icon->fDrawY) :
			- icon->fDrawY) * fFoldingY;
		icon->fAlpha = (pDock->fFoldingFactor > .7 ? (1 - pDock->fFoldingFactor) / (1 - .7) : 1.);  // apparition de 1 a 0.7
		
		// On regarde si l'icone est pointee, si oui elle est un peu plus visible que les autres.
		if((Mx > icon->fX - .5*my_diapo_simple_iconGapX) && 
			(My > icon->fY - .5*my_diapo_simple_iconGapY) &&
			(Mx < icon->fX + icon->fWidth  + .5*my_diapo_simple_iconGapX) &&
			(My < icon->fY + icon->fHeight + .5*my_diapo_simple_iconGapY))
		{
			icon->bPointed = TRUE;
			pointed_ic = ic;
		}
		else
		{
			icon->bPointed = FALSE;
			///icon->fAlpha *= 0.75;
		}
		
		// On affecte tous les parametres qui n'ont pas été défini précédement
		icon->fPhase = 0.;
		icon->fOrientation = 0 * 2. * G_PI * pDock->fFoldingFactor;
		icon->fWidthFactor = icon->fHeightFactor = (pDock->fFoldingFactor > .7 ? (1 - pDock->fFoldingFactor) / .3 : 1.);
		//icon->fWidthFactor = icon->fHeightFactor = 1.;
	}
	return (pointed_ic == NULL ? NULL : pointed_ic->data);
}
static void _cd_rendering_check_if_mouse_inside_diapo_simple (CairoDock *pDock)
{
	if ((pDock->container.iMouseX < 0) || (pDock->container.iMouseX > pDock->iMaxDockWidth - 1) || (pDock->container.iMouseY < 0) || (pDock->container.iMouseY > pDock->iMaxDockHeight - 0))
	{
		pDock->iMousePositionType = CAIRO_DOCK_MOUSE_OUTSIDE;
	}
	else  // on fait simple.
	{
		pDock->iMousePositionType = CAIRO_DOCK_MOUSE_INSIDE;
	}
}
static void _cd_rendering_check_can_drop_linear (CairoDock *pDock)
{
	pDock->bCanDrop = pDock->bIsDragging;  /// calculer bCanDrop ...
}
Icon *cd_rendering_calculate_icons_diapo_simple (CairoDock *pDock)
{
	if (pDock->icons == NULL)
		return NULL;
	
	// On calcule la configuration de la grille
	gint nRowsX = 0;
	gint nRowsY = 0;
	gint nIcones = 0;
	///nIcones = _cd_rendering_diapo_simple_guess_grid (pDock->icons, &nRowsX, &nRowsY);
	
	CDSlideData *pData = pDock->pRendererData;
	g_return_val_if_fail (pData != NULL, NULL);
	nRowsX = pData->nRowsX;
	nRowsY = pData->nRowsY;
	/*int iMaxIconWidth = ((Icon*)pDock->icons->data)->fWidth;  // approximation un peu bof.
	nRowsX = (pDock->iMaxDockWidth - 2*X_BORDER_SPACE) / (iMaxIconWidth + my_diapo_simple_iconGapX);
	nRowsY = (pDock->iMaxDockHeight - (my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth + myLabels.iLabelSize + pDock->iMaxIconHeight * pDock->container.fRatio * my_diapo_simple_fScaleMax)) / (pDock->iMaxIconHeight * pDock->container.fRatio + my_diapo_simple_iconGapY);*/
	
	// On calcule les parametres des icones
	Icon *pPointedIcon = _cd_rendering_calculate_icons_for_diapo_simple (pDock, nRowsX, nRowsY, pDock->container.iMouseX, pDock->container.iMouseY);
	
	_cd_rendering_check_if_mouse_inside_diapo_simple (pDock);
	
	_cd_rendering_check_can_drop_linear (pDock);
	
	return pPointedIcon;
}



static const double a = 2.5;  // definit combien la fleche est penchee.

#define RADIAN (G_PI / 180.0)  // Conversion Radian/Degres
#define DELTA_ROUND_DEGREE 5
#define TIP_OFFSET_FACTOR 2.
#define _recopy_prev_color(pColorTab, i) memcpy (&pColorTab[4*i], &pColorTab[4*(i-1)], 4*sizeof (GLfloat));
#define _copy_color(pColorTab, i, fAlpha, c) do { \
	pColorTab[4*i]   = c[0];\
	pColorTab[4*i+1] = c[1];\
	pColorTab[4*i+2] = c[2];\
	pColorTab[4*i+3] = c[3] * fAlpha; } while (0)
/*#define _copy_mean_color(pColorTab, i, fAlpha, c1, c2, f) do { \
	pColorTab[4*i]   = c1[0]*f + c2[0]*(1-f);\
	pColorTab[4*i+1] = c1[1]*f + c2[1]*(1-f);\
	pColorTab[4*i+2] = c1[2]*f + c2[2]*(1-f);\
	pColorTab[4*i+3] = (c1[3]*f + c2[3]*(1-f)) * fAlpha; } while (0)*/
static void cd_add_arrow_to_path (CairoDockGLPath *pPath, double fFrameWidth)
{
	double w = fFrameWidth / 2;
	double aw = my_diapo_simple_arrowWidth/2;
	double ah = my_diapo_simple_arrowHeight;
	double xa = my_diapo_simple_arrowShift * (w - aw);  // abscisse de l'extremite de la pointe.
	
	cairo_dock_gl_path_rel_line_to (pPath, w + xa - aw, 0.);  // pointe.
	cairo_dock_gl_path_rel_line_to (pPath, aw, -ah);
	cairo_dock_gl_path_rel_line_to (pPath, aw, ah);
}
static CairoDockGLPath *cd_generate_frame_path_without_arrow (double fFrameWidth, double fTotalHeight, double fRadius)
{
	static CairoDockGLPath *pPath = NULL;
	double fTotalWidth = fFrameWidth + 2 * fRadius;
	double fFrameHeight = MAX (0, fTotalHeight - 2 * fRadius);
	double w = fFrameWidth / 2;
	double h = fFrameHeight / 2;
	double r = fRadius;
	
	int iNbPoins1Round = 90/DELTA_ROUND_DEGREE;
	if (pPath == NULL)
		pPath = cairo_dock_new_gl_path ((iNbPoins1Round+1)*4+1+3, w, -h-r, fTotalWidth, fTotalHeight);  // on commence au coin haut droit pour avoir une bonne triangulation du polygone, et en raisonnant par rapport au centre du rectangle.
	else
		cairo_dock_gl_path_move_to (pPath, w, -h-r);
	
	cairo_dock_gl_path_arc (pPath, iNbPoins1Round,  w, -h, r, -G_PI/2, +G_PI/2);  // coin bas droit.
	
	cairo_dock_gl_path_arc (pPath, iNbPoins1Round, w, h, r, 0.,     +G_PI/2);  // coin haut droit.
	
	cairo_dock_gl_path_arc (pPath, iNbPoins1Round, -w,  h, r, G_PI/2,  +G_PI/2);  // coin haut gauche.
	
	cairo_dock_gl_path_arc (pPath, iNbPoins1Round, -w, -h, r, G_PI,    +G_PI/2);  // coin bas gauche.
	
	return pPath;
}

static CairoDockGLPath *cd_generate_arrow_path (double fFrameWidth, double fTotalHeight)
{
	static CairoDockGLPath *pPath = NULL;
	double w = fFrameWidth / 2;
	
	double aw = my_diapo_simple_arrowWidth/2;
	double ah = my_diapo_simple_arrowHeight;
	double xa = my_diapo_simple_arrowShift * (w - aw);  // abscisse de l'extremite de la pointe.
	
	if (pPath == NULL)
		pPath = cairo_dock_new_gl_path (3, xa - aw, -fTotalHeight/2, 0., 0.);
	else
		cairo_dock_gl_path_move_to (pPath, xa - aw, -fTotalHeight/2);
	
	cairo_dock_gl_path_rel_line_to (pPath, aw, -ah);
	cairo_dock_gl_path_rel_line_to (pPath, aw, ah);
	
	return pPath;
}

static const GLfloat *cd_generate_color_tab (double fAlpha, GLfloat *pMiddleBottomColor)
{
	static GLfloat *pColorTab = NULL;
	int iNbPoins1Round = 90/DELTA_ROUND_DEGREE;
	if (pColorTab == NULL)
		pColorTab = g_new (GLfloat, ((iNbPoins1Round+1)*4+1) * 4);
	
	double *pTopRightColor, *pTopLeftColor, *pBottomLeftColor, *pBottomRightColor;
	double pMeanColor[4] = {(my_diapo_simple_color_frame_start[0] + my_diapo_simple_color_frame_stop[0])/2,
		(my_diapo_simple_color_frame_start[1] + my_diapo_simple_color_frame_stop[1])/2,
		(my_diapo_simple_color_frame_start[2] + my_diapo_simple_color_frame_stop[2])/2,
		(my_diapo_simple_color_frame_start[3] + my_diapo_simple_color_frame_stop[3])/2};
	pTopLeftColor = my_diapo_simple_color_frame_start;
	if (my_diapo_simple_fade2bottom || my_diapo_simple_fade2right)
	{
		pBottomRightColor = my_diapo_simple_color_frame_stop;
		if (my_diapo_simple_fade2bottom && my_diapo_simple_fade2right)
		{
			pBottomLeftColor = pMeanColor;
			pTopRightColor = pMeanColor;
		}
		else if (my_diapo_simple_fade2bottom)
		{
			pBottomLeftColor = my_diapo_simple_color_frame_stop;
			pTopRightColor = my_diapo_simple_color_frame_start;
		}
		else
		{
			pBottomLeftColor = my_diapo_simple_color_frame_start;
			pTopRightColor = my_diapo_simple_color_frame_stop;
		}
	}
	else
	{
		pBottomRightColor = my_diapo_simple_color_frame_start;
		pBottomLeftColor = my_diapo_simple_color_frame_start;
		pTopRightColor = my_diapo_simple_color_frame_start;
	}
	
	pMiddleBottomColor[0] = (pBottomRightColor[0] + pBottomLeftColor[0])/2;
	pMiddleBottomColor[1] = (pBottomRightColor[1] + pBottomLeftColor[1])/2;
	pMiddleBottomColor[2] = (pBottomRightColor[2] + pBottomLeftColor[2])/2;
	pMiddleBottomColor[3] = (pBottomRightColor[3] + pBottomLeftColor[3])/2;
	
	int i=0, j;
	_copy_color (pColorTab, i, fAlpha, pBottomRightColor);
	i ++;
	
	for (j = 0; j < iNbPoins1Round; j ++, i ++)  // coin bas droit.
	{
		_copy_color (pColorTab, i, fAlpha, pBottomRightColor);
	}
	
	for (j = 0; j < iNbPoins1Round; j ++, i ++)  // coin haut droit.
	{
		_copy_color (pColorTab, i, fAlpha, pTopRightColor);
	}
	
	for (j = 0; j < iNbPoins1Round; j ++, i ++)  // coin haut gauche.
	{
		_copy_color (pColorTab, i, fAlpha, pTopLeftColor);
	}
	
	for (j = 0; j < iNbPoins1Round; j ++, i ++)  // coin bas gauche.
	{
		_copy_color (pColorTab, i, fAlpha, pBottomLeftColor);
	}
	
	return pColorTab;
}

static void cd_rendering_render_diapo_simple_opengl (CairoDock *pDock)
{
	static CairoDockGLPath *pScrollPath = NULL;
	
	//\____________________ On initialise le cadre.
	int iNbVertex;
	GLfloat *pColorTab, *pVertexTab;
	
	double fRadius = my_diapo_simple_radius;
	double fFrameWidth  = pDock->iMaxDockWidth - 2*X_BORDER_SPACE;  // longueur du trait horizontal.
	double fFrameHeight = pDock->iMaxDockHeight - (my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth);  // hauteur du cadre avec les rayons et sans la pointe.
	double fDockOffsetX, fDockOffsetY;
	
	fDockOffsetX = X_BORDER_SPACE;
	fDockOffsetY = my_diapo_simple_arrowHeight+ARROW_TIP;
	///fFrameWidth  = pDock->iMaxDockWidth - 2*X_BORDER_SPACE;  // longueur du trait horizontal.
	///fFrameHeight = pDock->iMaxDockHeight- (my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth);  // hauteur du cadre avec les rayons et sans la pointe.
	
	//\_____________ On genere les coordonnees du contour.
	CairoDockGLPath *pFramePath = cd_generate_frame_path_without_arrow (fFrameWidth, fFrameHeight, fRadius);
	
	//\_____________ On remplit avec le fond.
	glPushMatrix ();
	double fAlpha = (pDock->fFoldingFactor < .3 ? (.3 - pDock->fFoldingFactor) / .3 : 0.);  // apparition du cadre de 0.3 a 0
	cairo_dock_set_container_orientation_opengl (CAIRO_CONTAINER (pDock));
	glTranslatef (fDockOffsetX + (fFrameWidth)/2,
		fDockOffsetY + fFrameHeight/2,
		0.);
	_cairo_dock_set_blend_alpha ();
	
	if (my_diapo_simple_draw_background)
	{
		// le cadre sans la pointe.
		GLfloat pBottomMiddleColor[4];
		const GLfloat *pColorTab = cd_generate_color_tab (fAlpha, pBottomMiddleColor);
		glEnableClientState (GL_COLOR_ARRAY);
		glColorPointer (4, GL_FLOAT, 0, pColorTab);
		cairo_dock_fill_gl_path (pFramePath, 0);
		glDisableClientState (GL_COLOR_ARRAY);
		
		// la pointe.
		CairoDockGLPath *pArrowPath = cd_generate_arrow_path (fFrameWidth, fFrameHeight);
		glColor4f (pBottomMiddleColor[0], pBottomMiddleColor[1], pBottomMiddleColor[2], pBottomMiddleColor[3] * fAlpha);
		cairo_dock_fill_gl_path (pArrowPath, 0);
	}
	
	//\_____________ On trace le contour.
	if (my_diapo_simple_lineWidth != 0 && my_diapo_simple_color_border_line[3] != 0 && fAlpha != 0)
	{
		cd_add_arrow_to_path (pFramePath, fFrameWidth);
		glLineWidth (my_diapo_simple_lineWidth);
		glColor4f (my_diapo_simple_color_border_line[0], my_diapo_simple_color_border_line[1], my_diapo_simple_color_border_line[2], my_diapo_simple_color_border_line[3] * fAlpha);
		cairo_dock_stroke_gl_path (pFramePath, TRUE);
	}
	glPopMatrix ();
	_cairo_dock_set_blend_over ();
	
	//\____________________ On dessine les barres de defilement.
	CDSlideData *pData = pDock->pRendererData;
	if (pData != NULL && pData->iDeltaHeight != 0)
	{
		glPushMatrix ();
		if (pScrollPath == NULL)
			pScrollPath = cairo_dock_new_gl_path (4, 0., 0., 0, 0);  // des triangles ou des rectangles => 4 points max.
		glLineWidth (2.);
		
		double x_arrow = pDock->iMaxDockWidth - X_BORDER_SPACE - fScrollbarWidth/2;  // pointe de la fleche.
		double y_arrow_top, y_arrow_bottom;
		if (pDock->container.bDirectionUp)
		{
			y_arrow_bottom = my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth;
			y_arrow_top = pDock->iMaxDockHeight - my_diapo_simple_lineWidth;
		}
		else
		{
			y_arrow_top = pDock->iMaxDockHeight - (my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth);
			y_arrow_bottom = my_diapo_simple_lineWidth;
		}
		if (pData->iScrollOffset != 0)  // fleche vers le haut.
		{
			cairo_dock_gl_path_move_to (pScrollPath, x_arrow, y_arrow_top);
			cairo_dock_gl_path_rel_line_to (pScrollPath, fScrollbarWidth/2, -fArrowHeight);
			cairo_dock_gl_path_rel_line_to (pScrollPath, -fScrollbarWidth, 0.);
			
			glColor4f (my_diapo_simple_color_scrollbar_inside[0], my_diapo_simple_color_scrollbar_inside[1], my_diapo_simple_color_scrollbar_inside[2], my_diapo_simple_color_scrollbar_inside[3] * fAlpha);
			cairo_dock_fill_gl_path (pScrollPath, 0);
			
			glColor4f (my_diapo_simple_color_scrollbar_line[0], my_diapo_simple_color_scrollbar_line[1], my_diapo_simple_color_scrollbar_line[2], my_diapo_simple_color_scrollbar_line[3] * fAlpha);
			cairo_dock_stroke_gl_path (pScrollPath, TRUE);  // TRUE <=> close
		}
		if (pData->iScrollOffset != pData->iDeltaHeight)  // fleche vers le bas.
		{
			cairo_dock_gl_path_move_to (pScrollPath, x_arrow, y_arrow_bottom);
			cairo_dock_gl_path_rel_line_to (pScrollPath, fScrollbarWidth/2, fArrowHeight);
			cairo_dock_gl_path_rel_line_to (pScrollPath, -fScrollbarWidth, 0.);
			
			glColor4f (my_diapo_simple_color_scrollbar_inside[0], my_diapo_simple_color_scrollbar_inside[1], my_diapo_simple_color_scrollbar_inside[2], my_diapo_simple_color_scrollbar_inside[3] * fAlpha);
			cairo_dock_fill_gl_path (pScrollPath, 0);
			
			glColor4f (my_diapo_simple_color_scrollbar_line[0], my_diapo_simple_color_scrollbar_line[1], my_diapo_simple_color_scrollbar_line[2], my_diapo_simple_color_scrollbar_line[3] * fAlpha);
			cairo_dock_stroke_gl_path (pScrollPath, TRUE);  // TRUE <=> close
		}
		
		// scrollbar outline
		cairo_dock_gl_path_move_to (pScrollPath, x_arrow - fScrollbarWidth/2, y_arrow_bottom + fArrowHeight + fScrollbarArrowGap);
		cairo_dock_gl_path_rel_line_to (pScrollPath, fScrollbarWidth, 0.);
		cairo_dock_gl_path_rel_line_to (pScrollPath, 0., y_arrow_top - y_arrow_bottom - 2*(fArrowHeight+fScrollbarArrowGap));
		cairo_dock_gl_path_rel_line_to (pScrollPath, -fScrollbarWidth, 0.);
		
		glColor4f (my_diapo_simple_color_scrollbar_inside[0], my_diapo_simple_color_scrollbar_inside[1], my_diapo_simple_color_scrollbar_inside[2], my_diapo_simple_color_scrollbar_inside[3] * fAlpha);
		cairo_dock_fill_gl_path (pScrollPath, 0);
		
		glColor4f (my_diapo_simple_color_scrollbar_line[0], my_diapo_simple_color_scrollbar_line[1], my_diapo_simple_color_scrollbar_line[2], my_diapo_simple_color_scrollbar_line[3] * fAlpha);
		cairo_dock_stroke_gl_path (pScrollPath, TRUE);
		
		// grip
		double fGripHeight = fFrameHeight / (fFrameHeight + pData->iDeltaHeight) * (y_arrow_top - y_arrow_bottom - 2*(fArrowHeight+fScrollbarArrowGap));
		double ygrip = (double) pData->iScrollOffset / pData->iDeltaHeight * (y_arrow_top - y_arrow_bottom - 2*(fArrowHeight+fScrollbarArrowGap) - fGripHeight);
		glColor4f (my_diapo_simple_color_grip[0], my_diapo_simple_color_grip[1], my_diapo_simple_color_grip[2], my_diapo_simple_color_grip[3] * fAlpha);
		cairo_dock_gl_path_move_to (pScrollPath, x_arrow - fScrollbarWidth/2, y_arrow_top - (fArrowHeight+fScrollbarArrowGap) - ygrip);
		cairo_dock_gl_path_rel_line_to (pScrollPath, fScrollbarWidth, 0.);
		cairo_dock_gl_path_rel_line_to (pScrollPath, 0., - fGripHeight);
		cairo_dock_gl_path_rel_line_to (pScrollPath, -fScrollbarWidth, 0.);
		cairo_dock_fill_gl_path (pScrollPath, 0);
		
		glPopMatrix ();
	}
	
	if (pDock->icons == NULL)
		return ;
	
	//\____________________ On dessine la ficelle.
	if (myIcons.iStringLineWidth > 0)
		cairo_dock_draw_string_opengl (pDock, myIcons.iStringLineWidth, FALSE, FALSE);
	
	//\____________________ On dessine les icones.
	// on determine la 1ere icone a tracer : l'icone suivant l'icone pointee.
	GList *pFirstDrawnElement = cairo_dock_get_first_drawn_element_linear (pDock->icons);
	if (pFirstDrawnElement == NULL)
		return;
	
	// on dessine les icones, l'icone pointee en dernier.
	if (pData != NULL && pData->iDeltaHeight != 0) // on fait un clip pour les icones qui debordent.
	{
		int h = my_diapo_simple_arrowHeight + ARROW_TIP + my_diapo_simple_lineWidth;
		if (pDock->container.bIsHorizontal)
		{
			glEnable (GL_SCISSOR_TEST);
			glScissor (0,
				(pDock->container.bDirectionUp ? h : 0),  // lower left corner of the scissor box.
				pDock->container.iWidth,
				pDock->container.iHeight - h - my_diapo_simple_lineWidth);
		}  // sinon clip inutile.
	}
	
	Icon *icon;
	GList *ic = pFirstDrawnElement;
	do
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
		{
			ic = cairo_dock_get_next_element (ic, pDock->icons);
			continue;
		}
		
		cairo_dock_render_one_icon_opengl (icon, pDock, 1., FALSE);
		
		if (icon->iLabelTexture != 0 && (my_diapo_simple_display_all_labels || icon->bPointed))
		{
			glPushMatrix ();
			
			double fAlpha = (pDock->fFoldingFactor > .5 ? (1 - pDock->fFoldingFactor) / .5 : 1.);  // apparition du texte de 1 a 0.5
			
			double dx = .5 * (icon->iTextWidth & 1);  // on decale la texture pour la coller sur la grille des coordonnees entieres.
			double dy = .5 * (icon->iTextHeight & 1);
			double u0 = 0., u1 = 1.;
			double fOffsetX = 0.;
			if (pDock->container.bIsHorizontal)
			{
				if (icon->bPointed)
				{
					_cairo_dock_set_alpha (fAlpha);
					if (icon->fDrawX + icon->fWidth/2 + icon->iTextWidth/2 > pDock->container.iWidth)
						fOffsetX = pDock->container.iWidth - (icon->fDrawX + icon->fWidth/2 + icon->iTextWidth/2);
					if (icon->fDrawX + icon->fWidth/2 - icon->iTextWidth/2 < 0)
						fOffsetX = icon->iTextWidth/2 - (icon->fDrawX + icon->fWidth/2);
				}
				else
				{
					_cairo_dock_set_alpha (fAlpha * icon->fScale / my_diapo_simple_fScaleMax);
					if (icon->iTextWidth > icon->fWidth + 2 * myLabels.iLabelSize)
					{
						fOffsetX = 0.;
						u1 = (double) (icon->fWidth + 2 * myLabels.iLabelSize) / icon->iTextWidth;
					}
				}
				
				glTranslatef (ceil (icon->fDrawX + icon->fScale * icon->fWidth/2 + fOffsetX) + dx,
					ceil (pDock->container.iHeight - icon->fDrawY + icon->iTextHeight / 2) + dy,
					0.);
			}
			else
			{
				if (icon->bPointed)
				{
					_cairo_dock_set_alpha (fAlpha);
					if (icon->fDrawY + icon->fHeight/2 + icon->iTextWidth/2 > pDock->container.iHeight)
						fOffsetX = pDock->container.iHeight - (icon->fDrawY + icon->fHeight/2 + icon->iTextWidth/2);
					if (icon->fDrawY + icon->fHeight/2 - icon->iTextWidth/2 < 0)
						fOffsetX = icon->iTextWidth/2 - (icon->fDrawY + icon->fHeight/2);
				}
				else
				{
					_cairo_dock_set_alpha (fAlpha * icon->fScale / my_diapo_simple_fScaleMax);
					if (icon->iTextWidth > icon->fWidth + 2 * myLabels.iLabelSize)
					{
						fOffsetX = 0.;
						u1 = (double) (icon->fWidth + 2 * myLabels.iLabelSize) / icon->iTextWidth;
					}
				}
				
				glTranslatef (ceil (icon->fDrawY + icon->fScale * icon->fHeight/2 + fOffsetX / 2) + dx,
					ceil (pDock->container.iWidth - icon->fDrawX + icon->iTextHeight / 2) + dy,
					0.);
			}
			_cairo_dock_enable_texture ();
			_cairo_dock_set_blend_alpha ();
			glBindTexture (GL_TEXTURE_2D, icon->iLabelTexture);
			_cairo_dock_apply_current_texture_portion_at_size_with_offset (u0, 0.,
				u1 - u0, 1.,
				icon->iTextWidth * (u1 - u0), icon->iTextHeight,
				0., 0.);
			_cairo_dock_disable_texture ();
			_cairo_dock_set_alpha (1.);
			
			glPopMatrix ();
		}
		
		ic = cairo_dock_get_next_element (ic, pDock->icons);
	}
	while (ic != pFirstDrawnElement);
	glDisable (GL_SCISSOR_TEST);
}


void cd_rendering_free_slide_data (CairoDock *pDock)
{
	CDSlideData *pData = pDock->pRendererData;
	if (pData != NULL)
	{
		cairo_dock_remove_notification_func_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_SCROLL_ICON, (CairoDockNotificationFunc) _cd_slide_on_scroll, NULL);
		cairo_dock_remove_notification_func_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_CLICK_ICON, (CairoDockNotificationFunc) _cd_slide_on_click, NULL);
		cairo_dock_remove_notification_func_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_MOUSE_MOVED, (CairoDockNotificationFunc) _cd_slide_on_mouse_moved, NULL);
		g_signal_handler_disconnect (pDock->container.pWidget, pData->iSidPressEvent);
		g_signal_handler_disconnect (pDock->container.pWidget, pData->iSidReleaseEvent);
		
		g_free (pData);
		pDock->pRendererData = NULL;
	}
}

void cd_rendering_register_diapo_simple_renderer (const gchar *cRendererName)
{
//////////////////////////////////////////////////////////////////////////////////////// On definit le renderer :
	CairoDockRenderer *pRenderer = g_new0 (CairoDockRenderer, 1);                                           //Nouvelle structure	
	pRenderer->cReadmeFilePath = g_strdup_printf ("%s/readme-diapo-simple-view", MY_APPLET_SHARE_DATA_DIR);        //On affecte le readme
	pRenderer->cPreviewFilePath = g_strdup_printf ("%s/preview-diapo-simple.jpg", MY_APPLET_SHARE_DATA_DIR);       // la preview
	pRenderer->compute_size = cd_rendering_calculate_max_dock_size_diapo_simple;                        //La fonction qui défini les bornes     
	pRenderer->calculate_icons = cd_rendering_calculate_icons_diapo_simple;                                        //qui calcule les param des icones      
	pRenderer->render = cd_rendering_render_diapo_simple;                                                          //qui initie le calcul du rendu         
	pRenderer->render_optimized = NULL;//cd_rendering_render_diapo_simple_optimized;                                      //pareil en mieux                       
	pRenderer->set_subdock_position = cairo_dock_set_subdock_position_linear;                               // ?                                    
	pRenderer->render_opengl = cd_rendering_render_diapo_simple_opengl;
	
	pRenderer->free_data = cd_rendering_free_slide_data;
	
	
	pRenderer->bUseReflect = FALSE;                                                                         // On dit non au reflections
	pRenderer->cDisplayedName = D_ (cRendererName);
	
	cairo_dock_register_renderer (cRendererName, pRenderer);                                    //Puis on signale l'existence de notre rendu
}
