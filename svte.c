#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <glib-object.h>
#include <getopt.h>

#undef VTE_DISABLE_DEPRECATED
#include <vte/vte.h>

#include <glib/gi18n.h>

GdkScreen *screen;
#if GTK_CHECK_VERSION (2, 90, 8)
GdkVisual *visual;
#else
GdkColormap *colormap;
#endif
GtkWidget *window, *widget,
	  *hbox = NULL;
VteTerminal *terminal;

char *env_add[] = {NULL};
char *geometry = NULL;
char **command = NULL;
const char *message = "Launching interactive shell...\r\n";
const char *working_directory = NULL;
const char *output_file = NULL;
char *pty_flags_string = NULL;
static GdkColor bg_gcolor,
		fg_gcolor,
		bg_tint,
		*gcolor_palette;
int32_t z = 0;

#include "config.h"

int color_new(GdkColor *c, const char color_str[])
{
	if(! gdk_color_parse(color_str, c))
		perror("gdk_color_parse");
#if ! GTK_CHECK_VERSION (2, 90, 8)
	gdk_colormap_alloc_color(colormap, &c, False, True);
#endif
	return 0;
}

/* Take an array of hex colors and return a GdkColor palette */
int color_palette_new(const char **colors)
{
	uint32_t i;
	for(i = 0; colors[i]; i++);
	gcolor_palette = calloc(i, sizeof(GdkColor));
	for(i = 0; colors[i]; i++)
	{
		if(color_new(&gcolor_palette[i], colors[i]))
			perror("gdk_color_parse");
	}
	return i;
}

/*
 * Event Callbacks
 */
static void
window_title_changed(GtkWidget *widget, gpointer win)
{
	GtkWindow *window;

	g_assert(VTE_TERMINAL(widget));
	g_assert(GTK_IS_WINDOW(win));
	window = GTK_WINDOW(win);

	gtk_window_set_title(window, vte_terminal_get_window_title(VTE_TERMINAL(widget)));
}

static void
destroy_and_quit(VteTerminal *terminal, GtkWidget *window)
{
	const char *output_file = g_object_get_data (G_OBJECT (terminal), "output_file");

	if (output_file) {
		GFile *file;
		GOutputStream *stream;
		GError *error = NULL;

		file = g_file_new_for_commandline_arg (output_file);
		stream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));

		if (stream) {
			vte_terminal_write_contents (terminal, stream,
						     VTE_TERMINAL_WRITE_DEFAULT,
						     NULL, &error);
			g_object_unref (stream);
		}

		if (error) {
			g_printerr ("%s\n", error->message);
			g_error_free (error);
		}

		g_object_unref (file);
	}

	gtk_widget_destroy (window);
	gtk_main_quit ();
}

static void
delete_event(GtkWidget *window, GdkEvent *event, gpointer terminal)
{
	destroy_and_quit(VTE_TERMINAL (terminal), window);
}

static void
child_exited(GtkWidget *terminal, gpointer window)
{
	destroy_and_quit(VTE_TERMINAL (terminal), GTK_WIDGET (window));
}

static void
iconify_window(GtkWidget *widget, gpointer data)
{
	gtk_window_iconify(data);
}

static void
deiconify_window(GtkWidget *widget, gpointer data)
{
	gtk_window_deiconify(data);
}

static void
raise_window(GtkWidget *widget, gpointer data)
{
	GdkWindow *window;

	if (GTK_IS_WIDGET(data)) {
		window = gtk_widget_get_window(GTK_WIDGET(data));
		if (window) {
			gdk_window_raise(window);
		}
	}
}

static void
lower_window(GtkWidget *widget, gpointer data)
{
	GdkWindow *window;

	if (GTK_IS_WIDGET(data)) {
		window = gtk_widget_get_window(GTK_WIDGET(data));
		if (window) {
			gdk_window_lower(window);
		}
	}
}

static void
maximize_window(GtkWidget *widget, gpointer data)
{
	GdkWindow *window;

	if (GTK_IS_WIDGET(data)) {
		window = gtk_widget_get_window(GTK_WIDGET(data));
		if (window) {
			gdk_window_maximize(window);
		}
	}
}

static void
restore_window(GtkWidget *widget, gpointer data)
{
	GdkWindow *window;

	if (GTK_IS_WIDGET(data)) {
		window = gtk_widget_get_window(GTK_WIDGET(data));
		if (window) {
			gdk_window_unmaximize(window);
		}
	}
}

