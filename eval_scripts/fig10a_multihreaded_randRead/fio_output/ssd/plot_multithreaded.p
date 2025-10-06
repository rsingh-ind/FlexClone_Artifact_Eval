set terminal postscript eps enhanced color

set autoscale
unset grid

set key font ", 35"
set xtics font ", 35"
set ytics font ", 35"
set ylabel font "helvetica, 45"
set xlabel font "helvetica, 45"
set bmargin 7
set lmargin 17

set xlabel "Number of threads" offset 0,-2
set ylabel "Throughput (MiB/s)" offset -5.5,-2
set xtic auto offset 0, -1
set ytic auto
set yrange [0:]
#set logscale y 10
set xr [1:]
set yr [0:]
set key width 5
set key at 3.55,340

#set output 'ssd_randRead_coldCache_3072bs.eps'
#plot "ssd_randRead_coldCache_3072bs" using 1:3:xtic(2) t "BtrFS" w linespoints  lc 2 pt 2 ps 3.5 lw 8,\
#     '' using 1:5:xtic(2) t "Ext4" w linespoints lc 3 pt 5 ps 3.5 lw 8,\
#     '' using 1:7:xtic(2) t "FlexClone" w linespoints lc 12 pt 8 ps 3.5 lw 8,\
#     '' using 1:9:xtic(2) t "XFS" w linespoints lc 7 pt 6 ps 3.5 lw 8,\

set lmargin 12
set ylabel ""
#set output 'ssd_seqRead_coldCache_3072bs.eps'
#plot "ssd_seqRead_coldCache_3072bs" using 1:3:xtic(2) t "" w linespoints  lc 2 pt 2 ps 3.5 lw 8,\
#     '' using 1:5:xtic(2) t "" w linespoints lc 3 pt 5 ps 3.5 lw 8,\
#     '' using 1:7:xtic(2) t "" w linespoints lc 12 pt 8 ps 3.5 lw 8,\
#     '' using 1:9:xtic(2) t "" w linespoints lc 7 pt 6 ps 3.5 lw 8,\

#set output 'ssd_randWrite_coldCache_3072bs.eps'
#plot "ssd_randWrite_coldCache_3072bs" using 1:3:xtic(2) t "" w linespoints  lc 2 pt 2 ps 3.5 lw 8,\
#     '' using 1:5:xtic(2) t "" w linespoints lc 3 pt 5 ps 3.5 lw 8,\
#     '' using 1:7:xtic(2) t "" w linespoints lc 12 pt 8 ps 3.5 lw 8,\
#     '' using 1:9:xtic(2) t "" w linespoints lc 7 pt 6 ps 3.5 lw 8,\

#set output 'ssd_seqWrite_coldCache_3072bs.eps'
#plot "ssd_seqWrite_coldCache_3072bs" using 1:3:xtic(2) t "" w linespoints  lc 2 pt 2 ps 3.5 lw 8,\
#     '' using 1:5:xtic(2) t "" w linespoints lc 3 pt 5 ps 3.5 lw 8,\
#     '' using 1:7:xtic(2) t "" w linespoints lc 12 pt 8 ps 3.5 lw 8,\
#     '' using 1:9:xtic(2) t "" w linespoints lc 7 pt 6 ps 3.5 lw 8,\

set ylabel "Throughput (MiB/s)" offset -5.5,-2
set lmargin 17

set output 'ssd_randRead_coldCache_4096bs.eps'
set key at 3.65,340
plot "ssd_randRead_coldCache_4096bs" using 1:3:xtic(2) t "BtrFS" w linespoints  lc rgb "#FF0000" pt 2 ps 3.5 lw 8,\
     '' using 1:5:xtic(2) t "Ext4" w linespoints lc 3 pt 5 ps 3.5 lw 8,\
     '' using 1:7:xtic(2) t "Ext4-FC" w linespoints lc rgb "#FFA500" pt 8 ps 3.5 lw 8,\
     '' using 1:9:xtic(2) t "XFS" w linespoints lc rgb "#228B22" pt 6 ps 3.5 lw 8,\

#set lmargin 12
#set ylabel ""
#set output 'ssd_seqRead_coldCache_4096bs.eps'
#plot "ssd_seqRead_coldCache_4096bs" using 1:3:xtic(2) t "" w linespoints  lc 2 pt 2 ps 3.5 lw 8,\
#     '' using 1:5:xtic(2) t "" w linespoints lc 3 pt 5 ps 3.5 lw 8,\
#     '' using 1:7:xtic(2) t "" w linespoints lc 12 pt 8 ps 3.5 lw 8,\
#     '' using 1:9:xtic(2) t "" w linespoints lc 7 pt 6 ps 3.5 lw 8,\

#set output 'ssd_randWrite_coldCache_4096bs.eps'
#plot "ssd_randWrite_coldCache_4096bs" using 1:3:xtic(2) t "" w linespoints  lc rgb "#FF0000" pt 2 ps 3.5 lw 8,\
#     '' using 1:5:xtic(2) t "" w linespoints lc 3 pt 5 ps 3.5 lw 8,\
#     '' using 1:7:xtic(2) t "" w linespoints lc rgb "#FFA500" pt 8 ps 3.5 lw 8,\
#     '' using 1:9:xtic(2) t "" w linespoints lc rgb "#228B22" pt 6 ps 3.5 lw 8,\

#set output 'ssd_seqWrite_coldCache_4096bs.eps'
#plot "ssd_seqWrite_coldCache_4096bs" using 1:3:xtic(2) t "" w linespoints  lc 2 pt 2 ps 3.5 lw 8,\
#     '' using 1:5:xtic(2) t "" w linespoints lc 3 pt 5 ps 3.5 lw 8,\
#     '' using 1:7:xtic(2) t "" w linespoints lc 12 pt 8 ps 3.5 lw 8,\
#     '' using 1:9:xtic(2) t "" w linespoints lc 7 pt 6 ps 3.5 lw 8,\

