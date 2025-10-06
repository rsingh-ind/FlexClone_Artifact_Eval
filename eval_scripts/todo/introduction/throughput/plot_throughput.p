set terminal postscript eps enhanced color lw 1.5
#set term postscript eps enhanced monochrome 20 dashed dashlength 1 lw 1.5
#set size ratio 0.75
set lmargin 40 
set bmargin 7
set tmargin 5
set key font ", 30"
set xtics font ", 30" offset 0,-0.5
set ytics font ", 32"
set ylabel font "helvetica, 43"
set xlabel font "helvetica, 45"
set xlabel "Filesystem" offset 0,-2
set ylabel "Throughput(ops/s)" offset -6
#set logscale y 10
set ytic auto
#set format y "10^{%L}"
set yrange [1:]
#set ytic 50
set boxwidth 1.2 relative
set style data histograms
set style histogram cluster gap 1 errorbars linewidth 1.5
set style fill solid border -1
#set key spacing 1
set key samplen 2.5
set key horizontal center tmargin
set offset graph -0.1 , -0.15
set arrow from 0.5,12170 to 0.5,10550 head
set arrow from 0.2,10600 to 0.8,10600 dashtype 2 linewidth 2 nohead
set arrow from -0.6,12170 to 2.85,12170 dashtype 2 linewidth 2 nohead
set label "-16.6%" at 0.55,11300 font "Arial,28"

#set ylabel ""
#set lmargin 12

set output "plot_throughput.eps"
plot 'summary_throughput' u 2:3:xtic(1) title "" fillstyle pattern 5 lc rgb "#0080FF",\