static void
refresh_window(GtkWidget *widget, gpointer data)
{
	GdkWindow *window;
	GtkAllocation allocation;
	GdkRectangle rect;

	if (GTK_IS_WIDGET(data)) {
		window = gtk_widget_get_window(widget);
		if (window) {
			gtk_widget_get_allocation(widget, &allocation);
			rect.x = rect.y = 0;
			rect.width = allocation.width;
			rect.height = allocation.height;
			gdk_window_invalidate_rect(window, &rect, TRUE);
		}
	}
}

static void
resize_window(GtkWidget *widget, guint width, guint height, gpointer data)
{
	VteTerminal *terminal;

	if ((GTK_IS_WINDOW(data)) && (width >= 2) && (height >= 2)) {
		gint owidth, oheight, char_width, char_height, column_count, row_count;
		GtkBorder *inner_border;

		terminal = VTE_TERMINAL(widget);

		gtk_window_get_size(GTK_WINDOW(data), &owidth, &oheight);

		/* Take into account border overhead. */
		char_width = vte_terminal_get_char_width (terminal);
		char_height = vte_terminal_get_char_height (terminal);
		column_count = vte_terminal_get_column_count (terminal);
		row_count = vte_terminal_get_row_count (terminal);
		gtk_widget_style_get (widget, "inner-border", &inner_border, NULL);

		owidth -= char_width * column_count;
		oheight -= char_height * row_count;
		if (inner_border != NULL) {
			owidth -= inner_border->left + inner_border->right;
			oheight -= inner_border->top + inner_border->bottom;
		}
		gtk_window_resize(GTK_WINDOW(data),
				  width + owidth, height + oheight);
		gtk_border_free (inner_border);
	}
}

static void
move_window(GtkWidget *widget, guint x, guint y, gpointer data)
{
	GdkWindow *window;

	if (GTK_IS_WIDGET(data)) {
		window = gtk_widget_get_window(GTK_WIDGET(data));
		if (window) {
			gdk_window_move(window, x, y);
		}
	}
}

static void
st_char_size_changed(GtkWidget *widget, guint width, guint height, gpointer data)
{
	VteTerminal *terminal;
	GtkWindow *window;
	GdkGeometry geometry;
	GtkBorder *inner_border;

	g_assert(GTK_IS_WINDOW(data));
	g_assert(VTE_IS_TERMINAL(widget));

	terminal = VTE_TERMINAL(widget);
	window = GTK_WINDOW(data);
	if (!gtk_widget_get_realized (GTK_WIDGET (window)))
		return;

	gtk_widget_style_get (widget, "inner-border", &inner_border, NULL);
	geometry.width_inc = width;
	geometry.height_inc = height;
	geometry.base_width = inner_border ? (inner_border->left + inner_border->right) : 0;
	geometry.base_height = inner_border ? (inner_border->top + inner_border->bottom) : 0;
	geometry.min_width = geometry.base_width + width * 2;
	geometry.min_height = geometry.base_height + height * 2;
	gtk_border_free (inner_border);

	gtk_window_set_geometry_hints(window, widget, &geometry,
				      GDK_HINT_RESIZE_INC |
				      GDK_HINT_BASE_SIZE |
				      GDK_HINT_MIN_SIZE);
}

static void
char_size_realized(GtkWidget *widget, gpointer data)
{
	VteTerminal *terminal;
	GtkWindow *window;
	GdkGeometry geometry;
	guint width, height;
	GtkBorder *inner_border;

	g_assert(GTK_IS_WINDOW(data));
	g_assert(VTE_IS_TERMINAL(widget));

	terminal = VTE_TERMINAL(widget);
	window = GTK_WINDOW(data);
	if (!gtk_widget_get_realized (GTK_WIDGET(window)))
		return;

	gtk_widget_style_get (widget, "inner-border", &inner_border, NULL);
	width = vte_terminal_get_char_width (terminal);
	height = vte_terminal_get_char_height (terminal);
	geometry.width_inc = width;
	geometry.height_inc = height;
	geometry.base_width = inner_border ? (inner_border->left + inner_border->right) : 0;
	geometry.base_height = inner_border ? (inner_border->top + inner_border->bottom) : 0;
	geometry.min_width = geometry.base_width + width * 2;
	geometry.min_height = geometry.base_height + height * 2;
	gtk_border_free (inner_border);

	gtk_window_set_geometry_hints(window, widget, &geometry,
				      GDK_HINT_RESIZE_INC |
				      GDK_HINT_BASE_SIZE |
				      GDK_HINT_MIN_SIZE);
}

