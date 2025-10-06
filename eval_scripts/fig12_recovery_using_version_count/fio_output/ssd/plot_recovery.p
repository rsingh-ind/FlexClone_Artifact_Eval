set terminal postscript eps enhanced color
set autoscale
unset grid

set key font ", 41"
set xtics font ", 35"
set ytics font ", 35"
set ylabel font "helvetica, 50"
set xlabel font "helvetica, 50"
set bmargin 7
set lmargin 15
set tmargin 2

set xlabel "Files recovered (x1000)" offset -2.5,-2
set ylabel "Time (secs)" offset -6
set xtic auto offset 0, 0 
set ytic auto offset -1, 0 
set xr [0:10]
set yr [0:]
unset key
set grid x
set grid y

set output 'recovery_time.eps'
plot "recovery_time_summary" using ($2/1000):3 t "" w linespoints  lc 1 pt 3 ps 5 lw 10
