/*
 * Iconify is a small tool which allow to launch and monitor any
 * program through a simple GTK interface you can reduce.
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. 
 *
 *
 * Copyright 2012 Sylvain Rodon
 *
 */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<libintl.h>

#include<getopt.h>
#include<unistd.h>
#include<wait.h>

#include<gtk/gtk.h>


#define _(String) dgettext("iconify",String)

#define DEFAULT_ICON_PATH "/usr/share/pixmaps/iconify.png"
#define DEFAULT_TITLE "Iconify"
#define DEFAULT_TOOLTIP "Iconify"
#define DEFAULT_WIDTH 500
#define DEFAULT_HEIGHT 300

#define VPADDING 5
#define HPADDING 5

#define fFLAG 0x001
#define sFLAG 0x002

typedef struct {
    char *title;
    char *tooltip;
    char *iconpath;
    int width;
    int height;

    char **program;
    char various;
} args;

typedef struct {
    GtkWidget *window;
    GtkWidget *quitbutton;
    GtkWidget *killbutton;
    GtkStatusIcon *icon;

    gboolean askedforkill;
    int childdeath;
    int childisdead;
    int pid_child;
} app_elems;


int parse_cmdline(int argc, char **argv, args * arguments);
void init_interface(app_elems * interface, args * arguments, int source);
void usage();

void cb_child_is_dead(int pid, int status, gpointer user_data);
void cb_close_or_reduce(GtkWidget * p_widget, gpointer user_data);
void cb_toggle_display(GtkWidget * p_widget, gpointer user_data);
void cb_toggle_line_break(GtkWidget * p_widget, gpointer user_data);
void cb_kill_child(GtkWidget * p_widget, gpointer user_data);


/*
 * The main function checks command line, forks if asked,
 * launches the subprogram, waits for its death and then
 * waits until user closes the GUI.
 */
int main(int argc, char **argv)
{
    args arguments;
    app_elems interface;

    int tmp;
    int child_pipe[2];
    char c;

    gtk_init(&argc, &argv);

    
    memset(&interface, 0, sizeof(app_elems));
    memset(&arguments, 0, sizeof(args));
    
    
    if (!(parse_cmdline(argc, argv, &arguments)))
	return EXIT_FAILURE;


    /* Fork to background (if asked) */
    if (arguments.various & fFLAG) {
	tmp = fork();
	switch (tmp) {
	case -1:
	    perror("fork");
	    return EXIT_FAILURE;
	case 0:
	    if(setsid() == -1)
	        perror("setsid");
	    /* Child will execute the rest of the code */
	    break;
	default:
	    return EXIT_SUCCESS;
	}
    }


    /* Set the signal handler for SIGCHLD */
    /* We can't use classic signal handling method (sigaction) here, 
       because calling gtk_main_quit() wouldn't be safe */
    g_child_watch_add(-1, cb_child_is_dead, &interface);

    /* Sub-program will output through this pipe */
    if (pipe(child_pipe) == -1) {
	perror("pipe");
	return EXIT_FAILURE;
    }

    /* Launch sub-program */
    tmp = fork();
    switch (tmp) {
    case -1:
	perror("fork");
	close(child_pipe[0]);
	close(child_pipe[1]);
	return EXIT_FAILURE;

    case 0:
	/* Child */
	close(child_pipe[0]);

	/* Redirect stdout & stderr to our pipe */
	if ((dup2(child_pipe[1], 1) == -1)
	    || (dup2(child_pipe[1], 2) == -1)) {
	    perror("dup2");
	    return 42;
	}
	close(child_pipe[1]);

	/* Execute the given program */
	if (execvp(*(arguments.program), arguments.program) == -1) {
	    perror("execv");
	    return 42;
	}
	return EXIT_SUCCESS;

    default:
	/* Parent */
	close(child_pipe[1]);

	/* Create GTK interface */
	init_interface(&interface, &arguments, child_pipe[0]);
	interface.pid_child = tmp;
	if (arguments.various & sFLAG)
	    gtk_widget_show_all(GTK_WIDGET(interface.window));

	/* GTK 'infinite' event loop */
	if (!interface.childisdead)
	    gtk_main();


	/* Analyze child death */
	if (WIFEXITED(interface.childdeath)
	    && WEXITSTATUS(interface.childdeath) == 42) {
	    /* Sub-program didn't start */
	    fprintf(stderr, _("Program couldn't be launched ...\n"));
	    while (read(child_pipe[0], &c, 1) == 1) {
		fprintf(stderr, "%c", c);
	    }

	    close(child_pipe[0]);
	    return EXIT_FAILURE;
	} else {
	    /* Sub-program did start and then died */
	    if (!gtk_widget_get_visible(GTK_WIDGET(interface.window))) {
		gtk_widget_show_all(GTK_WIDGET(interface.window));
	    }
	    GtkWidget *messagefin =
		gtk_message_dialog_new(GTK_WINDOW(interface.window),
				       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_WARNING,
				       GTK_BUTTONS_OK,
				       _("Sub-program ended !"));
	    gtk_dialog_run(GTK_DIALOG(messagefin));
	    gtk_widget_destroy(messagefin);
	    gtk_widget_set_sensitive(interface.killbutton, FALSE);
	    gtk_button_set_label(GTK_BUTTON(interface.quitbutton),
				 _("_Quit"));
	    gtk_main();
	}

	close(child_pipe[0]);

	return EXIT_SUCCESS;
    }
}


