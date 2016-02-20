#!/bin/sh

xrandr --auto
xset -dpms
xset s off
openbox-session &
/usr/lib/gnome-settings-daemon/gnome-settings-daemon &

case "$1" in
high)
  gsettings set org.gnome.desktop.interface scaling-factor 2;
  GDK_SCALE=2; export GDK_SCALE;
  GDK_DPI_SCALE=0.5; export GDK_DPI_SCALE;
;;
*)
  gsettings set org.gnome.desktop.interface scaling-factor 2;
;;
esac

DISPLAYMSG="Human and Comparative
Genomics Laboratory"

"/home/reed/Projects/mcmxlii/mcmxlii" -f -w "400" -h "225" -m "4e-6" -s "1.44" -t "${DISPLAYMSG}"
