/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */
/*
** Login : <ctaf42@gmail.com>
** Started on  Fri Nov 30 05:31:31 2007 GESTES Cedric
** $Id$
**
** Author(s):
**  - Cedric GESTES <ctaf42@gmail.com>
**
** Copyright (C) 2007 GESTES Cedric
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkkeysyms-compat.h>
// gdk.h semble necessaire pour certains
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <vte/vte.h>

#include "terminal-callbacks.h"
#include "terminal-struct.h"
#include "terminal-menu-functions.h"
#include "terminal-widget.h"

void cd_terminal_grab_focus (void)
{
	int iCurrentNumPage = gtk_notebook_get_current_page (GTK_NOTEBOOK(myData.tab));
	GtkWidget *vterm = gtk_notebook_get_nth_page(GTK_NOTEBOOK(myData.tab), iCurrentNumPage);
	gtk_widget_grab_focus (vterm);
}

CairoDialog *cd_terminal_build_dialog (void)
{
	CairoDialogAttribute attr;
	memset (&attr, 0, sizeof (CairoDialogAttribute));
	attr.cText = D_ ("Terminal");
	attr.pInteractiveWidget = myData.tab;
	attr.bHideOnClick = TRUE;  // keep the dialog alive on click (hide it).
	return cairo_dock_build_dialog (&attr, myIcon, myContainer);
}

void term_on_keybinding_pull(const char *keystring, gpointer user_data)
{
	if (myData.tab)
	{
		if (myDesklet)
		{
			gboolean bHasFocus = (gtk_window_has_toplevel_focus (GTK_WINDOW (myDesklet->container.pWidget)) || gtk_widget_has_focus (myData.tab) || gtk_widget_has_focus (myDesklet->container.pWidget));
			if (! bHasFocus)
			{
				GtkWidget *vterm;
				int i, iNbPages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (myData.tab));
				for (i = 0; i < iNbPages && ! bHasFocus; ++i)
				{
					vterm = gtk_notebook_get_nth_page(GTK_NOTEBOOK(myData.tab), i);
					bHasFocus = gtk_widget_has_focus (vterm);
				}
				bHasFocus |= (gldi_container_get_Xid (myContainer) == cairo_dock_get_current_active_window ());
			}
			cd_debug ("%s (%d)\n", __func__, bHasFocus);
			
			if (bHasFocus)
			{
				cairo_dock_hide_desklet(myDesklet);
			}
			else
			{
				cairo_dock_show_desklet (myDesklet);
				
			}
		}
		else if (myData.dialog)
		{
			if (gldi_container_is_visible (CAIRO_CONTAINER (myData.dialog)))
			{
				cairo_dock_hide_dialog (myData.dialog);
			}
			else
			{
				cairo_dock_unhide_dialog (myData.dialog);
				cd_terminal_grab_focus ();
			}
		}
	}
	else
	{
		terminal_build_and_show_tab ();
	}
}


static gchar *_get_label_and_color (const gchar *cLabel, GdkColor *pColor, gboolean *bColorSet)
{
	gchar *cUsefulLabel;
	gchar *str = strchr (cLabel, '>');
	//g_print ("%s (%s)\n", __func__, cLabel);
	if (cLabel != NULL && strncmp (cLabel, "<span color='", 13) == 0 && str != NULL)  // approximatif mais devrait suffire.
	{
		const gchar *col = cLabel+13;
		gchar *col_end = strchr (col+1, '\'');
		if (col_end)
		{
			gchar *cColor = g_strndup (col, col_end - col);
			//g_print ("cColor : %s\n", cColor);
			*bColorSet = gdk_color_parse (cColor, pColor);
			g_free (cColor);
		}
		cUsefulLabel = g_strdup (str+1);
		str = strrchr (cUsefulLabel, '<');
		if (str != NULL && strcmp (str, "</span>") == 0)
			*str = '\0';
	}
	else
	{
		cUsefulLabel = g_strdup (cLabel);
	}
	return cUsefulLabel;
}

void terminal_rename_tab (GtkWidget *vterm)
{
	cd_message ("");
	if (vterm == NULL)
	{
		int iCurrentNumPage = gtk_notebook_get_current_page (GTK_NOTEBOOK(myData.tab));
		vterm = gtk_notebook_get_nth_page (GTK_NOTEBOOK(myData.tab), iCurrentNumPage);
	}
	GtkWidget *pTabLabelWidget = gtk_notebook_get_tab_label (GTK_NOTEBOOK(myData.tab), vterm);
	GList *pTabWidgetList = gtk_container_get_children (GTK_CONTAINER (pTabLabelWidget));
	GtkLabel *pLabel;
	const gchar *cCurrentName;
	if (pTabWidgetList != NULL && pTabWidgetList->data != NULL)
	{
		GtkLabel *pLabel = pTabWidgetList->data;
		const gchar *cCurrentName = gtk_label_get_label (pLabel);
		GdkColor color;
		gboolean bColorSet = FALSE;
		gchar *cUsefulLabel = _get_label_and_color (cCurrentName, &color, &bColorSet);
		
		gchar *cNewName = cairo_dock_show_demand_and_wait (D_("Set title for this tab:"), NULL, (myDock ? CAIRO_CONTAINER (myData.dialog) : CAIRO_CONTAINER (myDesklet)), cUsefulLabel);
		g_free (cUsefulLabel);
		
		if (cNewName != NULL)
		{
			if (bColorSet)
			{
				gchar *cColor = gdk_color_to_string (&color);
				gchar *cNewColoredName = g_strdup_printf ("<span color='%s'>%s</span>", cColor, cNewName);
				gtk_label_set_markup (pLabel, cNewColoredName);
				g_free (cNewColoredName);
				g_free (cColor);
			}
			else
			{
				gtk_label_set_text (pLabel, cNewName);
			}
			g_free (cNewName);
		}
		g_list_free (pTabWidgetList);
	}
}

static void _set_color (GtkColorSelection *pColorSelection, GtkLabel *pLabel)
{
	GdkColor color;
	gtk_color_selection_get_current_color (pColorSelection, &color);
	
	gchar *cColor = gdk_color_to_string (&color);
	
	const gchar *cCurrentLabel = gtk_label_get_text (pLabel);  // recupere le texte sans les balises pango.
	//gchar *cUsefulLabel = _get_label_and_color (cCurrentLabel, NULL, NULL);
	gchar *cNewLabel = g_strdup_printf ("<span color='%s'>%s</span>", cColor, cCurrentLabel);
	gtk_label_set_markup (pLabel, cNewLabel);
	
	g_free (cNewLabel);
	//g_free (cUsefulLabel);
	g_free (cColor);
}
void terminal_change_color_tab (GtkWidget *vterm)
{
	cd_message ("");
	if (vterm == NULL)
	{
		int iCurrentNumPage = gtk_notebook_get_current_page (GTK_NOTEBOOK(myData.tab));
		vterm = gtk_notebook_get_nth_page (GTK_NOTEBOOK(myData.tab), iCurrentNumPage);
	}
	GtkWidget *pTabLabelWidget = gtk_notebook_get_tab_label (GTK_NOTEBOOK(myData.tab), vterm);
	GList *pTabWidgetList = gtk_container_get_children (GTK_CONTAINER (pTabLabelWidget));
	GtkLabel *pLabel;
	const gchar *cCurrentName = NULL;
	if (pTabWidgetList != NULL && pTabWidgetList->data != NULL)
	{
		GtkLabel *pLabel = pTabWidgetList->data;
		
		GtkWidget *pColorDialog = gtk_color_selection_dialog_new (D_("Select a color"));
		GtkWidget *colorsel = gtk_color_selection_dialog_get_color_selection ((GtkColorSelectionDialog *) pColorDialog);
		
		const gchar *cCurrentLabel = gtk_label_get_text (pLabel);
		GdkColor color;
		gboolean bColorSet = FALSE;
		gchar *cUsefulLabel = _get_label_and_color (cCurrentLabel, &color, &bColorSet);
		if (bColorSet)
		{
			
			gtk_color_selection_set_current_color (GTK_COLOR_SELECTION (colorsel), &color);
		}
		
		gtk_color_selection_set_has_opacity_control (GTK_COLOR_SELECTION (colorsel), FALSE);
		g_signal_connect (colorsel, "color-changed", G_CALLBACK (_set_color), pLabel);
		#if (GTK_MAJOR_VERSION < 3)
		gtk_widget_hide (((GtkColorSelectionDialog *) pColorDialog)->cancel_button);
		gtk_widget_hide (((GtkColorSelectionDialog *) pColorDialog)->help_button);
		g_signal_connect_swapped (((GtkColorSelectionDialog *) pColorDialog)->ok_button, "clicked", G_CALLBACK (gtk_widget_destroy), pColorDialog);
		#else
		/// TODO: alternative for GTK3?...
		#endif
		
		gtk_window_present (GTK_WINDOW (pColorDialog));
	}
}

void terminal_close_tab (GtkWidget *vterm)
{
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(myData.tab)) > 1)
	{
		int iNumPage;
		if (vterm == NULL)
		{
			iNumPage = gtk_notebook_get_current_page (GTK_NOTEBOOK(myData.tab));
		}
		else
		{
			iNumPage = gtk_notebook_page_num (GTK_NOTEBOOK(myData.tab), vterm);
		}
		gtk_notebook_remove_page(GTK_NOTEBOOK(myData.tab), iNumPage);
	}
}


static void _term_apply_settings_on_vterm(GtkWidget *vterm)
{
  g_return_if_fail (vterm != NULL);
  vte_terminal_set_colors(VTE_TERMINAL(vterm), &myConfig.forecolor, &myConfig.backcolor, NULL, 0);
      //vte_terminal_set_background_saturation(VTE_TERMINAL(vterm), 1.0);
      //vte_terminal_set_background_transparent(VTE_TERMINAL(vterm), TRUE);
#if GTK_MINOR_VERSION >= 12
  vte_terminal_set_opacity(VTE_TERMINAL(vterm), myConfig.transparency);
#endif
  
	if (myDock)
	{
		//g_print ("set_size (%d , %d)\n", myConfig.iNbColumns, myConfig.iNbRows);
		g_object_set (vterm, "width-request", 0, NULL);
		g_object_set (vterm, "height-request", 0, NULL);
		vte_terminal_set_size(VTE_TERMINAL(vterm), myConfig.iNbColumns, myConfig.iNbRows);
		GtkRequisition requisition = {0, 0};
		#if (GTK_MAJOR_VERSION < 3)
		gtk_widget_size_request (vterm, &requisition);
		#else
		gtk_widget_get_preferred_size (vterm, &requisition, NULL);
		#endif
		//g_print (" -> %dx%d\n", requisition.width, requisition.height);
		if (myData.dialog)
			gtk_window_resize (GTK_WINDOW (myData.dialog->container.pWidget), requisition.width, requisition.height);
	}
	else
	{
		g_object_set (vterm, "width-request", 64, NULL);
		g_object_set (vterm, "height-request", 64, NULL);
	}
}


void term_apply_settings (void)
{
	int sz = 0;
	GtkWidget *vterm = NULL;

	if (myData.tab) {
		sz = gtk_notebook_get_n_pages(GTK_NOTEBOOK(myData.tab));
		for (int i = 0; i < sz; ++i) {
			vterm = gtk_notebook_get_nth_page(GTK_NOTEBOOK(myData.tab), i);
			_term_apply_settings_on_vterm(vterm);
		}
	}
}

static void on_terminal_child_exited(VteTerminal *vterm,
                                     gpointer t)
{
	gint p = gtk_notebook_page_num(GTK_NOTEBOOK(myData.tab), GTK_WIDGET(vterm));
	gint sz = gtk_notebook_get_n_pages(GTK_NOTEBOOK(myData.tab));

	if (sz > 1)
		gtk_notebook_remove_page(GTK_NOTEBOOK(myData.tab), p);
	else {
		// \r needed to return to the beginning of the line
		vte_terminal_feed(VTE_TERMINAL(vterm), "Shell exited. Another one is launching...\r\n\n", -1);
		
		pid_t pid; 
		#if (GLIB_MAJOR_VERSION > 2) || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 18)  // VTE_CHECK_VERSION doesn't exist in Hardy.
			#if VTE_CHECK_VERSION(0,26,0)
			const gchar *argv[] = {g_getenv ("SHELL"), NULL};
			vte_terminal_fork_command_full (VTE_TERMINAL(vterm),
				VTE_PTY_NO_LASTLOG | VTE_PTY_NO_UTMP | VTE_PTY_NO_WTMP,
				"~/",
				(gchar**)argv,  // argv
				NULL,  // envv
				0,  // GSpawnFlags spawn_flags
				NULL,  // GSpawnChildSetupFunc child_setup
				NULL,  // gpointer child_setup_data
				&pid,
				NULL);
			#else
			pid = vte_terminal_fork_command (VTE_TERMINAL(vterm),
				NULL,
				NULL,
				NULL,
				"~/",
				FALSE,
				FALSE,
				FALSE);
			#endif
		#else
			pid = vte_terminal_fork_command (VTE_TERMINAL(vterm),
				NULL,
				NULL,
				NULL,
				"~/",
				FALSE,
				FALSE,
				FALSE);
		#endif
		if (myData.dialog)
			cairo_dock_hide_dialog (myData.dialog);
		else if (myDesklet && myConfig.shortcut)
		{
			cairo_dock_hide_desklet (myDesklet);
			Icon *icon = cairo_dock_get_dialogless_icon ();
			g_return_if_fail (icon != NULL);
			cairo_dock_show_temporary_dialog_with_icon_printf (D_("You can recall the Terminal desklet by typing %s"), icon, CAIRO_CONTAINER (g_pMainDock), 3500, MY_APPLET_SHARE_DATA_DIR"/"MY_APPLET_ICON_FILE, myConfig.shortcut);
		}
	}
}


static void _terminal_copy (GtkMenuItem *menu_item, GtkWidget *data)
{
  vte_terminal_copy_clipboard(VTE_TERMINAL(data));
}
static void _terminal_paste (GtkMenuItem *menu_item, GtkWidget *data)
{
  vte_terminal_paste_clipboard(VTE_TERMINAL(data));
}



static void on_new_tab (GtkMenuItem *menu_item, gpointer *data)
{
	terminal_new_tab();
}
static void on_rename_tab (GtkMenuItem *menu_item, GtkWidget *vterm)
{
	terminal_rename_tab (vterm);
}
static void on_change_tab_color (GtkMenuItem *menu_item, GtkWidget *vterm)
{
	terminal_change_color_tab (vterm);
}
static void on_close_tab (GtkMenuItem *menu_item, GtkWidget *vterm)
{
	terminal_close_tab (vterm);
}
static GtkWidget *_terminal_build_menu_tab (GtkWidget *vterm)
{
	GtkWidget *menu = gtk_menu_new ();
	
	GtkWidget *menu_item, *image;
	if (vterm)
	{
		menu_item = gtk_image_menu_item_new_with_label (D_("Copy"));
		image = gtk_image_new_from_stock (GTK_STOCK_COPY, GTK_ICON_SIZE_MENU);
#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16)
		gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menu_item), TRUE);
#endif
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
		gtk_menu_shell_append  (GTK_MENU_SHELL (menu), menu_item);
		g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK(_terminal_copy), vterm);
		
		menu_item = gtk_image_menu_item_new_with_label (D_("Paste"));
		image = gtk_image_new_from_stock (GTK_STOCK_PASTE, GTK_ICON_SIZE_MENU);
#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16)
		gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menu_item), TRUE);
#endif
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
		gtk_menu_shell_append  (GTK_MENU_SHELL (menu), menu_item);
		g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK(_terminal_paste), vterm);
		
		menu_item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append  (GTK_MENU_SHELL (menu), menu_item);
	}

  menu_item = gtk_image_menu_item_new_with_label (D_("New Tab"));
  image = gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU);
#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16)
  gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menu_item), TRUE);
#endif
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
  gtk_menu_shell_append  (GTK_MENU_SHELL (menu), menu_item);
  g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK(on_new_tab), vterm);
  
  menu_item = gtk_image_menu_item_new_with_label (D_("Rename this Tab"));
  image = gtk_image_new_from_stock (GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU);
#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16)
  gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menu_item), TRUE);
#endif
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
  gtk_menu_shell_append  (GTK_MENU_SHELL (menu), menu_item);
  g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK(on_rename_tab), vterm);

  menu_item = gtk_image_menu_item_new_with_label (D_("Change this Tab's colour"));
  image = gtk_image_new_from_stock (GTK_STOCK_COLOR_PICKER, GTK_ICON_SIZE_MENU);
#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16)
  gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menu_item), TRUE);
#endif
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
  gtk_menu_shell_append  (GTK_MENU_SHELL (menu), menu_item);
  g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK(on_change_tab_color), vterm);

  menu_item = gtk_image_menu_item_new_with_label (D_("Close this Tab"));
  image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16)
  gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menu_item), TRUE);
#endif
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
  gtk_menu_shell_append  (GTK_MENU_SHELL (menu), menu_item);
  g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK(on_close_tab), vterm);

  return menu;
}

static gboolean applet_on_terminal_press_cb(GtkWidget *vterm, GdkEventButton *event, gpointer user_data)
{
	cd_debug ("%s ()\n", __func__);
	if (event->button == 3)
	{
		GtkWidget *menu = _terminal_build_menu_tab (vterm);

		gtk_widget_show_all (menu);

		gtk_menu_popup (GTK_MENU (menu),
			NULL,
			NULL,
			NULL,
			NULL,
			1,
			gtk_get_current_event_time ());
	}
	return FALSE;
}
static void applet_on_terminal_eof(VteTerminal *vterm,
                                   gpointer     user_data)
{
	cd_debug ("youkata EOF");
}
static void _terminal_switch_tab(int iDelta)
{
	int iNbPages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(myData.tab));
	int iCurrentNumPage = gtk_notebook_get_current_page (GTK_NOTEBOOK(myData.tab));
	iCurrentNumPage += iDelta;
	if (iCurrentNumPage < 0)
		iCurrentNumPage = iNbPages - 1;
	else if (iCurrentNumPage >= iNbPages)
		iCurrentNumPage = 0;
	gtk_notebook_set_current_page (GTK_NOTEBOOK (myData.tab), iCurrentNumPage);
}
static void _terminal_move_tab(int iDelta)
{
	int iNbPages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(myData.tab));
	int iCurrentNumPage = gtk_notebook_get_current_page (GTK_NOTEBOOK(myData.tab));
	GtkWidget *vterm = gtk_notebook_get_nth_page(GTK_NOTEBOOK(myData.tab), iCurrentNumPage);
	iCurrentNumPage += iDelta;
	if (iCurrentNumPage < 0)
		iCurrentNumPage = iNbPages - 1;
	else if (iCurrentNumPage >= iNbPages)
		iCurrentNumPage = 0;
	gtk_notebook_reorder_child (GTK_NOTEBOOK(myData.tab),
		vterm,
		iCurrentNumPage);
}
static gboolean on_key_press_term (GtkWidget *pWidget,
	GdkEventKey *pKey,
	gpointer data)
{
	gboolean bIntercept = FALSE;
	if (pKey->type == GDK_KEY_PRESS && (pKey->state & GDK_CONTROL_MASK))
	{
		switch (pKey->keyval)
		{
			case GDK_t :
				terminal_new_tab();
				bIntercept = TRUE;
			break ;
			case GDK_w :
				terminal_close_tab (NULL);
				bIntercept = TRUE;
			break ;
			case GDK_Page_Down :
				if (pKey->state & GDK_SHIFT_MASK)
					_terminal_move_tab (+1);
				else
					_terminal_switch_tab (+1);
				bIntercept = TRUE;
			break ;
			case GDK_Page_Up :
				if (pKey->state & GDK_SHIFT_MASK)
					_terminal_move_tab (-1);
				else
					_terminal_switch_tab (-1);
				bIntercept = TRUE;
			break ;
			default :
			break ;
		}
	}
	return bIntercept;
}
static gchar * _terminal_get_tab_name (int iNumPage)
{
	GtkWidget *vterm = gtk_notebook_get_nth_page (GTK_NOTEBOOK(myData.tab), iNumPage);
	GtkWidget *pTabLabelWidget = gtk_notebook_get_tab_label (GTK_NOTEBOOK(myData.tab), vterm);
	GList *pTabWidgetList = gtk_container_get_children (GTK_CONTAINER (pTabLabelWidget));
	GtkLabel *pLabel;
	const gchar *cCurrentName;
	if (pTabWidgetList != NULL && pTabWidgetList->data != NULL)
	{
		GtkLabel *pLabel = pTabWidgetList->data;
		const gchar *cCurrentName = gtk_label_get_text (pLabel);
		return _get_label_and_color (cCurrentName, NULL, NULL);
	}
	return NULL;
}
void terminal_new_tab(void)
{
	//\_________________ On cree un nouveau terminal.
	GtkWidget *vterm = vte_terminal_new();
	//transparency enable, otherwise we cant change value after
	//vte_terminal_set_background_transparent(VTE_TERMINAL(vterm), TRUE);
	#if GTK_MINOR_VERSION >= 12
	vte_terminal_set_opacity(VTE_TERMINAL(vterm), myConfig.transparency);
	#endif
	vte_terminal_set_emulation(VTE_TERMINAL(vterm), "xterm");
	pid_t pid; 
	#if (GLIB_MAJOR_VERSION > 2) || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 18)  // VTE_CHECK_VERSION doesn't exist in Hardy.
		#if VTE_CHECK_VERSION(0,26,0)
		const gchar *argv[] = {g_getenv ("SHELL"), NULL};
		vte_terminal_fork_command_full (VTE_TERMINAL(vterm),
			VTE_PTY_NO_LASTLOG | VTE_PTY_NO_UTMP | VTE_PTY_NO_WTMP,
			"~/",
			(gchar**)argv,  // argv
			NULL,  // envv
			0,  // GSpawnFlags spawn_flags
			NULL,  // GSpawnChildSetupFunc child_setup
			NULL,  // gpointer child_setup_data
			&pid,
			NULL);
		#else
		pid = vte_terminal_fork_command (VTE_TERMINAL(vterm),
			NULL,
			NULL,
			NULL,
			"~/",
			FALSE,
			FALSE,
			FALSE);
		#endif
	#else
		pid = vte_terminal_fork_command (VTE_TERMINAL(vterm),
			NULL,
			NULL,
			NULL,
			"~/",
			FALSE,
			FALSE,
			FALSE);
	#endif
	g_signal_connect (G_OBJECT (vterm), "child-exited",
				G_CALLBACK (on_terminal_child_exited), NULL);
	g_signal_connect (G_OBJECT (vterm), "button-release-event",
				G_CALLBACK (applet_on_terminal_press_cb), NULL);
	g_signal_connect (G_OBJECT (vterm),
			"key-press-event",
			G_CALLBACK (on_key_press_term),
			NULL);
	g_signal_connect (G_OBJECT (vterm), "eof",
				G_CALLBACK (applet_on_terminal_eof), NULL);
	
	cairo_dock_allow_widget_to_receive_data (vterm, G_CALLBACK (on_terminal_drag_data_received), NULL);

	#if (GTK_MAJOR_VERSION < 3)
	GtkWidget *pHBox = gtk_hbox_new (FALSE, 0);
	#else
	GtkWidget *pHBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	#endif

	//\_________________ On choisit un nom qui ne soit pas deja present, de la forme " # n ".
	int i, iNbPages = gtk_notebook_get_n_pages (GTK_NOTEBOOK(myData.tab));
	GList *pTabNameList = NULL;
	for (i = 0; i < iNbPages; i ++)
	{
		pTabNameList = g_list_prepend (pTabNameList, _terminal_get_tab_name (i));
	}
	int iChoosedNum = 1;
	gchar *cLabel = g_strdup_printf (" # %d ", iChoosedNum);
	gchar *cTabName;
	GList *pElement = pTabNameList;
	do
	{
		if (pElement == NULL)
			break ;
		cTabName = pElement->data;
		if (cTabName != NULL && strcmp (cTabName, cLabel) == 0)
		{
			g_free (cLabel);
			iChoosedNum ++;
			cLabel = g_strdup_printf (" # %d ", iChoosedNum); // on passe au nom suivant.
			
			g_free (cTabName);
			pElement->data = NULL;
			pElement = pTabNameList;  // on recommence a parcourir la liste avec notre nouveau nom.
		}
		else
			pElement = pElement->next;
	} while (TRUE);
	g_list_foreach (pTabNameList, (GFunc) g_free, NULL);
	g_list_free (pTabNameList);
	
	//\_________________ On cree un onglet avec le label et un bouton 'fermer' et on y place le terminal.
	GtkWidget *pLabel = gtk_label_new (cLabel);
	g_free (cLabel);
	gtk_label_set_use_markup (GTK_LABEL (pLabel), TRUE);
	gtk_box_pack_start (GTK_BOX (pHBox),
		pLabel,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pButton = gtk_button_new_with_label ("x");
	g_signal_connect (G_OBJECT (pButton),
		"clicked",
		G_CALLBACK (on_close_tab),
		NULL);
	gtk_box_pack_start (GTK_BOX (pHBox),
		pButton,
		FALSE,
		FALSE,
		0);
	
	gtk_widget_show_all (pHBox);
	int num_new_tab = gtk_notebook_append_page(GTK_NOTEBOOK(myData.tab), vterm, pHBox);
	GtkWidget *pNewTab = gtk_notebook_get_nth_page (GTK_NOTEBOOK(myData.tab), num_new_tab);
	
	gtk_widget_show(vterm);
	cd_message ("num_new_tab : %d", num_new_tab);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (myData.tab), num_new_tab);
	
	_term_apply_settings_on_vterm (vterm);
}


static void _hide_show_tab_button (GtkNotebook *pNotebook, int iNumPage, gboolean bShow)
{
	GtkWidget *pPageChild = gtk_notebook_get_nth_page (pNotebook, iNumPage);
	GtkWidget *pTabLabelWidget = gtk_notebook_get_tab_label (pNotebook, pPageChild);
	GList *pTabWidgetList = gtk_container_get_children (GTK_CONTAINER (pTabLabelWidget));
	if (pTabWidgetList != NULL && pTabWidgetList->next != NULL)
	{
		GtkWidget *pButton = pTabWidgetList->next->data;
		if (pButton != NULL)
		{
			if (bShow)
				gtk_widget_show (pButton);
			else
				gtk_widget_hide (pButton);
		}
	}
	g_list_free (pTabWidgetList);
}
static void on_switch_page (GtkNotebook *pNotebook,
	GtkWidget *pNextPage,
	guint iNextNumPage,
	gpointer user_data)
{
	int iCurrentNumPage = gtk_notebook_get_current_page (pNotebook);
	
	_hide_show_tab_button (pNotebook, iCurrentNumPage, FALSE);
	_hide_show_tab_button (pNotebook, iNextNumPage, TRUE);
}
static GtkWidget * _terminal_find_clicked_tab_child (int x, int y)  // x,y relativement au notebook.
{
	GtkRequisition requisition;
	GtkWidget *pPageChild, *pTabLabelWidget, *vterm = NULL;
	
	int iMaxTabHeight = 0;
	int iCurrentNumPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (myData.tab));
	pPageChild = gtk_notebook_get_nth_page(GTK_NOTEBOOK(myData.tab), iCurrentNumPage);
	pTabLabelWidget = gtk_notebook_get_tab_label (GTK_NOTEBOOK(myData.tab), pPageChild);
	#if (GTK_MAJOR_VERSION < 3)
	gtk_widget_get_child_requisition (pTabLabelWidget, &requisition);
	#else
	gtk_widget_get_preferred_size (pTabLabelWidget, &requisition, NULL);
	#endif
	iMaxTabHeight = requisition.height;
	//g_print ("iMaxTabHeight : %d\n", iMaxTabHeight);
	
	int i, iNbPages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (myData.tab));
	gint src_x, src_y, dest_x, dest_y;
	for (i = 0; i < iNbPages; ++i)
	{
		pPageChild = gtk_notebook_get_nth_page(GTK_NOTEBOOK(myData.tab), i);
		pTabLabelWidget = gtk_notebook_get_tab_label (GTK_NOTEBOOK(myData.tab), pPageChild);
		#if (GTK_MAJOR_VERSION < 3)
		gtk_widget_get_child_requisition (pTabLabelWidget, &requisition);
		#else
		gtk_widget_get_preferred_size (pTabLabelWidget, &requisition, NULL);
		#endif
		gtk_widget_translate_coordinates (myData.tab,
			pTabLabelWidget,
			x, y,
			&dest_x,
			&dest_y);
		//g_print ("%d) (%d;%d) (%dx%d)\n", i, dest_x, dest_y, requisition.width, requisition.height);
		if (dest_x >= 0 && dest_y >= 0 && dest_x <= requisition.width && dest_y <= iMaxTabHeight)
		{
			vterm = pPageChild;
			break;
		}
	}
	return vterm;
}
static gboolean on_button_press_tab (GtkWidget* pWidget,
	GdkEventButton* pButton,
	gpointer data)
{
	cd_debug ("%s (%d;%d)\n", __func__, (int)pButton->x, (int)pButton->y);
	GtkWidget *vterm = _terminal_find_clicked_tab_child (pButton->x, pButton->y);
	if (pButton->type == GDK_2BUTTON_PRESS)
	{
		if (vterm == NULL)
			terminal_new_tab ();
		else
			terminal_rename_tab (vterm);
	}
	else if (pButton->button == 3)
	{
		if (vterm != NULL)
		{
			GtkWidget *menu = _terminal_build_menu_tab (vterm);
			
			gtk_widget_show_all (menu);
			
			gtk_menu_popup (GTK_MENU (menu),
				NULL,
				NULL,
				NULL,
				NULL,
				1,
				gtk_get_current_event_time ());
			return TRUE;  // on empeche le menu de cairo-dock d'apparaitre par-dessus dans le cas d'un desklet.
		}
	}
	else if (pButton->button == 2)
	{
		if (vterm != NULL)
		{
			terminal_close_tab (vterm);
		}
	}
	else
		return FALSE;
	return TRUE;
}


/**static gboolean on_button_press_dialog (GtkWidget *widget,
	GdkEventButton *pButton,
	CairoDialog *pDialog)
{
	cd_message ("hide the dialog when clicking on it");
	cairo_dock_hide_dialog (pDialog);
	return TRUE;
}*/
void terminal_build_and_show_tab (void)
{
	//\_________________ On cree un notebook.
	myData.tab = gtk_notebook_new();
	g_signal_connect (G_OBJECT (myData.tab),
		"switch-page",
		G_CALLBACK (on_switch_page),
		NULL);
	g_signal_connect (G_OBJECT (myData.tab),
		"button-press-event",
		G_CALLBACK (on_button_press_tab),
		NULL);
	g_signal_connect (G_OBJECT (myData.tab),
		"key-press-event",
		G_CALLBACK (on_key_press_term),
		NULL);
	
	//\_________________ On ajoute un onglet avec un terminal.
	terminal_new_tab();
	gtk_widget_show(myData.tab);

	term_apply_settings();

	//\_________________ On insere le notebook dans le container.
	if (myDock)
	{
		myData.dialog = cd_terminal_build_dialog ();
		/**g_signal_connect (G_OBJECT (myData.dialog->container.pWidget),
			"button-press-event",
			G_CALLBACK (on_button_press_dialog),
			myData.dialog);*/  // on le cache quand on clique dessus.
		cd_terminal_grab_focus ();
	}
	else
	{
		cairo_dock_add_interactive_widget_to_desklet (myData.tab, myDesklet);
		CD_APPLET_SET_DESKLET_RENDERER (NULL);  // pour empecher le clignotement du au double-buffer.
	}
}