/*
 * Function: parse_cmdline
 *
 * Parses arguments on the command line, checks coherency,
 * and fills the 'arguments' structure.
 */
int parse_cmdline(int argc, char **argv, args * arguments)
{
    int i;

    arguments->iconpath = DEFAULT_ICON_PATH;
    arguments->title = DEFAULT_TITLE;
    arguments->tooltip = DEFAULT_TOOLTIP;
    arguments->width = DEFAULT_WIDTH;
    arguments->height = DEFAULT_HEIGHT;

    while ((i = getopt(argc, argv, ":i:t:o:h:w:fs")) > 0) {
	switch (i) {
	case 'i':
	    arguments->iconpath = optarg;
	    break;
	case 't':
	    arguments->title = optarg;
	    break;
	case 'o':
	    arguments->tooltip = optarg;
	    break;
	case 'h':
	    if (sscanf(optarg, "%d", &(arguments->height)) != 1)
		fprintf(stderr, _("Invalid height: %s\n"), optarg);
	    break;
	case 'w':
	    if (sscanf(optarg, "%d", &(arguments->width)) != 1)
		fprintf(stderr, _("Invalid width: %s\n"), optarg);
	    break;
	case 'f':
	    arguments->various |= fFLAG;
	    break;
	case 's':
	    arguments->various |= sFLAG;
	    break;
	case ':':
	    fprintf(stderr, _("Missing argument for option: %c\n\n"), optopt);
	    usage();
	    return 0;
	default:
	    fprintf(stderr, _("Unknown option: %c\n\n"), optopt);
	    usage();
	    return 0;
	}
    }

    if (optind >= argc) {
	fprintf(stderr, _("Too few arguments\n\n"));
	usage();
	return 0;
    }

    arguments->program = argv + optind;

    return 1;
}


/*
 * Function: usage
 *
 * Print usage.
 */
void usage()
{
    fprintf(stderr, "Usage: iconify [-fs] \n");
    fprintf(stderr,
	    "\t[-i <icon>] [-t <windowTitle>] [-o <tooltipText>]\n");
    fprintf(stderr, "\t[-w <width>] [-h <height>]\n");
    fprintf(stderr, "\t-- <program> [programOption]\n");

    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "\t-f Fork to background\n");
    fprintf(stderr, "\t-s Show the window when starting\n");
    fprintf(stderr, "\t-i Path to the notification icon to use\n");
    fprintf(stderr, "\t-t Title of the window\n");
    fprintf(stderr, "\t-o Tooltip text to show\n");
    fprintf(stderr, "\t-w Width of the window\n");
    fprintf(stderr, "\t-h Height of the window\n");
}


/*
 * Function: refresh_textview_from_fd
 *
 * This function is called when data are readable from our pipe.
 * It reads the data and adds it to the GTK textview.
 */
