Iconify
=======

Iconify is a tool written in C which allow to run any command in a simple GTK interface that can be minimized to an icon in the notification bar.

## Usage
 
    Usage: iconify [-fs] 
        [-i <icon>] [-t <windowTitle>] [-o <tooltipText>]
        [-w <width>] [-h <height>]
        -- <program> [programOption]
    
    Options:
        -f Fork to background
        -s Show the window when starting
        -i Path to the notification icon to use
        -t Title of the window
        -o Tooltip text to show
        -w Width of the window
        -h Height of the window