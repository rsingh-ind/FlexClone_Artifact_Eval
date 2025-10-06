set terminal postscript eps enhanced color
set autoscale
unset grid

set key font ", 35"
set xtics font ", 30"
set ytics font ", 32"
set ylabel font "helvetica, 45"
set xlabel font "helvetica, 45"
set bmargin 7
set lmargin 40
set tmargin 2
set rmargin 4

set xlabel "Runtime(secs) " offset 4,-2
set ylabel "Mem Usage(%)" offset -3,2
set xtic rotate by 0 offset 0, -1
set ytic auto
#set logscale y 10
set xr [0:]
set yr [1:]
set key width 5
set key at 600,50
set xtics autofreq 100
#set arrow from 350,60 to 300,70 head


set output 'plot_mem_usage.eps'
plot "all_mem_usage" every 50 using 1:2 t "Btrfs" w linespoints  lc 2 pt 2 ps 3 lw 7,\
        '' every 50 using 1:3 t "XFS" w linespoints lc 3 pt 5 ps 3 lw 7,\
        '' every 50 using 1:4 t "Ext4-FC" w linespoints lc 7 pt 3 ps 3 lw 7,\