gboolean refresh_textview_from_fd(GIOChannel * channel,
				  GIOCondition condition, gpointer data)
{
    GtkTextView *view = GTK_TEXT_VIEW(data);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

    gchar buf[1024];
    gsize len;
    GError *error = NULL;
    gint status;


    while ((status =
	    g_io_channel_read_chars(channel, buf, 1024, &len, &error))
	   == G_IO_STATUS_AGAIN) {

	while (gtk_events_pending())
	    gtk_main_iteration();
    }

    if (status != G_IO_STATUS_NORMAL) {
	/* status = G_IO_STATUS_ERROR or G_IO_STATUS_EOF */
	if (error) {
	    fprintf(stderr, _("Error while reading sub-child output : %s"),
		    error->message);
	    g_error_free(error);
	    error = NULL;
	}
	return FALSE;
    }

    if (len > 0) {
	GtkTextIter end;
	GtkTextMark *mark;
	gchar *utftext;
	gsize localelen;
	gsize utflen;

	gtk_text_buffer_get_end_iter(buffer, &end);

	if (!g_utf8_validate(buf, len, NULL)) {
	    utftext =
		g_convert_with_fallback(buf, len, "UTF-8",
					"ISO-8859-1", NULL, &localelen,
					&utflen, NULL);
	    gtk_text_buffer_insert(buffer, &end, utftext, utflen);
	    g_free(utftext);
	} else {
	    gtk_text_buffer_insert(buffer, &end, buf, len);
	}

	/* Scroll down TextView */
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(view), mark, 0.0, FALSE,
				     0.0, 1.0);
    }
    return TRUE;
}


/*
 * Function: add_textview_refresher
 *
 * Adds a watch in order to display (when available) data from the
 * given file descriptor in the given GTK textview.
 */
void add_textview_refresher(GtkTextView * text_view, int fd)
{
    GIOChannel *channel = g_io_channel_unix_new(fd);
    g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);
    g_io_add_watch(channel, G_IO_IN | G_IO_HUP, refresh_textview_from_fd,
		   text_view);
}


/*
 * Function: init_interface
 *
 * Creates the GUI according to 'arguments' structure.
 */
void init_interface(app_elems * interface, args * arguments, int source)
{
    interface->askedforkill = FALSE;

    GdkPixbuf *pixbuf;
    GError *error = NULL;
    int i, size = 10;

    /* Heading label text (sub-program command line) */
    for (i = 0; arguments->program[i] != NULL; i++) {
	size += strlen(arguments->program[i]) + 1;
    }
    char command[size];
    command[0] = '\0';
    for (i = 0; arguments->program[i] != NULL; i++) {
	strcat(command, arguments->program[i]);
	strncat(command, " ", size);
    }


    /* Main window */
    interface->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(interface->window), arguments->title);
    gtk_window_set_default_size(GTK_WINDOW(interface->window),
				arguments->width, arguments->height);
    gtk_container_set_border_width(GTK_CONTAINER(interface->window), 10);
    g_signal_connect(G_OBJECT(interface->window), "destroy",
		     G_CALLBACK(cb_close_or_reduce), interface);


    /* Notification icon */
    interface->icon = gtk_status_icon_new_from_file(arguments->iconpath);
    pixbuf = gdk_pixbuf_new_from_file(arguments->iconpath, &error);
    if (pixbuf == NULL) {
	fprintf(stderr, _("Can't find specified icon\n"));
    } else {
	gtk_window_set_icon(GTK_WINDOW(interface->window), pixbuf);
    }

    gtk_status_icon_set_tooltip_text(GTK_STATUS_ICON(interface->icon),
				     arguments->tooltip);
    gtk_status_icon_set_visible(GTK_STATUS_ICON(interface->icon), TRUE);
    g_signal_connect(GTK_STATUS_ICON(interface->icon), "activate",
		     G_CALLBACK(cb_toggle_display), interface);

    /* Body */
    /* To compile with GTK+2.0, replace by:
       GtkWidget *vb1 = gtk_vbox_new(FALSE, VPADDING);
       GtkWidget *hb1 = gtk_hbox_new(FALSE, HPADDING);
     */
    GtkWidget *vb1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VPADDING);
    GtkWidget *hb1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, HPADDING);

    /* Quit button */
    GtkWidget *b1 = gtk_button_new_with_mnemonic(_("_Hide"));
    g_signal_connect(G_OBJECT(b1), "clicked",
		     G_CALLBACK(cb_close_or_reduce), interface);
    interface->quitbutton = b1;

    /* Kill button */
    GtkWidget *b2 = gtk_button_new_with_mnemonic(_("_Stop (SIGINT)"));
    g_signal_connect(G_OBJECT(b2), "clicked", G_CALLBACK(cb_kill_child),
		     interface);
    interface->killbutton = b2;


    //STDOUT & STDERR Scrolling TextView
    GtkWidget *stdout_child = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(stdout_child), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(stdout_child), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(stdout_child),
				GTK_WRAP_CHAR);
    gtk_widget_modify_font(stdout_child,
			   pango_font_description_from_string
			   ("monospace"));

    add_textview_refresher(GTK_TEXT_VIEW(stdout_child), source);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll),
					GTK_SHADOW_IN);

    gtk_container_add(GTK_CONTAINER(scroll), stdout_child);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				   GTK_POLICY_AUTOMATIC,
				   GTK_POLICY_AUTOMATIC);

    /* Heading label */
    GtkWidget *command_line = gtk_label_new(command);
    gtk_label_set_line_wrap(GTK_LABEL(command_line), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(command_line), GTK_WRAP_CHAR);
    gtk_widget_modify_font(command_line,
			   pango_font_description_from_string("bold"));

    /* Line break checkbox */
    GtkWidget *ch1 = gtk_check_button_new_with_mnemonic(_("_Line breaks"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ch1), TRUE);
    g_signal_connect(G_OBJECT(ch1), "toggled",
		     G_CALLBACK(cb_toggle_line_break), stdout_child);


    /* Link all widgets together */
    gtk_box_pack_end(GTK_BOX(hb1), b1, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hb1), b2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hb1), ch1, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vb1), command_line, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb1), scroll, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vb1), hb1, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(interface->window), vb1);
}


