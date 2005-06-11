/* *
 * Copyright (C) 2005 Mekensleep
 *
 *	Mekensleep
 *	24 rue vieille du temple
 *	75004 Paris
 *       licensing@mekensleep.com
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 *  Loic Dachary <loic@gnu.org>
 */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include "gui.h"
#include "interface_io.h"
#include "dispatcher.h"

#define VARIANT_HOLDEM 0
#define VARIANT_OMAHA 1
#define VARIANT_OMAHA8 2
#define VARIANT_7STUD 3
#define VARIANTS_COUNT 4

static GladeXML* s_lobby_xml = 0;
static GtkWidget*	s_lobby_window = 0;
static GtkLabel*	s_players_label = 0;
static GtkLabel*	s_tables_label = 0;
static GtkLabel*	s_go_to_label = 0;
static GtkButton*	s_go_to_button = 0;
static GtkListStore* s_variants_store[VARIANTS_COUNT] = { 0, };
static GtkTreeSelection* s_variants_selection[VARIANTS_COUNT] = { 0, };
static char* s_variants_names[VARIANTS_COUNT] = {
  "holdem",
  "omaha",
  "omaha8",
  "7stud"
};
static GtkNotebook* s_notebook = 0;

static GtkWidget*	s_table_info_window = 0;
static GtkListStore* s_players_store = 0;
static int s_disable_buttons = 0;
static GtkWidget*	s_lobby_tabs_window = 0;

static GtkWidget*	s_cashier_button_window;

static GtkWidget*	s_clock_window;
static GtkWidget*	s_clock_label;

static void clear_stores(void) {
  int i;
  for(i = 0; i < VARIANTS_COUNT; i++) {
    gtk_list_store_clear(s_variants_store[i]);
  }
  gtk_list_store_clear(s_players_store);
}

static void	close_lobby(void)
{
  gtk_widget_hide(s_lobby_window);
  gtk_widget_hide(s_table_info_window);
  gtk_widget_hide(s_lobby_tabs_window);
  gtk_widget_hide(s_cashier_button_window);
  gtk_widget_hide(s_clock_window);
  clear_stores();
}

static void	on_go_to_clicked(GtkWidget *widget, gpointer user_data)
{
  (void) widget;
  int* selection = (int*)user_data;
  if(*selection > 0) {
      set_string("lobby");
      set_string("join");
      set_int(*selection);
      flush_io_channel();
  } else
    g_message("no row selected.\n");
}

static void	on_row_activated(GtkTreeView        *treeview,
                             GtkTreePath        *path,
                             GtkTreeViewColumn  *col,
                             gpointer            user_data)
{
  (void) col;
  (void) user_data;

  g_message("row clicked");
  GtkTreeModel*	model;
  GtkTreeIter   iter;

  model = gtk_tree_view_get_model(treeview);

  if (gtk_tree_model_get_iter(model, &iter, path))
    {
      int	id;

      gtk_tree_model_get(model, &iter, 0, &id, -1);
      g_message("Double-clicked row contains %d", id);
      set_string("lobby");
      set_string("join");
      set_int(id);
      flush_io_channel();
      close_lobby();
    }
  else
    g_warning("row_activated: unable to find active row");
}

static void	on_lobby_list_treeview_selection_changed(GtkTreeSelection *treeselection,
                                                          gpointer user_data)
{
  int* selection = (int*)user_data;

  GtkTreeModel*	model;
  GtkTreeIter   iter;

  if(gtk_tree_selection_get_selected(treeselection, &model, &iter)) {
      int	id;

      gtk_tree_model_get(model, &iter, 0, &id, -1);
      g_message("clicked row contains %d", id);
      set_string("lobby");
      set_string("details");
      set_int(id);
      flush_io_channel();

      *selection = id;
    }
  else
    g_warning("treeview_selection: unable to find active row");
}

static void	on_table_toggled(GtkWidget *widget, gpointer user_data)
{
  (void) user_data;

  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    const char* name = gtk_widget_get_name(widget);
    if(!strcmp(name, "holdem")) {
      gtk_notebook_set_current_page(s_notebook, VARIANT_HOLDEM);
    } else if(!strcmp(name, "omaha")) {
      gtk_notebook_set_current_page(s_notebook, VARIANT_OMAHA);
    } else if(!strcmp(name, "omaha8")) {
      gtk_notebook_set_current_page(s_notebook, VARIANT_OMAHA8);
    } else if(!strcmp(name, "7stud")) {
      gtk_notebook_set_current_page(s_notebook, VARIANT_7STUD);
    }
    set_string("lobby");
    set_string("refresh");
    set_string(name);
    flush_io_channel();
  }
}