static void
adjust_font_size(GtkWidget *widget, gpointer data, gint howmuch)
{
	VteTerminal *terminal;
	PangoFontDescription *desired;
	gint newsize;
	gint columns, rows, owidth, oheight;

	/* Read the screen dimensions in cells. */
	terminal = VTE_TERMINAL(widget);
	columns = vte_terminal_get_column_count(terminal);
	rows = vte_terminal_get_row_count(terminal);

	/* Take into account padding and border overhead. */
	gtk_window_get_size(GTK_WINDOW(data), &owidth, &oheight);
	owidth -= vte_terminal_get_char_width(terminal) * columns;
	oheight -= vte_terminal_get_char_height(terminal) * rows;

	/* Calculate the new font size. */
	desired = pango_font_description_copy(vte_terminal_get_font(terminal));
	newsize = pango_font_description_get_size(desired) / PANGO_SCALE;
	newsize += howmuch;
	pango_font_description_set_size(desired,
					CLAMP(newsize, 4, 144) * PANGO_SCALE);

	/* Change the font, then resize the window so that we have the same
	 * number of rows and columns. */
	vte_terminal_set_font(terminal, desired);
	gtk_window_resize(GTK_WINDOW(data),
			  columns * vte_terminal_get_char_width(terminal) + owidth,
			  rows * vte_terminal_get_char_height(terminal) + oheight);

	pango_font_description_free(desired);
}

static void
increase_font_size(GtkWidget *widget, gpointer data)
{
	adjust_font_size(widget, data, 1);
}

static void
decrease_font_size(GtkWidget *widget, gpointer data)
{
	adjust_font_size(widget, data, -1);
}

static void
add_weak_pointer(GObject *object, GtkWidget **target)
{
	g_object_add_weak_pointer(object, (gpointer*)target);
}





/*
 * Setup/Init
 */
int window_setup()
{
	/* Create a window to hold the scrolling shell, and hook its
	 * delete event to the quit function.. */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_resize_mode(GTK_CONTAINER(window),
				      GTK_RESIZE_IMMEDIATE);
	gtk_window_set_has_resize_grip(GTK_WINDOW(window), false);
	/* gtk_window_set_decorated(GTK_WINDOW(window), false); */

	/* Set ARGB colormap */
	screen = gtk_widget_get_screen (window);
#if GTK_CHECK_VERSION (2, 90, 8)
	visual = gdk_screen_get_rgba_visual(screen);
	if (visual)
		gtk_widget_set_visual(GTK_WIDGET(window), visual);
#else
	colormap = gdk_screen_get_rgba_colormap (screen);
	if (colormap)
	    gtk_widget_set_colormap(window, colormap);
#endif

	/* Create a box to hold everything. */
	hbox = gtk_hbox_new(0, FALSE);
	gtk_container_add(GTK_CONTAINER(window), hbox);

	/* Create the terminal widget and add it to the scrolling shell. */
	widget = vte_terminal_new();
	terminal = VTE_TERMINAL(widget);

	gtk_widget_set_double_buffered(widget, dbuffer);
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);

	/* Connect to the "st_char_size_changed" signal to set geometry hints
	 * whenever the font used by the terminal is changed. */
	if (use_geometry_hints) {
		st_char_size_changed(widget, 0, 0, window);
		g_signal_connect(widget, "char-size-changed",
				 G_CALLBACK(st_char_size_changed), window);
		g_signal_connect(widget, "realize",
				 G_CALLBACK(char_size_realized), window);
	}

	/* Connect to the "window_title_changed" signal to set the main
	 * window's title. */
	g_signal_connect(widget, "window-title-changed",
			 G_CALLBACK(window_title_changed), window);

	/* Connect to application request signals. */
	g_signal_connect(widget, "iconify-window",
			 G_CALLBACK(iconify_window), window);
	g_signal_connect(widget, "deiconify-window",
			 G_CALLBACK(deiconify_window), window);
	g_signal_connect(widget, "raise-window",
			 G_CALLBACK(raise_window), window);
	g_signal_connect(widget, "lower-window",
			 G_CALLBACK(lower_window), window);
	g_signal_connect(widget, "maximize-window",
			 G_CALLBACK(maximize_window), window);
	g_signal_connect(widget, "restore-window",
			 G_CALLBACK(restore_window), window);
	g_signal_connect(widget, "refresh-window",
			 G_CALLBACK(refresh_window), window);
	g_signal_connect(widget, "resize-window",
			 G_CALLBACK(resize_window), window);
	g_signal_connect(widget, "move-window",
			 G_CALLBACK(move_window), window);

	/* Connect to font tweakage. */
	g_signal_connect(widget, "increase-font-size",
			 G_CALLBACK(increase_font_size), window);
	g_signal_connect(widget, "decrease-font-size",
			 G_CALLBACK(decrease_font_size), window);

	g_signal_connect(widget, "child-exited", G_CALLBACK(child_exited), window);
	g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), widget);

	add_weak_pointer(G_OBJECT(widget), &widget);
	add_weak_pointer(G_OBJECT(window), &window);

