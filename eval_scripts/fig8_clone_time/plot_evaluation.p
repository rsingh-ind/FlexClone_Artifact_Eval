set terminal postscript eps enhanced color
set autoscale
unset grid

set key font ", 35"
set xtics font ", 30"
set ytics font ", 32"
set ylabel font "helvetica, 45"
set xlabel font "helvetica, 45"
set bmargin 7
set lmargin 37
set tmargin 2

set xlabel "File size " offset 0,-2
set ylabel "Copy Time(ms)" offset -1,0
set xtic auto offset 0, -1
set ytic auto
set logscale y 10
set xr [1:]
set yr [0:]
set key width 5
set key at 4.5,40000


set output 'ssd_copying_file_data_evaluation.eps'
set lmargin 20
set ylabel ""
set key at 4.5,50000
set xlabel "File size " offset 0,-2
set ylabel "Copy Time(ms)" offset -6,0

plot "ssd_copying_file_data_evaluation" using 1:3:xtic(2) t "Btrfs" w linespoints  lc rgb "#FF0000" pt 2 ps 3 lw 7,\
     '' using 1:5:xtic(2) t "Ext4" w linespoints lc 3 pt 5 ps 3 lw 7,\
     '' using 1:7:xtic(2) t "Ext4-FC" w linespoints lc rgb "#FFA500" pt 3 ps 3 lw 7,\
     '' using 1:9:xtic(2) t "XFS" w linespoints lc rgb "#228B22" pt 6 ps 4 lw 7,\
     '' using 1:11:xtic(2) t "Qcow2" w linespoints lc 1 pt 6 ps 3 lw 7,\
