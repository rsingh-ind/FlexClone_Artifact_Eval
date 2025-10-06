set terminal postscript eps enhanced color
set autoscale
unset grid

set key font ", 36"
set xtics font ", 40"
set ytics font ", 40"
set ylabel font "helvetica, 55"
set xlabel font "helvetica, 55"
set bmargin 7
set lmargin 12
set tmargin 5
set rmargin 4

set xlabel "Runtime (secs) " offset 0,-2
set ylabel "CPU Util(%)" offset -5,0
set xtic rotate by 0 offset 0, -1
set ytic auto
#set logscale y 10
set xr [0:]
set yr [1:10]
set key width 3
#set key at 4.5,10
#set xtics autofreq 100
#set arrow from 350,60 to 300,70 head
set key spacing 1
set key samplen 2.5
set key horizontal tmargin left
#set key at graph .85,1.2 horizontal


set yr [1:10]
set key width 1
set key spacing 2
set key samplen 2.5
set key horizontal tmargin center
set ylabel "CPU Util(%)" offset -3,0
set output 'plot_seqWrite_cpu_util.eps'
plot "all_seqWrite_cpu_util" using 1:2 t "Btrfs" w linespoints  lc rgb "#FF0000" pt 2 ps 3 lw 7,\
        '' using 1:3 t "XFS" w linespoints lc rgb "#228B22" pt 5 ps 3 lw 7,\
        '' using 1:4 t "" w linespoints lc 3 pt 3 ps 3 lw 7,\
        '' using 1:5 t "" w linespoints lc  rgb "#FFA500" pt 4 ps 3 lw 7,\

#set yr [1:20]
#set key width 1
#set key spacing 2
#set key samplen 2.5
#set key horizontal tmargin center
#set lmargin 14
##set ylabel "CPU Util(%)" offset -5,0
#unset ylabel
#set output 'plot_randWrite_cpu_util.eps'
#plot "all_randWrite_cpu_util" every 2 using 1:2 t "" w linespoints  lc rgb "#FF0000" pt 2 ps 3 lw 7,\
#        '' every 2 using 1:3 t "" w linespoints lc rgb "#228B22"  pt 5 ps 3 lw 7,\
#        '' every 2 using 1:4 t "Ext4" w linespoints lc 3 pt 3 ps 3 lw 7,\
#        '' every 2 using 1:5 t "Ext4-FC" w linespoints lc rgb "#FFA500"  pt 4 ps 3 lw 7,\