#if GTK_CHECK_VERSION(2, 91, 0)
	/* As of GTK+ 2.91.0, the default size of a window comes from its minimum
	 * size not its natural size, so we need to set the right default size
	 * explicitly */
	gtk_window_set_default_geometry (GTK_WINDOW (window),
					 vte_terminal_get_column_count (terminal),
					 vte_terminal_get_row_count (terminal));
#endif
	gtk_widget_realize(widget);
	return 0;
}

int terminal_setup(void)
{
	int32_t i = 0;
	/* Set the terminal options */
	vte_terminal_set_audible_bell(terminal, vbell);
	vte_terminal_set_visible_bell(terminal, abell);

	vte_terminal_set_background_transparent(terminal, transparent);
	vte_terminal_set_cursor_blink_mode(terminal, cursor_blink);
	vte_terminal_set_cursor_shape(terminal, cursor_shape);
	vte_terminal_set_emulation(terminal, emulation);
	if(*encoding != '\0')
		vte_terminal_set_encoding(terminal, encoding);
	vte_terminal_set_font_from_string(terminal, font);
	vte_terminal_set_mouse_autohide(terminal, mouse_autohide);
	vte_terminal_set_scrollback_lines(terminal, savelines);
	vte_terminal_set_word_chars(terminal, word_chars);
	vte_terminal_set_allow_bold(terminal, bolding);
	if (*bgimage != '\0')
		vte_terminal_set_background_image_file(terminal, bgimage);

	color_new(&bg_tint, bgtintcolor);
	vte_terminal_set_background_tint_color(terminal, &bg_tint);
	color_new(&bg_gcolor, bgcolor);
	color_new(&fg_gcolor, fgcolor);
	i = color_palette_new(colormapping);
	vte_terminal_set_colors(terminal,
			&fg_gcolor,
			&bg_gcolor,
			gcolor_palette, i);
	vte_terminal_set_opacity(terminal, opacity);
	return 0;
}

int connect_console()
{
	GError *err = NULL;
	GPid pid = -1;
	char *shell = {0};

	if (command == NULL || **command == '\0')
	{
		shell = vte_get_user_shell();
		if(shell == NULL || *shell == '\0')
			shell = (char *)g_getenv ("SHELL");
		if(shell == NULL || *shell == '\0')
			shell = "/bin/sh";

		//login shell
		char *shcmd[] = {shell, "-l", NULL};
		command = shcmd;
	}

	if(! vte_terminal_fork_command_full(terminal,
					   pty_flags, NULL,
					   command,
					   env_add,
					   G_SPAWN_SEARCH_PATH,
					   NULL, NULL,
					   &pid, &err))
	{
		g_warning("Failed to fork: %s\n", err->message);
		g_error_free(err);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int i;
	for(i = 1; i < argc; i++) {
		switch(argv[i][0] != '-' || argv[i][2] ? -1 : argv[i][1]) {
			case 'e':
				if(++i < argc)
					command = &argv[i];
					goto run;
		}
	}

run:
	gtk_init(0, NULL);

	if(window_setup())
		perror("window_setup");

	if(terminal_setup())
		perror("terminal_setup");

	if(connect_console())
		perror("connect_console");

	gtk_widget_show_all(window);

	gtk_main();

	g_assert(widget == NULL);
	g_assert(window == NULL);

	return 0;
}