/*
 * Callback functions 
 */

/* Called when a child is dead */
void cb_child_is_dead(int pid, int status, gpointer data)
{
    ((app_elems *) data)->childisdead = TRUE;
    ((app_elems *) data)->childdeath = status;
    if (gtk_main_level() > 0)
	gtk_main_quit();
}


/* Called when user clicked on the cross/quit button */
void cb_close_or_reduce(GtkWidget * p_widget, gpointer user_data)
{
    if (((app_elems *) user_data)->childisdead)
	gtk_main_quit();
    else
	cb_toggle_display(p_widget, user_data);
}


/* Called when user clicked on notification icon */
void cb_toggle_display(GtkWidget * p_widget, gpointer user_data)
{
    app_elems *interface = user_data;
    if (gtk_widget_get_visible(GTK_WIDGET(interface->window)) == TRUE)
	gtk_widget_hide(GTK_WIDGET(interface->window));
    else
	gtk_widget_show_all(GTK_WIDGET(interface->window));
}


/* Called when user clicked on notification icon */
void cb_toggle_line_break(GtkWidget * p_widget, gpointer user_data)
{
    GtkWidget *textview = user_data;
    if (gtk_text_view_get_wrap_mode(GTK_TEXT_VIEW(textview))
	== GTK_WRAP_CHAR)
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview),
				    GTK_WRAP_NONE);
    else
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview),
				    GTK_WRAP_CHAR);
}


/* Called when user clicked on the "kill" button */
void cb_kill_child(GtkWidget * p_widget, gpointer user_data)
{
    app_elems *interface = user_data;
    if (interface->askedforkill == FALSE) {
	/* If it's the first click, send SIGINT */
	if (kill(interface->pid_child, SIGINT) < 0) {
	    perror("kill");
	} else {
	    gtk_button_set_label(GTK_BUTTON(interface->killbutton),
				 _("_Stop (SIGKILL)"));
	    interface->askedforkill = TRUE;
	}
    } else {
	/* If it's the second click, send SIGKILL */
	if (kill(interface->pid_child, SIGKILL) < 0) {
	    perror("kill");
	} else {
	    interface->askedforkill = TRUE;
	}
    }
}
