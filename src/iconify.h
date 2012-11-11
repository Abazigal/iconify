#ifndef ICONIFY_H
#define ICONIFY_H

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

#endif