static void	on_tourney_toggled(GtkWidget *widget, gpointer user_data)
{
  (void) user_data;

  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    set_string("lobby");
    set_string("quit");
    set_string(gtk_widget_get_name(widget));
    flush_io_channel();
  }
}

static void	on_all_radio_clicked(GtkWidget* widget, gpointer data)
{
  (void) data;

  if(!s_disable_buttons && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    clear_stores();
    set_string("lobby");
    set_string("refresh");
    set_string("all");
    flush_io_channel();
  }
}

static void	on_play_money_radio_clicked(GtkWidget* widget, gpointer data)
{
  (void) data;

  if(!s_disable_buttons && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    clear_stores();
    set_string("lobby");
    set_string("refresh");
    set_string("play");
    flush_io_channel();
  }
}

static void	on_real_money_radio_clicked(GtkWidget* widget, gpointer data)
{
  (void) data;

  if(!s_disable_buttons && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    clear_stores();
    set_string("lobby");
    set_string("refresh");
    set_string("real");
    flush_io_channel();
  }
}

static void on_cashier_button_pressed(GtkButton* button, gpointer data)
{
  (void) button;
  (void) data;

  set_string("lobby");
  set_string("quit");
  set_string("cashier");
  flush_io_channel();
}

int	handle_lobby(GladeXML* g_lobby_xml, GladeXML* g_table_info_xml, GladeXML* g_lobby_tabs_xml, GladeXML* g_cashier_button_xml, GladeXML* g_clock_xml, GtkLayout* screen, int init)
{
  static int s_selected_table = 0;

  if(init) {
    int i;
    s_lobby_xml = g_lobby_xml;
    s_lobby_window = gui_get_widget(g_lobby_xml, "lobby_window");
    g_assert(s_lobby_window);
    set_nil_draw_focus(s_lobby_window);
    if(screen) gtk_layout_put(screen, s_lobby_window, 0, 0);
    s_notebook = GTK_NOTEBOOK(gui_get_widget(g_lobby_xml, "notebook"));
    g_assert(s_notebook);
    for(i = 0; i < VARIANTS_COUNT; i++) {
      char tmp[32];
      s_variants_store[i] = gtk_list_store_new(11, G_TYPE_INT,
                                               G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                               G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                               G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                               G_TYPE_STRING
                                               );
      snprintf(tmp, 32, "%s_treeview", s_variants_names[i]);
      GtkTreeView* treeview = GTK_TREE_VIEW(gui_get_widget(g_lobby_xml, tmp));
      GtkTreeSelection*	selection = gtk_tree_view_get_selection(treeview);
      g_signal_connect(selection, "changed", (GCallback)on_lobby_list_treeview_selection_changed, &s_selected_table);
      s_variants_selection[i] = selection;
      g_signal_connect(treeview, "row-activated", (GCallback)on_row_activated, &s_selected_table);
      gtk_tree_view_set_rules_hint(treeview, TRUE);
      gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(s_variants_store[i]));
      GtkCellRenderer*	text_renderer = gtk_cell_renderer_text_new();

#define SET_COLUMN(TITLE, INDEX) \
      { \
        GtkTreeViewColumn* column = gtk_tree_view_column_new(); \
        gtk_tree_view_append_column(treeview, column); \
        gtk_tree_view_column_set_title(column, TITLE); \
        gtk_tree_view_column_pack_start(column, text_renderer, TRUE); \
        gtk_tree_view_column_add_attribute(column, text_renderer, "text", INDEX); \
      }
#define TABLE_COLUMN_ID 0
#define TABLE_COLUMN_NAME 1
      SET_COLUMN("Name", TABLE_COLUMN_NAME);
#define TABLE_COLUMN_STRUCTURE 2
      SET_COLUMN("Structure", TABLE_COLUMN_STRUCTURE);
#define TABLE_COLUMN_SEATS 3
      SET_COLUMN("Seats", TABLE_COLUMN_SEATS);
#define TABLE_COLUMN_AVG_POT 4
      SET_COLUMN("Avg.pot", TABLE_COLUMN_AVG_POT);
#define TABLE_COLUMN_HANDS_PER_HOUR 5
      SET_COLUMN("Hands/h", TABLE_COLUMN_HANDS_PER_HOUR);
#define TABLE_COLUMN_PERCENT_FLOP 6
      SET_COLUMN("Flop%", TABLE_COLUMN_PERCENT_FLOP);
#define TABLE_COLUMN_PLAYING 7
      SET_COLUMN("Playing", TABLE_COLUMN_PLAYING);
#define TABLE_COLUMN_OBSERVING 8
#define TABLE_COLUMN_WAITING 9
#define TABLE_COLUMN_TIMEOUT 10
#undef SET_COLUMN
    }
    s_players_label = GTK_LABEL(gui_get_widget(g_lobby_xml, "players_label"));
    s_tables_label = GTK_LABEL(gui_get_widget(g_lobby_xml, "tables_label"));
    GUI_BRANCH(g_lobby_xml, on_all_radio_clicked);
    GUI_BRANCH(g_lobby_xml, on_play_money_radio_clicked);
    GUI_BRANCH(g_lobby_xml, on_real_money_radio_clicked);

    s_table_info_window = gui_get_widget(g_table_info_xml, "table_info_window");
    g_assert(s_table_info_window);
    if(screen) gtk_layout_put(screen, s_table_info_window, 0, 0);
    {
      s_players_store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
      GtkTreeView* treeview = GTK_TREE_VIEW(gui_get_widget(g_table_info_xml, "players_treeview"));
      gtk_tree_view_set_rules_hint(treeview, TRUE);
      gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(s_players_store));
      GtkCellRenderer*	text_renderer = gtk_cell_renderer_text_new();

#define PLAYERS_COLUMN_NAME 0
      {
        GtkTreeViewColumn* column = gtk_tree_view_column_new();
        gtk_tree_view_append_column(treeview, column);
        gtk_tree_view_column_set_title(column, "Players");
        gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
        gtk_tree_view_column_add_attribute(column, text_renderer, "text", 0);
      }
#define PLAYERS_COLUMN_CHIPS 1
      {
        GtkTreeViewColumn* column = gtk_tree_view_column_new();
        gtk_tree_view_append_column(treeview, column);
        gtk_tree_view_column_set_title(column, "Chips");
        gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
        gtk_tree_view_column_add_attribute(column, text_renderer, "text", 1);
      }
#define PLAYERS_COLUMN_FLAG 2
    }

    s_go_to_label = GTK_LABEL(glade_xml_get_widget(g_table_info_xml, "label_go_to"));
    s_go_to_button = GTK_BUTTON(glade_xml_get_widget(g_table_info_xml, "button_go_to"));
    g_signal_connect(GTK_OBJECT(s_go_to_button), "clicked", (GtkSignalFunc)on_go_to_clicked, &s_selected_table);

    s_lobby_tabs_window = gui_get_widget(g_lobby_tabs_xml, "lobby_tabs_window");
    g_assert(s_lobby_tabs_window);
    gtk_widget_set_size_request(s_lobby_tabs_window, gui_width(screen), -1);
    if(screen) gtk_layout_put(screen, s_lobby_tabs_window, 0, 0);
    
    GUI_BRANCH(g_lobby_tabs_xml, on_table_toggled);
    GUI_BRANCH(g_lobby_tabs_xml, on_tourney_toggled);

    s_cashier_button_window = gui_get_widget(g_cashier_button_xml, "cashier_button_window");
    g_assert(s_cashier_button_window);
    if(screen) gtk_layout_put(screen, s_cashier_button_window, 0, 0);
    GUI_BRANCH(g_cashier_button_xml, on_cashier_button_pressed);

    s_clock_window = gui_get_widget(g_clock_xml, "clock_window");
    g_assert(s_clock_window);
    if(screen) gtk_layout_put(screen, s_clock_window, 0, 0);
    s_clock_label = gui_get_widget(g_clock_xml, "clock_label");

    close_lobby();
  }

  char* tag = get_string();
  if(!strcmp(tag, "show")) {
    /*
     * calculate windows position
     */
    int	screen_width = gui_width(screen);
    int	screen_height = gui_height(screen);

    int	top_left_x = (screen_width - 900) / 2;
    int	top_left_y = (screen_height - 500) / 2;

    {
      static position_t position;
      position.x = top_left_x + 350;
      position.y = top_left_y;
      gui_place(s_lobby_window, &position, screen);
    }

    {
      static position_t position;
      position.x = top_left_x;
      position.y = top_left_y;
      gui_place(s_table_info_window, &position, screen);
    }

    {
      static position_t position;
      position.x = 0;
      position.y = 33;
      gui_place(s_lobby_tabs_window, &position, screen);
    }

    {
      static position_t position;
      position.x = top_left_x;
      position.y = top_left_y + 400;
      gui_place(s_cashier_button_window, &position, screen);
    }

    {
      gui_bottom_right(s_clock_window, screen);
    }

    s_selected_table = 0;

    {
      char* type = get_string();
      if(!strcmp(type, "holdem")) {
        gtk_notebook_set_current_page(s_notebook, VARIANT_HOLDEM);
      } else if(!strcmp(type, "omaha")) {
        gtk_notebook_set_current_page(s_notebook, VARIANT_OMAHA);
      } else if(!strcmp(type, "omaha8")) {
        gtk_notebook_set_current_page(s_notebook, VARIANT_OMAHA8);
      } else if(!strcmp(type, "7stud")) {
        gtk_notebook_set_current_page(s_notebook, VARIANT_7STUD);
      }
      GtkToggleButton* button = GTK_TOGGLE_BUTTON(gui_get_widget(g_lobby_tabs_xml, type));
      g_assert(button);
      gtk_toggle_button_set_active(button, TRUE);
      g_free(type);
    }

    {
      char* real_money = get_string();
      char* button;
      GtkWidget* radio;
      if(!strcmp(real_money, "y")) {
        button = "real_money_radio";
      } else if(!strcmp(real_money, "y")) {
        button = "play_money_radio";
      } else {
        button = "all_radio";
      }

      s_disable_buttons = 1;
      radio = gui_get_widget(s_lobby_xml, button);
      g_assert(radio);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
      s_disable_buttons = 0;

      g_free(real_money);
    }
  } else if(!strcmp(tag, "hide")) {
    close_lobby();

  } else if(!strcmp(tag, "info")) {
    char* players_count = get_string();
    char* tables_count = get_string();
    gtk_label_set_text(s_players_label, players_count);
    gtk_label_set_text(s_tables_label, tables_count);
    g_free(players_count);
    g_free(tables_count);

  } else if(!strcmp(tag, "holdem") || !strcmp(tag, "omaha") ||
            !strcmp(tag, "omaha8") || !strcmp(tag, "7stud")) {
    int selected = get_int();
    int rows = get_int();
    int i;
    int variant_index = VARIANT_HOLDEM;
    
    if(!strcmp(tag, "holdem")) {
      variant_index = VARIANT_HOLDEM;
    } else if(!strcmp(tag, "omaha")) {
      variant_index = VARIANT_OMAHA;
    } else if(!strcmp(tag, "omaha8")) {
      variant_index = VARIANT_OMAHA8;
    } else if(!strcmp(tag, "7stud")) {
      variant_index = VARIANT_7STUD;
    }

    gtk_list_store_clear(s_variants_store[variant_index]);
    for(i = 0; i < rows; i++) {
      int id = get_int();
      GtkTreeIter	iter;

      gtk_list_store_append(s_variants_store[variant_index], &iter);
      gtk_list_store_set(s_variants_store[variant_index], &iter, TABLE_COLUMN_ID, id, -1);
      if(selected == id)
        gtk_tree_selection_select_iter(s_variants_selection[variant_index], &iter);

#define SET_COLUMN(INDEX) \
      { \
        char* content = get_string(); \
        gtk_list_store_set(s_variants_store[variant_index], &iter, INDEX, content, -1); \
        g_free(content); \
      }
      SET_COLUMN(TABLE_COLUMN_NAME);
      SET_COLUMN(TABLE_COLUMN_STRUCTURE);
      SET_COLUMN(TABLE_COLUMN_SEATS);
      SET_COLUMN(TABLE_COLUMN_AVG_POT);
      SET_COLUMN(TABLE_COLUMN_HANDS_PER_HOUR);
      SET_COLUMN(TABLE_COLUMN_PERCENT_FLOP);
      SET_COLUMN(TABLE_COLUMN_PLAYING);
      SET_COLUMN(TABLE_COLUMN_OBSERVING);
      SET_COLUMN(TABLE_COLUMN_WAITING);
      SET_COLUMN(TABLE_COLUMN_TIMEOUT);
#undef SET_COLUMN
    }
    s_selected_table = selected;
    if(!selected) {
      gtk_list_store_clear(s_players_store);
      gtk_widget_set_sensitive(GTK_WIDGET(s_go_to_button), FALSE);
    }
  } else if(!strcmp(tag, "players")) {
    int players_count = get_int();
    int i;
    gtk_label_set_text(s_go_to_label, "Go To");
    gtk_widget_set_sensitive(GTK_WIDGET(s_go_to_button), TRUE);
    gtk_list_store_clear(s_players_store);
    for(i = 0; i < players_count; i++) {
      char* name = get_string();
      char* chips = get_string();
      int flag = get_int();
      GtkTreeIter	iter;
      gtk_list_store_append(s_players_store, &iter);
      gtk_list_store_set(s_players_store, &iter, PLAYERS_COLUMN_NAME, name, PLAYERS_COLUMN_CHIPS, chips, PLAYERS_COLUMN_FLAG, flag, -1);
      g_free(name);
      g_free(chips);
    }
  }

  /*
   * set clock time
   */
  {
    time_t	_time;
    char	date_buffer[8];
    time(&_time);
    struct tm*	_tm = localtime(&_time);
    snprintf(date_buffer, sizeof (date_buffer), "%02d:%02d",
             _tm->tm_hour, _tm->tm_min);
    gtk_label_set_text(GTK_LABEL(s_clock_label), date_buffer);
  }

  g_free(tag);
  
  return TRUE;
}
